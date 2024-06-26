#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include <stdlib.h>

#include "hardware/timer.h"

#include "pico/unique_id.h"

#include "system_state.h"

#include "hardware.h"
#include "si4707_const.h"

#include "si4707.h"

#if SI4707_WIZNET
#include "mqtt-publisher.h"
#endif // SI4707_WIZNET

#if SI4707_PICO_W
#include "lwipopts.h"
#endif // SI4707_PICO_W

#include "si4707_hal.h"

// TODO:  conditionalize HAL inclusion based on target info
#include "si4707_hal_rp2040_spi.h"

#define STATE_PRINT_INTERVAL 10

// forward declarations
void reset_SAME_interrupts_and_buffer();
// end forward declarations

System_State system_state = IDLE;

static uint64_t gs_first_EOM_timestamp_us = 0;
static uint64_t gs_consecutive_idle_handler_executions = 0;

void idle_handler(const struct Si4707_SAME_Status_Packet *status) {
    if (status->PREDET == 1) {
        printf("\n\n=== ZCZC - RECEIVING MESSAGE ===\n");
        System_State previous_state = system_state;
        system_state = RECEIVING_HEADER;
        gs_consecutive_idle_handler_executions = 0;
        printf("system_state: moving to state RECEIVING_HEADER\n");
        publish_system_state(previous_state, system_state); 
        return;
    }
    
    gs_consecutive_idle_handler_executions++;
    if (gs_consecutive_idle_handler_executions % STATE_PRINT_INTERVAL == 0) {
        printf("i");
    }
};

void receiving_header_handler(const struct Si4707_SAME_Status_Packet *status) {
    if (status->HDRRDY == 1) {
        printf("\n\n=== SAME HEADER RECEIVED AND READY ===\n");
        System_State previous_state = system_state;
        system_state = HEADER_READY;
        printf("system_state: moving to state HEADER_READY\n");
        publish_system_state(previous_state, system_state);
        return;
    }
    printf("r");
};

void header_ready_handler(const struct Si4707_SAME_Status_Packet *same_status) {
    struct Si4707_ASQ_Status asq_status;
    si4707_asq_get(&asq_status, false);
    
    if (asq_status.ALERT == 1)
    {
        printf("\n\n=== ALERT TONE ON ===\n");
        System_State previous_state = system_state;
        system_state = ALERT_TONE;
        printf("system_state:  moving to state ALERT_TONE\n");
        publish_system_state(previous_state, system_state);
        return;
    }
    else if (same_status->EOMDET == 1)
    {
        printf("\n\n=== EOM RECEIVED - WAITING FOR EOM TIMEOUT ===\n");
        gs_first_EOM_timestamp_us = time_us_64();
        System_State previous_state = system_state;
        system_state = EOM_WAIT;
        printf("system_state: moving to state EOM_WAIT\n");
        publish_system_state(previous_state, system_state);
        return;
    }
    printf("h");
};

// TODO:  currently not handling alert tone or broadcast specially - requires ASQ data
// kmo 6 dec 2023
void alert_tone_handler(const struct Si4707_SAME_Status_Packet *status) {
    struct Si4707_ASQ_Status asq_status;
    si4707_asq_get(&asq_status, false);
    
    if (asq_status.ALERT == 0)
    {
        // alert tone ended - broadcast beginning
        printf("\n\n=== ALERT TONE OFF - BROADCAST FOLLOWS ===\n");
        System_State previous_state = system_state;
        system_state = BROADCAST;
        printf("system_state: moving to state BROADCAST\n");
        publish_system_state(previous_state, system_state);
        return;
    }
    // should also probably handle EOMDET here.  kmo 21 mar 2024 12h58
    printf("a");
};

void broadcast_handler(const struct Si4707_SAME_Status_Packet *status) {
    // TODO:  below copypasta from `header_ready_handler` - commonize
    // kmo 21 mar 2024 12h59
    if (status->EOMDET == 1)
    {
        printf("\n\n=== EOM RECEIVED - WAITING FOR EOM TIMEOUT ===\n");
        gs_first_EOM_timestamp_us = time_us_64();
        System_State previous_state = system_state;
        system_state = EOM_WAIT;
        printf("system_state: moving to state EOM_WAIT\n");
        publish_system_state(previous_state, system_state);
        return;
    }
    printf("b");
};


