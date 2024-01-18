#ifndef SI4707_HAL_H
#define SI4707_HAL_H

#include <stdint.h>
#include "si4707_structs.h"

// HAL Assumptions:
// - should work with SPI or I2C
// - on i2c, select/deselect (successors start/end) will be no-ops

struct Si4707_HAL_FPs {
  void (*prepare_interface)();
  void (*txn_start)();
  void (*txn_end)();
  void (*reset)();
  void (*power_up)();
  void (*send_command)(const uint8_t cmd, const struct Si4707_Command_Args* args);

};

#endif // SI4707_HAL_H