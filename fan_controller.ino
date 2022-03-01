
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
const uint16_t INTERVAL_PAUSE_BLIP_OFF_DURATION_MS = 5000;  // [ms] LED blips during pause: HIGH state
const uint16_t INTERVAL_PAUSE_BLIP_ON_DURATION_MS = 200;    // [ms] LED LOW state

// Fan soft start and stop:
const uint16_t FAN_START_DURATION_MS = 4000;  // [ms] duration from full stop to full throttle
const uint16_t FAN_STOP_DURATION_MS = 2000;   // [ms] duration from full throttle to full stop
const bool BLINK_LED_DURING_SPEED_TRANSITION = false;

// --------------------
// DO NOT TOUCH THE VALUES OF THE FOLLOWING CONSTANTS
// --------------------

#ifdef __AVR_ATmega328P__
  #define VERBOSE
  const uint8_t MODE_SWITCH_IN_PIN_1 = 8;         // PB0 - digital: PB0==LOW  && PB1==HIGH  --> INTERVAL
  const uint8_t MODE_SWITCH_IN_PIN_2 = 9;         // PB1 - digital: PB0==HIGH && PB1==LOW   --> CONTINUOUS
                                                  //                PB0==HIGH && PB1==HIGH  --> OFF
  const uint8_t INTENSITY_SWITCH_IN_PIN_1 = 6;    // PD6 - digital: PD6==LOW  && PD7==HIGH  --> LOW INTENSITY
  const uint8_t INTENSITY_SWITCH_IN_PIN_2 = 7;    // PD7 - digital: PD6==HIGH && PD7==LOW   --> HIGH INTENSITY
                                                  //                PD6==HIGH && PD7==HIGH  --> MEDIUM INTENSITY
  const uint8_t FAN_PWM_OUT_PIN = 3;              // PD3 - PWM signal @ native frequency (490 Hz)
  const uint8_t FAN_POWER_OUT_PIN = 4;            // PD4 - fan power: MOSFET on/off
  const uint8_t STATUS_LED_OUT_PIN = 5;           // PD5 - digital out; is on when fan is of, blinks during transitioning
#endif 

#ifdef __AVR_ATtiny85__
  const uint8_t MODE_SWITCH_IN_PIN = PB2;         // digital: LOW --> CONTINOUS, HIGH --> INTERVAL
  const uint8_t INTENSITY_SWITCH_IN_PIN_1 = PB3;  // digital: PB3==LOW && PB4==HIGH --> LOW INTENSITY, BOTH=LOW --> MEDIUM
  const uint8_t INTENSITY_SWITCH_IN_PIN_2 = PB4;  // digital: PB3==HIGH && PB4==LOW --> HIGH INTENSITY
  
  const uint8_t FAN_POWER_OUT_PIN = PB5;          // Fan power: MOSFET on/off (some fans don't stop at PWM duty cycle = 0%)
  const uint8_t FAN_PWM_OUT_PIN = PB1;            // PWM signal @ 25 kHz
  const uint8_t STATUS_LED_OUT_PIN = PB0;         // digital out; blinks shortly in long intervals when fan is in interval mode
#endif 


//
// INPUTs
//
enum FanMode {MODE_OFF, MODE_CONTINUOUS, MODE_INTERVAL};
FanMode fanMode = MODE_OFF;

enum FanIntensity {INTENSITY_LOW, INTENSITY_MEDIUM, INTENSITY_HIGH};
FanIntensity fanIntensity = INTENSITY_LOW;


//
// CONTROLLER STATES
//
enum FanState {FAN_OFF, FAN_SPEEDING_UP, FAN_STEADY, FAN_SLOWING_DOWN, FAN_PAUSING};
FanState fanState = FAN_OFF;                      // current fan state

enum Event {EVENT_NONE, MODE_CHANGED, INTENSITY_CHANGED, TARGET_SPEED_REACHED, INTERVAL_PHASE_ENDED};
Event pendingEvent = EVENT_NONE;

uint32_t intervalPhaseBeginTime = 0; // [ms]

// Control cycle: PWM parameters are set only once per cycle
const uint32_t SPEED_TRANSITION_CYCLE_DURATION_MS = 200; // [ms]


//
// ANALOG OUT
//
//
#ifdef __AVR_ATmega328P__
const uint8_t ANALOG_OUT_MIN = 0;        // Arduino constant
const uint8_t ANALOG_OUT_MAX = 255;      // PWM control
#endif
#ifdef __AVR_ATtiny85__
// PWM frequency = 1 MHz / 1 / 40 = 25 kHz 
const uint8_t TIMER1_PRESCALER = 1;     // divide by 1
const uint8_t TIMER1_COUNT_TO = 40;    // count to 40

const uint8_t ANALOG_OUT_MIN = 0;                 // Arduino constant
const uint8_t ANALOG_OUT_MAX = TIMER1_COUNT_TO;   // PWM control
#endif

