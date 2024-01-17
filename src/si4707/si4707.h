// make a struct for RSQ data
// make a struct for SAME data
#ifndef SI4707_H
#define SI4707_H

#include "si4707_structs.h"
#include "hardware/spi.h"
#include "si4707_hal.h"

void si4707_set_hal(struct Si4707_HAL_FPs* hal);

void si4707_free_SAME_Status_FullResponse(struct Si4707_SAME_Status_FullResponse* response);

void si4707_set_pinmap(spi_inst_t *spi, uint mosi_pin, uint miso_pin,
                       uint sck_pin, uint cs_pin, uint rst_pin, uint gpio1_pin,
                       uint gpio2_pin);
void si4707_setup_spi();

// some of this should be private
void si4707_reset();
bool si4707_await_cts(int maxWait);
uint8_t read_status();
void si4707_read_resp(uint8_t* resp);
void si4707_power_up();
void si4707_get_rev();
void si4707_tune();
void si4707_print_rsq();
void si4707_print_same_status(const struct Si4707_SAME_Status_FullResponse* response);

void si4707_get_rsq(struct Si4707_RSQ_Status *rsq_status);

void si4707_get_same_packet(const struct Si4707_SAME_Status_Params *params,
                            struct Si4707_SAME_Status_Packet *packet);

void si4707_get_same_status(const struct Si4707_SAME_Status_Params *params,
							struct Si4707_SAME_Status_FullResponse *full_response);

// TODO: docs

#endif // SI4707_H
