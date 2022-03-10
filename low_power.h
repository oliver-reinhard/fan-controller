#ifndef LOW_POWER_H_INCLUDED
  #define LOW_POWER_H_INCLUDED
 
  #include <Arduino.h>
  #include "fan_util.h"

  void configLowPower();

  time32_ms_t _millis();
  boolean interruptibleDelay(time32_ms_t duration);
  void waitForInterrupt();

  void setInterruptCause();
  
#endif
