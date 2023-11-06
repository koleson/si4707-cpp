#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
	
	// GPO1 = MISO - we can use it before SPI is set up
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
			printf("cts wait-loop status = %d (i = %d)\n", status, i);
		}
		
		sleep_ms(5);
		i++;
	}
	
	if (status & 0x80) {
		return true;
	} else {
		// timed out :(
		printf("cts wait-loop timed out (%d patience)\n", maxWait);
		return false;
	}
	
	//printf("cts wait-loop exit status: %d\n\n", status);
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
	// kmo 10 oct 2023 21h54
}

void send_command(uint8_t cmd, struct Si4707_Command_Args* args) {
    uint8_t cmd_buf[9] = { 0x00 };
    cmd_buf[0] = SI4707_SPI_SEND_CMD;         // SPI command send - 8 bytes follow - 1 byte of cmd, 7 bytes of arg
    cmd_buf[1] = cmd;

    cmd_buf[2] = args->ARG1; cmd_buf[3] = args->ARG2; cmd_buf[4] = args->ARG3; cmd_buf[5] = args->ARG4;
    cmd_buf[6] = args->ARG5; cmd_buf[7] = args->ARG6; cmd_buf[8] = args->ARG7;

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

void send_command_noargs(uint8_t cmd) {
	struct Si4707_Command_Args args;
    args.ARG1 = 0x00; args.ARG2 = 0x00; args.ARG3 = 0x00; args.ARG4 = 0x00;
    args.ARG5 = 0x00; args.ARG6 = 0x00; args.ARG7 = 0x00;

    send_command(cmd, &args);
}

uint8_t read_status() {
	const uint8_t status_cmd[1] = { 0xA0 }; // read status byte via GPO1
	
	// buffer:  receive power up status to this buffer
	uint8_t status_result[1] = { 0x00 };
	
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
	uint8_t product_data[16] = { 0x00 };
	
	bool cts_cmd = await_si4707_cts(CTS_WAIT);
	if (cts_cmd) {
		send_command_noargs(SI4707_CMD_GET_REV);
	} else {
		puts("could not request product info - CTS timeout");
		return;
	}
	
	bool cts_read = await_si4707_cts(CTS_WAIT);
	if (cts_read) {
		read_resp(product_data);
		
		uint8_t pn = product_data[1];
		// printf("product number: %d\n", pn);
		
		if (pn != 7) {
			printf("product number invalid - halting\n\n");

            // halt
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
			while (true) {
				busy_wait_ms(100000);
			}
#pragma clang diagnostic pop
		}
	} else {
		puts("could not read product info - CTS timeout");
		return;
	}
}

void get_si4707_rsq(struct Si4707_RSQ_Status *rsq_status) {
	uint8_t wb_rsq_resp[16] = { 0x00 };
	send_command_noargs(SI4707_CMD_WB_RSQ_STATUS);
	read_resp(wb_rsq_resp);
	uint8_t valid = wb_rsq_resp[2] & 0x01;
	uint8_t rssi = wb_rsq_resp[4];
	uint8_t snr = wb_rsq_resp[5];
	
	rsq_status->RSSI = rssi;
	rsq_status->ASNR = snr;
}

void print_si4707_rsq() 
{
	struct Si4707_RSQ_Status status;
	get_si4707_rsq(&status);
	puts("RSSI  SNR");
	printf("%4d  %3d\n\n", status.RSSI, status.ASNR);
}

void get_si4707_same_packet(struct Si4707_SAME_Status_Params *params, 
							struct Si4707_SAME_Status_Packet *packet) {
	uint8_t wb_same_resp[16] = { 0x00 };

    struct Si4707_Command_Args args;
    // ARG1:  D1 = CLRBUF; D0 = INTACK
    args.ARG1 = 0x00 | (params->INTACK ? 0x01 : 0x00); // never clear buffer for now.  kmo 27 oct 2023 11h19  | (params->CLRBUF ? 0x02 : 0x00);
    // ARG2:  READADDR
    args.ARG2 = params->READADDR;
    args.ARG3 = 0x00; args.ARG4 = 0x00; args.ARG5 = 0x00; args.ARG6 = 0x00; args.ARG7 = 0x00;

    send_command(SI4707_CMD_WB_SAME_STATUS, &args);
	read_resp(wb_same_resp);
	
	// byte 0:  CTS/ERR/-/-/RSQINT/SAMEINT/ASQINT/STCINT
	packet->CTS = 			((wb_same_resp[0] & 0x80) != 0);
	packet->ERR = 			((wb_same_resp[0] & 0x40) != 0);
	// empty 0x10, 0x20
	packet->RSQINT = 		((wb_same_resp[0] & 0x08) != 0);
	packet->SAMEINT = 		((wb_same_resp[0] & 0x04) != 0);
	packet->ASQINT = 		((wb_same_resp[0] & 0x02) != 0);
	packet->STCINT = 		((wb_same_resp[0] & 0x01) != 0);

	// byte 1:  -/-/-/-/EOMDET/SOMDET/PREDET/HDRRDY
	packet->EOMDET =	((wb_same_resp[1] & 0x08) != 0);
	packet->SOMDET =	((wb_same_resp[1] & 0x04) != 0);
	packet->PREDET =	((wb_same_resp[1] & 0x02) != 0);
	packet->HDRRDY =	((wb_same_resp[1] & 0x01) != 0);

	packet->STATE = 	wb_same_resp[2];
    // printf("get_si4707_same_packet: wb_same_resp[3] = %d\n", wb_same_resp[3]);
	packet->MSGLEN = 	wb_same_resp[3];
	
	// copy strings (not null-terminated, fixed-length)
	memcpy(packet->CONF, wb_same_resp+4, 2);
	memcpy(packet->DATA, wb_same_resp+6, 8);
}

void get_si4707_same_status(struct Si4707_SAME_Status_Params *params, struct Si4707_SAME_Status_FullResponse *full_response)
{
	struct Si4707_SAME_Status_Packet first_packet;

	get_si4707_same_packet(params, &first_packet);

	// byte 0
	full_response->CTS = 		first_packet.CTS;
	full_response->ERR =		first_packet.ERR;
	full_response->RSQINT =		first_packet.RSQINT;
	full_response->SAMEINT = 	first_packet.SAMEINT;
	full_response->ASQINT =		first_packet.ASQINT;
	full_response->STCINT =		first_packet.STCINT;

	// byte 1
	full_response->EOMDET = 	first_packet.EOMDET;
	full_response->SOMDET = 	first_packet.SOMDET;
	full_response->PREDET = 	first_packet.PREDET;
	full_response->HDRRDY = 	first_packet.HDRRDY;

	full_response->STATE  = 	first_packet.STATE;
	full_response->MSGLEN = 	first_packet.MSGLEN;
    // printf("get_si4707_same_status: first_packet.MSGLEN = %d\n", first_packet.MSGLEN);

	// maximum message length is ~250 chars.
	int whole_responses_needed = first_packet.MSGLEN / 8;
	int remainder = first_packet.MSGLEN % 8;
	int responses_needed;
	
	// kmo 18 oct 2023 10h51:  seems like this is still under-counting
    // kmo 20 oct 2023 18h03:  i think i fixed the undercounting.
	if (remainder > 0) {
		responses_needed = whole_responses_needed + 1;
	} else {
		responses_needed = whole_responses_needed;
	}

	printf("%d responses needed to get message of length %d\n", responses_needed, first_packet.MSGLEN);

	// TODO:  malloc size based on MSGLEN.  kmo 18 oct 2023 15h54
	// MSGLEN can be at most 255, so adding null termination, 256 max length
	int alloc_length = 256;
	uint8_t* same_buf = (uint8_t*)malloc(sizeof(uint8_t) * alloc_length);

    // TODO: confidence might not actually need to be this big? kmo 4 nov 2023 11h23
    uint8_t* conf_buf = (uint8_t*)malloc(sizeof(uint8_t) * alloc_length);
	
	// auto-null-termination
	for (int i = 0; i < alloc_length; i++) {
		same_buf[i] = 0x00;
	}

	struct Si4707_SAME_Status_Params same_buf_params;
	struct Si4707_SAME_Status_Packet same_buf_packet;
	same_buf_params.INTACK = 0;
	same_buf_params.READADDR = 0;

	for (int i = 0; i < whole_responses_needed; i++) {
		int offset = i * 8;
		int chars_remaining = first_packet.MSGLEN - (i * 8);
		same_buf_params.READADDR = offset;
		
		int chars_to_read;
		if (chars_remaining < 8) {
			chars_to_read = 8;
		} else {
			chars_to_read = chars_remaining;
		}

		same_buf_params.READADDR = offset;
		get_si4707_same_packet(&same_buf_params, &same_buf_packet);

		printf("msg i before memcpy - same_buf = '%s\n'", same_buf);
		memcpy((same_buf + offset), same_buf_packet.DATA, chars_to_read);
		printf("msg i after memcpy - same_buf = '%s'\n", same_buf);
	}

	// heap-allocated variables exit here
	full_response->DATA = same_buf;
    full_response->CONF = conf_buf;
}

void print_si4707_same_status(struct Si4707_SAME_Status_FullResponse* response) {
	puts("EOMDET SOMDET PREDET HDRRDY STATE MSGLEN");
	printf("%6d %6d %6d %6d %5d %6d\n", 
				response->EOMDET, response->SOMDET, response->PREDET, 
				response->HDRRDY, response->STATE, response->MSGLEN);

	printf("Current SAME MSGLEN: %d\n", response->MSGLEN);
	printf("Current SAME DATA: '%s'\n", response->DATA);
}

void free_Si4707_SAME_Status_FullResponse(struct Si4707_SAME_Status_FullResponse* response) {
	// printf("freeing full response %p\n", response);

    if (response->DATA != NULL) {
        // printf("freeing response->DATA\n");
		free(response->DATA);
	}

	if (response->CONF) {
		free(response->CONF);
	}
}