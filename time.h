#ifndef TIME_H_INCLUDED
  #define TIME_H_INCLUDED

  #include <Arduino.h> 
  #include "io_util.h"

  void configTime();
  
  time32_s_t _time_s();
  
  void enableArduinoTimer0();
  void disableArduinoTimer0();
  
  
  /*
   * Sets up timer2 to generate interrupts at each overflow:
   * - OVF 
   * Note: 16-bit range (up to 65 seconds)
   */
  void setTimer2_millis(time16_ms_t ms);
  
  /*
   * Note: 16-bit range (up to 18 hours)
   */
  void setTimer2_seconds(time16_s_t sec);

  
/**
 * Stops the timer and disables its interrupts.
 */
void stopTimer2() ;

boolean timer2CountingDown();

#endif
