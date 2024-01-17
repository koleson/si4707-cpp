// make a struct for RSQ data
// make a struct for SAME data
#ifndef SI4707_H
#define SI4707_H

#include "si4707_structs.h"
#include "hardware/spi.h"
#include "si4707_hal.h"

void si4707_set_hal(struct Si4707_HAL_FPs* hal);

void free_Si4707_SAME_Status_FullResponse(struct Si4707_SAME_Status_FullResponse* response);

void set_si4707_pinmap(spi_inst_t *spi, uint mosi_pin, uint miso_pin,
                       uint sck_pin, uint cs_pin, uint rst_pin, uint gpio1_pin,
                       uint gpio2_pin);
void setup_si4707_spi();

// some of this should be private
void reset_si4707();
bool await_si4707_cts(int maxWait);
uint8_t read_status();
void read_resp(uint8_t* resp);
void power_up_si4707();
void get_si4707_rev();
void tune_si4707();
void print_si4707_rsq();
void print_si4707_same_status(const struct Si4707_SAME_Status_FullResponse* response);

void get_si4707_rsq(struct Si4707_RSQ_Status *rsq_status);

void get_si4707_same_packet(const struct Si4707_SAME_Status_Params *params,
                            struct Si4707_SAME_Status_Packet *packet);

void get_si4707_same_status(const struct Si4707_SAME_Status_Params *params,
							struct Si4707_SAME_Status_FullResponse *full_response);

// TODO: docs

#endif // SI4707_H
