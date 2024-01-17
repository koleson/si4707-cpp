#ifndef SI4707_HAL_H
#define SI4707_HAL_H

struct Si4707_HAL_FPs {
  void (*cs_select)();
  void (*cs_deselect)();
  void (*setup_spi)();
  void (*reset)();
  void (*await_cts)(const int maxWait);
};

#endif // SI4707_HAL_H