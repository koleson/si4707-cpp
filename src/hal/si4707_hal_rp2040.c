
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "si4707_hal.h"

// need full pinmap in here

uint g_hal_rp2040_cs_pin = 0;
bool g_hal_rp2040_pinmap_set = false;

void hal_rp2040_set_si4707_pinmap(spi_inst_t *spi, uint mosi_pin, uint miso_pin,
                       uint sck_pin, uint cs_pin, uint rst_pin, uint gpio1_pin,
                       uint gpio2_pin) 
{
  g_hal_rp2040_cs_pin = cs_pin;
	g_hal_rp2040_pinmap_set = true;
}

void hal_rp2040_assert_pinmap_set() {
	if (!g_hal_rp2040_pinmap_set) {
    printf("WARNING:  pinmap not set for Si4707 but you're trying to use it\n");
		return;
  }
}

void hal_rp2040_si4707_cs_select() 
{
  asm volatile("nop \n nop \n nop");
	gpio_put(g_hal_rp2040_cs_pin, 0);  // Active low
	asm volatile("nop \n nop \n nop");
}

void hal_rp2040_si4707_cs_deselect() 
{
  hal_rp2040_assert_pinmap_set();
	asm volatile("nop \n nop \n nop");
	gpio_put(g_hal_rp2040_cs_pin, 1);
	asm volatile("nop \n nop \n nop");
}

struct Si4707_HAL_FPs* hal_rp2040_FPs() 
{
  struct Si4707_HAL_FPs* function_pointers = (struct Si4707_HAL_FPs*)malloc(sizeof(struct Si4707_HAL_FPs));
  function_pointers->cs_select = hal_rp2040_si4707_cs_select;
  function_pointers->cs_deselect = hal_rp2040_si4707_cs_deselect;
  return function_pointers;
}