#include "fan_io.h"

bool statusLEDState = LOW;

volatile pwm_duty_t fanDutyCycleValue = 0; // value actually set on output pin

volatile FanMode fanMode = MODE_UNDEF;
volatile FanIntensity fanIntensity = INTENSITY_UNDEF;
  
// volatile --> variable can change at any time --> prevents compiler from optimising away a read access that would 
// return a value changed by an interrupt handler
volatile InputInterrupt interruptSource = NO_INPUT_INTERRUPT;
 
void configInputPins() {
  #if defined(__AVR_ATmega328P__)
    configInputWithPullup(MODE_SWITCH_IN_PIN_1);
    configInputWithPullup(MODE_SWITCH_IN_PIN_2);

  #elif defined(__AVR_ATtiny85__)
    configInputWithPullup(MODE_SWITCH_IN_PIN);
  #endif

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
  #if defined(__AVR_ATmega328P__)
    uint8_t p1 = digitalRead(MODE_SWITCH_IN_PIN_1);
    uint8_t p2 = digitalRead(MODE_SWITCH_IN_PIN_2);

  #elif defined(__AVR_ATtiny85__)
    uint8_t p1 = LOW;
    uint8_t p2 = digitalRead(MODE_SWITCH_IN_PIN);
  #endif

  FanMode value;
  if (p1) {
    value = MODE_OFF;
  } else if(p2) {
    value = MODE_CONTINUOUS;
  } else {
    value = MODE_INTERVAL;
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
void resetInputInterrupt() {
  interruptSource = NO_INPUT_INTERRUPT;
}
bool isInputInterrupt() {
  return interruptSource != NO_INPUT_INTERRUPT;
} 

// 
// Event handlers (function pointers)
//
void (* modeChangedHandler)();
void (* intensityChangedHandler)();
  
  
void configInt0Interrupt() {
  #if defined(__AVR_ATmega328P__)
    EIMSK |= _BV(INT0);      // Enable INT0 (external interrupt) 
    EICRA |= _BV(ISC00);     // Any logical change triggers an interrupt

  #elif defined(__AVR_ATtiny85__)
    GIMSK |= _BV(INT0);      // Enable INT0 (external interrupt) 
    MCUCR |= _BV(ISC00);     // Any logical change triggers an interrupt
  #endif
}


ISR (INT0_vect) {       // Interrupt service routine for INT0 on PB2
  debounceSwitch();
  interruptSource = MODE_CHANGED_INTERRUPT;
  modeChangedHandler();
}

void configPinChangeInterrupts() {
  // Pin-change interrupts are triggered for each level-change; this cannot be configured
  #if defined(__AVR_ATmega328P__)
    PCICR |= _BV(PCIE0);                       // Enable pin-change interrupt 0 
    PCMSK0 |= _BV(PCINT0) | _BV(PCINT1);       // Configure pins PB0, PB1
    
    PCICR |= _BV(PCIE2);                       // Enable pin-change interrupt 2  
    PCMSK2 |= _BV(PCINT22) | _BV(PCINT23);     // Configure pins PD6, PD7

  #elif defined(__AVR_ATtiny85__)
    GIMSK|= _BV(PCIE);
    PCMSK|= _BV(PCINT2) | _BV(PCINT3) | _BV(PCINT4);    // Configure PB0, PB3 and PB4 as pin-change interrupt source
  #endif
}


ISR (PCINT0_vect) {       // Interrupt service routine for Pin Change Interrupt Request 0
  debounceSwitch();
  if (updateFanModeFromInputPins()) {
    interruptSource = MODE_CHANGED_INTERRUPT;
    modeChangedHandler();
  } else if (updateFanIntensityFromInputPins()) {
    interruptSource = INTENSITY_CHANGED_INTERRUPT;
    intensityChangedHandler();
  }
}

void configPWM1() {
  #if defined(__AVR_ATmega328P__)
    // Arduino default PWM frequency = 490 Hz

    // Configure Timer_1 for PWM @ 25 kHz.
    // Source: https://www.arduined.eu/arduino-pwm-pc-fan-control/
    //
    // Undo the configuration done by the Arduino core library:
    TCCR1A = 0; 
    TCCR1B = 0;

    // Waveform Generator Mode (WGM):
    // -> See Table 15-5 of ATmega328P Datasheet
    // - 4 bits, distributed across TCCR1A and TCCR1B 
    // - Set to mode #10: "PWM, phase correct, 9-bit, TOP = ICR1" 
    //   | WGM13 | WGM12 | WGM11 | WGM10 |
    //   |   1   |   0   |   1   |   0   |

    // Compare Output Mode for chanlels A / B (COM)
    // !!! Channels A and B have NOTHING TO DO WITH CONTROL REGISTERS A and B !!!
    // -> See Table 15-4 of ATmega328P Datasheet (this table applies due to the WGM13 bit)
    // - Set to "Clear OC1A/OC1B on compare match when up-counting."
    // | COM1A1 | COM1A0 | COM1B1 | COM1B0 | 
    // |   1    |   0    |    1   |   0    |

    // Prescaler / Clock Select (CS)
    // - 3 bits
    // - Set to 1 (no prescaling)
    //   | CS12 | CS11 | CS10 | 
    //   |  0   |  0   |   1  |
  
    // Configure Timer/Counter1 Control Register A (TCCR1A) 
    // | COM1A1 | COM1A0 | COM1B1 | COM1B0 |  -  |  -  | WGM11 | WGM10 |
    // |   1    |   0    |    1   |   0    |  0  |  0  |   1   |   0   |
    TCCR1A = _BV(COM1A1)
          | _BV(COM1B1)
          | _BV(WGM11);

    // Configure Timer/Counter1 Control Register B (TCCR1B) 
    // - Input Capture Noise Canceler (ICNC)
    // - Input Capture Edge Select (ICNS)
    // | ICNC1 |  ICES1 |  -  | WGM13 | WGM12 | CS12 | CS11 | CS10 | 
    // |   0   |    0   |  0  |   1   |   0   |  0   |  0   |   1  |
    TCCR1B = _BV(WGM13)  
          | _BV(CS10);

    TCNT1 = 0;  // Reset timer
    ICR1 = TIMER1_COUNT_TO; // TOP (= count to this value)
            
    // Set the PWM pin as output.
    // configOutput(9); // OC1A -- we use this port for input
    configOutput(10);  // OC1B 
  
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

void setFanDutyCycle(pwm_duty_t value) {
  fanDutyCycleValue = value;
  #if defined(__AVR_ATmega328P__)
    analogWrite(FAN_PWM_OUT_PIN, value); // Send PWM signal

  #elif defined(__AVR_ATtiny85__)
    OCR1A = value;
  #endif
}

pwm_duty_t getFanDutyCycle() {
  return fanDutyCycleValue;
}

bool isPwmActive() {
  return fanDutyCycleValue != ANALOG_OUT_MIN   // fan off – no PWM required
      && fanDutyCycleValue != ANALOG_OUT_MAX;       // fan on at maximum – no PWM required
}
void setStatusLED(bool on) {
  statusLEDState = on;
  digitalWrite(STATUS_LED_OUT_PIN, on);
}

void invertStatusLED() {
  setStatusLED(statusLEDState == HIGH ? LOW : HIGH);
}

void showPauseBlip() {
  setStatusLED(HIGH);
  delay(INTERVAL_PAUSE_BLIP_ON_DURATION_MS);
  setStatusLED(LOW);
}
