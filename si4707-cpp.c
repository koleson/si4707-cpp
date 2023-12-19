#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"

#include "hardware/timer.h"

#include "pico/unique_id.h"

#include "hardware.h"
#include "si4707_const.h"

#include "si4707.h"
#include "mqtt-publisher.h"

typedef enum { IDLE=0, RECEIVING_HEADER, HEADER_READY, ALERT_TONE, BROADCAST, EOM_WAIT } System_State;
System_State system_state = IDLE;

static uint64_t gs_first_EOM_timestamp_us = 0;
static bool reset_SAME_interrupts_and_buffer_on_next_status_check = false;
static uint64_t gs_consecutive_idle_handler_executions = 0;

void idle_handler(const struct Si4707_SAME_Status_Packet *status) {
    if (status->PREDET == 1) {
        printf("\n\n=== ZCZC - RECEIVING MESSAGE ===");
        system_state = RECEIVING_HEADER;
        gs_consecutive_idle_handler_executions = 0;
    }
    
    gs_consecutive_idle_handler_executions++;
    if (gs_consecutive_idle_handler_executions % 5 == 0) {
        printf("i");
    }
};

void receiving_header_handler(const struct Si4707_SAME_Status_Packet *status) {
    if (status->HDRRDY == 1) {
        printf("\n\n=== SAME HEADER RECEIVED AND READY ===");
        system_state = HEADER_READY;
    }
    printf("r");
};

void header_ready_handler(const struct Si4707_SAME_Status_Packet *status) {
    // TODO:  handle alert tone by getting ASQ status
    // kmo 6 dec 2023 18h34

    if (status->EOMDET == 1) {
        printf("\n\n=== EOM RECEIVED - WAITING FOR EOM TIMEOUT ===");
        gs_first_EOM_timestamp_us = time_us_64();
        system_state = EOM_WAIT;

    }
    printf("h");
};

// TODO:  currently not handling alert tone or broadcast specially - requires ASQ data
// kmo 6 dec 2023
void alert_tone_handler(const struct Si4707_SAME_Status_Packet *status) {};
void broadcast_handler(const struct Si4707_SAME_Status_Packet *status) {};


void eom_wait_handler(const struct Si4707_SAME_Status_Packet *status) {
    // TODO:  implementing this correctly requires passing in timing information
    // (or getting it directly, but in general prefer testability of passing in time)
    uint64_t now = time_us_64();
    if (now - gs_first_EOM_timestamp_us > 5000000) {
        // TODO:  reset interrupts, etc.
        printf("\n\n=== EOM TIMEOUT COMPLETE - RESETTING INTERRUPTS ===");
        // CRITICAL:  MUST RESET INTERRUPTS / STATUS BEFORE RESETTING system_state TO IDLE!
        // otherwise you will fly through all the various states unendingly.
        // kmo 6 dec 2023 18h43
        reset_SAME_interrupts_and_buffer_on_next_status_check = true;
    }  
    printf("e");
};

// system state machine

typedef void (*System_State_Handler)(const struct Si4707_SAME_Status_Packet *);
static System_State_Handler state_functions[] = {
        idle_handler, receiving_header_handler, header_ready_handler,
                                        alert_tone_handler, broadcast_handler, eom_wait_handler};


bool g_Si4707_booted_successfully = false;
char g_board_id_string[32];

int64_t g_current_heartbeat_interval = 5000000;    // 5000000 microseconds = 5 seconds

void prepare() {
    // this is also called in mqtt-publisher.c which i find suspect.  kmo 11 nov 2023 13h50
    stdio_init_all();

    puts("\n\n\n========================================================");
    puts("si4707-cpp: prepare()");

    // prep LED GPIO
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
}

uint64_t maintain_dhcp_lease(uint64_t dhcp_interval, uint64_t now, uint64_t last_DHCP_run) {
    const uint64_t microseconds_since_last_DHCP_run = now - last_DHCP_run;
    if (microseconds_since_last_DHCP_run > dhcp_interval) {
        last_DHCP_run = now;
        puts("updating DHCP");
        dhcp_run_wrapper();
        puts("done updating DHCP");
    } else {
        const uint64_t microseconds_to_next_DHCP_run = dhcp_interval - microseconds_since_last_DHCP_run;
        const int seconds_to_next_DHCP_run = floor(
                microseconds_to_next_DHCP_run / 1000000.0); // NOLINT(*-narrowing-conversions)
        printf("next DHCP run in about %d seconds\n", seconds_to_next_DHCP_run);
    }
    return last_DHCP_run;
}

void construct_and_publish_heartbeat(int main_loops, const struct Si4707_RSQ_Status *rsq_status) {
    struct Si4707_Heartbeat heartbeat;
    heartbeat.iteration = main_loops;
    heartbeat.si4707_started = g_Si4707_booted_successfully;
    heartbeat.snr = (*rsq_status).ASNR;
    heartbeat.rssi = (*rsq_status).RSSI;
    publish_heartbeat(&heartbeat);
}

void get_and_publish_full_SAME_status(const struct Si4707_SAME_Status_Params *same_params) {
    struct Si4707_SAME_Status_FullResponse same_status;
    const bool same_cts = await_si4707_cts(100);
    if (same_cts) {
        if (same_params->CLRBUF) {
            puts("get_and_publish_full_SAME_status:  clearing buffer");
        }
        get_si4707_same_status(same_params, &same_status);
    } else {
        puts("SAME status CTS timed out :(");
    }

    //printf("printing SAME status\n");
    print_si4707_same_status(&same_status);
    //printf("publishing SAME status\n");
    publish_SAME_status(&same_status);

    free_Si4707_SAME_Status_FullResponse(&same_status);
}

