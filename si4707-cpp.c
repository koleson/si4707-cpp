#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "bus_scan.h"

#include "hardware.h"
#include "si4707_const.h"

#include "si4707.h"
#include "mqtt-publisher.h"

bool g_Si4707_booted_successfully = false;

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    // not really doing anything right now.  just a demo.
    puts("alarm_callback!");
    return 0;
}

void prepare()
{
    stdio_init_all();
    
    puts("\n\n\n========================================================");
    puts("si4707-cpp: prepare()");
    
    // prep LED GPIO
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
}

void setup_i2c()
{
    printf("si4707-cpp: setup_i2c() - SCL GPIO%d, SDA GPIO%d\n", I2C_SCL, I2C_SDA);
    
    // I2C Initialization. Using it at 100kHz.
    i2c_init(I2C_PORT, 100*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    
    puts("not pulling up SCL/SDA");
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

int main()
{
    prepare();
    
    // must `prepare` before trying to print this message.  kmo 9 oct 2023 17h29
    puts("si4707-cpp: main() 11 oct 17h31");
    
    init_mqtt();
    
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
    
    static uint64_t heartbeat_interval = 10000000;  // 10000000 microseconds = 10 seconds
    
    while(true) {
        // TODO: check status registers for interesting things here and force a
        // mqtt message or heartbeat if anything interesting happens
        if (outer_loops_since_last_heartbeat % 100 == 0) {
            printf("outerloop %d\n", outer_loops_since_last_heartbeat);
        }
        
        
        uint64_t now = time_us_64();
        uint64_t microseconds_since_last_heartbeat = now - last_heartbeat;
        if (microseconds_since_last_heartbeat > heartbeat_interval) {
            last_heartbeat = now;
            outer_loops_since_last_heartbeat = 0;
            puts("heartbeating");
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
                
                bool cts = await_si4707_cts(100);
                if (cts) {
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
                    print_si4707_same_status();
                    
                    struct Si4707_RSQ_Status rsq_status;
                    get_si4707_rsq(&rsq_status);
                    
                    heartbeat.snr = rsq_status.ASNR;
                    heartbeat.rssi = rsq_status.RSSI;
                    
                } else {
                    puts("RSQ/SAME status CTS timed out :(");
                }
            }
            
            
            
            // publish_helloworld();
            publish_heartbeat(&heartbeat);
            
            gpio_put(PICO_DEFAULT_LED_PIN, 0);
            main_loops++;
        }
        
        // TODO:  remove this once outer loop is 
        // "safe"
        busy_wait_ms(10);
        outer_loops_since_last_heartbeat++;
       
    }
    
    return 0;
}
