#include "Arduino.h"
#include <limits.h>
#include "log_io.h"
#include "phys_io.h"

// Singleton instance
LogicalIOModel LOGICAL_IO = LogicalIOModel();

LogicalIOModel* logicalIO() {
  return &LOGICAL_IO;
}

void LogicalIOModel::init() {
  updateFanIntensityFromInputPins();
  updateFanModeFromInputPins();
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
      Serial.flush();
    #endif

    if (modeChangedHandler != NULL) modeChangedHandler();
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
      Serial.flush();
    #endif
  
    if (intensityChangedHandler != NULL) intensityChangedHandler();
  }
}

#if defined(__AVR_ATmega328P__)
  void LogicalIOModel::wdtWakeupLEDBlip() {
    digitalWrite(WDT_WAKEUP_OUT_PIN, HIGH);
    delay(50);  // do not use Scheduler because this is part of the wakeup routine
    digitalWrite(WDT_WAKEUP_OUT_PIN, LOW);
  }

  // Interrupt service routine for Pin Change Interrupt Request 0 => MODE
  ISR (PCINT0_vect) {  
    debounceSwitch();
    LOGICAL_IO.updateFanModeFromInputPins();
  }

  // Interrupt service routine for Pin Change Interrupt Request 2 => INTENSITY
  ISR (PCINT2_vect) {  
    debounceSwitch();
    LOGICAL_IO.updateFanIntensityFromInputPins();
  }

#elif defined(__AVR_ATtiny85__)
  // Interrupt service routine for Pin Change Interrupt Request 0 => MODE & INTENSITY
  ISR (PCINT0_vect) {  
    debounceSwitch();
    LOGICAL_IO.updateFanModeFromInputPins();
    LOGICAL_IO.updateFanIntensityFromInputPins();
  }
#endif

pwm_duty_t mapToDutyValue(FanSpeed speed) {
  switch (speed) {
    case SPEED_OFF:     return PWM_DUTY_MIN;
    case SPEED_MIN:     return FAN_CONTINUOUS_LOW_DUTY_VALUE;
    case SPEED_MEDIUM:  return FAN_CONTINUOUS_MEDIUM_DUTY_VALUE;
    default:            return FAN_CONTINUOUS_HIGH_DUTY_VALUE; 
  }
}

void LogicalIOModel::fanSpeed(FanSpeed speed) {
  this->speed = speed;
  fanDutyCycleValue = mapToDutyValue(speed);
  pwmDutyCycle(fanDutyCycleValue);
}

bool LogicalIOModel::isPwmActive() {
  return ! (fanDutyCycleValue == PWM_DUTY_MIN     // fan off – no PWM required
          || fanDutyCycleValue == PWM_DUTY_MAX);  // fan on at maximum – no PWM required)
}

void LogicalIOModel::statusLED(bool on) {
  digitalWrite(STATUS_LED_OUT_PIN, on);
}
