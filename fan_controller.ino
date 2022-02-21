
#include<avr/io.h>
#include<util/delay.h>

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

// --------------------
// DO NOT TOUCH THE VALUES OF THE FOLLOWING CONSTANTS
// --------------------

#define _ATMEGA328_
//#define _ATTINY85_

#ifdef _ATMEGA328_
  #define VERBOSE
  const uint8_t MODE_SWITCH_IN_PIN = 2;           // PD2 - digital: LOW --> CONTINOUS, HIGH   --> INTERVAL
  const uint8_t INTENSITY_SWITCH_IN_PIN_1 = 5;    // PD5 - digital: PD5==LOW && PD6==HIGH     --> LOW INTENSITY
  const uint8_t INTENSITY_SWITCH_IN_PIN_2 = 6;    // PD6 - digital: PD5==HIGH && PD6==LOW     --> HIGH INTENSITY
                                                  //                PD5==LOW && PD6==HIGH=LOW --> MEDIUM INTENSITY
  const uint8_t FAN_OUT_PIN = 3;                  // PD3 - PWM @ native frequency
  const uint8_t STATUS_LED_OUT_PIN = 4;           // PD4 - digital out; is on when fan is of, blinks during transitioning
#endif 

#ifdef _ATTINY85_
  const uint8_t MODE_SWITCH_IN_PIN = PB2;         // digital: LOW --> CONTINOUS, HIGH --> INTERVAL
  const uint8_t INTENSITY_SWITCH_IN_PIN_1 = PB3;  // digital: PB3==LOW && PB4==HIGH --> LOW INTENSITY, BOTH=LOW --> MEDIUM
  const uint8_t INTENSITY_SWITCH_IN_PIN_2 = PB4;  // digital: PB3==HIGH && PB4==LOW --> HIGH INTENSITY
  
  const uint8_t FAN_OUT_PIN = PB1;                // PWM @ 25 kHz
  const uint8_t STATUS_LED_OUT_PIN = PB0;         // digital out; blinks shortly in long intervals when fan is in interval mode
#endif 


//
// CONTROLLER STATES
//
enum FanMode {MODE_OFF, MODE_CONTINUOUS, MODE_INTERVAL};
FanMode fanMode = MODE_OFF;                       // actual fan mode

enum FanIntensity {INTENSITY_LOW, INTENSITY_MEDIUM, INTENSITY_HIGH};
FanIntensity fanIntensity = INTENSITY_LOW;        // actual fan intensity

// Interval mode:
enum IntervalPhase {PHASE_FAN_ON, PHASE_PAUSE};
IntervalPhase intervalPhase = PHASE_FAN_ON;

uint32_t intervalPhaseBeginTime = 0; // [ms]

// Fan control:
enum FanState {FAN_OFF, FAN_SPEEDING_UP, FAN_STEADY, FAN_SLOWING_DOWN};

FanState fanState = FAN_OFF;

// Control cycle: output values are set only once per cycle
const uint32_t CONTROL_CYCLE_DURATION_MS = 200; // [ms]
uint32_t controlCycleBeginTime = 0; // [ms]


//
// ANALOG OUT
//
//
#ifdef _ATMEGA328_
const uint8_t ANALOG_OUT_MIN = 0;        // Arduino constant
const uint8_t ANALOG_OUT_MAX = 255;      // PWM control
#endif
#ifdef _ATTINY85_
// PWM frequency = 1 MHz / 1 / 40 = 25 kHz 
const uint8_t TIMER1_PRESCALER = 1;     // divide by 1
const uint8_t TIMER1_COUNT_TO = 40;    // count to 40

const uint8_t ANALOG_OUT_MIN = 0;                 // Arduino constant
const uint8_t ANALOG_OUT_MAX = TIMER1_COUNT_TO;   // PWM control
#endif

const uint8_t FAN_OUT_LOW_THRESHOLD = (long) ANALOG_OUT_MAX * FAN_LOW_THRESHOLD_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint8_t FAN_OUT_FAN_OFF = ANALOG_OUT_MIN;

