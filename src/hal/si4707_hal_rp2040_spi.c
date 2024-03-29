
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "si4707_hal.h"
#include "si4707_const.h"
#include "si4707.h"

static spi_inst_t* hal_rp2040_spi = NULL;
static uint8_t hal_rp2040_mosi_pin = 0;
static uint8_t hal_rp2040_miso_pin = 0;
static uint8_t hal_rp2040_sck_pin = 0;
static uint8_t hal_rp2040_cs_pin = 0;
static uint8_t hal_rp2040_reset_pin = 0;
static uint8_t hal_rp2040_gpo1_pin = 0;
static uint8_t hal_rp2040_gpo2_pin = 0;

static bool hal_rp2040_pinmap_set = false;

void hal_rp2040_set_si4707_pinmap(spi_inst_t *spi, uint mosi_pin, uint miso_pin,
                       uint sck_pin, uint cs_pin, uint rst_pin, uint gpio1_pin,
                       uint gpio2_pin) 
{
  hal_rp2040_spi = spi;
  hal_rp2040_mosi_pin = mosi_pin;
  hal_rp2040_miso_pin = miso_pin;
  hal_rp2040_sck_pin = sck_pin;
  hal_rp2040_cs_pin = cs_pin;
  hal_rp2040_reset_pin = rst_pin;
  hal_rp2040_gpo1_pin = gpio1_pin;
  hal_rp2040_gpo2_pin = gpio2_pin;
  
	hal_rp2040_pinmap_set = true;
}

static void hal_rp2040_assert_pinmap_set() {
	if (!hal_rp2040_pinmap_set) {
    printf("RP2040 HAL:  pinmap not set for Si4707 but you're trying to use it\n");
		abort();
  }
}

static void hal_rp2040_setup_si4707_spi() {
  hal_rp2040_assert_pinmap_set();

  // SPI initialization. 400kHz.
  spi_init(hal_rp2040_spi, 400 * 1000);
  gpio_set_function(hal_rp2040_mosi_pin, GPIO_FUNC_SPI);
  gpio_set_function(hal_rp2040_miso_pin, GPIO_FUNC_SPI);
  gpio_set_function(hal_rp2040_sck_pin, GPIO_FUNC_SPI);
  gpio_set_function(hal_rp2040_cs_pin, GPIO_FUNC_SIO);

  // Chip select is active-low, so we'll initialize it to a driven-high state
  gpio_set_dir(hal_rp2040_cs_pin, GPIO_OUT);
  gpio_put(hal_rp2040_cs_pin, true);
}

void hal_rp2040_prepare_interface() {
  hal_rp2040_setup_si4707_spi();
}

void hal_rp2040_si4707_txn_start() 
{
  hal_rp2040_assert_pinmap_set();
  asm volatile("nop \n nop \n nop");
	gpio_put(hal_rp2040_cs_pin, false);  // Active low
	asm volatile("nop \n nop \n nop");
}

void hal_rp2040_si4707_txn_end() 
{
  hal_rp2040_assert_pinmap_set();
	asm volatile("nop \n nop \n nop");
	gpio_put(hal_rp2040_cs_pin, true);
	asm volatile("nop \n nop \n nop");
}

