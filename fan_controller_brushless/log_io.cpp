#include <limits.h>
#include "log_io.h"
#include "phys_io.h"

// Singleton instance
LogicalIOModel LOGICAL_IO = LogicalIOModel();

LogicalIOModel* logicalIO() {
  return &LOGICAL_IO;
}

void LogicalIOModel::init() {
  configPhysicalIO();
  updateFanModeFromInputPins();
  updateFanIntensityFromInputPins();
}

void LogicalIOModel::updateFanModeFromInputPins() {
  #if defined(__AVR_ATmega328P__)
    uint8_t p1 = digitalRead(MODE_SWITCH_IN_PIN_1);
    uint8_t p2 = digitalRead(MODE_SWITCH_IN_PIN_2);

  #elif defined(__AVR_ATtiny85__)
    uint8_t p1 = LOW;
    uint8_t p2 = digitalRead(MODE_SWITCH_IN_PIN);
  #endif

  FanMode previous = mode;

  if (p1) {
    mode = MODE_OFF;
  } else if(p2) {
    mode = MODE_CONTINUOUS;
  } else {
    mode = MODE_INTERVAL;
  }
  if (mode != previous) {
    #ifdef VERBOSE
      Serial.print("Read Fan Mode: ");
      Serial.println(mode == MODE_INTERVAL ? "INTERVAL" : (mode == MODE_CONTINUOUS ? "CONTINUOUS" :"OFF"));
    #endif
    modeChangedHandler();
  }
}

void LogicalIOModel::updateFanIntensityFromInputPins() {
  uint8_t p1 = digitalRead(INTENSITY_SWITCH_IN_PIN_1);
  uint8_t p2 = digitalRead(INTENSITY_SWITCH_IN_PIN_2);

  FanIntensity previous = intensity;

  if (! p1 && p2) {
    intensity = INTENSITY_LOW;
  } else if(p1 && ! p2) {
    intensity = INTENSITY_HIGH;
  } else {
    intensity = INTENSITY_MEDIUM;
  }
  if (intensity != previous) {
    #ifdef VERBOSE
      Serial.print("Read Fan Intensity: ");
      Serial.println(intensity == INTENSITY_LOW ? "LOW" : (intensity == INTENSITY_HIGH ? "HIGH" :"MEDIUM"));
    #endif
    intensityChangedHandler();
  }
}

// Interrupt service routine for Pin Change Interrupt Request 0
ISR (PCINT0_vect) {  
  debounceSwitch();
  LOGICAL_IO.updateFanModeFromInputPins();
  LOGICAL_IO.updateFanIntensityFromInputPins();
}

pwm_duty_t mapToDutyValue(FanSpeed speed) {
  switch (speed) {
    case SPEED_OFF:     return ANALOG_OUT_MIN;
    case SPEED_MIN:     return FAN_CONTINUOUS_LOW_DUTY_VALUE;
    case SPEED_MEDIUM:  return FAN_CONTINUOUS_MEDIUM_DUTY_VALUE;
    default:            return FAN_CONTINUOUS_HIGH_DUTY_VALUE; 
  }
}

void LogicalIOModel::fanSpeed(FanSpeed speed) {
  this->speed = speed;
  fanDutyCycle(mapToDutyValue(speed));
}

void LogicalIOModel::fanDutyCycle(pwm_duty_t value) {
  fanDutyCycleValue = value;
  #if defined(__AVR_ATmega328P__)
    analogWrite(FAN_PWM_OUT_PIN, value); // Send PWM signal

  #elif defined(__AVR_ATtiny85__)
    pwm_duty_t scaled = value;
    if (ANALOG_OUT_MAX < UCHAR_MAX) {
      scaled = (pwm_duty_t) (((uint16_t) value) * ANALOG_OUT_MAX /  UCHAR_MAX);
    }
    OCR1A = scaled;
  #endif
}

bool LogicalIOModel::isPwmActive() {
  return ! (fanDutyCycleValue == ANALOG_OUT_MIN   // fan off – no PWM required
          || fanDutyCycleValue == ANALOG_OUT_MAX);       // fan on at maximum – no PWM required)
}

void LogicalIOModel::statusLED(bool on) {
  digitalWrite(STATUS_LED_OUT_PIN, on);
}
