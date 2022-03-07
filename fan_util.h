#ifndef FAN_UTIL_H_INCLUDED
  #define FAN_UTIL_H_INCLUDED
 
  #include <Arduino.h>
  
  void configInput(uint8_t pin);
  
  void configInputWithPullup(uint8_t pin);
  
  void configOutput(uint8_t pin);
  
#endif
