#include <stdio.h>
#include "si4707_const.h"
#include "hardware.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "si4707.h"

#ifndef MIN
#define MIN(a, b) ((b)>(a)?(a):(b))
#endif

#define CTS_WAIT 250

// TODO:  parameterize - take in MOSI, MISO, CS, SCK
void setup_si4707_spi() {
	puts("setting up SPI");
	// SPI initialization. This example will use SPI at 100kHz.
	spi_init(SI4707_SPI_PORT, 400*1000);
	gpio_set_function(SI4707_SPI_MISO, GPIO_FUNC_SPI);
	gpio_set_function(SI4707_SPI_CS,   GPIO_FUNC_SIO);
	gpio_set_function(SI4707_SPI_SCK,  GPIO_FUNC_SPI);
	gpio_set_function(SI4707_SPI_MOSI, GPIO_FUNC_SPI);
	
	// Chip select is active-low, so we'll initialize it to a driven-high state
	gpio_set_dir(SI4707_SPI_CS, GPIO_OUT);
	gpio_put(SI4707_SPI_CS, 1);
}



static inline void si4707_cs_select() {
	asm volatile("nop \n nop \n nop");
	gpio_put(SI4707_SPI_CS, 0);  // Active low
	asm volatile("nop \n nop \n nop");
}

static inline void si4707_cs_deselect() {
	asm volatile("nop \n nop \n nop");
	gpio_put(SI4707_SPI_CS, 1);
	asm volatile("nop \n nop \n nop");
}

// end SPI base stuff


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

bool await_si4707_cts(int maxWait) {
	//puts("waiting for cts");
	
	int i = 0;
	char status = 0;
	while ((status & 0x80) == 0x00 && i < maxWait) {
		status = read_status();
		
		// only print status if it's taking a long time
		if (i > 0 && i % 200 == 0) {
			printf("cts waitloop status = %d (i = %d)\n", status, i);
		}
		
		sleep_ms(5);
		i++;
	}
	
	if (status & 0x80) {
		return true;
	} else {
		// timed out :(
		printf("cts waitloop timed out (%d patience)\n", maxWait);
		return false;
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
	
	si4707_cs_select();
	// write 9 bytes - control + cmd + 7 args
	spi_write_blocking(SI4707_SPI_PORT, cmd, 9);
	si4707_cs_deselect();
	
	await_si4707_cts(CTS_WAIT); 
	
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
	
	bool cts = await_si4707_cts(CTS_WAIT);
	if (cts) {
		si4707_cs_select();
		spi_write_blocking(SI4707_SPI_PORT, cmd, 9);
		si4707_cs_deselect();
	} else {
		puts("could not tune - CTS timeout");
	}
	
	
	// tune status takes a moment to populate, so we don't check it here.
	// kmo 10 oct 20234 21h54
}

void send_command(uint8_t cmd) {
	uint8_t cmd_buf[9] = { 0x00 };
	cmd_buf[0] = SI4707_SPI_SEND_CMD;         // SPI command send - 8 bytes follow - 1 byte of cmd, 7 bytes of arg
	cmd_buf[1] = cmd;
	
	si4707_cs_select();
	spi_write_blocking(SI4707_SPI_PORT, cmd_buf, 9);
	si4707_cs_deselect();
	
	bool cts = await_si4707_cts(CTS_WAIT);
	if (cts) {
		uint8_t resp_buf[16] = { 0x00 };
		read_resp(resp_buf);
	} else {
		printf("could not send command %02x - CTS timeout\n", cmd);
	}
	
}

uint8_t read_status() {
	char status_cmd[1];
	status_cmd[0] = 0xA0;        // read status byte via GPO1
	
	// buffer:  receive powerup status to this buffer
	char status_result[1];
	status_result[0] = 0x00;
	
	si4707_cs_select();
	spi_write_blocking(SI4707_SPI_PORT, status_cmd, 1);
	spi_read_blocking(SI4707_SPI_PORT, 0, status_result, 1);
	si4707_cs_deselect();
	
	return status_result[0];
}

void read_resp(uint8_t* resp) {
	uint8_t resp_cmd[1];
	resp_cmd[0] = 0xE0;        // read 16 response bytes via GPO1
	

	bool cts = await_si4707_cts(CTS_WAIT);
	if (cts) {
		si4707_cs_select();
		spi_write_blocking(SI4707_SPI_PORT, resp_cmd, 1);
		spi_read_blocking(SI4707_SPI_PORT, 0, resp, 16);
		si4707_cs_deselect();
	} else {
		puts("could not read response - CTS timeout");
	}
}

void get_si4707_rev() {   
	char product_data[16] = { 0 };
	
	bool cts_cmd = await_si4707_cts(CTS_WAIT);
	if (cts_cmd) {
		send_command(SI4707_CMD_GET_REV);
	} else {
		puts("could not request product info - CTS timeout");
		return;
	}
	
	bool cts_read = await_si4707_cts(CTS_WAIT);
	if (cts_read) {
		read_resp(product_data);
		
		uint8_t pn = product_data[1];
		printf("product number: %d\n", pn);
		
		if (pn != 7) {
			printf("product number invalid - halting\n\n");
			while (true) {
				busy_wait_ms(100000);
			}
		}
	} else {
		puts("could not read product info - CTS timeout");
		return;
	}
}

void get_si4707_rsq(struct Si4707_RSQ_Status *rsq_status) {
	uint8_t wb_rsq_resp[16] = { 0x00 };
	send_command(SI4707_CMD_WB_RSQ_STATUS);
	read_resp(wb_rsq_resp);
	uint8_t valid = wb_rsq_resp[2] & 0x01;
	uint8_t rssi = wb_rsq_resp[4];
	uint8_t snr = wb_rsq_resp[5];

}

void print_si4707_rsq() {
	// TODO:  make this use get_si4707_rsq instead of duplicating code
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