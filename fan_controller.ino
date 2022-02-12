// --------------------
// CONFIGURABLE VALUES
// --------------------

// #define VERBOSE

const uint8_t MODE_SWITCH_IN_PIN = A1;  // analog in, 3 voltages
const uint8_t INTENSITY_SWITCH_IN_PIN = A2;  // analog in, 3 voltages
const uint8_t STATUS_LED_OUT_PIN = 13; // digital out; is on when motor is of, blinks during transitioning
const uint8_t MOTOR_OUT_PIN = 9; // PWM

const uint8_t MOTOR_MAX_VOLTAGE = 13000; // [mV]
const uint8_t MOTOR_LOW_THRESHOLD_VOLTAGE = 4400; // [mV] // below this voltage, the motor will not move

// Continuous operation:
const uint8_t MOTOR_CONTINUOUS_LOW_VOLTAGE = 4400; // [mV] do not set lower than MOTOR_LOW_THRESHOLD_VOLTAGE
const uint8_t MOTOR_CONTINUOUS_MEDIUM_VOLTAGE = 9000; // [mV]
const uint8_t MOTOR_CONTINUOUS_HIGH_VOLTAGE = MOTOR_MAX_VOLTAGE; // [mV]

// Interval operation:
const uint8_t MOTOR_INTERVAL_FAN_ON_VOLTAGE = MOTOR_MAX_VOLTAGE; // [mV]

const uint8_t INTERVAL_FAN_ON_DURATION = 10; // [s]
const uint8_t INTERVAL_PAUSE_SHORT_DURATION = 10; // [s]
const uint8_t INTERVAL_PAUSE_MEDIUM_DURATION = 20; // [s]
const uint8_t INTERVAL_PAUSE_LONG_DURATION = 30; // [s]


// Motor soft start and stop:
const uint8_t MOTOR_START_DURATION = 4000;  // [ms] duration from full stop to full throttle
const uint8_t MOTOR_STOP_DURATION = 2000;  // [ms] duration from full throttle to full stop

// --------------------
// DO NOT TOUCH THE VALUES OF THE FOLLOWING CONSTANTS
// --------------------

enum FanMode {MODE_OFF, MODE_CONTINUOUS, MODE_INTERVAL};
FanMode fanMode = MODE_OFF;   // actual fan mode
FanMode fanModePreviousValue = MODE_OFF;   // fanMode value previously read from input pin

enum FanIntensity {INTENSITY_LOW, INTENSITY_MEDIUM, INTENSITY_HIGH};
FanIntensity fanIntensity = INTENSITY_LOW;   // actual fan intensity
FanIntensity fanIntensityPreviousValue = INTENSITY_LOW;   // fanIntensity value previously read from input pin

uint8_t statusLEDState = LOW;

const uint16_t ANALOG_IN_MIN = 0;  // Arduino constant
const uint16_t ANALOG_IN_MAX = 1023;  // Arduino constant
const uint16_t ANALOG_IN_RANGE_THIRD = ANALOG_IN_MAX / 3;

const uint8_t ANALOG_OUT_MIN = 0;  // Arduino constant
const uint8_t ANALOG_OUT_MAX = 255;  // Arduino constant
const uint8_t MOTOR_OUT_LOW_THRESHOLD = (long) ANALOG_OUT_MAX * MOTOR_LOW_THRESHOLD_VOLTAGE /  MOTOR_MAX_VOLTAGE;
const uint8_t MOTOR_OUT_MOTOR_OFF = ANALOG_OUT_MIN;

// Continuous mode:
const uint8_t MOTOR_OUT_CONTINUOUS_LOW_DUTY_VALUE = (long) ANALOG_OUT_MAX * MOTOR_CONTINUOUS_LOW_VOLTAGE /  MOTOR_MAX_VOLTAGE;
const uint8_t MOTOR_OUT_CONTINUOUS_MEDIUM_DUTY_VALUE = (long) ANALOG_OUT_MAX * MOTOR_CONTINUOUS_MEDIUM_VOLTAGE /  MOTOR_MAX_VOLTAGE;
const uint8_t MOTOR_OUT_CONTINUOUS_HIGH_DUTY_VALUE = (long) ANALOG_OUT_MAX * MOTOR_CONTINUOUS_HIGH_VOLTAGE /  MOTOR_MAX_VOLTAGE;

// Interval mode:
enum IntervalPhase {PHASE_FAN_ON, PHASE_PAUSE};
IntervalPhase intervalPhase = PHASE_FAN_ON;

uint32_t intervalPhaseBeginTime = 0; // [ms]

