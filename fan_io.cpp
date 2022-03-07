
#include "fan_io.h"
#include "fan_util.h"

bool statusLEDState = LOW;

uint32_t lastPauseBlipTime = 0;

uint8_t fanDutyCycleValue = 0; // value actually set on output pin

FanMode fanMode = MODE_UNDEF;
FanIntensity fanIntensity = INTENSITY_UNDEF;

void configInputPins() {
  configInputWithPullup(MODE_SWITCH_IN_PIN_1);
  configInputWithPullup(MODE_SWITCH_IN_PIN_2);
  configInputWithPullup(INTENSITY_SWITCH_IN_PIN_1);
  configInputWithPullup(INTENSITY_SWITCH_IN_PIN_2);
}

void configOutputPins() {
  configOutput(STATUS_LED_OUT_PIN);
  #if defined(__AVR_ATmega328P__)
    configOutput(SLEEP_LED_OUT_PIN);
  #endif
}


bool updateFanModeFromInputPins() {
  uint8_t p1 = digitalRead(MODE_SWITCH_IN_PIN_1);
  uint8_t p2 = digitalRead(MODE_SWITCH_IN_PIN_2);
  FanMode value;
  if (! p1 && p2) {
    value = MODE_INTERVAL;
  } else if(p1 && ! p2) {
    value = MODE_CONTINUOUS;
  } else {
    value = MODE_OFF;
  }
  #ifdef VERBOSE
    Serial.print("Read Fan Mode: ");
    Serial.println(value == MODE_INTERVAL ? "INTERVAL" : (value == MODE_CONTINUOUS ? "CONTINUOUS" :"OFF"));
  #endif
  
  if (value != fanMode) {
    fanMode = value;
    return true;
  }
  return false;
}

bool updateFanIntensityFromInputPins() {
  uint8_t p1 = digitalRead(INTENSITY_SWITCH_IN_PIN_1);
  uint8_t p2 = digitalRead(INTENSITY_SWITCH_IN_PIN_2);
  FanIntensity value;
  if (! p1 && p2) {
    value = INTENSITY_LOW;
  } else if(p1 && ! p2) {
    value = INTENSITY_HIGH;
  } else {
    value = INTENSITY_MEDIUM;
  }
  #ifdef VERBOSE
    Serial.print("Read Fan Intensity: ");
    Serial.println(value == INTENSITY_LOW ? "LOW" : (value == INTENSITY_HIGH ? "HIGH" :"MEDIUM"));
  #endif
  
  if (value != fanIntensity) {
    fanIntensity = value;
    return true;
  }
  return false;
}


FanMode getFanMode() {
  if (fanMode == MODE_UNDEF) {
    updateFanModeFromInputPins();
  }
  return fanMode;
}


FanIntensity getFanIntensity() {
  if (fanIntensity == INTENSITY_UNDEF) {
    updateFanIntensityFromInputPins();
  }
  return fanIntensity;
}


//
// INTERRUPTS
//

void configInt0Interrupt() {
  #if defined(__AVR_ATmega328P__)
//    EIMSK |= _BV(INT0);      // Enable INT0 (external interrupt) 
//    EICRA |= _BV(ISC00);     // Any logical change triggers an interrupt

  #elif defined(__AVR_ATtiny85__)
    GIMSK |= _BV(INT0);      // Enable INT0 (external interrupt) 
    MCUCR |= _BV(ISC00);     // Any logical change triggers an interrupt
  #endif
}


void configPinChangeInterrupts() {
  // Pin-change interrupts are triggered for each level-change; this cannot be configured
  #if defined(__AVR_ATmega328P__)
    PCICR |= _BV(PCIE0);                       // Enable pin-change interrupt 0 
//    PCIFR |= _BV(PCIF0);                       // Enable PCINT0..5 (pins PB0..PB5) 
    PCMSK0 |= _BV(PCINT0) | _BV(PCINT1);       // Configure pins PB0, PB1
    
    PCICR |= _BV(PCIE2);                       // Enable pin-change interrupt 2 
//    PCIFR |= _BV(PCIF2);                       // Enable PCINT16..23 (pins PD0..PD7) 
    PCMSK2 |= _BV(PCINT22) | _BV(PCINT23);     // Configure pins PD6, PD7

  #elif defined(__AVR_ATtiny85__)
    GIMSK|= _BV(PCIE);
    PCMSK|= _BV(PCINT1) | _BV(PCINT3);    // Configure PB1 and PB3 as interrupt source
  #endif
}


void setFanDutyCycle(uint8_t value) {
  fanDutyCycleValue = value;
  #if defined(__AVR_ATmega328P__)
    analogWrite(FAN_PWM_OUT_PIN, value); // Send PWM signal

  #elif defined(__AVR_ATtiny85__)
    OCR1A = value;
  #endif
}

uint8_t getFanDutyCycle() {
  return fanDutyCycleValue;
}
void setStatusLED(bool on) {
  statusLEDState = on;
  digitalWrite(STATUS_LED_OUT_PIN, on);
}

void setFanPower(bool on) {
  digitalWrite(FAN_POWER_OUT_PIN, on);
}

void invertStatusLED() {
  setStatusLED(statusLEDState == HIGH ? LOW : HIGH);
}

void showPauseBlip() {
  resetPauseBlip();
  setStatusLED(HIGH);
  delay(INTERVAL_PAUSE_BLIP_ON_DURATION_MS);
  setStatusLED(LOW);
}

void resetPauseBlip() {
  lastPauseBlipTime = millis();
}
  
uint32_t getLastPauseBlipTime() {
  return lastPauseBlipTime;
}
