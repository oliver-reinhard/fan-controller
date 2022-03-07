
#include "fan_control.h"
#include "fan_util.h"
#include "fan_io.h"

//
// ANALOG OUT
//;
const uint8_t FAN_OUT_FAN_OFF = ANALOG_OUT_MIN;

// Continuous mode:
const uint8_t FAN_OUT_CONTINUOUS_LOW_DUTY_VALUE = (uint32_t) ANALOG_OUT_MAX * FAN_CONTINUOUS_LOW_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint8_t FAN_OUT_CONTINUOUS_MEDIUM_DUTY_VALUE = (uint32_t) ANALOG_OUT_MAX * FAN_CONTINUOUS_MEDIUM_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint8_t FAN_OUT_CONTINUOUS_HIGH_DUTY_VALUE = (uint32_t) ANALOG_OUT_MAX * FAN_CONTINUOUS_HIGH_VOLTAGE /  FAN_MAX_VOLTAGE;

const uint8_t FAN_OUT_INTERVAL_FAN_ON_DUTY_VALUE = (uint32_t) ANALOG_OUT_MAX * FAN_INTERVAL_FAN_ON_VOLTAGE /  FAN_MAX_VOLTAGE;

// Fan soft start and soft stop:
const uint16_t FAN_START_INCREMENT = (uint32_t) (ANALOG_OUT_MAX - FAN_OUT_LOW_THRESHOLD) * SPEED_TRANSITION_CYCLE_DURATION_MS / FAN_START_DURATION_MS;
const uint16_t FAN_STOP_DECREMENT = (uint32_t) (ANALOG_OUT_MAX - FAN_OUT_LOW_THRESHOLD) * SPEED_TRANSITION_CYCLE_DURATION_MS / FAN_STOP_DURATION_MS;

//
// CONTROLLER
//

FanState fanState = FAN_OFF;          // current fan state
Event pendingEvent = EVENT_NONE;
  
uint8_t fanTargetDutyValue = FAN_OUT_FAN_OFF; // value derived from input-pin values

uint8_t transitioningDutyValue = ANALOG_OUT_MIN; // incremented in discrete steps until fan is at its target speed or its low threshold

uint32_t intervalPhaseBeginTime = 0; // [ms]
uint32_t intervalPauseDuration;      // [ms]

//
// FUNCTIONS
//

FanState getFanState() {
  return fanState;
}

// [ms]
uint32_t getIntervalPhaseBeginTime() { 
  return intervalPhaseBeginTime;
}

// [ms]
uint32_t getIntervalPauseDuration() {
  return intervalPauseDuration;
}

bool isPwmActive() {
  return getFanMode() == MODE_CONTINUOUS && getFanDutyCycle() > ANALOG_OUT_MIN && getFanDutyCycle() < ANALOG_OUT_MAX;
}

// Applicable only in mode CONTINUOUS
uint8_t mapToFanDutyValue(FanIntensity intensity) {
  switch(intensity) {
    case INTENSITY_HIGH: 
      return FAN_OUT_CONTINUOUS_HIGH_DUTY_VALUE;
    case INTENSITY_MEDIUM: 
      return FAN_OUT_CONTINUOUS_MEDIUM_DUTY_VALUE;
    default: 
      return FAN_OUT_CONTINUOUS_LOW_DUTY_VALUE;
  }
}

// Applicable only in mode INTERVAL
// Returns [ms]
uint32_t mapToIntervalPauseDuration(FanIntensity intensity) {
  switch(intensity) {
    case INTENSITY_HIGH: 
      return (uint32_t) INTERVAL_PAUSE_SHORT_DURATION * 1000; // [ms]
    case INTENSITY_MEDIUM: 
      return (uint32_t) INTERVAL_PAUSE_MEDIUM_DURATION * 1000; // [ms]
    default: 
      return (uint32_t) INTERVAL_PAUSE_LONG_DURATION * 1000; // [ms]
  }
}
  
#ifdef VERBOSE
  char* fanStateName(FanState state) {
    switch (state) {
      case FAN_OFF: return "OFF";
      case FAN_SPEEDING_UP: return "SPEEDING UP";
      case FAN_STEADY: return "STEADY";
      case FAN_SLOWING_DOWN: return "SLOWING DOWN";
      case FAN_PAUSING: return "PAUSE";
      default: return "?";
    }
  }
  
  char* eventName(Event event) {
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
  configOutput(FAN_POWER_OUT_PIN);
  configOutput(FAN_PWM_OUT_PIN);
  setFanPower(HIGH);
  setFanDutyCycle(FAN_OUT_LOW_THRESHOLD);
  if (mode == MODE_CONTINUOUS) {
    fanTargetDutyValue = mapToFanDutyValue(getFanIntensity());
  } else { /* getFanMode() == MODE_INTERVAL */
    fanTargetDutyValue = FAN_OUT_INTERVAL_FAN_ON_DUTY_VALUE;
  }
}

