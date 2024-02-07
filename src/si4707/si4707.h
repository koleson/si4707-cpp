// make a struct for RSQ data
// make a struct for SAME data
#ifndef SI4707_H
#define SI4707_H

#include "si4707_structs.h"
#include "hardware/spi.h"
#include "si4707_hal.h"

// hardware abstraction layer
void si4707_set_hal(struct Si4707_HAL_FPs* hal);
void si4707_setup_interface();

// some of this should be private

bool si4707_await_cts(int maxWait);
uint8_t si4707_read_status();
void si4707_read_resp_16(uint8_t* resp);

// power / tuning

void si4707_reset();
void si4707_power_up();
void si4707_get_rev();
void si4707_tune();

// received signal quality metrics

void si4707_rsq_get(struct Si4707_RSQ_Status *rsq_status);
void si4707_rsq_print();


// 1050Hz alert tone

void si4707_asq_enable_interrupts();
void si4707_asq_get(struct Si4707_ASQ_Status *asq_status, bool asq_int_ack);
void si4707_asq_print();

// SAME (Specific Area Message Encoding)

void si4707_same_packet_get(const struct Si4707_SAME_Status_Params *params,
                            struct Si4707_SAME_Status_Packet *packet);

void si4707_same_status_get(const struct Si4707_SAME_Status_Params *params,
							struct Si4707_SAME_Status_FullResponse *full_response);
void si4707_same_status_print(const struct Si4707_SAME_Status_FullResponse* response);
void si4707_SAME_Status_FullResponse_free(struct Si4707_SAME_Status_FullResponse* response);

// TODO: docs

#endif // SI4707_H
