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



int main()
{
    stdio_init_all();

    // SPI initialisation. This example will use SPI at 100kHz.
    spi_init(SPI_PORT, 100*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialize it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Timer example code - This example fires off the callback after 2000ms
    // panic / hard assert - maybe `sleep_ms` causing problems?
    add_alarm_in_ms(2000, alarm_callback, NULL, false);
        
    while(true) {
        // TODO:  add blinky
        puts("si4707-cpp: Hello, world!");
        busy_wait_ms(1000);
        bus_scan();
    }
    
    return 0;
}
