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
    
    // resetting to SPI mode requires
    // GPO2 *AND* GPO1 are high.  GPO2 must be driven (easy, it has no other use here)
    // GPO1 can float or be driven - since it's used for SPI, we have to deinit it before
    // reset_si4707 ends.  it seems easiest to drive it momentarily to make sure.
    reset_si4707();
    
    setup_si4707_spi();
    
    power_up_si4707();
    
    get_si4707_rev();
    
    tune_si4707();
    
    
    // setup_i2c();
    
    // test hardware timer
    // add_alarm_in_ms(20000, alarm_callback, NULL, false);
    
    puts("LOOP TIME! ======");
    
    int mainLoops = 0;
    
    while(true) {
        printf("si4707-cpp: loopin' iteration %d ===================== \n", mainLoops);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        
        // FIXME:  getting si4707 rev info before checking status
        // seems to result in the tune being valid more reliably?
        // kmo 10 oct 2023 21h59
        get_si4707_rev();
        // bus_scan();
        
        await_si4707_cts();
        uint8_t status = read_status();
        
        if (status & 0x01) {
            puts("tune valid");
        } else {
            puts("tune invalid :(");
            printf("(status %d)\n", status);
        }
        
        print_si4707_rsq();
        print_si4707_same_status();
        
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        busy_wait_ms(1000);
        mainLoops++;
    }
    
    return 0;
}
