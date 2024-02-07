// ReSharper disable CppRedundantParentheses
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "si4707_const.h"

#include "si4707_hal.h"
// TODO:  move to HAL
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "si4707.h"
#include "util.h"

#ifndef MIN
#define MIN(a, b) ((b)>(a)?(a):(b))
#endif


struct Si4707_HAL_FPs* current_hal = NULL;
bool g_hal_checked = false;

void si4707_set_hal(struct Si4707_HAL_FPs* hal) {
	g_hal_checked = false;
	current_hal = hal;
}


static void assert_HAL_set() {
	if (g_hal_checked) 
	{
		return;
	}

	if (!current_hal)
	{
		printf("SET HAL BEFORE USING Si4707!\n");
		abort();
	}

	// TODO:  also check that all expected FPs are set
	if (current_hal->prepare_interface == NULL
			|| current_hal->txn_start == NULL
			|| current_hal->txn_end == NULL
			|| current_hal->reset == NULL
			|| current_hal->send_command_get_response_16 == NULL
			|| current_hal->read_status == NULL
			)
	{
		printf("selected Si4707 HAL does not implement all expected methods.\n");
		abort();
	}

	g_hal_checked = true;
}

void si4707_setup_interface() {
	assert_HAL_set();
	current_hal->prepare_interface();
}

void si4707_reset() {
	assert_HAL_set();
	current_hal->reset();
	puts("done resetting Si4707");
}

