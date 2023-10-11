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

#ifndef MIN
#define MIN(a, b) ((b)>(a)?(a):(b))
#endif

// TODO:  move us out
uint8_t read_status();
void read_resp(uint8_t* resp);
// END TODO


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
    sleep_ms(5);
    
    gpio_put(SI4707_GPO1, 0);
    gpio_put(SI4707_GPO2, 0);
    sleep_ms(2);
    
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
    //puts("waiting for cts");
    
    int i = 0;
    char status = 0;
    while ((status & 0x80) == 0x00) {
        status = read_status();
        
        // only print status if it's taking a long time
        if (i > 0 && i % 200 == 0) {
            printf("cts waitloop status = %d (i = %d)\n", status, i);
        }
        
        sleep_ms(5);
        i++;
    }
    
    //printf("cts waitloop exit status: %d\n\n", status);
}

void power_up_si4707() {
    puts("power_up_si4707");
    // si4707 startup command buffer
    uint8_t cmd[9] = { 0x00 };
    cmd[0] = SI4707_SPI_SEND_CMD;       // write a command (drives 8 bytes on SDIO)
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
    
    cs_select();
    // write 9 bytes - control + cmd + 7 args
    spi_write_blocking(spi_default, cmd, 9);
    cs_deselect();
    
    await_si4707_cts();    
    
    sleep_ms(10);
}

void tune_si4707() {
    puts("tuning si4707 to 162.475MHz");
    uint8_t freqHigh = 0xFD;
    uint8_t freqLow = 0xDE;
    
    uint8_t cmd[9] = { 0x00 };
    cmd[0] = SI4707_SPI_SEND_CMD;       // write a command (drives 8 bytes on SDIO)
    cmd[1] = SI4707_CMD_WB_TUNE_FREQ;
    cmd[2] = 0x00;                      // AN332 page 180 - reserved, always write 0, no meaning
    cmd[3] = freqHigh;
    cmd[4] = freqLow;
    
    await_si4707_cts();
    cs_select();
    spi_write_blocking(spi_default, cmd, 9);
    cs_deselect();
    
    // tune status takes a moment to populate, so we don't check it here.
    // kmo 10 oct 20234 21h54
}

void send_command(uint8_t cmd) {
    uint8_t cmd_buf[9] = { 0x00 };
    cmd_buf[0] = SI4707_SPI_SEND_CMD;         // SPI command send - 8 bytes follow - 1 byte of cmd, 7 bytes of arg
    cmd_buf[1] = cmd;
    
    cs_select();
    spi_write_blocking(spi_default, cmd_buf, 9);
    cs_deselect();
    
    await_si4707_cts();
    uint8_t resp_buf[16] = { 0x00 };
    read_resp(resp_buf);
    
}

uint8_t read_status() {
    char status_cmd[1];
    status_cmd[0] = 0xA0;        // read status byte via GPO1
    
    // buffer:  receive powerup status to this buffer
    char status_result[1];
    status_result[0] = 0x00;
    
    cs_select();
    spi_write_blocking(spi_default, status_cmd, 1);
    spi_read_blocking(spi_default, 0, status_result, 1);
    cs_deselect();
    
    return status_result[0];
}

void read_resp(uint8_t* resp) {
    uint8_t resp_cmd[1];
    resp_cmd[0] = 0xE0;        // read 16 response bytes via GPO1
    
    await_si4707_cts();
    
    cs_select();
    spi_write_blocking(spi_default, resp_cmd, 1);
    spi_read_blocking(spi_default, 0, resp, 16);
    cs_deselect();
}

void get_si4707_rev() {   
    char product_data[16] = { 0 };
    
    await_si4707_cts();
    
    send_command(SI4707_CMD_GET_REV);
    
    await_si4707_cts();
   
    read_resp(product_data);
    
    uint8_t pn = product_data[1];
    printf("product number: %d\n", pn);
    
    if (pn != 7) {
        printf("product number invalid - halting");
        while (true) {
            busy_wait_ms(100000);
        }
    }
}

void print_si4707_rsq() {
    uint8_t wb_rsq_resp[16] = { 0x00 };
    send_command(SI4707_CMD_WB_RSQ_STATUS);
    read_resp(wb_rsq_resp);
    uint8_t valid = wb_rsq_resp[2] & 0x01;
    uint8_t rssi = wb_rsq_resp[4];
    uint8_t snr = wb_rsq_resp[5];
    
    puts("VALID  RSSI  SNR");
    printf("%5d  %4d  %3d\n\n", valid, rssi, snr);
}

void print_si4707_same_status() {
    uint8_t wb_same_status_resp[16] = { 0x00 };
    send_command(SI4707_CMD_WB_SAME_STATUS);
    read_resp(wb_same_status_resp);
    
    uint8_t end_of_message_detected     = wb_same_status_resp[1] & 0x08;
    uint8_t start_of_message_detected   = wb_same_status_resp[1] & 0x04;
    uint8_t preamble_detected           = wb_same_status_resp[1] & 0x02;
    uint8_t header_ready                = wb_same_status_resp[1] & 0x01;
    uint8_t state                       = wb_same_status_resp[2];
    uint8_t message_length              = wb_same_status_resp[3];
    
    puts("EOMDET SOMDET PREDET HDRRDY STATE MSGLEN");
    printf("%6d %6d %6d %6d %5d %6d\n", 
                end_of_message_detected, start_of_message_detected, preamble_detected, 
                header_ready, state, message_length);
                
    if (true) { // (message_length > 0) {
        // TODO:  this is where we have to send multiple messages with differing params
        // (need to set offset into buffer for read)
        // kmo 10 oct 2023 22h30
        uint8_t same_buf[255] = { 0x00 }; // null-terminated for your safety
        
        // for now, read at most 8 bytes of buffer so we don't have to deal with
        // making multiple requests.
        int bytesToRead = MIN(message_length, 8);
        printf("reading %d bytes of SAME buffer\n", bytesToRead);
        
        for (int i = 0; i < bytesToRead; i++) {
            same_buf[i] = wb_same_status_resp[i + 6];
        }
        
        printf("SAME buffer: '%s'\n\n", same_buf);
    }
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