// Continuous mode:
const uint8_t FAN_OUT_CONTINUOUS_LOW_DUTY_VALUE = (long) ANALOG_OUT_MAX * FAN_CONTINUOUS_LOW_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint8_t FAN_OUT_CONTINUOUS_MEDIUM_DUTY_VALUE = (long) ANALOG_OUT_MAX * FAN_CONTINUOUS_MEDIUM_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint8_t FAN_OUT_CONTINUOUS_HIGH_DUTY_VALUE = (long) ANALOG_OUT_MAX * FAN_CONTINUOUS_HIGH_VOLTAGE /  FAN_MAX_VOLTAGE;

const uint8_t FAN_OUT_INTERVAL_FAN_ON_DUTY_VALUE = (long) ANALOG_OUT_MAX * FAN_INTERVAL_FAN_ON_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint32_t INTERVAL_FAN_ON_DURATION_MS = (uint32_t) INTERVAL_FAN_ON_DURATION * 1000; // [ms]
uint32_t intervalPauseDuration;

// Fan soft start and soft stop:
const uint16_t FAN_START_INCREMENT = ((long) (ANALOG_OUT_MAX - FAN_OUT_LOW_THRESHOLD)) * CONTROL_CYCLE_DURATION_MS / FAN_START_DURATION_MS;
const uint16_t FAN_STOP_DECREMENT = ((long) (ANALOG_OUT_MAX - FAN_OUT_LOW_THRESHOLD)) * CONTROL_CYCLE_DURATION_MS / FAN_STOP_DURATION_MS;

uint8_t fanTargetDutyValue = FAN_OUT_FAN_OFF; // potentiometer value read from input pin
uint8_t fanActualDutyValue = FAN_OUT_FAN_OFF; // value actually set on output pin

uint32_t transitionBeginTime = 0;
uint8_t transitioningDutyValue = ANALOG_OUT_MIN; // incremented in discrete steps until fan is at its target speed or its low threshold

//
// DIGITAL OUT
//
uint8_t statusLEDState = LOW;
uint16_t controlCycleCount = 0;

//
// SETUP
//
void setup() {
  configInputWithPullup(MODE_SWITCH_IN_PIN);
  configInputWithPullup(INTENSITY_SWITCH_IN_PIN_1);
  configInputWithPullup(INTENSITY_SWITCH_IN_PIN_2);
  configOutput(STATUS_LED_OUT_PIN);
  configOutput(FAN_OUT_PIN);
  
  configInt0Interrupt(); // triggered by PD2 (mode switch)
  configPinChangeInterrupts();
  sei();
  configPWM1();

  #ifdef VERBOSE
  // Setup Serial Monitor
  Serial.begin(9600);
  
  Serial.print("Fan out max: ");
  Serial.println(ANALOG_OUT_MAX);
  Serial.print("Fan out low threshold: ");
  Serial.println(FAN_OUT_LOW_THRESHOLD);
  #endif
  
  controlCycleBeginTime = millis();
  intervalPhaseBeginTime = millis();
  
  fanMode = readFanModeFromInputPin();
  fanIntensity = readFanIntensityFromInputPins();
  handleInputChange(fanMode, fanIntensity, controlCycleBeginTime); 
  
  setStatusLED(LOW);
}