const uint8_t MOTOR_OUT_INTERVAL_FAN_ON_DUTY_VALUE = (long) ANALOG_OUT_MAX * MOTOR_INTERVAL_FAN_ON_VOLTAGE /  MOTOR_MAX_VOLTAGE;
const uint32_t INTERVAL_FAN_ON_DURATION_MS = (uint32_t) INTERVAL_FAN_ON_DURATION * 1000; // [ms]
uint32_t intervalPauseDuration;

// Control cycle: output values are set only once per cycle
const uint32_t CONTROL_CYCLE_DURATION = 100; // [ms]
const uint32_t SWITCH_POLL_PERIOD = 25; // [ms]

uint32_t controlCycleBeginTime = 0; // [ms]

// Motor control:
enum MotorState {MOTOR_OFF, MOTOR_SPEEDING_UP, MOTOR_STEADY, MOTOR_SLOWING_DOWN};
MotorState motorState = MOTOR_OFF;

// Motor soft start and soft stop:
const uint16_t MOTOR_START_INCREMENT = (ANALOG_OUT_MAX - MOTOR_OUT_LOW_THRESHOLD) * CONTROL_CYCLE_DURATION / MOTOR_START_DURATION;
const uint16_t MOTOR_STOP_DECREMENT = (ANALOG_OUT_MAX - MOTOR_OUT_LOW_THRESHOLD) * CONTROL_CYCLE_DURATION / MOTOR_STOP_DURATION;

uint8_t motorTargetDutyValue = MOTOR_OUT_MOTOR_OFF; // potentiometer value read from input pin
uint8_t motorActualDutyValue = MOTOR_OUT_MOTOR_OFF; // value actually set on output pin

uint32_t transitionBeginTime = 0;
uint8_t transitioningDutyValue = ANALOG_OUT_MIN; // incremented in discrete steps until motor is at its target speed or its low threshold



void setup() {
  pinMode(MODE_SWITCH_IN_PIN, INPUT);
  pinMode(INTENSITY_SWITCH_IN_PIN, INPUT);
  pinMode(STATUS_LED_OUT_PIN, OUTPUT);
  pinMode(MOTOR_OUT_PIN, OUTPUT);

  #ifdef VERBOSE
  // Setup Serial Monitor
  Serial.begin(9600);
  
  Serial.print("Motor out low threshold: ");
  Serial.println(MOTOR_OUT_LOW_THRESHOLD);
  #endif
  
  controlCycleBeginTime = millis();
  intervalPhaseBeginTime = millis();
  intervalPauseDuration = calcIntervalPauseDuration(INTENSITY_LOW);
  
  setMotorDutyValue(MOTOR_OUT_MOTOR_OFF);
  setStatusLED(HIGH);
}

void loop() {
  uint32_t now = millis();
  
  if (handleModeSwitchFlicked() || handleIntensitySwitchFlicked()) {
    handleInputChange(fanMode, fanIntensity, now);
  }

  if (now - controlCycleBeginTime >= CONTROL_CYCLE_DURATION) {
    controlCycleBeginTime = now;

    if (fanMode == MODE_INTERVAL) {
      
      if(intervalPhase == PHASE_FAN_ON) {
        if (now - intervalPhaseBeginTime >= INTERVAL_FAN_ON_DURATION_MS) { // fan on is over
          #ifdef VERBOSE
            Serial.print("Mode INTERVAL, Phase: PAUSE, ");
            Serial.println(intervalPauseDuration);
          #endif
          prepareMotorDutyChange(MOTOR_OUT_MOTOR_OFF, now);
          intervalPhase = PHASE_PAUSE;
          intervalPhaseBeginTime = now;
        }
        
      } else { // PHASE_PAUSE
        if (now - intervalPhaseBeginTime >= intervalPauseDuration) { // pause is over
          #ifdef VERBOSE
            Serial.print("Mode INTERVAL, Phase: FAN ON, ");
            Serial.println(INTERVAL_FAN_ON_DURATION_MS);
          #endif
          prepareMotorDutyChange(MOTOR_OUT_INTERVAL_FAN_ON_DUTY_VALUE, now);
          intervalPhase = PHASE_FAN_ON;
          intervalPhaseBeginTime = now;
        }
      }
    }
    
    if (motorState == MOTOR_SPEEDING_UP || motorState == MOTOR_SLOWING_DOWN) {
      handleMotorSpeedTransition(motorTargetDutyValue);
    }
  }
  
  delay(SWITCH_POLL_PERIOD);
}