void fanOff(FanMode mode) {
  setFanPower(LOW);
  setFanDutyCycle(FAN_OUT_FAN_OFF);
  if (mode == MODE_INTERVAL) {
     intervalPauseDuration = mapToIntervalPauseDuration(getFanIntensity());
  }
  configInput(FAN_POWER_OUT_PIN);
  configInput(FAN_PWM_OUT_PIN);
}


void speedUp() {
  uint8_t transitioningDutyValue = getFanDutyCycle();
  
  // ensure we don't overrun the max value of uint8_t when incrementing:
  uint8_t increment = min(ANALOG_OUT_MAX - transitioningDutyValue, FAN_START_INCREMENT);

  if (transitioningDutyValue + increment < fanTargetDutyValue) {
    transitioningDutyValue += increment;
  } else {
    transitioningDutyValue = fanTargetDutyValue;
  }
  #ifdef VERBOSE
    Serial.print("Speeding up: ");
    Serial.println(transitioningDutyValue);
  #endif
  
  setFanDutyCycle(transitioningDutyValue);
  if (getFanDutyCycle() >= fanTargetDutyValue) {
    handleStateTransition(TARGET_SPEED_REACHED);
    
  } else if (BLINK_LED_DURING_SPEED_TRANSITION) {
    invertStatusLED();
  }
}


void slowDown() {
  uint8_t transitioningDutyValue = getFanDutyCycle();
  
  // ensure transitioningDutyValue will not become < 0 when decrementing (is an uint8_t!)
  uint8_t decrement = min(transitioningDutyValue, FAN_STOP_DECREMENT);

  if (getFanDutyCycle() == FAN_OUT_LOW_THRESHOLD && fanTargetDutyValue < FAN_OUT_LOW_THRESHOLD) {
    transitioningDutyValue = FAN_OUT_FAN_OFF;
  
  } else if (transitioningDutyValue - decrement > max(fanTargetDutyValue, FAN_OUT_LOW_THRESHOLD)) {
    transitioningDutyValue -= decrement;
  } else {
    transitioningDutyValue = max(fanTargetDutyValue, FAN_OUT_LOW_THRESHOLD);
  }
  #ifdef VERBOSE
    Serial.print("Slowing down: ");
    Serial.println(transitioningDutyValue);
  #endif
  
  setFanDutyCycle(transitioningDutyValue);
  if (getFanDutyCycle() <= fanTargetDutyValue) {
    handleStateTransition(TARGET_SPEED_REACHED);
    
  } else if (BLINK_LED_DURING_SPEED_TRANSITION) {
    invertStatusLED();
  }
}



