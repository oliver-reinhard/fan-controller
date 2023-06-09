#ifndef LOG_IO_H_INCLUDED
  #define LOG_IO_H_INCLUDED

  #include <Arduino.h> 
  #include "phys_io.h"
  
  #if defined(__AVR_ATmega328P__)
    #define VERBOSE
  #endif

  const time16_ms_t INTERVAL_PAUSE_BLIP_OFF_DURATION_S = 5;      // [s] LED blips during pause: HIGH state
  const time16_ms_t INTERVAL_PAUSE_BLIP_ON_DURATION_MS = 200;    // [ms] LED LOW state

  typedef enum {MODE_UNDEF, MODE_OFF, MODE_CONTINUOUS, MODE_INTERVAL} FanMode;
  typedef enum {INTENSITY_UNDEF, INTENSITY_LOW, INTENSITY_MEDIUM, INTENSITY_HIGH} FanIntensity;
  typedef enum {SPEED_OFF, SPEED_MIN, SPEED_MEDIUM, SPEED_FULL } FanSpeed;

  class LogicalIOModel {
    public:
      void init();
      FanMode fanMode() { return mode; }
      FanIntensity fanIntensity() { return intensity; }
      FanSpeed fanSpeed() { return speed; }
      void fanSpeed(FanSpeed speed);
      bool isPwmActive();
      
      void statusLED(bool on);

      // invoked only be interrupt service routine (ISR)
      void updateFanModeFromInputPins();
      void updateFanIntensityFromInputPins();

      // Interrupt handlers (function pointers variables) to set:
      void (* modeChangedHandler)();
      void (* intensityChangedHandler)();
    
    protected:
      FanMode mode = MODE_UNDEF;
      FanIntensity intensity = INTENSITY_UNDEF;
      FanSpeed speed = SPEED_OFF;
      // the value that is actually set on the PWM output pin
      pwm_duty_t fanDutyCycleValue = 0; 
      void fanDutyCycle(pwm_duty_t value);
      pwm_duty_t fanDutyCycle() { return fanDutyCycleValue; }
  };

// Singleton instance of the LogicalIOModel class:
LogicalIOModel* logicalIO();

#endif
