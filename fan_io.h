#ifndef FAN_IO_H_INCLUDED
  #define FAN_IO_H_INCLUDED

  #include <Arduino.h> 
  #include "io_util.h"
  
  #if defined(__AVR_ATmega328P__)
    #define VERBOSE
  #endif
  
  typedef uint8_t pwm_duty_t;
  
  #if defined(__AVR_ATmega328P__)
    const pin_t MODE_SWITCH_IN_PIN_1 = 8;         // PB0 - digital: PB0==LOW  && PB1==HIGH  --> INTERVAL
    const pin_t MODE_SWITCH_IN_PIN_2 = 9;         // PB1 - digital: PB0==HIGH && PB1==LOW   --> CONTINUOUS
                                                  //                PB0==HIGH && PB1==HIGH  --> OFF
    const pin_t INTENSITY_SWITCH_IN_PIN_1 = 6;    // PD6 - digital: PD6==LOW  && PD7==HIGH  --> LOW INTENSITY
    const pin_t INTENSITY_SWITCH_IN_PIN_2 = 7;    // PD7 - digital: PD6==HIGH && PD7==LOW   --> HIGH INTENSITY
                                                  //                PD6==HIGH && PD7==HIGH  --> MEDIUM INTENSITY
    const pin_t FAN_PWM_OUT_PIN = 10;             // PB2 - PWM signal @ native frequency (490 Hz)
    const pin_t STATUS_LED_OUT_PIN = 5;           // PD5 - digital out; is on when fan is of, blinks during transitioning 
    const pin_t SLEEP_LED_OUT_PIN = 11;           // PB3 - digital out; on while MCU is in sleep mode 
  
  #elif defined(__AVR_ATtiny85__)
    const pin_t MODE_SWITCH_IN_PIN = PB2;         // digital: LOW --> CONTINOUS, HIGH --> INTERVAL
    const pin_t INTENSITY_SWITCH_IN_PIN_1 = PB3;  // digital: PB3==LOW && PB4==HIGH --> LOW INTENSITY, BOTH=LOW --> MEDIUM
    const pin_t INTENSITY_SWITCH_IN_PIN_2 = PB4;  // digital: PB3==HIGH && PB4==LOW --> HIGH INTENSITY
    
    const pin_t FAN_POWER_ON_OUT_PIN = PB5;          // Fan power: MOSFET on/off (some fans don't stop at PWM duty cycle = 0%)
    const pin_t FAN_PWM_OUT_PIN = PB1;            // PWM signal @ 25 kHz
    const pin_t STATUS_LED_OUT_PIN = PB0;         // digital out; blinks shortly in long intervals when fan is in interval mode
  #endif 

  // Fan electrical characteristics:
  const millivolt_t FAN_MAX_VOLTAGE = 13000;                       // [mV]
  const millivolt_t FAN_LOW_THRESHOLD_VOLTAGE = 4400;              // [mV] // below this voltage, the fan will not move
  
  // --------------------
  // FIXED VALUES -- DO NOT CHANGE (unless you know what you're doing)
  // --------------------
  
  //
  // ANALOG OUT
  //
  #if defined(__AVR_ATmega328P__)
    const pwm_duty_t ANALOG_OUT_MIN = 0;        // Arduino constant
    const pwm_duty_t ANALOG_OUT_MAX = 255;      // PWM control
    
  #elif defined(__AVR_ATtiny85__)
    const pwm_duty_t ANALOG_OUT_MIN = 0;                 // Arduino constant
    const pwm_duty_t ANALOG_OUT_MAX = TIMER1_COUNT_TO;   // PWM control
  #endif

  const pwm_duty_t FAN_OUT_LOW_THRESHOLD = (uint32_t) ANALOG_OUT_MAX * FAN_LOW_THRESHOLD_VOLTAGE /  FAN_MAX_VOLTAGE;

  // Interfaces:
  const time16_ms_t INTERVAL_PAUSE_BLIP_OFF_DURATION_S = 5;      // [s] LED blips during pause: HIGH state
  const time16_ms_t INTERVAL_PAUSE_BLIP_ON_DURATION_MS = 200;    // [ms] LED LOW state

  //
  // PWM / Timer1 scaling
  //
  #if defined(__AVR_ATtiny85__)
    #if (F_CPU == 1000000UL)
      // PWM frequency = 1 MHz / 1 / 40 = 25 kHz 
      const uint8_t TIMER1_PRESCALER = 1;     // divide by 1
      const uint8_t TIMER1_COUNT_TO = 40;     // count to 40
    #elif #if (F_CPU == 128000UL)
      // PWM frequency = 128 kHz / 1 / 5 = 25.6 kHz 
      const uint8_t TIMER1_PRESCALER = 1;     // divide by 1
      const uint8_t TIMER1_COUNT_TO = 5;      // count to 5
    #else
      #error("F_CPU is undefined or its value is unknown")
    #endif
  #endif
  
  //
  // INPUTS
  //
  typedef enum {MODE_UNDEF, MODE_OFF, MODE_CONTINUOUS, MODE_INTERVAL} FanMode;
  
  typedef enum {INTENSITY_UNDEF, INTENSITY_LOW, INTENSITY_MEDIUM, INTENSITY_HIGH} FanIntensity;

  typedef enum {NO_INPUT_INTERRUPT, MODE_CHANGED_INTERRUPT, INTENSITY_CHANGED_INTERRUPT} InputInterrupt;


  // 
  // Interrupt handling
  //
  void resetInputInterrupt();
  boolean isInputInterrupt();
  
  //
  // FUNCTIONS
  //
  void configInputPins();
  void configOutputPins();
  void configInt0Interrupt();
  void configPinChangeInterrupts();
  void configPWM1();
  
  // Returns true if value changed
  boolean updateFanModeFromInputPins();
  FanMode getFanMode();
  
  // Returns true if value changed
  boolean updateFanIntensityFromInputPins();
  FanIntensity getFanIntensity();

  
  void setFanPower(boolean on);
  void setFanDutyCycle(pwm_duty_t value);
  pwm_duty_t getFanDutyCycle();
  boolean isPwmActive();
  
  void setStatusLED(boolean on);
  void invertStatusLED();
  
  void showPauseBlip();

#endif
