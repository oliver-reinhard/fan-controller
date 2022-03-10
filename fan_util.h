#ifndef FAN_UTIL_H_INCLUDED
  #define FAN_UTIL_H_INCLUDED
 
  #include <Arduino.h>

  typedef uint8_t pin_t;
  
  typedef uint16_t time16_ms_t;
  typedef uint32_t time32_ms_t;
  typedef uint16_t time16_s_t;
  
  void configInput(pin_t pin);
  
  void configInputWithPullup(pin_t pin);
  
  void configOutput(pin_t pin);
  
#endif