void loop() {
  uint32_t now = millis();

  if (fanMode == MODE_INTERVAL) {
    
    if(intervalPhase == PHASE_FAN_ON) {
      if (now - intervalPhaseBeginTime >= INTERVAL_FAN_ON_DURATION_MS) { // fan on is over
        #ifdef VERBOSE
          Serial.print("Mode INTERVAL, Phase: PAUSE, ");
          Serial.println(intervalPauseDuration);
        #endif
        prepareFanDutyChange(FAN_OUT_FAN_OFF, now);
        intervalPhase = PHASE_PAUSE;
        intervalPhaseBeginTime = now;
      }
      
    } else { // PHASE_PAUSE
      if (now - intervalPhaseBeginTime >= intervalPauseDuration) { // pause is over
        #ifdef VERBOSE
          Serial.print("Mode INTERVAL, Phase: FAN ON, ");
          Serial.println(INTERVAL_FAN_ON_DURATION_MS);
        #endif
        prepareFanDutyChange(FAN_OUT_INTERVAL_FAN_ON_DUTY_VALUE, now);
        intervalPhase = PHASE_FAN_ON;
        intervalPhaseBeginTime = now;
        controlCycleCount = 0;
      }
      // blink LED
      controlCycleCount++;
      if (controlCycleCount >= 50) {
        controlCycleCount = 0;
        setStatusLED(HIGH);
        delay(CONTROL_CYCLE_DURATION_MS);
        setStatusLED(LOW);
      }
    }
  }
  
  if (fanState == FAN_SPEEDING_UP || fanState == FAN_SLOWING_DOWN) {
    handleFanSpeedTransition(fanTargetDutyValue);
  }
  
  delay(CONTROL_CYCLE_DURATION_MS);
  
}

void handleInputChange(FanMode newMode, FanIntensity newIntensity, uint32_t now) {
  if (newMode == MODE_OFF) {
      #ifdef VERBOSE
        Serial.println("Mode: OFF");
      #endif
      prepareFanDutyChange(FAN_OUT_FAN_OFF, now);

  } else if (newMode == MODE_CONTINUOUS) {
      uint8_t newTargetDutyValue = calcFanDutyValue(newIntensity);
      #ifdef VERBOSE
        Serial.print("Mode CONTINUOUS, Duty Value: ");
        Serial.println(newTargetDutyValue);
      #endif
      prepareFanDutyChange(newTargetDutyValue, now);

  } else if (newMode == MODE_INTERVAL) {
    intervalPauseDuration = calcIntervalPauseDuration(newIntensity);
    
    if (fanState == FAN_OFF) {
        intervalPhase = PHASE_PAUSE;
        #ifdef VERBOSE
          Serial.print("Mode INTERVAL, Phase: PAUSE, Pause: ");
          Serial.println(intervalPauseDuration);
        #endif
        
    } else {
        intervalPhase = PHASE_FAN_ON;
        uint8_t newTargetDutyValue = FAN_OUT_INTERVAL_FAN_ON_DUTY_VALUE;
        prepareFanDutyChange(newTargetDutyValue, now);
        #ifdef VERBOSE
          Serial.print("Mode INTERVAL, Phase: FAN ON, Pause: ");
          Serial.println(intervalPauseDuration);
        #endif
    }
    intervalPhaseBeginTime = now;
  }
}

void prepareFanDutyChange(uint8_t newTargetDutyValue, uint32_t now) {
  if (newTargetDutyValue == fanActualDutyValue) {
    return;
  }
  
  transitionBeginTime = now;
  
  if (newTargetDutyValue == FAN_OUT_FAN_OFF && fanActualDutyValue > fanActualDutyValue) {
    fanState = FAN_SLOWING_DOWN;
    fanTargetDutyValue = FAN_OUT_FAN_OFF;
    transitioningDutyValue = fanActualDutyValue;
    #ifdef VERBOSE
      Serial.println("STOPPING.");
    #endif
    return;
  }
    
  if (newTargetDutyValue > fanActualDutyValue) {
    fanState = FAN_SPEEDING_UP;
    #ifdef VERBOSE
      Serial.print("SPEEDING UP to ");
      Serial.println(newTargetDutyValue);
    #endif
      
  } else if (newTargetDutyValue < fanActualDutyValue) { 
    fanState = FAN_SLOWING_DOWN;
    #ifdef VERBOSE
      Serial.print("SLOWING DOWN to ");
      Serial.println(newTargetDutyValue);
    #endif
  }
  fanTargetDutyValue = newTargetDutyValue;
  transitioningDutyValue = (fanActualDutyValue == FAN_OUT_FAN_OFF) ? FAN_OUT_LOW_THRESHOLD : fanActualDutyValue;
}