const uint8_t FAN_OUT_LOW_THRESHOLD = (uint32_t) ANALOG_OUT_MAX * FAN_LOW_THRESHOLD_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint8_t FAN_OUT_FAN_OFF = ANALOG_OUT_MIN;

// Continuous mode:
const uint8_t FAN_OUT_CONTINUOUS_LOW_DUTY_VALUE = (uint32_t) ANALOG_OUT_MAX * FAN_CONTINUOUS_LOW_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint8_t FAN_OUT_CONTINUOUS_MEDIUM_DUTY_VALUE = (uint32_t) ANALOG_OUT_MAX * FAN_CONTINUOUS_MEDIUM_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint8_t FAN_OUT_CONTINUOUS_HIGH_DUTY_VALUE = (uint32_t) ANALOG_OUT_MAX * FAN_CONTINUOUS_HIGH_VOLTAGE /  FAN_MAX_VOLTAGE;

const uint8_t FAN_OUT_INTERVAL_FAN_ON_DUTY_VALUE = (uint32_t) ANALOG_OUT_MAX * FAN_INTERVAL_FAN_ON_VOLTAGE /  FAN_MAX_VOLTAGE;
const uint32_t INTERVAL_FAN_ON_DURATION_MS = (uint32_t) INTERVAL_FAN_ON_DURATION * 1000; // [ms]
uint32_t intervalPauseDuration;

// Fan soft start and soft stop:
const uint16_t FAN_START_INCREMENT = (uint32_t) (ANALOG_OUT_MAX - FAN_OUT_LOW_THRESHOLD) * SPEED_TRANSITION_CYCLE_DURATION_MS / FAN_START_DURATION_MS;
const uint16_t FAN_STOP_DECREMENT = (uint32_t) (ANALOG_OUT_MAX - FAN_OUT_LOW_THRESHOLD) * SPEED_TRANSITION_CYCLE_DURATION_MS / FAN_STOP_DURATION_MS;

uint8_t fanTargetDutyValue = FAN_OUT_FAN_OFF; // value derived from input-pin values
uint8_t fanActualDutyValue = FAN_OUT_FAN_OFF; // value actually set on output pin

uint32_t lastPauseBlipTime = 0;
uint8_t transitioningDutyValue = ANALOG_OUT_MIN; // incremented in discrete steps until fan is at its target speed or its low threshold

//
// DIGITAL OUT
//
uint8_t statusLEDState = LOW;

//
// SETUP
//
void setup() {
  configInputWithPullup(MODE_SWITCH_IN_PIN_1);
  configInputWithPullup(MODE_SWITCH_IN_PIN_2);
  configInputWithPullup(INTENSITY_SWITCH_IN_PIN_1);
  configInputWithPullup(INTENSITY_SWITCH_IN_PIN_2);
  configOutput(STATUS_LED_OUT_PIN);
  
//  configInt0Interrupt(); // triggered by PD2 (mode switch)
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
  
  setStatusLED(LOW);
  fanIntensity = readFanIntensityFromInputPins();
  fanMode = readFanModeFromInputPins();
  if (fanMode != MODE_OFF) {
     handleStateTransition(MODE_CHANGED);
  }
  
}


