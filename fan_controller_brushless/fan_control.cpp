#include "HardwareSerial.h"
#include "fan_control.h"
#include "low_power.h"
#include "wdt_time.h"

//
// ANALOG OUT
//
const pwm_duty_t FAN_OUT_FAN_OFF = ANALOG_OUT_MIN;

//
// CONTROLLER
//

volatile FanState fanState = FAN_OFF;          // current fan state

volatile time32_s_t      intervalPhaseBeginTime = 0; // [s]
volatile duration32_s_t  intervalPauseDuration;      // [s]

volatile time32_ms_t     speedTransistionEndTime = 0;

volatile time32_ms_t     lastPauseBlipTime = 0;
  
// (function pointers)
extern void (* modeChangedHandler)();
extern void (* intensityChangedHandler)();

//
// Event handlers
//

void handleModeChange() {
  handleStateTransition(MODE_CHANGED);
}

void handleIntensityChange() {
  handleStateTransition(INTENSITY_CHANGED);
}

void initFanControl() {
  // Install input-change handlers (= assign function pointers)
  modeChangedHandler = handleModeChange;
  intensityChangedHandler = handleIntensityChange; 

  getFanIntensity(); // ensure initialisation
  if (getFanMode() != MODE_OFF) {
    handleStateTransition(MODE_CHANGED);
  }
}

//
// FUNCTIONS
//

FanState getFanState() {
  return fanState;
}

// [s]
time32_s_t getIntervalPhaseBeginTime() { 
  return intervalPhaseBeginTime;
}

// [s]
duration16_s_t getIntervalPauseDuration() {
  return intervalPauseDuration;
}

void resetPauseBlip() {
  lastPauseBlipTime = wdtTime_s();
}

// [s]
time32_s_t getLastPauseBlipTime() {
  return lastPauseBlipTime;
}

void beginSpeedTransition(pwm_duty_t newTargetDutyValue) {
  int16_t deltaDutyValue = (int16_t)getFanDutyCycle() - (int16_t)newTargetDutyValue;
  if (deltaDutyValue > 0) {
    speedTransistionEndTime =  millis() + FAN_START_DURATION_MS * deltaDutyValue / ANALOG_OUT_MAX;
  } else if (deltaDutyValue < 0) {
    speedTransistionEndTime =  millis() - FAN_STOP_DURATION_MS * deltaDutyValue / ANALOG_OUT_MAX;
  } else {
    speedTransistionEndTime =  millis();
  }
  setFanDutyCycle(newTargetDutyValue);
}

void endSpeedTransition() {
  speedTransistionEndTime = 0;
  setStatusLED(LOW);
}

// Applicable only in mode CONTINUOUS
pwm_duty_t mapToFanDutyValue(FanIntensity intensity) {
  switch(intensity) {
    case INTENSITY_HIGH: 
      return FAN_CONTINUOUS_HIGH_DUTY_VALUE;
    case INTENSITY_MEDIUM: 
      return FAN_CONTINUOUS_MEDIUM_DUTY_VALUE;
    default: 
      return FAN_CONTINUOUS_LOW_DUTY_VALUE;
  }
}

// Applicable only in mode INTERVAL
// Returns [s]
time16_s_t mapToIntervalPauseDuration(FanIntensity intensity) {
  switch(intensity) {
    case INTENSITY_HIGH: 
      return INTERVAL_PAUSE_SHORT_DURATION; // [s]
    case INTENSITY_MEDIUM: 
      return INTERVAL_PAUSE_MEDIUM_DURATION; // [s]
    default: 
      return INTERVAL_PAUSE_LONG_DURATION; // [s]
  }
}
  
#ifdef VERBOSE
  const char* fanStateName(FanState state) {
    switch (state) {
      case FAN_OFF: return "OFF";
      case FAN_SPEEDING_UP: return "SPEEDING UP";
      case FAN_STEADY: return "STEADY";
      case FAN_SLOWING_DOWN: return "SLOWING DOWN";
      case FAN_PAUSING: return "PAUSE";
      default: return "?";
    }
  }
  
  const char* eventName(Event event) {
    switch (event) {
      case EVENT_NONE: return "NONE";
      case MODE_CHANGED: return "Mode changed";
      case INTENSITY_CHANGED: return "Intensity changed";
      case TARGET_SPEED_REACHED: return "Speed reached";
      case INTERVAL_PHASE_ENDED: return "Phase ended";
      default: return "?";
    }
  }