void handleFanSpeedTransition(uint8_t targetDutyValue) {
  if (fanState == FAN_OFF || fanState == FAN_STEADY) {
    return;
  }
  
  if (fanState == FAN_SPEEDING_UP) {
    // ensure we don't overrun the max value of uint8_t when incrementing:
    uint8_t increment = min(ANALOG_OUT_MAX - transitioningDutyValue, FAN_START_INCREMENT);

    if (fanActualDutyValue == FAN_OUT_FAN_OFF) {
      // start fan up first, don't increment transitioningDutyValue at this time
      
    } else if (transitioningDutyValue + increment < targetDutyValue) {
      transitioningDutyValue += increment;
    } else {
      transitioningDutyValue = targetDutyValue;
    }
    #ifdef VERBOSE
      Serial.print("Speeding up: ");
      Serial.println(transitioningDutyValue);
    #endif
    
  } else if (fanState == FAN_SLOWING_DOWN) {
    // ensure transitioningDutyValue will not become < 0 when decrementing (is an uint8_t!)
    uint8_t decrement = min(transitioningDutyValue, FAN_STOP_DECREMENT);

    if (fanActualDutyValue == FAN_OUT_LOW_THRESHOLD && targetDutyValue < FAN_OUT_LOW_THRESHOLD) {
      transitioningDutyValue = FAN_OUT_FAN_OFF;
    
    } else if (transitioningDutyValue - decrement > max(targetDutyValue, FAN_OUT_LOW_THRESHOLD)) {
      transitioningDutyValue -= decrement;
    } else {
      transitioningDutyValue = max(targetDutyValue, FAN_OUT_LOW_THRESHOLD);
    }
    #ifdef VERBOSE
      Serial.print("Slowing down: ");
      Serial.println(transitioningDutyValue);
    #endif
  }
  setFanDutyValue(transitioningDutyValue);
    
  if (transitioningDutyValue == FAN_OUT_FAN_OFF) {
    fanState = FAN_OFF;
    transitionBeginTime = 0;
    setStatusLED(LOW);
    #ifdef VERBOSE
      Serial.println("FAN OFF");
    #endif
  } else  if (transitioningDutyValue == targetDutyValue) {
    fanState = FAN_STEADY;
    transitionBeginTime = 0;
    transitioningDutyValue = FAN_OUT_FAN_OFF;
    setStatusLED(LOW);
    #ifdef VERBOSE
      Serial.println("FAN STEADY.");
    #endif
  } else {
    if (BLINK_LED_DURING_SPEED_TRANSITION) {
      invertStatusLED();
    }
  }
}


FanMode readFanModeFromInputPin() {
  FanMode value = digitalRead(MODE_SWITCH_IN_PIN) ? MODE_INTERVAL : MODE_CONTINUOUS;
  #ifdef VERBOSE
    Serial.print("Refresh Fan Mode:  ");
    Serial.println(fanMode == MODE_INTERVAL ? "INTERVAL" : "CONTINUOUS");
  #endif
  return value;
}

FanIntensity readFanIntensityFromInputPins() {
  uint8_t v1 = digitalRead(INTENSITY_SWITCH_IN_PIN_1);
  uint8_t v2 = digitalRead(INTENSITY_SWITCH_IN_PIN_2);
  FanIntensity value;
  if (! v1 && v2) {
    value = INTENSITY_LOW;
  } else if(v1 && ! v2) {
    value = INTENSITY_HIGH;
  } else {
    value = INTENSITY_MEDIUM;
  }
  #ifdef VERBOSE
    Serial.print("Refresh Fan Intensity:  ");
    Serial.println(value == INTENSITY_LOW ? "LOW" : (value == INTENSITY_HIGH ? "HIGH" :"MEDIUM"));
  #endif
  return value;
}


