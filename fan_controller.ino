//#define F_CPU 128000UL                  // Defaults to 1 MHz

//#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>

#include "fan_io.h"
#include "fan_control.h"

const uint32_t INTERVAL_FAN_ON_DURATION_MS = (uint32_t) INTERVAL_FAN_ON_DURATION * 1000; // [ms]

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
      blockingSleep();    // --> wait for interrupt
//      delay(10000);
      break;
      
    case FAN_SPEEDING_UP:
      // When transistioning from OFF or STEADY, duty value is set to minimum -> let fab speed up to this first -> delay first
      delay(SPEED_TRANSITION_CYCLE_DURATION_MS);
      speedUp();
      break;
      
    case FAN_STEADY:
      if (getFanMode() == MODE_INTERVAL) {
        // sleep until active phase is over
        int32_t remaingingPhaseDuration = INTERVAL_FAN_ON_DURATION_MS - (now - getIntervalPhaseBeginTime());
        if (remaingingPhaseDuration > 0) {
          delay(remaingingPhaseDuration);
        } else {
          handleStateTransition(INTERVAL_PHASE_ENDED);
        }
      } else {
        blockingSleep();    // --> wait for interrupt
//        delay(10000);
      }
      break;
      
    case FAN_SLOWING_DOWN:
      slowDown();
      delay(SPEED_TRANSITION_CYCLE_DURATION_MS);
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
          delay(remaingingPhaseDuration);
        } else {
          delay(remaingingBlipDelay);
          showPauseBlip();
        }
      } else {
        handleStateTransition(INTERVAL_PHASE_ENDED);
      }
      break;
  }
}

ISR (INT0_vect) {       // Interrupt service routine for INT0 on PB2
  if (updateFanModeFromInputPins()) {
    handleStateTransition(MODE_CHANGED);
  }
}

ISR (PCINT0_vect) {       // Interrupt service routine for Pin Change Interrupt Request 0
  if (updateFanModeFromInputPins()) {
    handleStateTransition(MODE_CHANGED);
  }
}

ISR (PCINT2_vect) {       // Interrupt service routine for Pin Change Interrupt Request 2
  if (updateFanIntensityFromInputPins()) {
    handleStateTransition(INTENSITY_CHANGED);
  }
}


void configPowerSave() {
  #if defined(__AVR_ATmega328P__) && ! defined(VERBOSE)
    power_usart0_disable();
  #endif
  #if defined(__AVR_ATtiny85__)
    power_usi_disable(); 
  #endif
  
  ADCSRA &= ~_BV(ADEN);   // Disable ADC --> saves 320 µA
  ACSR   |=  _BV(ACD);
  
  cli();                  // Stop interrupts to ensure the BOD timed sequence executes as required
  sleep_bod_disable();    // Brown-out disable
  sei();                  // Otherwise we will not wake up again
}


void blockingSleep() { 
  sleep_enable();
  
  if (isPwmActive()) {
    // We require Timer1 for PWM --> IDLE
    MCUCR &= ~(_BV(SM1) | _BV(SM0));  // Sleep mode = IDLE --- > any INT0 level-change will trigger an interrupt
    
//    power_timer0_disable();
//    power_timer1_disable();
    
  } else {
    MCUCR |=  _BV(SM1);               // Sleep mode = POWER_DOWN --> REQUIRES INT0 low-level
  }
  
  #if defined(__AVR_ATmega328P__) // cannot use Serial.println because this generates interrupts that wake up the CPU
    digitalWrite(SLEEP_LED_OUT_PIN, HIGH);
  #endif
  
  sleep_cpu();                        // Controller stops executing HERE  #ifdef VERBOSE

  #if defined(__AVR_ATmega328P__)
    delay(50); // so we see LED blink between short sleeps
    digitalWrite(SLEEP_LED_OUT_PIN, LOW);
    delay(50);
  #endif
  sleep_disable();
//  power_timer0_enable();
}
