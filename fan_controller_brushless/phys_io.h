#ifndef PHYS_IO_H_INCLUDED
  #define PHYS_IO_H_INCLUDED

  #include <Arduino.h> 
  #include <io_util.h>
  #include <limits.h>
  
  #if defined(__AVR_ATmega328P__)
    // #define VERBOSE
  #endif
  
  //
  // PINS
  //
  #if defined(__AVR_ATmega328P__)
    const pin_t MODE_SWITCH_IN_PIN_1 = 8;         // PB0 - digital: PB0==HIGH               --> OFF (HIGH --> port configured as pull-up)
    const pin_t MODE_SWITCH_IN_PIN_2 = 9;         // PB1 - digital: PB0==HIGH && PB1==LOW   --> CONTINUOUS
                                                  // PB0 - digital: PB0==HIGH && PB1==HIGH  --> INTERVAL
    const pin_t INTENSITY_SWITCH_IN_PIN_1 = 6;    // PD6 - digital: PD6==LOW  && PD7==HIGH  --> LOW INTENSITY
    const pin_t INTENSITY_SWITCH_IN_PIN_2 = 7;    // PD7 - digital: PD6==HIGH && PD7==LOW   --> HIGH INTENSITY
                                                  //                PD6==HIGH && PD7==HIGH  --> MEDIUM INTENSITY
    const pin_t FAN_PWM_OUT_PIN = 10;             // PB2 - OC1B PWM signal !! DO NOT CHANGE PIN !! (PWM configuration is specific to Timer 1)
    const pin_t STATUS_LED_OUT_PIN = 5;           // PD5 - digital out; is on when fan is off, blinks during transitioning 
    const pin_t WDT_WAKEUP_OUT_PIN = 12;          // PB4 - digital out; blinks briefly after watchdog-timer wakeup 
  
  #elif defined(__AVR_ATtiny85__)
    const pin_t MODE_SWITCH_IN_PIN = PB2;         // digital: LOW --> CONTINOUS, HIGH --> INTERVAL (HIGH --> port configured as pull-up)
    const pin_t INTENSITY_SWITCH_IN_PIN_1 = PB4;  // digital: PB4==LOW  && PB3==HIGH  --> LOW INTENSITY
    const pin_t INTENSITY_SWITCH_IN_PIN_2 = PB3;  // digital: PB4==HIGH && PB3==LOW   --> HIGH INTENSITY
                                                  //          PD4==HIGH && PD3==HIGH  --> MEDIUM INTENSITY
    const pin_t FAN_PWM_OUT_PIN = PB1;            // PWM signal @ 25 kHz
    const pin_t STATUS_LED_OUT_PIN = PB0;         // digital out; blinks shortly in long intervals when fan is in interval mode
  #endif 

  //
  // ANALOG OUT (PWM / Timer1 scaling to 25 kHz)
  //
  #if defined(__AVR_ATmega328P__)
    const uint16_t TIMER1_COUNT_TO = 320;    // count to this value (Timer 1 is 16 bit)

  #elif defined(__AVR_ATtiny85__)
    #if (F_CPU == 1000000UL)
      // PWM frequency = 1 MHz / 1 / 40 = 25 kHz 
      const uint8_t TIMER1_COUNT_TO = 40;    // count to this value
    #else
      #error("F_CPU is undefined or its value is unexpected")
    #endif
  #endif
  
  // FAN SPEED CONTROL:
  const  pwm_duty_t FAN_LOW_THRESHOLD_DUTY_VALUE = 20;  // [mV] // below this DUTY_VALUE @ 13 Volts, the fan will not move

  // Continuous operation:
  const pwm_duty_t FAN_CONTINUOUS_LOW_DUTY_VALUE = FAN_LOW_THRESHOLD_DUTY_VALUE;   // do not set lower than FAN_LOW_THRESHOLD_DUTY_VALUE --> fan would not start
  const pwm_duty_t FAN_CONTINUOUS_MEDIUM_DUTY_VALUE = 35;
  const pwm_duty_t FAN_CONTINUOUS_HIGH_DUTY_VALUE = PWM_DUTY_MAX;
  
  // Interval operation:
  const pwm_duty_t INTERVAL_FAN_ON_DUTY_VALUE = PWM_DUTY_MAX;
  
  //
  // CONFIGURATION
  //
  void configPhysicalIO();
  void pwmDutyCycle(pwm_duty_t value);
#endif
