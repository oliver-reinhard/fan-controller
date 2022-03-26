#ifndef LOW_POWER_H_INCLUDED
  #define LOW_POWER_H_INCLUDED
 
  #include <Arduino.h>
  #include "io_util.h"

  void configLowPower();
  
  boolean delayInterruptible(time16_ms_t duration);
  
  void waitForUserInput();
  
#endif
