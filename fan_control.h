#ifndef FAN_CONTROL_H_INCLUDED
  #define FAN_CONTROL_H_INCLUDED
  
  #include <Arduino.h> 
  
  // --------------------
  // CONFIGURABLE VALUES
  //
  // Voltages are always in [mV].
  // Durations are always in [s], unless where symbol name ends in _MS --> [ms]
  // --------------------
  
  const uint16_t FAN_MAX_VOLTAGE = 13000; // [mV]
  const uint16_t FAN_LOW_THRESHOLD_VOLTAGE = 4400; // [mV] // below this voltage, the fan will not move
  
  // Continuous operation:
  const uint16_t FAN_CONTINUOUS_LOW_VOLTAGE = 4400; // [mV] do not set lower than FAN_LOW_THRESHOLD_VOLTAGE
  const uint16_t FAN_CONTINUOUS_MEDIUM_VOLTAGE = 9000; // [mV]
  const uint16_t FAN_CONTINUOUS_HIGH_VOLTAGE = FAN_MAX_VOLTAGE; // [mV]
  
  // Interval operation:
  const uint16_t FAN_INTERVAL_FAN_ON_VOLTAGE = FAN_MAX_VOLTAGE; // [mV]
  
  const uint16_t INTERVAL_FAN_ON_DURATION = 10; // [s]
  const uint16_t INTERVAL_PAUSE_SHORT_DURATION = 10; // [s]
  const uint16_t INTERVAL_PAUSE_MEDIUM_DURATION = 20; // [s]
  const uint16_t INTERVAL_PAUSE_LONG_DURATION = 30; // [s]
  
  // Fan soft start and stop:
  const uint16_t FAN_START_DURATION_MS = 4000;  // [ms] duration from full stop to full throttle
  const uint16_t FAN_STOP_DURATION_MS = 2000;   // [ms] duration from full throttle to full stop
  const bool BLINK_LED_DURING_SPEED_TRANSITION = false;
  
  // Control cycle: PWM parameters are set only once per cycle
  const uint32_t SPEED_TRANSITION_CYCLE_DURATION_MS = 200; // [ms]
  
  //
  // ANALOG OUT
  //
  #if defined(__AVR_ATmega328P__)
    const uint8_t ANALOG_OUT_MIN = 0;        // Arduino constant
    const uint8_t ANALOG_OUT_MAX = 255;      // PWM control
    
  #elif defined(__AVR_ATtiny85__)
  // PWM frequency = 1 MHz / 1 / 40 = 25 kHz 
    const uint8_t TIMER1_PRESCALER = 1;     // divide by 1
    const uint8_t TIMER1_COUNT_TO = 40;    // count to 40
    
    const uint8_t ANALOG_OUT_MIN = 0;                 // Arduino constant
    const uint8_t ANALOG_OUT_MAX = TIMER1_COUNT_TO;   // PWM control
  #endif

  const uint8_t FAN_OUT_LOW_THRESHOLD = (uint32_t) ANALOG_OUT_MAX * FAN_LOW_THRESHOLD_VOLTAGE /  FAN_MAX_VOLTAGE;
  
  //
  // CONTROLLER STATES
  //
  enum FanState {FAN_OFF, FAN_SPEEDING_UP, FAN_STEADY, FAN_SLOWING_DOWN, FAN_PAUSING};
  
  enum Event {EVENT_NONE, MODE_CHANGED, INTENSITY_CHANGED, TARGET_SPEED_REACHED, INTERVAL_PHASE_ENDED};
  
  
  //
  // FUNCTIONS
  //
  void handleStateTransition(Event event);
  FanState getFanState();

  bool isPwmActive();

  void speedUp();
  void slowDown();

  uint32_t getIntervalPhaseBeginTime(); // [ms]
  uint32_t getIntervalPauseDuration();  // [ms]

#endif
