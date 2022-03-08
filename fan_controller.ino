#define F_CPU 1000000UL                  // Defaults to 1 MHz
//#define F_CPU 128000UL                  // Defaults to 1 MHz

//#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
  
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
  configLowPower();

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
  updateWatchdog();
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

void handleModeChange() {
  if (updateFanModeFromInputPins()) {
    interruptCause = MODE_CHANGED;
    handleStateTransition(MODE_CHANGED);
    updateWatchdog();
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

void updateWatchdog() {
  if (getFanMode() == MODE_INTERVAL) {
    // setup a watchdog to wake MCU every 4 seconds so we can check for intervals
    wdt_enable(WDTO_4S); 
    _WD_CONTROL_REG |= _BV(WDIE);
  } else {
    #if defined(__AVR_ATtiny85__)
      MCUSR &= ~_BV(WDRF); // see commend in ATtiny85 Datasheet, p.46, Note under "Bit 3 – WDE: Watchdog Enable" 
    #endif
    wdt_disable();
  }
}

ISR (WDT_vect) {
  // wake up MCU
  _WD_CONTROL_REG |= _BV(WDIE);  // do not delete this line --> watchdog would reset MCU continuously
}

ISR (INT0_vect) {       // Interrupt service routine for INT0 on PB2
  delay(SWITCH_DEBOUNCE_WAIT_MS);
  handleModeChange();
}

ISR (PCINT0_vect) {       // Interrupt service routine for Pin Change Interrupt Request 0
  delay(SWITCH_DEBOUNCE_WAIT_MS); 
  handleModeChange();
}

ISR (PCINT2_vect) {       // Interrupt service routine for Pin Change Interrupt Request 2
  delay(SWITCH_DEBOUNCE_WAIT_MS);
  if (updateFanIntensityFromInputPins()) {
    interruptCause = INTENSITY_CHANGED;
    handleStateTransition(INTENSITY_CHANGED);
  }
}

#if defined(__AVR_ATmega328P__)
  
  void configLowPower() {
    ADCSRA &= ~(1 << ADEN);
    power_adc_disable();
    power_spi_disable();
    #ifndef VERBOSE
      power_usart0_disable();
    #endif
    power_twi_disable();
    
//    power_timer0_disable(); // required for delay() function
    power_timer1_disable();
//    power_timer2_disable(); // required for PWM output
  }
  
  void waitForInterrupt() {    
    digitalWrite(SLEEP_LED_OUT_PIN, HIGH);
    uint32_t start = millis();
    cli();
    
    if (isPwmActive()) {
      // We require Timer2 for PWM --> IDLE
      set_sleep_mode(SLEEP_MODE_IDLE);
    } else {
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    }
    sleep_enable();
    sei();
    sleep_cpu();                        // Controller stops executing HERE
    sleep_disable();
  
    if (millis() - start < 50) {
      delay(50); // wait so we have a flashing LED on rapid short sleeps
    }
    digitalWrite(SLEEP_LED_OUT_PIN, LOW);
    delay(50); // wait so we have a flashing LED on rapidly successive sleeps
  }


#elif defined(__AVR_ATtiny85__)
  
  void configLowPower() {
    #if defined(__AVR_ATmega328P__) && ! defined(VERBOSE)
      power_usart0_disable();
    #endif
    #if defined(__AVR_ATtiny85__)
      power_usi_disable(); 
    #endif
    
    ADCSRA &= ~_BV(ADEN);   // Disable ADC --> saves 320 µA on ATtiny85
    ACSR   |=  _BV(ACD);
    power_adc_disable();
    
    cli();                  // Stop interrupts to ensure the BOD timed sequence executes as required
    sleep_bod_disable();    // Brown-out disable
    sei();                  // Otherwise we will not wake up again
  }
  
  
  void waitForInterrupt() { 
    sleep_enable();
    
    if (isPwmActive()) {
      // We require Timer1 for PWM --> IDLE
      set_sleep_mode(SLEEP_MODE_IDLE);
//      MCUCR &= ~(_BV(SM1) | _BV(SM0));  // Sleep mode = IDLE --- > any INT0 level-change will trigger an interrupt
      
  //    power_timer0_disable();
  //    power_timer1_disable();
      
    } else {
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);
//      MCUCR |=  _BV(SM1);               // Sleep mode = POWER_DOWN --> REQUIRES INT0 low-level
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
