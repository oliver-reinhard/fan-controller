#ifndef FAN_CONTROL_H_INCLUDED
  #define FAN_CONTROL_H_INCLUDED
  
  #include <Arduino.h> 
  #include "fan_util.h"
  #include "fan_io.h"
  
  // --------------------
  // CONFIGURABLE VALUES
  //
  // Voltages are always in [mV].
  // Durations are always in seconds [s], unless where symbol name ends in _MS --> milliseconds [ms]
  // --------------------

  // Continuous operation:
  const uint16_t FAN_CONTINUOUS_LOW_VOLTAGE = 4400;             // [mV] do not set lower than FAN_LOW_THRESHOLD_VOLTAGE
  const uint16_t FAN_CONTINUOUS_MEDIUM_VOLTAGE = 9000;          // [mV]
  const uint16_t FAN_CONTINUOUS_HIGH_VOLTAGE = FAN_MAX_VOLTAGE; // [mV]
  
  // Interval operation:
  const uint16_t INTERVAL_FAN_ON_VOLTAGE = FAN_MAX_VOLTAGE;     // [mV]
  
  const time16_s_t INTERVAL_FAN_ON_DURATION = 10;                 // [s]
  const time16_s_t INTERVAL_PAUSE_SHORT_DURATION = 10;            // [s]
  const time16_s_t INTERVAL_PAUSE_MEDIUM_DURATION = 20;           // [s]
  const time16_s_t INTERVAL_PAUSE_LONG_DURATION = 30;             // [s]
  
  // Fan soft start and stop:
  const time16_ms_t FAN_START_DURATION_MS = 2000;                  // [ms] duration from full stop to full throttle
  const time16_ms_t FAN_STOP_DURATION_MS = 1000;                   // [ms] duration from full throttle to full stop
  const boolean  BLINK_LED_DURING_SPEED_TRANSITION = true;
  
  // Control cycle: PWM parameters are set only once per cycle
  const time32_ms_t SPEED_TRANSITION_CYCLE_DURATION_MS = 200;      // [ms]
  
  
  //
  // CONTROLLER STATES
  //
  typedef enum  {FAN_OFF, FAN_SPEEDING_UP, FAN_STEADY, FAN_SLOWING_DOWN, FAN_PAUSING} FanState;
  
  typedef enum  {EVENT_NONE, MODE_CHANGED, INTENSITY_CHANGED, TARGET_SPEED_REACHED, INTERVAL_PHASE_ENDED} Event;
  
  
  //
  // FUNCTIONS
  //
  void handleStateTransition(Event event);
  FanState getFanState();


  void speedUp();
  void slowDown();

  time32_ms_t getIntervalPhaseBeginTime(); // [ms]
  time32_ms_t getIntervalPauseDuration();  // [ms]

#endif