void loop() {
  uint32_t now = millis();

  switch(fanState) {
    case FAN_OFF:
       // sleep forever --> until interrupt happens
      delay(10000);
      break;
      
    case FAN_SPEEDING_UP:
      // When transistioning from OFF or STEADY, duty value is set to minimum -> let fab speed up to this first -> delay first
      delay(SPEED_TRANSITION_CYCLE_DURATION_MS);
      speedUp();
      break;
      
    case FAN_STEADY:
      if (fanMode == MODE_INTERVAL) {
        // sleep until active phase is over
        int32_t remaingingPhaseDuration = INTERVAL_FAN_ON_DURATION_MS - (now - intervalPhaseBeginTime);
        if (remaingingPhaseDuration > 0) {
          delay(remaingingPhaseDuration);
        } else {
          handleStateTransition(INTERVAL_PHASE_ENDED);
        }
      } else {
        // sleep forever --> until interrupt happens
        delay(10000);
      }
      break;
      
    case FAN_SLOWING_DOWN:
      slowDown();
      delay(SPEED_TRANSITION_CYCLE_DURATION_MS);
      break;
      
    case FAN_PAUSING:
      // sleep until next LED flash or until pause is over (whatever will happen first)
      int32_t remaingingPhaseDuration = intervalPauseDuration - (now - intervalPhaseBeginTime);
      if (remaingingPhaseDuration > 0) {
        int32_t remaingingBlipDelay = INTERVAL_PAUSE_BLIP_OFF_DURATION_MS - (now - lastPauseBlipTime);
        if (remaingingBlipDelay < 0) {
          remaingingBlipDelay = 0;
        }
        if (remaingingPhaseDuration <= remaingingBlipDelay) {
          delay(remaingingPhaseDuration);
        } else {
          delay(remaingingBlipDelay);
          setStatusLED(HIGH);
          lastPauseBlipTime = millis();
          delay(INTERVAL_PAUSE_BLIP_ON_DURATION_MS);
          setStatusLED(LOW);
        }
      } else {
        handleStateTransition(INTERVAL_PHASE_ENDED);
      }
      break;
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
          if (fanMode != MODE_OFF) {
            fanState = FAN_SPEEDING_UP;
            fanOn(fanMode);
            intervalPhaseBeginTime = now;
          }
          break;
          
        case INTENSITY_CHANGED:
          // noop: new value has been recorded in fanIntensity, will take effect on next mode change
          break;
          
        default: 
          break;
      }
      break;
      
    case FAN_SPEEDING_UP: 
      switch(event) {
        case MODE_CHANGED:
          if (fanMode == MODE_OFF) {
            fanState = FAN_SLOWING_DOWN;
            fanTargetDutyValue = FAN_OUT_FAN_OFF;
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (fanMode == MODE_CONTINUOUS) {
            uint8_t newTargetDutyValue = mapToFanDutyValue(fanIntensity);
            if (newTargetDutyValue > fanActualDutyValue) {
              fanTargetDutyValue = newTargetDutyValue;
            } else if (newTargetDutyValue < fanActualDutyValue) {
              fanState = FAN_SLOWING_DOWN;
              fanTargetDutyValue = newTargetDutyValue;
            } else { /* newTargetDutyValue == fanActualDutyValue */
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
          if (fanMode == MODE_OFF) {
            fanState = FAN_SLOWING_DOWN;
            fanTargetDutyValue = FAN_OUT_FAN_OFF;
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (fanMode == MODE_CONTINUOUS) {
            uint8_t newTargetDutyValue = mapToFanDutyValue(fanIntensity);
            if (newTargetDutyValue > fanActualDutyValue) {
              fanState = FAN_SPEEDING_UP;
              fanTargetDutyValue = newTargetDutyValue;
            } else if (newTargetDutyValue < fanActualDutyValue) {
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
          if (fanMode == MODE_OFF) {
            fanTargetDutyValue = FAN_OUT_FAN_OFF;
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (fanMode == MODE_CONTINUOUS) {
            uint8_t newTargetDutyValue = mapToFanDutyValue(fanIntensity);
            if (newTargetDutyValue > fanActualDutyValue) {
              fanState = FAN_SPEEDING_UP;
              fanTargetDutyValue = newTargetDutyValue;
            } else if (newTargetDutyValue < fanActualDutyValue) {
              fanTargetDutyValue = newTargetDutyValue;
            } else {  /* newTargetDutyValue == fanActualDutyValue */
              fanState = FAN_STEADY;
              setStatusLED(LOW);
            }
          }
          break;
          
        case TARGET_SPEED_REACHED: 
          if (fanMode == MODE_OFF) {
            fanState = FAN_OFF;
            fanOff(MODE_INTERVAL);
          } else if (fanMode == MODE_CONTINUOUS) {
            fanState = FAN_STEADY;
          } else {  // fanMode == MODE_INTERVAL
            fanState = FAN_PAUSING;
            fanOff(MODE_INTERVAL);
            intervalPhaseBeginTime = now;
            lastPauseBlipTime = now;
          }
          setStatusLED(LOW);
          break;
      }
      break;
      
    case FAN_PAUSING:
      switch(event) {
        case MODE_CHANGED: 
          if (fanMode == MODE_OFF) {
            fanState = FAN_OFF;
            fanOff(MODE_INTERVAL);
          } else if (fanMode == MODE_CONTINUOUS) {
            fanState = FAN_SPEEDING_UP;
            fanOn(MODE_CONTINUOUS);
          }
          break;
          
        case INTENSITY_CHANGED: 
          intervalPauseDuration = mapToIntervalPauseDuration(fanIntensity);
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

void fanOn(FanMode mode) {
  configOutput(FAN_POWER_OUT_PIN);
  configOutput(FAN_PWM_OUT_PIN);
  setFanPower(HIGH);
  setFanDutyValue(FAN_OUT_LOW_THRESHOLD);
  if (mode == MODE_CONTINUOUS) {
    fanTargetDutyValue = mapToFanDutyValue(fanIntensity);
  } else { /* fanMode == MODE_INTERVAL */
    fanTargetDutyValue = FAN_OUT_INTERVAL_FAN_ON_DUTY_VALUE;
  }
}

void fanOff(FanMode mode) {
  setFanPower(LOW);
  setFanDutyValue(FAN_OUT_FAN_OFF);
  if (mode == MODE_INTERVAL) {
     intervalPauseDuration = mapToIntervalPauseDuration(fanIntensity);
  }
  configInput(FAN_POWER_OUT_PIN);
  configInput(FAN_PWM_OUT_PIN);
}


void speedUp() {
  uint8_t transitioningDutyValue = fanActualDutyValue;
  
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
  
  setFanDutyValue(transitioningDutyValue);
  if (fanActualDutyValue >= fanTargetDutyValue) {
    handleStateTransition(TARGET_SPEED_REACHED);
    
  } else if (BLINK_LED_DURING_SPEED_TRANSITION) {
    invertStatusLED();
  }
}


void slowDown() {
  uint8_t transitioningDutyValue = fanActualDutyValue;
  
  // ensure transitioningDutyValue will not become < 0 when decrementing (is an uint8_t!)
  uint8_t decrement = min(transitioningDutyValue, FAN_STOP_DECREMENT);

  if (fanActualDutyValue == FAN_OUT_LOW_THRESHOLD && fanTargetDutyValue < FAN_OUT_LOW_THRESHOLD) {
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
  
  setFanDutyValue(transitioningDutyValue);
  if (fanActualDutyValue <= fanTargetDutyValue) {
    handleStateTransition(TARGET_SPEED_REACHED);
    
  } else if (BLINK_LED_DURING_SPEED_TRANSITION) {
    invertStatusLED();
  }
}


FanMode readFanModeFromInputPins() {
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
  return value;
}

FanIntensity readFanIntensityFromInputPins() {
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
  return value;
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

//
// Utility Routines
//

void setFanDutyValue(uint8_t value) {
  fanActualDutyValue = value;
  #ifdef __AVR_ATmega328P__
    analogWrite(FAN_PWM_OUT_PIN, value); // Send PWM signal
  #endif
  #ifdef __AVR_ATtiny85__
    OCR1A = value;
  #endif
}

void setStatusLED(int value) {
  statusLEDState = value;
  digitalWrite(STATUS_LED_OUT_PIN, value);
}

void setFanPower(int value) {
  digitalWrite(FAN_POWER_OUT_PIN, value);
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
  #ifdef __AVR_ATmega328P__
//    EIMSK |= (1<<INT0);      // Enable INT0 (external interrupt) 
//    EICRA |= (1<<ISC00);     // Any logical change triggers an interrupt
  #endif
  #ifdef __AVR_ATtiny85__
    GIMSK |= (1<<INT0);      // Enable INT0 (external interrupt) 
    MCUCR |= (1<<ISC00);     // Any logical change triggers an interrupt
  #endif
}

ISR (INT0_vect) {       // Interrupt service routine for INT0 on PB2
  FanMode value = readFanModeFromInputPins();
  if (value != fanMode) {    // debounce switch (may cause multiple interrupts)
    fanMode = value;
    handleStateTransition(MODE_CHANGED);
  }
}

void configPinChangeInterrupts() {
  // Pin-change interrupts are triggered for each level-change; this cannot be configured
  #ifdef __AVR_ATmega328P__
    PCICR |= (1<<PCIE0);                       // Enable pin-change interrupt 0 
//    PCIFR |= (1<<PCIF0);                       // Enable PCINT0..5 (pins PB0..PB5) 
    PCMSK0 |= (1<<PCINT0) | (1<<PCINT1);       // Configure pins PB0, PB1
    
    PCICR |= (1<<PCIE2);                       // Enable pin-change interrupt 2 
//    PCIFR |= (1<<PCIF2);                       // Enable PCINT16..23 (pins PD0..PD7) 
    PCMSK2 |= (1<<PCINT22) | (1<<PCINT23);     // Configure pins PD6, PD7
  #endif
  #ifdef __AVR_ATtiny85__
    GIMSK|= (1<<PCIE);
    PCMSK|= (1<<PCINT1) | (1<<PCINT3);    // Configure PB1 and PB3 as interrupt source
  #endif
}

ISR (PCINT0_vect) {       // Interrupt service routine for Pin Change Interrupt Request 0
  FanMode value = readFanModeFromInputPins();
  if (value != fanMode) {    // debounce switch (may cause multiple interrupts)
    fanMode = value;
    handleStateTransition(MODE_CHANGED);
  }
}

ISR (PCINT2_vect) {       // Interrupt service routine for Pin Change Interrupt Request 2
  FanIntensity value = readFanIntensityFromInputPins();  
  if (value != fanIntensity) {    // debounce switch (may cause multiple interrupts)
    fanIntensity = value;
    handleStateTransition(INTENSITY_CHANGED);
  }
}

void configPWM1() {
  #ifdef __AVR_ATmega328P__
    // nothing --> use analogWrite as is
    // No specific PWM frequency
  #endif
  
  #ifdef __AVR_ATtiny85__
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
