#ifndef LOW_POWER_H_INCLUDED
  #define LOW_POWER_H_INCLUDED
 
  #include <Arduino.h>
  #include "io_util.h"

  void configLowPower();
  
  bool delayInterruptible_millis(time16_ms_t duration);
  bool delayInterruptible_seconds(time16_s_t duration);
  
  void waitForUserInput();
  
#endif
