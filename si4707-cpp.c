#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
// TODO:  remove i2c needs from this file
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "bus_scan.h"

#include "pico/unique_id.h"

#include "hardware.h"
#include "si4707_const.h"

#include "si4707.h"
#include "mqtt-publisher.h"

bool g_Si4707_booted_successfully = false;
char g_board_id_string[32];

//int64_t alarm_callback(alarm_id_t id, void *user_data) {
//    // not really doing anything right now.  just a demo.
//    puts("alarm_callback!");
//    return 0;
//}

void prepare()
{
    stdio_init_all();
    
    puts("\n\n\n========================================================");
    puts("si4707-cpp: prepare()");
    
    // prep LED GPIO
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
}

//void setup_i2c()
//{
//    printf("si4707-cpp: setup_i2c() - SCL GPIO%d, SDA GPIO%d\n", I2C_SCL, I2C_SDA);
//
//    // I2C Initialization. Using it at 100kHz.
//    i2c_init(I2C_PORT, 100*1000);
//
//    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
//    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
//
//    puts("not pulling up SCL/SDA");
//    gpio_pull_up(I2C_SDA);
//    gpio_pull_up(I2C_SCL);
//}

int main()
{
    prepare();
    
    // must `prepare` before trying to print this message.  kmo 9 oct 2023 17h29
    puts("si4707-cpp: main()");
    
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);

    printf("====== board ID: %llx\n", board_id);
    printf("===== bytes: %02x %02x [...] %02x %02x ==========\n\n", board_id.id[0], board_id.id[1], board_id.id[6], board_id.id[7]);
    printf("proposed MAC ending: %02x:%02x\n", board_id.id[5], board_id.id[4]);
    for (int len = 0; len < 8; len++) {
            printf("%d: %02X /", len, board_id.id[len]);
    }
    
    snprintf(g_board_id_string, 32, "si4707/%x%x", board_id.id[5], board_id.id[4]);
    update_root_topic(g_board_id_string);

    printf("\n\n\n");
    // fflush(stdout);
    printf("initializing MQTT...\n");
    puts("sleeping before init_mqtt");
    sleep_ms(10);
    puts("done sleeping before init_mqtt");
    init_mqtt();
    printf("... done initializing MQTT");

    // resetting to SPI mode requires
    // GPO2 *AND* GPO1 are high.  GPO2 must be driven (easy, it has no other use here)
    // GPO1 can float or be driven - since it's used for SPI, we have to deinit it before
    // reset_si4707 ends.  it seems easiest to drive it momentarily to make sure.
    
    reset_si4707();
    
    setup_si4707_spi();
    
    power_up_si4707();
    
    int cts = await_si4707_cts(500);
    if (cts) {
        puts("si4707 CTS - getting rev and tuning");
        get_si4707_rev();
        tune_si4707();
        
        g_Si4707_booted_successfully = true;
    } else {
        puts("failed to start si4707 :(");
    }
    
    // setup_i2c();
    
    // test hardware timer
    // add_alarm_in_ms(20000, alarm_callback, NULL, false);
    
    puts("oneshot done - LOOP TIME! ======");
    
    int main_loops = 0;
    int outer_loops_since_last_heartbeat = 0;
    uint64_t last_heartbeat = 0;
    uint64_t last_DHCP_run = 0;
    
    static uint64_t heartbeat_interval = 5000000;  // 5000000 microseconds = 5 seconds
    static uint64_t dhcp_interval = (uint64_t)60 * (uint64_t)60 * 1000000; // 60 minutes
    // static uint64_t dhcp_interval = (uint64_t)20 * (uint64_t)1000000; // 60 seconds

    // superloop
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (true) {
        // TODO: check status registers for interesting things here and force a
        // mqtt message or heartbeat if anything interesting happens
        if (outer_loops_since_last_heartbeat % 50 == 0) {
            printf(".");
            //printf("outerloop %d\n", outer_loops_since_last_heartbeat);
        }
        
        
        uint64_t now = time_us_64();
        uint64_t microseconds_since_last_heartbeat = now - last_heartbeat;
        uint64_t microseconds_since_last_DHCP_run = now - last_DHCP_run;

        if (microseconds_since_last_heartbeat > heartbeat_interval) {
            last_heartbeat = now;
            outer_loops_since_last_heartbeat = 0;
            puts("\n\nheartbeating");
            gpio_put(PICO_DEFAULT_LED_PIN, 1);
            
            struct Si4707_Heartbeat heartbeat;
            heartbeat.iteration = main_loops;
            heartbeat.si4707_started = g_Si4707_booted_successfully;
            heartbeat.rssi = 0;
            heartbeat.snr = 0;
            
            // bus_scan();
            if (g_Si4707_booted_successfully) {
                bool rev_cts = await_si4707_cts(100);
                if (rev_cts) {
                    // FIXME:  getting si4707 rev info before checking status
                    // seems to result in the tune being valid more reliably?
                    // kmo 10 oct 2023 21h59
                    get_si4707_rev();
                }
                
                bool rsq_cts = await_si4707_cts(100);
                if (rsq_cts) {
                    uint8_t status = read_status();
                    
                    if (status & 0x01) {
                        puts("tune valid");
                        heartbeat.tune_valid = true;
                    } else {
                        puts("tune invalid :(");
                        printf("(status %d)\n", status);
                        heartbeat.tune_valid = false;
                    }
                    
                    print_si4707_rsq();
                    
                    struct Si4707_RSQ_Status rsq_status;
                    get_si4707_rsq(&rsq_status);
                    
                    heartbeat.snr = rsq_status.ASNR;
                    heartbeat.rssi = rsq_status.RSSI;
                    publish_heartbeat(&heartbeat);
                    
                } else {
                    puts("RSQ/SAME status CTS timed out :(");
                }

                bool same_cts = await_si4707_cts(100);
                if (same_cts) {
                    struct Si4707_SAME_Status_FullResponse same_status;
                    struct Si4707_SAME_Status_Params same_params;
                    same_params.INTACK = 0; // leave it alone
                    same_params.READADDR = 0;   // TODO?
                    get_si4707_same_status(&same_params, &same_status);
                    
                    print_si4707_same_status(&same_status);
                    publish_SAME_status(&same_status);

                    free_Si4707_SAME_Status_FullResponse(&same_status);
                } else {
                    puts("SAME status CTS timed out :(");
                }
            }
            
            
            
            
            // DHCP maintenance
            if (microseconds_since_last_DHCP_run > dhcp_interval) {
                last_DHCP_run = now;
                puts("updating DHCP");
                dhcp_run_wrapper();
                puts("done updating DHCP");
            } else {
                uint64_t microseconds_to_next_DHCP_run = dhcp_interval - microseconds_since_last_DHCP_run;
                int seconds_to_next_DHCP_run = floor(microseconds_to_next_DHCP_run / 1000000.0); // NOLINT(*-narrowing-conversions)
                printf("next DHCP run in about %d seconds\n", seconds_to_next_DHCP_run);
            }

            gpio_put(PICO_DEFAULT_LED_PIN, 0);
            main_loops++;
        }

        
        
        // TODO:  remove this once outer loop is 
        // "safe"
        busy_wait_ms(10);
        outer_loops_since_last_heartbeat++;
       
    }
#pragma clang diagnostic pop
    
    return 0;
}