// Applicable only in mode CONTINUOUS
uint8_t calcFanDutyValue(FanIntensity intensity) {
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

//
// Utility Routines
//

void setFanDutyValue(uint8_t value) {
  fanActualDutyValue = value;
  #ifdef _ATMEGA328_
    analogWrite(FAN_OUT_PIN, value); // Send PWM signal
  #endif
  #ifdef _ATTINY85_
    OCR1A = value;
  #endif
}

void setStatusLED(int value) {
  statusLEDState = value;
  digitalWrite(STATUS_LED_OUT_PIN, value);
}

void invertStatusLED() {
  setStatusLED(statusLEDState == HIGH ? LOW : HIGH);
}

void configInput(uint8_t pin) {
  pinMode(pin, INPUT);
}

void configInputWithPullup(uint8_t pin) {
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH);              // Activate pull-up resistor on pin (input)
}

void configOutput(uint8_t pin) {
  pinMode(pin, OUTPUT);
}

void configInt0Interrupt() {
  #ifdef _ATMEGA328_
    EIMSK |= (1<<INT0);      // Enable INT0 (external interrupt) 
    EICRA |= (1<<ISC00);     // Any logical change triggers an interrupt
  #endif
  #ifdef _ATTINY85_
    GIMSK |= (1<<INT0);      // Enable INT0 (external interrupt) 
    MCUCR |= (1<<ISC00);     // Any logical change triggers an interrupt
  #endif
}

ISR (INT0_vect) {       // Interrupt service routine for INT0 on PB2
  FanMode value = readFanModeFromInputPin();
  if (value != fanMode) {    // debounce switch (may cause multiple interrupts)
    fanMode = value;
    uint32_t now = millis();
    handleInputChange(fanMode, fanIntensity, now);
  }
}

void configPinChangeInterrupts() {
  // Pin-change interrupts are triggered for each level-change; this cannot be configured
  #ifdef _ATMEGA328_
    PCICR |= (1<<PCIE2);                       // Enable pin-change interrupt 2 
    PCIFR |= (1<<PCIF2);                       // Enable PCINT16..23 (pins PD0..PD7) 
    PCMSK2 |= (1<<PCINT21) | (1<<PCINT22) | (1<<PCINT23);     // Configure pins PD5, PD6
  #endif
  #ifdef _ATTINY85_
    GIMSK|= (1<<PCIE);
    PCMSK|= (1<<PCINT1) | (1<<PCINT3);    // Configure PB1 and PB3 as interrupt source
  #endif
}

ISR (PCINT2_vect) {       // Interrupt service routine for Pin Change Interrupt Request 0
  FanIntensity value = readFanIntensityFromInputPins();  
  if (value != fanIntensity) {    // debounce switch (may cause multiple interrupts)
    fanIntensity = value;
    uint32_t now = millis();
    handleInputChange(fanMode, fanIntensity, now);
  }
}

void configPWM1() {
  #ifdef _ATMEGA328_
    // nothing --> use analogWrite as is
    // No specific PWM frequency
  #endif
  
  #ifdef _ATTINY85_
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
    TCCR1 |= (1<<CTC1);
    
    // Enable PWM A based on OCR1A
    TCCR1 |= (1<<PWM1A);
    
    // On Compare Match with OCR1A (counter == OCR1A): Clear the output line (-> LOW), set on $00
    TCCR1 |= (1<<COM1A1);
  
    // Configure PWM frequency:
    TCCR1 |= TIMER1_PRESCALER;  // Prescale factor
    OCR1C = TIMER1_COUNT_TO;    // Count 0,1,2..compare-match,0,1,2..compare-match, etc
  
    // Determines Duty Cycle: OCR1A / OCR1C e.g. value of 50 / 200 --> 25%,  value of 50 --> 0%
    OCR1A = 0;
  #endif
}