void eom_wait_handler(const struct Si4707_SAME_Status_Packet *status) {
    // TODO:  implementing this correctly requires passing in timing information
    // (or getting it directly, but in general prefer testability of passing in time)
    const uint64_t now = time_us_64();
    if (now - gs_first_EOM_timestamp_us > 8000000) {
        printf("\n\n=== EOM TIMEOUT COMPLETE - RESETTING INTERRUPTS ===\n");
        // CRITICAL:  MUST RESET INTERRUPTS / STATUS BEFORE RESETTING system_state TO IDLE!
        // otherwise you will fly through all the various states unendingly.
        // kmo 6 dec 2023 18h43
        reset_SAME_interrupts_and_buffer();
        System_State previous_state = system_state;
        system_state = IDLE;
        printf("system_state: moving to state IDLE");
        publish_system_state(previous_state, system_state);
        return;
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

    puts("si4707-cpp: prepare() ========================================================");

    // prep LED GPIO
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif // PICO_DEFAULT_LED_PIN
}

#if SI4707_WIZNET
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
#endif // SI4707_WIZNET

void construct_and_publish_heartbeat(int main_loops, const struct Si4707_RSQ_Status *rsq_status) {
    struct Si4707_Heartbeat heartbeat;
    heartbeat.iteration = main_loops;
    heartbeat.si4707_started = g_Si4707_booted_successfully;
    heartbeat.snr = rsq_status->ASNR;
    heartbeat.rssi = rsq_status->RSSI;
#if SI4707_WIZNET
    publish_heartbeat(&heartbeat);
#endif // SI4707_WIZNET
}

void get_and_publish_full_SAME_status(const struct Si4707_SAME_Status_Params *same_params) {
    struct Si4707_SAME_Status_FullResponse same_status;
    const bool same_cts = si4707_await_cts(100);
    if (same_cts) {
        if (same_params->CLRBUF) {
            puts("get_and_publish_full_SAME_status:  clearing buffer");
        }
        si4707_same_status_get(same_params, &same_status);
    } else {
        puts("SAME status CTS timed out :(");
    }

    //printf("printing SAME status\n");
    si4707_same_status_print(&same_status);
    //printf("publishing SAME status\n");

#if SI4707_WIZNET
    publish_SAME_status(&same_status);
#endif // SI4707_WIZNET

    si4707_SAME_Status_FullResponse_free(&same_status);
}

void set_heartbeat_interval_for_SAME_state(const int same_state) {
    // TODO:  make SAME state constants.  kmo 22 nov 2023 11h33
    // NB: misnomer - "EOM" also state when no message has yet been received.
    // kmo 6 dec 2023 15h31
    if (same_state == SI4707_SAME_STATE_END_OF_MESSAGE) {
        if (g_current_heartbeat_interval < 10000000) {
            printf("\n\nSAME state 0 - reducing heartbeat interval to 10 seconds\n");
            g_current_heartbeat_interval = 10000000;
        }
    } else {
        // TODO:  reduce this even further - say, 0.2 or 0.1 seconds.  kmo 22 nov 2023 11h35
        if (g_current_heartbeat_interval > 500000) {
            // 0.5 seconds
            printf("\n\nSAME state > 0 (%d) - reducing heartbeat interval to 0.5 seconds\n", same_state);


            g_current_heartbeat_interval = 500000;
        }
    }
}

// FIXME:  this method is formatted weirdly.  need to update
// vscode formatting rules.  kmo 10 jan 2024 11h19
void reset_SAME_interrupts_and_buffer() {
  struct Si4707_SAME_Status_Packet same_packet;
  struct Si4707_SAME_Status_Params same_params;

  same_params.CLRBUF = true;
  same_params.INTACK = true;

  struct Si4707_RSQ_Status rsq_status;

  const bool rsq_cts = si4707_await_cts(100);
  if (rsq_cts) 
  {
    si4707_rsq_get(&rsq_status);
  } 
  else 
  {
    puts("RSQ/SAME status CTS timed out :(");
  }

  const bool same_packet_cts = si4707_await_cts(100);
  if (same_packet_cts) 
  {
    same_params.READADDR = 0;
    si4707_same_packet_get(&same_params, &same_packet);
    // we do nothing with the result.
  }
}

void set_si4707_hal() {
    // TODO:  conditionalize based on target MCU
    hal_rp2040_set_si4707_pinmap(SI4707_SPI_PORT, SI4707_SPI_MOSI, SI4707_SPI_MISO, SI4707_SPI_SCK, 
                        SI4707_SPI_CS, SI4707_RESET, SI4707_GPO1, SI4707_GPO2);
    struct Si4707_HAL_FPs* hal = hal_rp2040_FPs();
    si4707_set_hal(hal);
}

void oneshot() {
    prepare();

    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);

    printf("proposed MAC ending (bytes 5 and 4): %02x:%02x\n", board_id.id[5], board_id.id[4]);
    printf("board ID bytes: ");
    for (int len = 0; len < 8; len++) {
        printf("%d: %02X  ", len, board_id.id[len]);
    }
    printf("\n\n");
    
#if SI4707_WIZNET
    printf("this firmware uses Wiznet (W5x00) networking.\n");
    set_final_MAC_bytes(board_id.id[5], board_id.id[4]);
    snprintf(g_board_id_string, 32, "si4707/%x%x", board_id.id[5], board_id.id[4]);
    update_root_topic(g_board_id_string);

    printf("\n\n");
    sleep_ms(10);

    init_mqtt();
#elif SI4707_PICO_W
    printf("this firmware uses Pico W (CYW43) networking.\n");
#else // SI4707_WIZNET off
    printf("no networking in this build.\n");
#endif // SI4707_WIZNET

    set_si4707_hal();
    
    si4707_reset();
    si4707_setup_interface();

    si4707_power_up();
    const int cts = si4707_await_cts(500);
    if (cts) {
        puts("si4707 CTS - getting rev and tuning");
        si4707_get_rev();

        g_Si4707_booted_successfully = true;
    } else {
        puts("failed to start si4707 :(");
    }

    // Delay at least 500 ms between powerup command and first tune command.
    // (Allows crystal oscillator to stabilize.)
    // AN332 page 12
    // kmo 2 feb 2024
    puts("waiting before tune for crystal oscillator to stabilize");
    sleep_ms(600);

    const int tune_cts = si4707_await_cts(CTS_WAIT);
    if (tune_cts) {
        si4707_tune();
    } else {
        puts("got rev but could not get CTS for tune tune");
    }
}

