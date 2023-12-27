// ReSharper disable CppRedundantParentheses
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "si4707_const.h"
#include "hardware.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "si4707.h"
#include "util.h"

#ifndef MIN
#define MIN(a, b) ((b)>(a)?(a):(b))
#endif

#define CTS_WAIT 250

void setup_si4707_spi_ez() {
	setup_si4707_spi(SI4707_SPI_PORT, SI4707_SPI_MOSI, SI4707_SPI_MISO, SI4707_SPI_SCK, SI4707_SPI_CS);
}

void setup_si4707_spi(spi_inst_t* spi, uint mosi_pin, uint miso_pin, uint sck_pin, uint cs_pin) {
	// SPI initialization. 400kHz.
	spi_init(spi, 400*1000);
	gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
	gpio_set_function(miso_pin, GPIO_FUNC_SPI);
	gpio_set_function(sck_pin,  GPIO_FUNC_SPI);
	gpio_set_function(cs_pin,   GPIO_FUNC_SIO);

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

bool await_si4707_cts(const int maxWait) {
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
	const uint8_t freqHigh = 0xFD;
	const uint8_t freqLow = 0xDE;
	
	uint8_t cmd[9] = { 0x00 };
	cmd[0] = SI4707_SPI_SEND_CMD;       // write a command (drives 8 bytes on SDIO)
	cmd[1] = SI4707_CMD_WB_TUNE_FREQ;
	cmd[2] = 0x00;                      // AN332 page 180 - reserved, always write 0, no meaning
	cmd[3] = freqHigh;
	cmd[4] = freqLow;
	
	const bool cts = await_si4707_cts(CTS_WAIT);
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

void send_command(const uint8_t cmd, const struct Si4707_Command_Args* args) {
    uint8_t cmd_buf[9] = { 0x00 };
		
		// SPI command send - 8 bytes follow - 1 byte of cmd, 7 bytes of arg
    cmd_buf[0] = SI4707_SPI_SEND_CMD;
    cmd_buf[1] = cmd;

    cmd_buf[2] = args->ARG1; cmd_buf[3] = args->ARG2; cmd_buf[4] = args->ARG3; cmd_buf[5] = args->ARG4;
    cmd_buf[6] = args->ARG5; cmd_buf[7] = args->ARG6; cmd_buf[8] = args->ARG7;

    si4707_cs_select();
    spi_write_blocking(SI4707_SPI_PORT, cmd_buf, 9);
    si4707_cs_deselect();

    const bool cts = await_si4707_cts(CTS_WAIT);
    if (cts) {
        uint8_t resp_buf[16] = { 0x00 };
        read_resp(resp_buf);
    } else {
        printf("could not send command %02x - CTS timeout\n", cmd);
    }
}

void send_command_noargs(const uint8_t cmd) {
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
	

	const bool cts = await_si4707_cts(CTS_WAIT);
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
	
	const bool cts_cmd = await_si4707_cts(CTS_WAIT);
	if (cts_cmd) {
		send_command_noargs(SI4707_CMD_GET_REV);
	} else {
		puts("could not request product info - CTS timeout");
		return;
	}
	
	const bool cts_read = await_si4707_cts(CTS_WAIT);
	if (cts_read) {
		read_resp(product_data);
		
		const uint8_t pn = product_data[1];
		// printf("product number: %d\n", pn);
		
		if (pn != 7) {
			printf("product number invalid - halting\n\n");

            // halt
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
			// ReSharper disable once CppDFAEndlessLoop
			while (true) {
				busy_wait_ms(100000);
			}
#pragma clang diagnostic pop
		}
	} else {
		puts("could not read product info - CTS timeout");
	}
}

void get_si4707_rsq(struct Si4707_RSQ_Status *rsq_status) {
	uint8_t wb_rsq_resp[16] = { 0x00 };
	send_command_noargs(SI4707_CMD_WB_RSQ_STATUS);

	read_resp(wb_rsq_resp);
	const uint8_t valid = wb_rsq_resp[2] & 0x01;
	const uint8_t rssi = wb_rsq_resp[4];
	const uint8_t snr = wb_rsq_resp[5];
	
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

void get_si4707_same_packet(const struct Si4707_SAME_Status_Params *params,
							struct Si4707_SAME_Status_Packet *packet) {
  uint8_t wb_same_resp[16] = { 0x00 };

  struct Si4707_Command_Args args;
  // ARG1:  D1 = CLRBUF; D0 = INTACK
  args.ARG1 = (params->INTACK ? 0x01 : 0x00) | (params->CLRBUF ? 0x02 : 0x00);

  // ARG2:  READADDR
  args.ARG2 = params->READADDR; args.ARG3 = 0x00; args.ARG4 = 0x00; 
	args.ARG5 = 0x00; args.ARG6 = 0x00; args.ARG7 = 0x00;

  send_command(SI4707_CMD_WB_SAME_STATUS, &args);
  read_resp(wb_same_resp);

  // byte 0:  CTS/ERR/-/-/RSQINT/SAMEINT/ASQINT/STCINT
  packet->CTS = ((wb_same_resp[0] & 0x80) != 0);
  packet->ERR = ((wb_same_resp[0] & 0x40) != 0);
  // empty 0x10, 0x20
  packet->RSQINT = 	((wb_same_resp[0] & 0x08) != 0);
  packet->SAMEINT = ((wb_same_resp[0] & 0x04) != 0);
  packet->ASQINT = 	((wb_same_resp[0] & 0x02) != 0);
  packet->STCINT = 	((wb_same_resp[0] & 0x01) != 0);

  // byte 1:  -/-/-/-/EOMDET/SOMDET/PREDET/HDRRDY
  packet->EOMDET = ((wb_same_resp[1] & 0x08) != 0);
  packet->SOMDET = ((wb_same_resp[1] & 0x04) != 0);
  packet->PREDET = ((wb_same_resp[1] & 0x02) != 0);
  packet->HDRRDY = ((wb_same_resp[1] & 0x01) != 0);

  packet->STATE = wb_same_resp[2];
  // printf("get_si4707_same_packet: wb_same_resp[3] = %d\n", wb_same_resp[3]);
  packet->MSGLEN = wb_same_resp[3];

  // copy strings (not null-terminated, fixed-length)
  memcpy(packet->CONF, wb_same_resp + 4, 2);
  memcpy(packet->DATA, wb_same_resp + 6, 8);
}

void _copy_si4707_status_packet_to_full_response(const struct Si4707_SAME_Status_Packet * status_packet, struct Si4707_SAME_Status_FullResponse * full_response) {
	// byte 0
	full_response->CTS = 			status_packet->CTS;
	full_response->ERR =			status_packet->ERR;
	full_response->RSQINT =		status_packet->RSQINT;
	full_response->SAMEINT = 	status_packet->SAMEINT;
	full_response->ASQINT =		status_packet->ASQINT;
	full_response->STCINT =		status_packet->STCINT;

	// byte 1
	full_response->EOMDET = 	status_packet->EOMDET;
	full_response->SOMDET = 	status_packet->SOMDET;
	full_response->PREDET = 	status_packet->PREDET;
	full_response->HDRRDY = 	status_packet->HDRRDY;

	full_response->STATE  = 	status_packet->STATE;
	full_response->MSGLEN = 	status_packet->MSGLEN;
    // printf("get_si4707_same_status: first_packet.MSGLEN = %d\n", first_packet.MSGLEN);
}

int _responses_needed(int msglen) {
	// maximum message length is ~250 chars.
	const int whole_responses_needed = msglen / 8;
	const int remainder = msglen % 8;
	int responses_needed;
	
	// kmo 18 oct 2023 10h51:  seems like this is still under-counting
    // kmo 20 oct 2023 18h03:  I think I fixed the undercounting.
	// kmo 22 nov 2023 15h19:  this remains an enigma, computer science!
	if (remainder > 0) {
		responses_needed = whole_responses_needed + 1;
	} else {
		responses_needed = whole_responses_needed;
	}

	// printf("%d responses needed to get message of length %d\n", responses_needed, msglen);

	return responses_needed;
}

void get_si4707_same_status(const struct Si4707_SAME_Status_Params *params, struct Si4707_SAME_Status_FullResponse *full_response)
{
	struct Si4707_SAME_Status_Packet first_packet;

	get_si4707_same_packet(params, &first_packet);

	_copy_si4707_status_packet_to_full_response(&first_packet, full_response);

	unsigned int whole_responses_needed = _responses_needed(first_packet.MSGLEN);

	// TODO:  malloc size based on MSGLEN.  kmo 18 oct 2023 15h54
	// MSGLEN can be at most 255, so adding null termination, 256 max length
	const unsigned int alloc_length = 256;
	uint8_t* same_buf = malloc(sizeof(uint8_t) * alloc_length);

  // TODO:  confidence might not actually need to be this big? kmo 4 nov 2023 11h23
	// TODO:  populate this buffer
  uint8_t* conf_buf = malloc(sizeof(uint8_t) * alloc_length);
	
	// auto-null-termination
	for (unsigned int i = 0; i < alloc_length; i++) {
		same_buf[i] = 0x00;
	}

	struct Si4707_SAME_Status_Params same_buf_params;
	struct Si4707_SAME_Status_Packet same_buf_packet;
	printf("si4707.c: get_si4707_same_status: INTACK = %u / CLRBUF = %u\n", 
		params->INTACK, params->CLRBUF);
	same_buf_params.INTACK = params->INTACK;
	same_buf_params.CLRBUF = params->CLRBUF;
	same_buf_params.READADDR = 0;


	// NOTE:  The length reported by MSGLEN must include the implicit
	// ZCZC?  (Narrator:  It does not.)
	
	// preamble: This is a consecutive string of bits (sixteen bytes of AB 
	// hexadecimal [8 bit byte 10101011]) sent to clear the system, set AGC and 
	// set asynchronous decoder clocking cycles. The preamble must be 
	// transmitted before each header and End Of Message code.
	
	// AN332:  "[MSGLEN] excludes the preamble and the header code block identifier 'ZCZC'."
	// So I guess MSGLEN includes low-confidence garbage bytes at the end of the buffer?
	// kmo 22 nov 2023 16h32
	// confirmed - MSGLEN includes 0-confidence garbage bytes.
	// kmo 22 nov 2023 16h38
	
	// in practice, the 3 bytes following the SAME message seem to be null characters (0x00).
	// but i don't have sufficient data points to say if that's universal or not, so eventually
	// i will indeed need to mask using the confidence data.
	// kmo 22 nov 2023 17h13
	
	for (int i = 0; i < whole_responses_needed; i++) {
		const unsigned int offset = i * 8;
		const unsigned int chars_remaining = first_packet.MSGLEN - (i * 8);
		
		unsigned int chars_to_read;

        // kmo temp note:  this logic was reversed - was setting chars_to_read = 8 when chars_remaining <8;
        // was setting chars_to_read = chars_remaining when chars_remaining >= 8.
        // kmo 29 nov 2023 11h18
		if (chars_remaining < 8) {
            // printf("only reading %d chars in last copy\n", chars_remaining);
            // printf("same_buf before final memcpy: '%s'\n", same_buf);
			chars_to_read = chars_remaining;
		} else {
			chars_to_read = 8;
		}

		same_buf_params.READADDR = offset;
		get_si4707_same_packet(&same_buf_params, &same_buf_packet);

		memcpy((same_buf + offset), same_buf_packet.DATA, chars_to_read);

		// confidence data is in reverse order of corresponding SAME data for some reason.
		// (AN332 page 186) kmo 27 dec 2023 12h05

		// TODO:  un-hold this code once CLRBUF operation is validated
		// kmo 27 dec 2023 12h14
		// r_memcpy((conf_buf + offset), same_buf_packet.CONF, chars_to_read);
	}

	// heap-allocated variables exit here
	full_response->DATA = same_buf;
  full_response->CONF = conf_buf;
}

void print_si4707_same_status(const struct Si4707_SAME_Status_FullResponse* response) {
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
        response->DATA = NULL;
	}

	if (response->CONF != NULL) {
        // printf("freeing response->CONF\n");
		free(response->CONF);
        response->CONF = NULL;
	}

    // printf("done freeing full response\n");
}