#endif

void fanOn(FanMode mode) {
  configOutput(FAN_PWM_OUT_PIN);
  pwm_duty_t fanTargetDutyValue;
  if (mode == MODE_CONTINUOUS) {
    fanTargetDutyValue = mapToFanDutyValue(getFanIntensity());
  } else { /* getFanMode() == MODE_INTERVAL */
    fanTargetDutyValue = INTERVAL_FAN_ON_DUTY_VALUE;
  }
  beginSpeedTransition(fanTargetDutyValue);
}

// Fan has reached min speed.
void fanOff(FanMode mode) {
  if (mode == MODE_INTERVAL) {
     intervalPauseDuration = mapToIntervalPauseDuration(getFanIntensity());
  }
  setFanDutyCycle(FAN_OUT_FAN_OFF);
  endSpeedTransition();
  configInput(FAN_PWM_OUT_PIN);
}

void speedUp(time32_s_t now) {
  #ifdef VERBOSE
    Serial.print("Speeding up: ");
    Serial.print((speedTransistionEndTime - now));
    Serial.println(" ms remaining");
    Serial.flush();
  #endif
  if (now >= speedTransistionEndTime) {
    handleStateTransition(TARGET_SPEED_REACHED);
    
  } else if (BLINK_LED_DURING_SPEED_TRANSITION) {
    invertStatusLED();
  }
}


void slowDown(time32_s_t now) {
  #ifdef VERBOSE
    Serial.print("Slowing down: ");
    Serial.print((speedTransistionEndTime - now));
    Serial.println(" ms remaining");
    Serial.flush();
  #endif
  if (now >= speedTransistionEndTime) {
    handleStateTransition(TARGET_SPEED_REACHED);
    
  } else if (BLINK_LED_DURING_SPEED_TRANSITION) {
    invertStatusLED();
  }
}