// TODO:  a lot of this could be extracted if we just
// add some GPIO methods.  the sleeps should be common code.
// (then again sleep is also platform-specific.)
// kmo 17 jan 2024
void hal_rp2040_spi_si4707_reset()
{
  hal_rp2040_assert_pinmap_set();

  // resetting to SPI mode requires
  // GPO2 *AND* GPO1 are high.  GPO2 must be driven (easy, it has no other use
  // here) GPO1 can float or be driven - since it's used for SPI, we have to
  // deinit it before si4707_reset ends.  it seems easiest to drive it
  // momentarily to make sure.

  puts("resetting Si4707");

  gpio_init(hal_rp2040_reset_pin);
  gpio_set_dir(hal_rp2040_reset_pin, GPIO_OUT);
  gpio_put(hal_rp2040_reset_pin, false);
  sleep_ms(10);

  // drive GPO2/INT + GPO1/MISO high to select SPI bus mode on Si4707
  gpio_init(hal_rp2040_gpo1_pin);
  gpio_set_dir(hal_rp2040_gpo1_pin, GPIO_OUT);
  gpio_put(hal_rp2040_gpo1_pin, true);

  // GPO1 = MISO - we can use it before SPI is set up
  gpio_init(hal_rp2040_gpo2_pin);
  gpio_set_dir(hal_rp2040_gpo2_pin, GPIO_OUT);
  gpio_put(hal_rp2040_gpo2_pin, true);

  sleep_ms(5);

  gpio_put(hal_rp2040_reset_pin, true);
  sleep_ms(5);

  gpio_put(hal_rp2040_gpo1_pin, false);
  gpio_put(hal_rp2040_gpo2_pin, false);
  sleep_ms(2);

  // GPO could be used as INT later
  gpio_deinit(hal_rp2040_gpo1_pin);
  // GPO1 is used as MISO later
  gpio_deinit(hal_rp2040_gpo2_pin);
  sleep_ms(2);
}

void hal_rp2040_spi_si4707_send_command_get_response(const uint8_t cmd, 
    const struct Si4707_Command_Args* args, uint8_t* resp_buf)
{
  const bool cmd_cts = si4707_await_cts(CTS_WAIT);

  if (!cmd_cts) {
    printf("exhausted patience waiting for CTS - aborting\n");
    abort();
  }

  uint8_t cmd_buf[9] = { 0x00 };
		
  // SPI command send - 8 bytes follow - 1 byte of cmd, 7 bytes of arg
  cmd_buf[0] = SI4707_SPI_SEND_CMD;
  cmd_buf[1] = cmd;

  cmd_buf[2] = args->ARG1; cmd_buf[3] = args->ARG2; cmd_buf[4] = args->ARG3; cmd_buf[5] = args->ARG4;
  cmd_buf[6] = args->ARG5; cmd_buf[7] = args->ARG6; cmd_buf[8] = args->ARG7;

  hal_rp2040_si4707_txn_start();

  spi_write_blocking(hal_rp2040_spi, cmd_buf, 9);

  hal_rp2040_si4707_txn_end();

  const bool resp_cts = si4707_await_cts(CTS_WAIT);

  if (!resp_cts) {
    printf("exhausted patience waiting for CTS - aborting\n");
    abort();
  }
  uint8_t resp_cmd[1];
  resp_cmd[0] = SI4707_SPI_READ16_GPO1;
  
  hal_rp2040_si4707_txn_start();
  
  spi_write_blocking(hal_rp2040_spi, resp_cmd, 1);
  spi_read_blocking(hal_rp2040_spi, 0, resp_buf, 16);
  
  hal_rp2040_si4707_txn_end();
}

uint8_t hal_rp2040_spi_si4707_read_status() {
  uint8_t status_resp[1] = { 0x00 };
  const uint8_t cmd[1] = { SI4707_SPI_READ1_GPO1 };

  hal_rp2040_si4707_txn_start();
  spi_write_blocking(hal_rp2040_spi, cmd, 1);
  spi_read_blocking(hal_rp2040_spi, 0, status_resp, 1);
  hal_rp2040_si4707_txn_end();

  return status_resp[0];
}

struct Si4707_HAL_FPs* hal_rp2040_FPs() 
{
  struct Si4707_HAL_FPs* function_pointers = (struct Si4707_HAL_FPs*)malloc(sizeof(struct Si4707_HAL_FPs));
  
  function_pointers->txn_start = hal_rp2040_si4707_txn_start;
  function_pointers->txn_end = hal_rp2040_si4707_txn_end;
  function_pointers->prepare_interface = hal_rp2040_prepare_interface;
  function_pointers->reset = hal_rp2040_spi_si4707_reset;
  function_pointers->send_command_get_response_16 = hal_rp2040_spi_si4707_send_command_get_response;
  function_pointers->read_status = hal_rp2040_spi_si4707_read_status;

  return function_pointers;
}