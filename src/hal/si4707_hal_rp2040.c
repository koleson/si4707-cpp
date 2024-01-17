
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "si4707_hal.h"

// need full pinmap in here

spi_inst_t* g_hal_rp2040_spi = NULL;
uint g_hal_rp2040_mosi_pin = 0;
uint g_hal_rp2040_miso_pin = 0;
uint g_hal_rp2040_sck_pin = 0;
uint g_hal_rp2040_cs_pin = 0;
uint g_hal_rp2040_reset_pin = 0;
uint g_hal_rp2040_gpo1_pin = 0;
uint g_hal_rp2040_gpo2_pin = 0;

bool g_hal_rp2040_pinmap_set = false;

void hal_rp2040_set_si4707_pinmap(spi_inst_t *spi, uint mosi_pin, uint miso_pin,
                       uint sck_pin, uint cs_pin, uint rst_pin, uint gpio1_pin,
                       uint gpio2_pin) 
{
  g_hal_rp2040_spi = spi;
  g_hal_rp2040_mosi_pin = mosi_pin;
  g_hal_rp2040_miso_pin = miso_pin;
  g_hal_rp2040_sck_pin = sck_pin;
  g_hal_rp2040_cs_pin = cs_pin;
  g_hal_rp2040_reset_pin = rst_pin;
  g_hal_rp2040_gpo1_pin = gpio1_pin;
  g_hal_rp2040_gpo2_pin = gpio2_pin;
  
	g_hal_rp2040_pinmap_set = true;
}

void hal_rp2040_assert_pinmap_set() {
	if (!g_hal_rp2040_pinmap_set) {
    printf("RP2040 HAL:  pinmap not set for Si4707 but you're trying to use it\n");
		abort();
  }
}

void hal_rp2040_setup_si4707_spi() {
  hal_rp2040_assert_pinmap_set();

  // SPI initialization. 400kHz.
  spi_init(g_hal_rp2040_spi, 400 * 1000);
  gpio_set_function(g_hal_rp2040_mosi_pin, GPIO_FUNC_SPI);
  gpio_set_function(g_hal_rp2040_miso_pin, GPIO_FUNC_SPI);
  gpio_set_function(g_hal_rp2040_sck_pin, GPIO_FUNC_SPI);
  gpio_set_function(g_hal_rp2040_cs_pin, GPIO_FUNC_SIO);

  // Chip select is active-low, so we'll initialize it to a driven-high state
  gpio_set_dir(g_hal_rp2040_cs_pin, GPIO_OUT);
  gpio_put(g_hal_rp2040_cs_pin, 1);
}

void hal_rp2040_si4707_cs_select() 
{
  hal_rp2040_assert_pinmap_set();
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

void hal_rp2040_si4707_reset()
{
  hal_rp2040_assert_pinmap_set();
  puts("resetting Si4707");
	
	gpio_init(g_hal_rp2040_reset_pin);
	gpio_set_dir(g_hal_rp2040_reset_pin, GPIO_OUT);
	gpio_put(g_hal_rp2040_reset_pin, 0);
	sleep_ms(10);
	
	// drive GPO2/INT + GPO1/MISO high to select SPI bus mode on Si4707
	gpio_init(g_hal_rp2040_gpo1_pin);
	gpio_set_dir(g_hal_rp2040_gpo1_pin, GPIO_OUT);
	gpio_put(g_hal_rp2040_gpo1_pin, 1);
	
	// GPO1 = MISO - we can use it before SPI is set up
	gpio_init(g_hal_rp2040_gpo2_pin);
	gpio_set_dir(g_hal_rp2040_gpo2_pin, GPIO_OUT);
	gpio_put(g_hal_rp2040_gpo2_pin, 1);
	
	sleep_ms(5);
	
	gpio_put(g_hal_rp2040_reset_pin, 1);
	sleep_ms(5);
	
	gpio_put(g_hal_rp2040_gpo1_pin, 0);
	gpio_put(g_hal_rp2040_gpo2_pin, 0);
	sleep_ms(2);
	
	// GPO could be used as INT later
	gpio_deinit(g_hal_rp2040_gpo1_pin);
	// GPO1 is used as MISO later
	gpio_deinit(g_hal_rp2040_gpo2_pin);
	sleep_ms(2);
}

/*
void hal_rp2040_si4707_await_cts()
{
  hal_rp2040_assert_pinmap_set();
  // TODO:  need to implement read_status first
}
*/

uint8_t hal_rp2040_si4707_read_status()
{
  const uint8_t status_cmd[1] = { 0xA0 }; // read status byte via GPO1
	
	// buffer:  receive power up status to this buffer
	uint8_t status_result[1] = { 0x00 };

  hal_rp2040_si4707_cs_select();
  spi_write_blocking(g_hal_rp2040_spi, status_cmd, 1);
  spi_read_blocking(g_hal_rp2040_spi, 0, status_result, 1);
  hal_rp2040_si4707_cs_deselect();

  return status_result[0];
}

struct Si4707_HAL_FPs* hal_rp2040_FPs() 
{
  struct Si4707_HAL_FPs* function_pointers = (struct Si4707_HAL_FPs*)malloc(sizeof(struct Si4707_HAL_FPs));
  function_pointers->cs_select = hal_rp2040_si4707_cs_select;
  function_pointers->cs_deselect = hal_rp2040_si4707_cs_deselect;
  function_pointers->setup_spi = hal_rp2040_setup_si4707_spi;
  function_pointers->reset = hal_rp2040_si4707_reset;
  function_pointers->read_status = hal_rp2040_si4707_read_status;
  return function_pointers;
}