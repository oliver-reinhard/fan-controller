#ifndef LOW_POWER_H_INCLUDED
  #define LOW_POWER_H_INCLUDED
 
  #include <Arduino.h>
  #include "io_util.h"

  void configLowPower();

  time32_ms_t sleeplessMillis();
  
  boolean delayInterruptible(time32_ms_t duration);
  void waitForInterrupt();
  
#endif
