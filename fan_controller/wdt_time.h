#ifndef WDT_TIME_H_INCLUDED
  #define WDT_TIME_H_INCLUDED

  #include <Arduino.h> 
  #include "io_util.h"

  void configWatchdogTime();
  
  time32_s_t wdtTime_s();

  void enableArduinoTimer0(); // Timer0 is used for millis() function --> not used by watchdog
  void disableArduinoTimer0();

#endif
