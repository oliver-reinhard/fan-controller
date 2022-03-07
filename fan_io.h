#ifndef FAN_IO_H_INCLUDED
  #define FAN_IO_H_INCLUDED

  #include <Arduino.h> 
  
  #if defined(__AVR_ATmega328P__)
    #define VERBOSE
    const uint8_t MODE_SWITCH_IN_PIN_1 = 8;         // PB0 - digital: PB0==LOW  && PB1==HIGH  --> INTERVAL
    const uint8_t MODE_SWITCH_IN_PIN_2 = 9;         // PB1 - digital: PB0==HIGH && PB1==LOW   --> CONTINUOUS
                                                    //                PB0==HIGH && PB1==HIGH  --> OFF
    const uint8_t INTENSITY_SWITCH_IN_PIN_1 = 6;    // PD6 - digital: PD6==LOW  && PD7==HIGH  --> LOW INTENSITY
    const uint8_t INTENSITY_SWITCH_IN_PIN_2 = 7;    // PD7 - digital: PD6==HIGH && PD7==LOW   --> HIGH INTENSITY
                                                    //                PD6==HIGH && PD7==HIGH  --> MEDIUM INTENSITY
    const uint8_t FAN_PWM_OUT_PIN = 3;              // PD3 - PWM signal @ native frequency (490 Hz)
    const uint8_t FAN_POWER_OUT_PIN = 4;            // PD4 - fan power: MOSFET on/off
    const uint8_t STATUS_LED_OUT_PIN = 5;           // PD5 - digital out; is on when fan is of, blinks during transitioning 
    const uint8_t SLEEP_LED_OUT_PIN = 10;           // PB2 - digital out; on while MCU is in sleep mode 
  
  #elif defined(__AVR_ATtiny85__)
    const uint8_t MODE_SWITCH_IN_PIN = PB2;         // digital: LOW --> CONTINOUS, HIGH --> INTERVAL
    const uint8_t INTENSITY_SWITCH_IN_PIN_1 = PB3;  // digital: PB3==LOW && PB4==HIGH --> LOW INTENSITY, BOTH=LOW --> MEDIUM
    const uint8_t INTENSITY_SWITCH_IN_PIN_2 = PB4;  // digital: PB3==HIGH && PB4==LOW --> HIGH INTENSITY
    
    const uint8_t FAN_POWER_OUT_PIN = PB5;          // Fan power: MOSFET on/off (some fans don't stop at PWM duty cycle = 0%)
    const uint8_t FAN_PWM_OUT_PIN = PB1;            // PWM signal @ 25 kHz
    const uint8_t STATUS_LED_OUT_PIN = PB0;         // digital out; blinks shortly in long intervals when fan is in interval mode
  #endif 

  const uint16_t INTERVAL_PAUSE_BLIP_OFF_DURATION_MS = 5000;  // [ms] LED blips during pause: HIGH state
  const uint16_t INTERVAL_PAUSE_BLIP_ON_DURATION_MS = 200;    // [ms] LED LOW state
  
  //
  // INPUTS
  //
  enum FanMode {MODE_UNDEF, MODE_OFF, MODE_CONTINUOUS, MODE_INTERVAL};
  
  enum FanIntensity {INTENSITY_UNDEF, INTENSITY_LOW, INTENSITY_MEDIUM, INTENSITY_HIGH};
  
  //
  // FUNCTIONS
  //
  void configInputPins();
  void configOutputPins();
  void configInt0Interrupt();
  void configPinChangeInterrupts();
  void configPWM1();
  
  // Returns true if value changed
  bool updateFanModeFromInputPins();
  FanMode getFanMode();
  
  // Returns true if value changed
  bool updateFanIntensityFromInputPins();
  FanIntensity getFanIntensity();

  
  void setFanPower(bool on);
  void setFanDutyCycle(uint8_t value);
  uint8_t getFanDutyCycle();
  
  void setStatusLED(bool on);
  void invertStatusLED();
  
  void showPauseBlip();
  void resetPauseBlip();  // reset time
  uint32_t getLastPauseBlipTime();      // [ms]

#endif