void set_heartbeat_interval_for_SAME_state(const int same_state) {
    // TODO:  make SAME state constants.  kmo 22 nov 2023 11h33
    // NB: misnomer - "EOM" also state when no message has yet been received.
    // kmo 6 dec 2023 15h31
    if (same_state == SI4707_SAME_STATE_END_OF_MESSAGE) {
        if (g_current_heartbeat_interval < 10000000) {
            printf("\n\nSAME state 0 - reducing heartbeat interval to 10 seconds");
            g_current_heartbeat_interval = 10000000;
        }
    } else {
        // TODO:  reduce this even further - say, 0.2 or 0.1 seconds.  kmo 22 nov 2023 11h35
        if (g_current_heartbeat_interval > 500000) {
            // 0.5 seconds
            printf("\n\nSAME state > 0 (%d) - reducing heartbeat interval to 0.5 seconds", same_state);


            g_current_heartbeat_interval = 500000;
        }
    }
}

int oneshot() {
    prepare();

    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);

    printf("====== board ID: %llx\n", board_id);
    printf("===== bytes: %02x %02x [...] %02x %02x ==========\n\n", board_id.id[0], board_id.id[1], board_id.id[6],
           board_id.id[7]);
    printf("proposed MAC ending: %02x:%02x\n", board_id.id[5], board_id.id[4]);
    for (int len = 0; len < 8; len++) {
        printf("%d: %02X /", len, board_id.id[len]);
    }

    snprintf(g_board_id_string, 32, "si4707/%x%x", board_id.id[5], board_id.id[4]);
    update_root_topic(g_board_id_string);

    printf("\n\n\n");
    // fflush(stdout);
    puts("initializing MQTT...");
    puts("sleeping before init_mqtt");
    sleep_ms(10);
    puts("done sleeping before init_mqtt");
    init_mqtt();
    puts("... done initializing MQTT.");

    // resetting to SPI mode requires
    // GPO2 *AND* GPO1 are high.  GPO2 must be driven (easy, it has no other use here)
    // GPO1 can float or be driven - since it's used for SPI, we have to deinit it before
    // reset_si4707 ends.  it seems easiest to drive it momentarily to make sure.

    reset_si4707();

    setup_si4707_spi();

    power_up_si4707();
    const int cts = await_si4707_cts(500);
    if (cts) {
        puts("si4707 CTS - getting rev and tuning");
        get_si4707_rev();
        tune_si4707();

        g_Si4707_booted_successfully = true;
    } else {
        puts("failed to start si4707 :(");
    }
}

int main() {
    oneshot();

    // must `prepare` before trying to print this message.  kmo 9 oct 2023 17h29
    puts("si4707-cpp: main() after oneshot()");

    int main_loops = 0;
    int outer_loops_since_last_heartbeat = 0;
    uint64_t last_heartbeat = 0;
    uint64_t last_DHCP_run = 0;

    static uint64_t dhcp_interval = (uint64_t) 60 * (uint64_t) 60 * 1000000; // 60 minutes
    // static uint64_t dhcp_interval = (uint64_t)20 * (uint64_t)1000000; // 60 seconds
    if (!g_Si4707_booted_successfully) {
        printf("Si4707 did not boot successfully, calling abort()...");
        abort();
    }

    
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"        // yes, we know, thanks.
    // superloop
    while (true) {
        //printf("superloop outer\n\n");

        struct Si4707_SAME_Status_Packet same_packet;
        struct Si4707_SAME_Status_Params same_params;

        if (reset_SAME_interrupts_and_buffer_on_next_status_check) 
        {
            reset_SAME_interrupts_and_buffer_on_next_status_check = false;
            same_params.CLRBUF = true;
            same_params.INTACK = true;
        } 
        else 
        {
            same_params.CLRBUF = false;
            same_params.INTACK = false;
        }
        
        struct Si4707_RSQ_Status rsq_status;
        uint8_t status = 0;

        const bool rsq_cts = await_si4707_cts(100);
        if (rsq_cts) {
            status = read_status();
            get_si4707_rsq(&rsq_status);
        } else {
            puts("RSQ/SAME status CTS timed out :(");
        }

        const bool same_packet_cts = await_si4707_cts(100);
        if (same_packet_cts) {
            get_si4707_same_packet(&same_params, &same_packet);

            // TODO:  move all state logic into state_functions
            state_functions[system_state](&same_packet);
            set_heartbeat_interval_for_SAME_state(same_packet.STATE);
        }


        if (outer_loops_since_last_heartbeat % 5 == 0) {
            printf(".");
            //printf("outerloop %d\n", outer_loops_since_last_heartbeat);
        }


        const uint64_t now = time_us_64();
        const uint64_t microseconds_since_last_heartbeat = now - last_heartbeat;


        if (microseconds_since_last_heartbeat > g_current_heartbeat_interval) {
            gpio_put(PICO_DEFAULT_LED_PIN, 1);
            printf("\n ====== heartbeating ======================================\n");

            print_si4707_rsq();

            if (!(status & 0x01)) {
                puts("tune invalid :(");
                printf("(status %d)\n", status);
            }

            construct_and_publish_heartbeat(main_loops, &rsq_status);
            last_heartbeat = now;
            outer_loops_since_last_heartbeat = 0;

            get_and_publish_full_SAME_status(&same_params);

            last_DHCP_run = maintain_dhcp_lease(dhcp_interval, now, last_DHCP_run);

            gpio_put(PICO_DEFAULT_LED_PIN, 0);
            main_loops++;
        }


        busy_wait_ms(10);
        outer_loops_since_last_heartbeat++;
    }
#pragma clang diagnostic pop

    return 0;
}
