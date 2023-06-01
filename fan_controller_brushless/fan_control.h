#ifndef FAN_CONTROL_H_INCLUDED
  #define FAN_CONTROL_H_INCLUDED
  
  #include <Arduino.h> 
  #include "io_util.h"
  #include "fan_io.h"
  
  // --------------------
  // CONFIGURABLE VALUES
  //
  // Voltages are always in [mV].
  // Durations are always in seconds [s], unless where symbol name ends in _MS --> milliseconds [ms]
  // --------------------

  // Continuous operation:
  const pwm_duty_t FAN_CONTINUOUS_LOW_DUTY_VALUE = FAN_LOW_THRESHOLD_DUTY_VALUE;   // do not set lower than FAN_LOW_THRESHOLD_DUTY_VALUE --> fan would not start
  const pwm_duty_t FAN_CONTINUOUS_MEDIUM_DUTY_VALUE = 40;
  const pwm_duty_t FAN_CONTINUOUS_HIGH_DUTY_VALUE = ANALOG_OUT_MAX;
  
  // Interval operation:
  const pwm_duty_t INTERVAL_FAN_ON_DUTY_VALUE = ANALOG_OUT_MAX;
  
  const duration16_s_t INTERVAL_FAN_ON_DURATION = 300;                // [s]
  const duration16_s_t INTERVAL_PAUSE_SHORT_DURATION = 60;            // [s]
  const duration16_s_t INTERVAL_PAUSE_MEDIUM_DURATION = 600;          // [s]
  const duration16_s_t INTERVAL_PAUSE_LONG_DURATION = 3600 - INTERVAL_FAN_ON_DURATION;           // [s]
  
  // Fan soft start and stop:
  const duration16_ms_t FAN_START_DURATION_MS = 6000;                  // [ms] duration from full stop to full throttle
  const duration16_ms_t FAN_STOP_DURATION_MS = 8000;                   // [ms] duration from full throttle to full stop
  const bool BLINK_LED_DURING_SPEED_TRANSITION = true;
  
  // Control cycle: PWM parameters are set only once per cycle
  const duration16_ms_t SPEED_TRANSITION_CYCLE_DURATION_MS = 200;      // [ms]
  
  
  //
  // CONTROLLER STATES
  //
  typedef enum  {FAN_OFF, FAN_SPEEDING_UP, FAN_STEADY, FAN_SLOWING_DOWN, FAN_PAUSING} FanState;
  
  typedef enum  {EVENT_NONE, MODE_CHANGED, INTENSITY_CHANGED, TARGET_SPEED_REACHED, INTERVAL_PHASE_ENDED} Event;

  void initFanControl();
  
  //
  // FUNCTIONS
  //
  void handleStateTransition(Event event);
  FanState getFanState();

  void speedUp(time32_s_t now);
  void slowDown(time32_s_t now);

  time32_s_t getIntervalPhaseBeginTime(); // [s]
  duration16_s_t getIntervalPauseDuration();  // [s]
  
  void resetPauseBlip();  // reset time
  time32_s_t getLastPauseBlipTime();      // [s]

#endif