void handleStateTransition(Event event) {
  if (event == EVENT_NONE) {
    return;
  }
  time32_s_t now = wdtTime_s();
  #ifdef VERBOSE
    FanState beforeState = fanState;
  #endif
  
  switch(fanState) {
    
    case FAN_OFF:
      switch(event) {
        case MODE_CHANGED: 
          {
            FanMode newMode = getFanMode();
            if (newMode != MODE_OFF) {
              fanState = FAN_SPEEDING_UP;
              fanOn(newMode);
              intervalPhaseBeginTime = now;
            }
          }
          break;
          
        case INTENSITY_CHANGED:
          // noop: new value has been recorded in getFanIntensity(), will take effect on next mode change
          break;
          
        default: 
          break;
      }
      break;
      
    case FAN_SPEEDING_UP: 
      switch(event) {
        case MODE_CHANGED: 
          {
            FanMode newMode = getFanMode();
            if (newMode == MODE_OFF) {
              fanState = FAN_SLOWING_DOWN;
              beginSpeedTransition(FAN_OUT_FAN_OFF);
            }
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (getFanMode() == MODE_CONTINUOUS) {
            pwm_duty_t newTargetDutyValue = mapToFanDutyValue(getFanIntensity());
            if (newTargetDutyValue > getFanDutyCycle()) {
              /* continue: fanState = FAN_SPEEDING_UP */
              beginSpeedTransition(newTargetDutyValue);
            } else if (newTargetDutyValue < getFanDutyCycle()) {
              fanState = FAN_SLOWING_DOWN;
              beginSpeedTransition(newTargetDutyValue);
            } else { /* newTargetDutyValue == getFanDutyCycle() */
              fanState = FAN_STEADY;
              endSpeedTransition();
            }
          }
          break;
          
        case TARGET_SPEED_REACHED: 
          fanState = FAN_STEADY;
          intervalPhaseBeginTime = now;
          endSpeedTransition();
          break;

        default:
          break;
      }
      break;
      
    case FAN_STEADY: 
      switch(event) {
        case MODE_CHANGED: 
          {
            FanMode newMode = getFanMode();
            if (newMode == MODE_OFF) {
              fanState = FAN_SLOWING_DOWN;
              beginSpeedTransition(FAN_OUT_FAN_OFF);
            }
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (getFanMode() == MODE_CONTINUOUS) {
            pwm_duty_t newTargetDutyValue = mapToFanDutyValue(getFanIntensity());
            if (newTargetDutyValue > getFanDutyCycle()) {
              fanState = FAN_SPEEDING_UP;
              beginSpeedTransition(newTargetDutyValue);
            } else if (newTargetDutyValue < getFanDutyCycle()) {
              fanState = FAN_SLOWING_DOWN;
              beginSpeedTransition(FAN_OUT_FAN_OFF);
            } 
          }
          break;

        case INTERVAL_PHASE_ENDED:
          fanState = FAN_SLOWING_DOWN;
          beginSpeedTransition(FAN_OUT_FAN_OFF);
          intervalPhaseBeginTime = now;
          break;
          
        default:
          break;
      }
      break;
      
    case FAN_SLOWING_DOWN: 
      switch(event) {
        case MODE_CHANGED:
          {
            FanMode newMode = getFanMode();
            if (newMode == MODE_OFF) {
              beginSpeedTransition(FAN_OUT_FAN_OFF);
            }
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (getFanMode() == MODE_CONTINUOUS) {
            pwm_duty_t newTargetDutyValue = mapToFanDutyValue(getFanIntensity());
            if (newTargetDutyValue > getFanDutyCycle()) {
              fanState = FAN_SPEEDING_UP;
              beginSpeedTransition(newTargetDutyValue);
            } else if (newTargetDutyValue < getFanDutyCycle()) {
              /* continue: fanState = FAN_SLOWING_DOWN */
              beginSpeedTransition(newTargetDutyValue);
            } else {  /* newTargetDutyValue == getFanDutyCycle() */
              fanState = FAN_STEADY;
              endSpeedTransition();
            }
          }
          break;
          
        case TARGET_SPEED_REACHED: 
          if (getFanMode() == MODE_OFF) {
            fanState = FAN_OFF;
            fanOff(MODE_INTERVAL);
          } else if (getFanMode() == MODE_CONTINUOUS) {
            fanState = FAN_STEADY;
            endSpeedTransition();
          } else {  // getFanMode() == MODE_INTERVAL
            fanState = FAN_PAUSING;
            fanOff(MODE_INTERVAL);
            intervalPhaseBeginTime = now;
            resetPauseBlip();
          }
          setStatusLED(LOW);
          break;

        default:
          break;
      }
      break;
      
    case FAN_PAUSING:
      switch(event) {
        case MODE_CHANGED:
          {
            FanMode newMode = getFanMode();
            if (newMode == MODE_OFF) {
              fanState = FAN_OFF;
              fanOff(MODE_INTERVAL);
            } else if (newMode == MODE_CONTINUOUS) {
              fanState = FAN_SPEEDING_UP;
              fanOn(MODE_CONTINUOUS);
            }
          }
          break;
          
        case INTENSITY_CHANGED: 
          intervalPauseDuration = mapToIntervalPauseDuration(getFanIntensity());
          if ((duration32_s_t) (now - intervalPhaseBeginTime) >= intervalPauseDuration) {  // pause is over
            fanState = FAN_SPEEDING_UP;
            fanOn(MODE_INTERVAL);
            intervalPhaseBeginTime = now;
          }
          break;

        case INTERVAL_PHASE_ENDED:
          fanState = FAN_SPEEDING_UP;
          fanOn(MODE_INTERVAL);
          intervalPhaseBeginTime = now;
          break;
          
        default: 
          break;
      }
      break;
  }
  
  #ifdef VERBOSE
    Serial.print("State ");
    Serial.print(fanStateName(beforeState));
    Serial.print(" -- [");
    Serial.print(eventName(event));
    Serial.print("] --> State ");
    Serial.println(fanStateName(fanState));
  #endif
}