void handleStateTransition(Event event) {
  if (event == EVENT_NONE) {
    return;
  }
  uint32_t now = millis();
  FanState beforeState = fanState;
  
  switch(fanState) {
    
    case FAN_OFF:
      switch(event) {
        case MODE_CHANGED:
          if (getFanMode() != MODE_OFF) {
            fanState = FAN_SPEEDING_UP;
            fanOn(getFanMode());
            intervalPhaseBeginTime = now;
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
          if (getFanMode() == MODE_OFF) {
            fanState = FAN_SLOWING_DOWN;
            fanTargetDutyValue = FAN_OUT_FAN_OFF;
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (getFanMode() == MODE_CONTINUOUS) {
            uint8_t newTargetDutyValue = mapToFanDutyValue(getFanIntensity());
            if (newTargetDutyValue > getFanDutyCycle()) {
              fanTargetDutyValue = newTargetDutyValue;
            } else if (newTargetDutyValue < getFanDutyCycle()) {
              fanState = FAN_SLOWING_DOWN;
              fanTargetDutyValue = newTargetDutyValue;
            } else { /* newTargetDutyValue == getFanDutyCycle() */
              fanState = FAN_STEADY;
              setStatusLED(LOW);
            }
          }
          break;
          
        case TARGET_SPEED_REACHED: 
          fanState = FAN_STEADY;
          intervalPhaseBeginTime = now;
          setStatusLED(LOW);
          break;
      }
      break;
      
    case FAN_STEADY: 
      switch(event) {
        case MODE_CHANGED: 
          if (getFanMode() == MODE_OFF) {
            fanState = FAN_SLOWING_DOWN;
            fanTargetDutyValue = FAN_OUT_FAN_OFF;
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (getFanMode() == MODE_CONTINUOUS) {
            uint8_t newTargetDutyValue = mapToFanDutyValue(getFanIntensity());
            if (newTargetDutyValue > getFanDutyCycle()) {
              fanState = FAN_SPEEDING_UP;
              fanTargetDutyValue = newTargetDutyValue;
            } else if (newTargetDutyValue < getFanDutyCycle()) {
              fanState = FAN_SLOWING_DOWN;
              fanTargetDutyValue = newTargetDutyValue;
            } 
          }
          break;

        case INTERVAL_PHASE_ENDED:
          fanState = FAN_SLOWING_DOWN;
          fanTargetDutyValue = FAN_OUT_FAN_OFF;
          intervalPhaseBeginTime = now;
          break;
          
        default:
          break;
      }
      break;
      
    case FAN_SLOWING_DOWN: 
      switch(event) {
        case MODE_CHANGED:
          if (getFanMode() == MODE_OFF) {
            fanTargetDutyValue = FAN_OUT_FAN_OFF;
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (getFanMode() == MODE_CONTINUOUS) {
            uint8_t newTargetDutyValue = mapToFanDutyValue(getFanIntensity());
            if (newTargetDutyValue > getFanDutyCycle()) {
              fanState = FAN_SPEEDING_UP;
              fanTargetDutyValue = newTargetDutyValue;
            } else if (newTargetDutyValue < getFanDutyCycle()) {
              fanTargetDutyValue = newTargetDutyValue;
            } else {  /* newTargetDutyValue == getFanDutyCycle() */
              fanState = FAN_STEADY;
              setStatusLED(LOW);
            }
          }
          break;
          
        case TARGET_SPEED_REACHED: 
          if (getFanMode() == MODE_OFF) {
            fanState = FAN_OFF;
            fanOff(MODE_INTERVAL);
          } else if (getFanMode() == MODE_CONTINUOUS) {
            fanState = FAN_STEADY;
          } else {  // getFanMode() == MODE_INTERVAL
            fanState = FAN_PAUSING;
            fanOff(MODE_INTERVAL);
            intervalPhaseBeginTime = now;
            resetPauseBlip();
          }
          setStatusLED(LOW);
          break;
      }
      break;
      
    case FAN_PAUSING:
      switch(event) {
        case MODE_CHANGED: 
          if (getFanMode() == MODE_OFF) {
            fanState = FAN_OFF;
            fanOff(MODE_INTERVAL);
          } else if (getFanMode() == MODE_CONTINUOUS) {
            fanState = FAN_SPEEDING_UP;
            fanOn(MODE_CONTINUOUS);
          }
          break;
          
        case INTENSITY_CHANGED: 
          intervalPauseDuration = mapToIntervalPauseDuration(getFanIntensity());
          if (now - intervalPhaseBeginTime >= intervalPauseDuration) {  // pause is over
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

void configPWM1() {
  #if defined(__AVR_ATmega328P__)
    // nothing --> use analogWrite as is
    // No specific PWM frequency
  
  #elif defined(__AVR_ATtiny85__)
    // Configure Timer/Counter1 Control Register 1 (TCR1) 
    // | CTC1 | PWM1A | COM1A | CS |
    // |  1   |  1    |  2    | 4  |  ->  #bits
    //
    // CTC1 - Clear Timer/Counter on Compare Match: When set (==1), TCC1 is reset to $00 in the CPU clock cycle after a compare match with OCR1C register value.
    // PWM1A - Pulse Width Modulator A Enable: When set (==1), enables PWM mode based on comparator OCR1A in TC1 and the counter value is reset to $00 in the CPU clock cycle after a compare match with OCR1C register value.
    // COM1A - Comparator A Output Mode: determines output-pin action following a compare match with compare register A (OCR1A) in TC1
    // CS - Clock Select Bits: defines the prescaling factor of TC1
  
    // Clear all TCCR1 bits:
    TCCR1 &= B00000000;      // Clear 
  
    // Clear Timer/Counter on Compare Match: count from 0, 1, 2 .. OCR1C, 0, 1, 2 .. ORC1C, etc
    TCCR1 |= _BV(CTC1);
    
    // Enable PWM A based on OCR1A
    TCCR1 |= _BV(PWM1A);
    
    // On Compare Match with OCR1A (counter == OCR1A): Clear the output line (-> LOW), set on $00
    TCCR1 |= _BV(COM1A1);
  
    // Configure PWM frequency:
    TCCR1 |= TIMER1_PRESCALER;  // Prescale factor
    OCR1C = TIMER1_COUNT_TO;    // Count 0,1,2..compare-match,0,1,2..compare-match, etc
  
    // Determines Duty Cycle: OCR1A / OCR1C e.g. value of 50 / 200 --> 25%,  value of 50 --> 0%
    OCR1A = 0;
  #endif
}
