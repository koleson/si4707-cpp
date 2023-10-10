#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "bus_scan.h"

#include "hardware.h"

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    // Put your timeout handler code in here
    puts("alarm_callback!");
    return 0;
}

void reset_si4707() {
    puts("resetting Si4707");
    
    gpio_put(SI4707_RESET, 1);
    sleep_ms(100);
    
    gpio_put(SI4707_RESET, 0);
    sleep_ms(100);
    
    gpio_put(SI4707_RESET, 1);
    puts("done resetting Si4707");
}

void prepare()
{
    stdio_init_all();
    
    // Prep Si4707 reset GPIO
    gpio_init(SI4707_RESET);
    gpio_set_dir(SI4707_RESET, GPIO_OUT);
    
    // prep LED GPIO
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    // SPI initialization. This example will use SPI at 100kHz.
    spi_init(SPI_PORT, 100*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialize it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
}

int main()
{
    puts("si4707-cpp: main()");
    prepare();
    
    reset_si4707();
    
    // I2C Initialization. Using it at 100kHz.
    i2c_init(I2C_PORT, 100*1000);
    
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    
    puts("not pulling up SCL/SDA");
    // gpio_pull_up(I2C_SDA);
    // gpio_pull_up(I2C_SCL);
    
    // test hardware timer
    add_alarm_in_ms(2000, alarm_callback, NULL, false);
        
    while(true) {
        puts("si4707-cpp: loopin'");
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        
        reset_si4707();
        busy_wait_ms(10);
        bus_scan();
        
        // blinky to indicate idle
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        busy_wait_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        busy_wait_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        busy_wait_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        busy_wait_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        busy_wait_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        busy_wait_ms(500);
    }
    
    return 0;
}
