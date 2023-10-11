#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "bus_scan.h"

#include "hardware.h"
#include "si4707_const.h"

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

void setup_spi() {
    puts("setting up SPI");
    // SPI initialization. This example will use SPI at 100kHz.
    spi_init(SPI_PORT, 400*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialize it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
}

void reset_si4707() {
    puts("resetting Si4707");
    
    gpio_init(SI4707_RESET);
    gpio_set_dir(SI4707_RESET, GPIO_OUT);
    gpio_put(SI4707_RESET, 0);
    sleep_ms(10);
    
    
    // drive GPO2/INT + GPO1/MISO high to select SPI bus mode on Si4707
    gpio_init(SI4707_GPO1);
    gpio_set_dir(SI4707_GPO1, GPIO_OUT);
    gpio_put(SI4707_GPO1, 1);
    
    // GPO1 = MISO - we can use it before SPI is setup
    gpio_init(SI4707_GPO2);
    gpio_set_dir(SI4707_GPO2, GPIO_OUT);
    gpio_put(SI4707_GPO2, 1);
    
    sleep_ms(5);
    
    gpio_put(SI4707_RESET, 1);
    sleep_ms(10);
    
    gpio_put(SI4707_GPO1, 0);
    gpio_put(SI4707_GPO2, 0);
    sleep_ms(10);
    
    // GPO could be used as INT later
    gpio_deinit(SI4707_GPO1);
    // GPO1 is used as MISO later
    gpio_deinit(SI4707_GPO2);
    sleep_ms(2);
    
    puts("done resetting Si4707");
    
}

static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);  // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
    asm volatile("nop \n nop \n nop");
}

void await_si4707_cts() {
    // command: get status byte
    char status_cmd[1];
    status_cmd[0] = 0xA0;        // read status byte via GPO1
    
    // buffer:  receive powerup status to this buffer
    char status_result[1];
    status_result[0] = 0x00;
    
    puts("waiting for cts");
    
    int i = 0;
    char status = status_result[0];
    while ((status_result[0] & 0x80) == 0x00) {
        cs_select();
        spi_write_blocking(spi_default, status_cmd, 1);
        spi_read_blocking(spi_default, 0, status_result, 1);
        cs_deselect();
        
        status = status_result[0];
        if (i % 200 == 0 || status != 0) {
            printf("cts waitloop status = %d (i = %d)\n", status, i);
        }
        
        sleep_ms(5);
        i++;
    }
    
    printf("cts waitloop exit status: %d\n\n", status);
}

void power_up_si4707() {
    puts("power_up_si4707");
    // si4707 startup command buffer
    uint8_t cmd[9];
    cmd[0] = 0x48;       // write a command (drives 8 bytes on SDIO)
    cmd[1] = SI4707_CMD_POWER_UP;
    
    // 0x53
    // CTS interrupt disable
    // GPO2 output enabled
    // boot normally
    // crystal oscillator enabled
    // weatherband receive
    cmd[2] = 0x53;
    
    // 0x05:  analog output mode
    cmd[3] = 0x05;
    cmd[4] = 0x00; cmd[5] = 0x00; cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x00;
    
    cs_select();
    // write 9 bytes - control + cmd + 7 args
    spi_write_blocking(spi_default, cmd, 9);
    cs_deselect();
    
    await_si4707_cts();    
    
    sleep_ms(10);
}

void get_rev() {
    uint8_t cmd[8];
    cmd[0] = 0x48;
    cmd[1] = SI4707_CMD_GET_REV;
    cmd[2] = 0; cmd[3] = 0; cmd[4] = 0; cmd[5] = 0; cmd[6] = 0; cmd[7] = 0; cmd[8] = 0;
    
    char product_data[16] = { 0 };
    
    await_si4707_cts();
    
    puts("getting rev");
    
    cs_select();
    spi_write_blocking(spi_default, cmd, 9);
    cs_deselect();
    
    await_si4707_cts();
   
    char resp_cmd[1];
    resp_cmd[0] = 0xE0;        // read 16 response bytes via GPO1
   
    cs_select();
    spi_write_blocking(spi_default, resp_cmd, 1);
    spi_read_blocking(spi_default, 0, product_data, 16);
    cs_deselect();
    
    uint8_t pn = product_data[1];
    printf("product number: %d\n", pn);
    
    if (pn != 7) {
        printf("product number invalid - halting");
        while (true) {
            busy_wait_ms(100000);
        }
    }
    
    puts("\n");
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
    puts("si4707-cpp: main() 10 oct 19h26");
    
    // resetting to SPI mode requires
    // GPO2 *AND* GPO1 are high.  GPO2 must be driven (easy, it has no other use here)
    // GPO1 can float or be driven - since it's used for SPI, we have to deinit it before
    // reset_si4707 ends.  it seems easiest to drive it momentarily to make sure.
    reset_si4707();
    
    setup_spi();
    sleep_ms(50);
    
    power_up_si4707();
    
    get_rev();
    
    // setup_i2c();
    
    // test hardware timer
    // add_alarm_in_ms(20000, alarm_callback, NULL, false);
        
    while(true) {
        puts("si4707-cpp: loopin'");
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        
        // bus_scan();
        
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