void handleInputChange(FanMode newMode, FanIntensity newIntensity, uint32_t now) {
  if (newMode == MODE_OFF) {
      #ifdef VERBOSE
        Serial.println("Mode: OFF");
      #endif
      prepareMotorDutyChange(MOTOR_OUT_MOTOR_OFF, now);

  } else if (newMode == MODE_CONTINUOUS) {
      uint8_t newTargetDutyValue = calcMotorDutyValue(newIntensity);
      #ifdef VERBOSE
        Serial.print("Mode CONTINUOUS, Duty Value: ");
        Serial.println(newTargetDutyValue);
      #endif
      prepareMotorDutyChange(newTargetDutyValue, now);

  } else if (newMode == MODE_INTERVAL) {
    intervalPauseDuration = calcIntervalPauseDuration(newIntensity);
    
    if (motorState == MOTOR_OFF) {
        intervalPhase = PHASE_PAUSE;
        #ifdef VERBOSE
          Serial.print("Mode INTERVAL, Phase: PAUSE, Pause: ");
          Serial.println(intervalPauseDuration);
        #endif
        
    } else {
        intervalPhase = PHASE_FAN_ON;
        uint8_t newTargetDutyValue = MOTOR_OUT_INTERVAL_FAN_ON_DUTY_VALUE;
        prepareMotorDutyChange(newTargetDutyValue, now);
        #ifdef VERBOSE
          Serial.print("Mode INTERVAL, Phase: FAN ON, Pause: ");
          Serial.println(intervalPauseDuration);
        #endif
    }
    intervalPhaseBeginTime = now;
  }
}

void prepareMotorDutyChange(uint8_t newTargetDutyValue, uint32_t now) {
  if (newTargetDutyValue == motorActualDutyValue) {
    return;
  }
  
  transitionBeginTime = now;
  
  if (newTargetDutyValue == MOTOR_OUT_MOTOR_OFF && motorActualDutyValue > motorActualDutyValue) {
    motorState = MOTOR_SLOWING_DOWN;
    motorTargetDutyValue = MOTOR_OUT_MOTOR_OFF;
    transitioningDutyValue = motorActualDutyValue;
    #ifdef VERBOSE
      Serial.println("STOPPING.");
    #endif
    return;
  }
    
  if (newTargetDutyValue > motorActualDutyValue) {
    motorState = MOTOR_SPEEDING_UP;
    #ifdef VERBOSE
      Serial.print("SPEEDING UP to ");
      Serial.println(newTargetDutyValue);
    #endif
      
  } else if (newTargetDutyValue < motorActualDutyValue) { 
    motorState = MOTOR_SLOWING_DOWN;
    #ifdef VERBOSE
      Serial.print("SLOWING DOWN to ");
      Serial.println(newTargetDutyValue);
    #endif
  }
  motorTargetDutyValue = newTargetDutyValue;
  transitioningDutyValue = (motorActualDutyValue == MOTOR_OUT_MOTOR_OFF) ? MOTOR_OUT_LOW_THRESHOLD : motorActualDutyValue;
}

void handleMotorSpeedTransition(uint8_t targetDutyValue) {
  if (motorState == MOTOR_OFF || motorState == MOTOR_STEADY) {
    return;
  }
  
  if (motorState == MOTOR_SPEEDING_UP) {
    // ensure we don't overrun the max value of uint8_t when incrementing:
    uint8_t increment = min(ANALOG_OUT_MAX - transitioningDutyValue, MOTOR_START_INCREMENT);

    if (motorActualDutyValue == MOTOR_OUT_MOTOR_OFF) {
      // start motor up first, don't increment transitioningDutyValue at this time
      
    } else if (transitioningDutyValue + increment < targetDutyValue) {
      transitioningDutyValue += increment;
    } else {
      transitioningDutyValue = targetDutyValue;
    }
    #ifdef VERBOSE
      Serial.print("Speeding up: ");
      Serial.println(transitioningDutyValue);
    #endif
    
  } else if (motorState == MOTOR_SLOWING_DOWN) {
    // ensure transitioningDutyValue will not become < 0 when decrementing (is an uint8_t!)
    uint8_t decrement = min(transitioningDutyValue, MOTOR_STOP_DECREMENT);

    if (motorActualDutyValue == MOTOR_OUT_LOW_THRESHOLD && targetDutyValue < MOTOR_OUT_LOW_THRESHOLD) {
      transitioningDutyValue = MOTOR_OUT_MOTOR_OFF;
    
    } else if (transitioningDutyValue - decrement > max(targetDutyValue, MOTOR_OUT_LOW_THRESHOLD)) {
      transitioningDutyValue -= decrement;
    } else {
      transitioningDutyValue = max(targetDutyValue, MOTOR_OUT_LOW_THRESHOLD);
    }
    #ifdef VERBOSE
      Serial.print("Slowing down: ");
      Serial.println(transitioningDutyValue);
    #endif
  }
  setMotorDutyValue(transitioningDutyValue);
    
  if (transitioningDutyValue == MOTOR_OUT_MOTOR_OFF) {
    motorState = MOTOR_OFF;
    transitionBeginTime = 0;
    setStatusLED(HIGH);
    #ifdef VERBOSE
      Serial.println("MOTOR OFF");
    #endif
  } else  if (transitioningDutyValue == targetDutyValue) {
    motorState = MOTOR_STEADY;
    transitionBeginTime = 0;
    transitioningDutyValue = MOTOR_OUT_MOTOR_OFF;
    setStatusLED(LOW);
    #ifdef VERBOSE
      Serial.println("MOTOR STEADY.");
    #endif
  } else {
    invertStatusLED();
  }
}


