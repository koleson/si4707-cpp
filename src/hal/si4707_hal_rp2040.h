#include "si4707_hal.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

void hal_rp2040_set_si4707_pinmap(spi_inst_t *spi, uint mosi_pin, uint miso_pin,
                       uint sck_pin, uint cs_pin, uint rst_pin, uint gpio1_pin,
                       uint gpio2_pin);

struct Si4707_HAL_FPs* hal_rp2040_FPs();
