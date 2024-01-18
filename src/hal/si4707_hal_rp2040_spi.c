
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "si4707_const.h"
#include "si4707_hal.h"
#include "si4707.h"

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

void hal_rp2040_assert_pinmap_set() 
{
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

void hal_rp2040_prepare_interface() {
  hal_rp2040_setup_si4707_spi();
}

void hal_rp2040_si4707_txn_start() 
{
  hal_rp2040_assert_pinmap_set();
  asm volatile("nop \n nop \n nop");
	gpio_put(g_hal_rp2040_cs_pin, 0);  // Active low
	asm volatile("nop \n nop \n nop");
}

void hal_rp2040_si4707_txn_end() 
{
  hal_rp2040_assert_pinmap_set();
	asm volatile("nop \n nop \n nop");
	gpio_put(g_hal_rp2040_cs_pin, 1);
	asm volatile("nop \n nop \n nop");
}

// TODO:  a lot of this could be extracted if we just
// add some GPIO methods.  the sleeps should be common code.
// (then again sleep is also platform-specific.)
// kmo 17 jan 2024
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

void si4707_hal_rpi2040_spi_send_command_and_read_response(const bool wait_for_cts_command, const uint8_t cmd, const struct Si4707_Command_Args* args, 
                                          const bool wait_for_cts_response, const uint8_t resp_length, uint8_t* resp_buf)
{
  // note to self:  unclear whether i should use the 
  // HAL version or the driver version.  kmo 17 jan 2024
  // si4707_txn_start();
  hal_rp2040_si4707_txn_start();
  
  if (wait_for_cts_command) {
    si4707_await_cts(CTS_WAIT);
  }

  uint8_t cmd_buf[9] = { 0x00 };
  cmd_buf[0] = cmd;
  cmd_buf[1] = args->ARG1; cmd_buf[2] = args->ARG2; cmd_buf[3] = args->ARG3;
  cmd_buf[4] = args->ARG4; cmd_buf[5] = args->ARG5; cmd_buf[6] = args->ARG6;
  cmd_buf[7] = args->ARG7;

  // TODO:  i cannot remember why this is 9 instead of 8 length
  // kmo 17 jan 2024 20h50
  spi_write_blocking(g_hal_rp2040_spi, cmd_buf, 9);

  if (wait_for_cts_response) {
    si4707_await_cts(CTS_WAIT);
  }

  spi_read_blocking(g_hal_rp2040_spi, 0, resp_buf, resp_length);

  hal_rp2040_si4707_txn_end();

}

struct Si4707_HAL_FPs* hal_rp2040_FPs() 
{
  struct Si4707_HAL_FPs* function_pointers = (struct Si4707_HAL_FPs*)malloc(sizeof(struct Si4707_HAL_FPs));
  
  function_pointers->txn_start = hal_rp2040_si4707_txn_start;
  function_pointers->txn_end = hal_rp2040_si4707_txn_end;
  function_pointers->prepare_interface = hal_rp2040_prepare_interface;
  function_pointers->reset = hal_rp2040_si4707_reset;

  return function_pointers;
}
