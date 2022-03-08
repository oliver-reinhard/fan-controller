#define F_CPU 1000000UL                  // Defaults to 1 MHz
//#define F_CPU 128000UL                  // Defaults to 1 MHz

//#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
  
#include "fan_io.h"
#include "fan_control.h"

//
//  #define VERBOSE --> see fan_io.h
//
const uint32_t INTERVAL_FAN_ON_DURATION_MS = (uint32_t) INTERVAL_FAN_ON_DURATION * 1000; // [ms]

// volatile --> variable can change at any time --> prevents compiler from optimising away a read access that would 
// return a value changed by an interrupt handler
volatile Event interruptCause = EVENT_NONE;

//
// SETUP
//
void setup() {
 
  configInputPins();
  configOutputPins();
  
  //  configInt0Interrupt(); // triggered by PD2 (mode switch)
  configPinChangeInterrupts();
  sei();
  configPWM1();
  configPowerSave();

  #ifdef VERBOSE
    // Setup Serial Monitor
    Serial.begin(9600);
    
    Serial.print("Fan out max: ");
    Serial.println(ANALOG_OUT_MAX);
    Serial.print("Fan out low threshold: ");
    Serial.println(FAN_OUT_LOW_THRESHOLD);
    #define USART0_SERIAL USART0_ON
  #else
    #define USART0_SERIAL USART0_OFF
  #endif
  
  setStatusLED(LOW);
  getFanIntensity(); // ensure initialisation
  if (getFanMode() != MODE_OFF) {
     handleStateTransition(MODE_CHANGED);
  }
}


void loop() {
  uint32_t now = millis();

  switch(getFanState()) {
    case FAN_OFF:
      waitForInterrupt();  // blocking wait
      break;
      
    case FAN_SPEEDING_UP:
      // When transistioning from OFF or STEADY, duty value is set to minimum -> let fab speed up to this first -> delay first
      if (! interruptibleDelay(SPEED_TRANSITION_CYCLE_DURATION_MS)) {
        speedUp();
      }
      break;
      
    case FAN_STEADY:
      if (getFanMode() == MODE_INTERVAL) {
        // sleep until active phase is over
        int32_t remaingingPhaseDuration = INTERVAL_FAN_ON_DURATION_MS - (now - getIntervalPhaseBeginTime());
        if (remaingingPhaseDuration > 0) {
          interruptibleDelay(remaingingPhaseDuration);
        } else {
          handleStateTransition(INTERVAL_PHASE_ENDED);
        }
      } else {
        waitForInterrupt();
      }
      break;
      
    case FAN_SLOWING_DOWN:
      slowDown();
      interruptibleDelay(SPEED_TRANSITION_CYCLE_DURATION_MS);
      break;
      
    case FAN_PAUSING:
      // sleep until next LED flash or until pause is over (whatever will happen first)
      int32_t remaingingPhaseDuration = getIntervalPauseDuration() - (now - getIntervalPhaseBeginTime());
      if (remaingingPhaseDuration > 0) {
        int32_t remaingingBlipDelay = INTERVAL_PAUSE_BLIP_OFF_DURATION_MS - (now - getLastPauseBlipTime());
        if (remaingingBlipDelay < 0) {
          remaingingBlipDelay = 0;
        }
        if (remaingingPhaseDuration <= remaingingBlipDelay) {
          interruptibleDelay(remaingingPhaseDuration);
        } else {
          if( ! interruptibleDelay(remaingingBlipDelay)) {
            showPauseBlip();
          }
        }
      } else {
        handleStateTransition(INTERVAL_PHASE_ENDED);
      }
      break;
  }
}

// returns true if interrupted
boolean interruptibleDelay(uint32_t duration) { 
  uint32_t start = millis();
  interruptCause = EVENT_NONE;
  do {
    waitForInterrupt();
    if (interruptCause != EVENT_NONE) {
      return true;
    } 
  } while (millis() - start < duration);
  return false;
}

ISR (INT0_vect) {       // Interrupt service routine for INT0 on PB2
  delay(SWITCH_DEBOUNCE_WAIT_MS);
  if (updateFanModeFromInputPins()) {
    interruptCause = MODE_CHANGED;
    handleStateTransition(MODE_CHANGED);
  }
}

ISR (PCINT0_vect) {       // Interrupt service routine for Pin Change Interrupt Request 0
  delay(SWITCH_DEBOUNCE_WAIT_MS); 
  if (updateFanModeFromInputPins()) {
    interruptCause = MODE_CHANGED;
    handleStateTransition(MODE_CHANGED);
  }
}

ISR (PCINT2_vect) {       // Interrupt service routine for Pin Change Interrupt Request 2
  delay(SWITCH_DEBOUNCE_WAIT_MS);
  if (updateFanIntensityFromInputPins()) {
    interruptCause = INTENSITY_CHANGED;
    handleStateTransition(INTENSITY_CHANGED);
  }
}


#if defined(__AVR_ATmega328P__)

  #include "LowPower.h"
  
  void configPowerSave() {
   // Taken care of by LowPower library, see below
  }
  
  void waitForInterrupt() { 
    digitalWrite(SLEEP_LED_OUT_PIN, HIGH);
    #ifdef VERBOSE
      delay(isPwmActive() ? 100 : 400);
    #else
      uint32_t start = millis();
      if (isPwmActive()) {
        period_t period = getFanState() == FAN_STEADY ? SLEEP_4S : SLEEP_120MS;
        LowPower.idle(period, ADC_OFF, TIMER2_ON /*PWM */, TIMER1_OFF , TIMER0_OFF, SPI_OFF, USART0_SERIAL, TWI_OFF);
      } else {
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      }
      if (millis() - start < 50) {
        delay(50); // wait so we have a flashing LED on rapid short sleeps
      }
    #endif
    digitalWrite(SLEEP_LED_OUT_PIN, LOW);
    delay(50); // wait so we have a flashing LED on rapidly successive sleeps
  }

#elif defined(__AVR_ATtiny85__)
  
  void configPowerSave() {
    #if defined(__AVR_ATmega328P__) && ! defined(VERBOSE)
      power_usart0_disable();
    #endif
    #if defined(__AVR_ATtiny85__)
      power_usi_disable(); 
    #endif
    
    ADCSRA &= ~_BV(ADEN);   // Disable ADC --> saves 320 µA on ATtiny85
    ACSR   |=  _BV(ACD);
    
    cli();                  // Stop interrupts to ensure the BOD timed sequence executes as required
    sleep_bod_disable();    // Brown-out disable
    sei();                  // Otherwise we will not wake up again
  }
  
  
  void waitForInterrupt() { 
    sleep_enable();
    
    if (isPwmActive()) {
      // We require Timer1 for PWM --> IDLE
      MCUCR &= ~(_BV(SM1) | _BV(SM0));  // Sleep mode = IDLE --- > any INT0 level-change will trigger an interrupt
      
  //    power_timer0_disable();
  //    power_timer1_disable();
      
    } else {
      MCUCR |=  _BV(SM1);               // Sleep mode = POWER_DOWN --> REQUIRES INT0 low-level
    }
    
    #ifdef VERBOSE
      digitalWrite(SLEEP_LED_OUT_PIN, HIGH);
    #endif
    
    sleep_cpu();                        // Controller stops executing HERE  #ifdef VERBOSE
  
    #ifdef VERBOSE
      delay(50); // so we see LED blink between short sleeps
      digitalWrite(SLEEP_LED_OUT_PIN, LOW);
      delay(50);
    #endif
    sleep_disable();
  //  power_timer0_enable();
  }

#endif