bool handleModeSwitchFlicked() {
  unsigned int value = analogRead(MODE_SWITCH_IN_PIN);
//  #ifdef VERBOSE
//      Serial.print("Mode value: ");
//      Serial.println(value);
//   #endif
  FanMode mode = MODE_OFF;
  if (value > 2 * ANALOG_IN_RANGE_THIRD) {
    mode = MODE_INTERVAL;
  } else if (value > ANALOG_IN_RANGE_THIRD) {
    mode = MODE_CONTINUOUS;
  } 

  if (mode != fanModePreviousValue) {
    fanMode = mode;
    fanModePreviousValue = mode;
    #ifdef VERBOSE
      Serial.print("Mode changed -> ");
      Serial.println(mode == MODE_OFF ? "OFF" : mode == MODE_CONTINUOUS ? "CONTINUOUS" : "INVERVAL");
    #endif
    return true;
  }
  return false;
}

bool handleIntensitySwitchFlicked() {
  unsigned int value = analogRead(INTENSITY_SWITCH_IN_PIN);
//  #ifdef VERBOSE
//      Serial.print("Intensity value: ");
//      Serial.println(value);
//   #endif
  FanIntensity intensity = INTENSITY_LOW;
  if (value > 2 * ANALOG_IN_RANGE_THIRD) {
    intensity = INTENSITY_HIGH;
  } else if (value > ANALOG_IN_RANGE_THIRD) {
    intensity = INTENSITY_MEDIUM;
  } 

  if (intensity != fanIntensityPreviousValue) {
    fanIntensity = intensity;
    fanIntensityPreviousValue = intensity;
    #ifdef VERBOSE
      Serial.print("Intensity changed -> ");
      Serial.println(intensity == INTENSITY_LOW ? "LOW" : intensity == INTENSITY_MEDIUM ? "MEDIUM" : "HIGH");
    #endif
    return true;
  }
  return false;
}

void setMotorDutyValue(uint8_t value) {
  motorActualDutyValue = value;
  analogWrite(MOTOR_OUT_PIN, motorActualDutyValue); // Send PWM signal
}

void setStatusLED(int value) {
  statusLEDState = value;
  digitalWrite(STATUS_LED_OUT_PIN, value);
}

void invertStatusLED() {
  setStatusLED(statusLEDState == HIGH ? LOW : HIGH);
}


// Applicable only in mode CONTINUOUS
uint8_t calcMotorDutyValue(FanIntensity intensity) {
  switch(intensity) {
    case INTENSITY_HIGH: 
      return MOTOR_OUT_CONTINUOUS_HIGH_DUTY_VALUE;
    case INTENSITY_MEDIUM: 
      return MOTOR_OUT_CONTINUOUS_MEDIUM_DUTY_VALUE;
    default: 
      return MOTOR_OUT_CONTINUOUS_LOW_DUTY_VALUE;
  }
}

// Applicable only in mode INTERVAL
// Returns [ms]
uint32_t calcIntervalPauseDuration(FanIntensity intensity) {
  switch(intensity) {
    case INTENSITY_HIGH: 
      return (uint32_t) INTERVAL_PAUSE_SHORT_DURATION * 1000; // [ms]
    case INTENSITY_MEDIUM: 
      return (uint32_t) INTERVAL_PAUSE_MEDIUM_DURATION * 1000; // [ms]
    default: 
      return (uint32_t) INTERVAL_PAUSE_LONG_DURATION * 1000; // [ms]
  }
}