bool si4707_await_cts(const int maxWait) {
	assert_HAL_set();
	
	int i = 0;
	uint8_t status = 0;
	while ((status & 0x80) == 0x00 && i < maxWait) {
		status = si4707_read_status();
		
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
}

void si4707_power_up() {
	assert_HAL_set();
	puts("si4707_power_up");

	const uint8_t cmd = SI4707_CMD_POWER_UP;
	struct Si4707_Command_Args args;
	
	// 0x53
	// CTS interrupt disable
	// GPO2 output enabled
	// boot normally
	// crystal oscillator enabled (XOSCEN)
	// weatherband receive
	args.ARG1 = 0x53;
	
	// 0x05:  analog output mode
	args.ARG2 = 0x05;

	uint8_t resp_buf[16];
	
	current_hal->send_command_get_response_16(cmd, &args, resp_buf);

	sleep_ms(10);
}


// PRECONDITION:  ensure external crystal oscillator is stable
// before attempting to tune.  (AN332 suggests 500ms.)
void si4707_tune() {
	assert_HAL_set();
	puts("tuning si4707 to 162.475MHz");
	const uint8_t freqHigh = 0xFD;
	const uint8_t freqLow = 0xDE;
	
	const uint8_t cmd = SI4707_CMD_WB_TUNE_FREQ;		// write a command (drives 8 bytes on SDIO)
	struct Si4707_Command_Args args;
	args.ARG1 = 0x00;											// AN332 page 180 - reserved, always write 0, no meaning
	args.ARG2 = freqHigh;
	args.ARG3 = freqLow;
	args.ARG4 = 0x00; args.ARG5 = 0x00; args.ARG6 = 0x00; args.ARG7 = 0x00;

	uint8_t resp_buf[16];
	current_hal->send_command_get_response_16(cmd, &args, resp_buf);

	// tune status takes a moment to populate, so we don't check it here.
	// kmo 10 oct 2023 21h54
}

void si4707_send_command(const uint8_t cmd, const struct Si4707_Command_Args* args) {
	assert_HAL_set();
	// we will ignore response but we need a place for it
	// kmo 19 jan 2024 11h29

	uint8_t resp_buf[16];
	current_hal->send_command_get_response_16(cmd, args, resp_buf);
}

void si4707_send_command_noargs(const uint8_t cmd) {
	assert_HAL_set();
	struct Si4707_Command_Args args;
	args.ARG1 = 0x00; args.ARG2 = 0x00; args.ARG3 = 0x00; args.ARG4 = 0x00;
	args.ARG5 = 0x00; args.ARG6 = 0x00; args.ARG7 = 0x00;

	si4707_send_command(cmd, &args);
}

void si4707_set_property(uint8_t prop_high, uint8_t prop_low, uint8_t val_high, uint8_t val_low)
{
	// 0x12 SET_PROPERTY - AN332 page 177
	assert_HAL_set();
	struct Si4707_Command_Args args;
	args.ARG1 = 0x00; // Always write to 0.
	args.ARG2 = prop_high; args.ARG3 = prop_low;
	args.ARG4 = val_high; args.ARG5 = val_low;
	args.ARG6 = 0x00; args.ARG7 = 0x00;
	si4707_send_command(SI4707_CMD_SET_PROPERTY, &args);
}

// NOTE:  DO NOT try to replace this with read_resp, as that requires CTS
// and therefore causes recursion/stack overflow
// kmo 17 jan 2024 16h26
uint8_t si4707_read_status() {
	assert_HAL_set();
	
	return current_hal->read_status();
}

void si4707_get_rev() {
	uint8_t product_data[16] = { 0x00 };
	const struct Si4707_Command_Args args;
	
	current_hal->send_command_get_response_16(SI4707_CMD_GET_REV, &args, product_data);
	
	const uint8_t pn = product_data[1];
	if (pn != 7) {
		printf("product number invalid - halting\n\n");
		abort();
	}
}

void si4707_get_rsq(struct Si4707_RSQ_Status *rsq_status) 
{
	uint8_t wb_rsq_resp[16] = { 0x00 };
	const struct Si4707_Command_Args args;
	
	current_hal->send_command_get_response_16(SI4707_CMD_WB_RSQ_STATUS, &args, wb_rsq_resp);
	// printf("si4707_get_rsq[0]: %d", wb_rsq_resp[0]);
	// printf("si4707_get_rsq[1]: %d", wb_rsq_resp[1]);
	// printf("si4707_get_rsq[2]: %d", wb_rsq_resp[2]);

	memcpy(rsq_status, wb_rsq_resp, sizeof(struct Si4707_RSQ_Status));
}

void si4707_print_rsq() 
{
	struct Si4707_RSQ_Status status;
	si4707_get_rsq(&status);
	puts("RSSI  SNR");
	printf("%4d  %3d\n\n", status.RSSI, status.ASNR);
}


void si4707_get_asq(struct Si4707_ASQ_Status *asq_status, bool asq_int_ack) 
{
	uint8_t wb_asq_resp[16] = { 0x00 };
	struct Si4707_Command_Args args;
	args.ARG1 = (asq_int_ack ? 1 : 0);

	current_hal->send_command_get_response_16(SI4707_CMD_WB_ASQ_STATUS, &args, wb_asq_resp);
	memcpy(asq_status, wb_asq_resp, sizeof(struct Si4707_ASQ_Status));
}

void si4707_print_asq()
{
	struct Si4707_ASQ_Status status;
	si4707_get_asq(&status, false);
	printf("ALERTOFF_INT  ALERTON_INT  ALERT\n");
	printf("%12d  %11d  %5d\n\n", status.ALERTOFF_INT, status.ALERTON_INT, status.ALERT);
}

void si4707_get_same_packet(const struct Si4707_SAME_Status_Params *params,
							struct Si4707_SAME_Status_Packet *packet) {
  uint8_t wb_same_resp[16] = { 0x00 };

  struct Si4707_Command_Args args;
  // ARG1:  D1 = CLRBUF; D0 = INTACK
  args.ARG1 = (params->INTACK ? 0x01 : 0x00) | (params->CLRBUF ? 0x02 : 0x00);

  // ARG2:  READADDR
  args.ARG2 = params->READADDR; args.ARG3 = 0x00; args.ARG4 = 0x00; 
	args.ARG5 = 0x00; args.ARG6 = 0x00; args.ARG7 = 0x00;

	current_hal->send_command_get_response_16(SI4707_CMD_WB_SAME_STATUS, &args, wb_same_resp);

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
  // printf("si4707_get_same_packet: wb_same_resp[3] = %d\n", wb_same_resp[3]);
  packet->MSGLEN = wb_same_resp[3];

  // copy strings (not null-terminated, fixed-length)
  memcpy(packet->CONF, wb_same_resp + 4, 2);
  memcpy(packet->DATA, wb_same_resp + 6, 8);
}

static void copy_si4707_status_packet_to_full_response(const struct Si4707_SAME_Status_Packet * status_packet, struct Si4707_SAME_Status_FullResponse * full_response) {
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
    // printf("si4707_get_same_status: first_packet.MSGLEN = %d\n", first_packet.MSGLEN);
}

static int responses_needed(int msglen) {
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

void si4707_get_same_status(const struct Si4707_SAME_Status_Params *params, struct Si4707_SAME_Status_FullResponse *full_response)
{
	struct Si4707_SAME_Status_Packet first_packet;

	si4707_get_same_packet(params, &first_packet);

	copy_si4707_status_packet_to_full_response(&first_packet, full_response);

	unsigned int whole_responses_needed = responses_needed(first_packet.MSGLEN);

	// TODO:  malloc size based on MSGLEN.  kmo 18 oct 2023 15h54
	// MSGLEN can be at most 255, so adding null termination, 256 max length
	const unsigned int alloc_length = 256;
	uint8_t* same_buf = (uint8_t*)malloc(sizeof(uint8_t) * alloc_length);

  // TODO:  confidence might not actually need to be this big? kmo 4 nov 2023 11h23
	// TODO:  populate this buffer
  uint8_t* conf_buf = (uint8_t*)malloc(sizeof(uint8_t) * alloc_length);
	
	// auto-null-termination
	for (unsigned int i = 0; i < alloc_length; i++) {
		same_buf[i] = 0x00;
	}

	struct Si4707_SAME_Status_Params same_buf_params;
	struct Si4707_SAME_Status_Packet same_buf_packet;
	if (params->INTACK || params->CLRBUF) {
		printf("si4707.c: si4707_get_same_status: INTACK = %u / CLRBUF = %u\n", 
			params->INTACK, params->CLRBUF);
	}
	
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
		si4707_get_same_packet(&same_buf_params, &same_buf_packet);

		memcpy((same_buf + offset), same_buf_packet.DATA, chars_to_read);

		// confidence data is in reverse order of corresponding SAME data for some reason.
		// (AN332 page 186) kmo 27 dec 2023 12h05
		r_memcpy((conf_buf + offset), same_buf_packet.CONF, chars_to_read);
	}

	// heap-allocated variables exit here
	full_response->DATA = same_buf;
  full_response->CONF = conf_buf;
}

void si4707_print_same_status(const struct Si4707_SAME_Status_FullResponse* response) {
	puts("EOMDET SOMDET PREDET HDRRDY STATE MSGLEN");
	printf("%6d %6d %6d %6d %5d %6d\n", 
				response->EOMDET, response->SOMDET, response->PREDET, 
				response->HDRRDY, response->STATE, response->MSGLEN);

	printf("SAME DATA: '%s'\n", response->DATA);
}

void si4707_free_SAME_Status_FullResponse(struct Si4707_SAME_Status_FullResponse* response) {
  if (response->DATA != NULL) 
	{
		free(response->DATA);
    response->DATA = NULL;
	}

	if (response->CONF != NULL) 
	{
		free(response->CONF);
  	response->CONF = NULL;
	}
}