int main() {
    oneshot();

    int main_loops = 0;
    int outer_loops_since_last_heartbeat = 0;
    uint64_t last_heartbeat = 0;
    uint64_t last_DHCP_run = 0;
    static uint64_t dhcp_interval = (uint64_t) 60 * (uint64_t) 60 * 1000000; // 60 minutes
    
    if (!g_Si4707_booted_successfully) {
        printf("Si4707 did not boot successfully, calling abort()...");
        abort();
    }

    
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"        // yes, we know, thanks.
    
    // superloop
    while (true) {

        struct Si4707_SAME_Status_Packet same_packet;
        struct Si4707_SAME_Status_Params same_params;

        same_params.CLRBUF = false;
        same_params.INTACK = false;
        
        uint8_t status = 0;
        const bool status_cts = si4707_await_cts(CTS_WAIT);
        if (status_cts) {
            status = si4707_read_status();
        } else {
            puts("status CTS timed out :(");
        }

        struct Si4707_RSQ_Status rsq_status;
        const bool rsq_cts = si4707_await_cts(CTS_WAIT);
        if (rsq_cts) {
            si4707_rsq_get(&rsq_status);
        } else {
            puts("RSQ/SAME status CTS timed out :(");
        }

        const bool same_packet_cts = si4707_await_cts(100);
        if (same_packet_cts) {
            same_params.READADDR = 0;
            si4707_same_packet_get(&same_params, &same_packet);

            // TODO:  move all state logic into state_functions
            state_functions[system_state](&same_packet);
            set_heartbeat_interval_for_SAME_state(same_packet.STATE);
        }

        const uint64_t now = time_us_64();
        const uint64_t microseconds_since_last_heartbeat = now - last_heartbeat;


        if (microseconds_since_last_heartbeat > g_current_heartbeat_interval) {
            #ifdef PICO_DEFAULT_LED_PIN
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            #endif // PICO_DEFAULT_LED_PIN
            printf("\n ====== heartbeating ======================================\n");

            si4707_rsq_print();

            if (!(status & 0x01)) {
                puts("tune invalid :(");
                printf("(status %d)\n", status);
            }

            si4707_asq_print();

            construct_and_publish_heartbeat(main_loops, &rsq_status);
            last_heartbeat = now;
            outer_loops_since_last_heartbeat = 0;

            get_and_publish_full_SAME_status(&same_params);
#if SI4707_WIZNET
            last_DHCP_run = maintain_dhcp_lease(dhcp_interval, now, last_DHCP_run);
#endif // SI4707_WIZNET
            
            #ifdef PICO_DEFAULT_LED_PIN
            gpio_put(PICO_DEFAULT_LED_PIN, false);
            #endif // PICO_DEFAULT_LED_PIN

            main_loops++;
        }

        busy_wait_ms(5);
        outer_loops_since_last_heartbeat++;
    }
#pragma clang diagnostic pop

    return 0;
}
