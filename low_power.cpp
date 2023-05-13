#include <wiring.c>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>

#include "low_power.h"
#include "wdt_time.h"
#include "fan_io.h"

#if defined(__AVR_ATmega328P__)
  
  void configLowPower() {
    ADCSRA &= ~(1 << ADEN);
    power_adc_disable();
    power_spi_disable();
    #ifndef VERBOSE
      power_usart0_disable();
    #endif
    power_twi_disable();
    
//    power_timer0_disable(); // cannot disable, required for millis() function
    power_timer1_disable();
//    power_timer2_disable(); // cannot disable, required for PWM output
  }

#elif defined(__AVR_ATtiny85__)
  
  void configLowPower() {
    power_usi_disable(); 
    
    ADCSRA &= ~_BV(ADEN);   // Disable ADC --> saves 320 µA on ATtiny85
    ACSR   |=  _BV(ACD);
    power_adc_disable();
    
    cli();                  // Stop interrupts to ensure the BOD timed sequence executes as required
    sleep_bod_disable();    // Brown-out disable
    sei();
  }

#endif

//
// FUNCTIONS
//

void enterSleep();


/* 
 * Power-Saving delay that is canceled prematurely by user-input interrupts.
 * 
 * returns true if interrupted (i.e. cut short) or false, if not interrupted
 */
boolean delayInterruptible_millis(time16_ms_t duration) {
  delay(duration);
  return false;
}

boolean delayInterruptible_seconds(time16_s_t duration) { 
  time32_s_t now = wdtTime_s();
  time32_s_t delayUntil = now + duration;
  while (now < delayUntil) {
    enterSleep();
    // invertStatusLED();  // use to debug watchdog / interrupt problems ///////////
    if (isInputInterrupt()) {
      return true;
    } 
    now = wdtTime_s();
  }
  // resetInputInterrupt();
  // setTimer2_seconds(duration);
  
  // while (timer2CountingDown()) {
  //   enterSleep();
  //   invertStatusLED();  // use to debug watchdog / interrupt problems ///////////
  //   if (isInputInterrupt()) {
  //     stopTimer2();
  //     return true;
  //   } 
  // }
  return false;
}

void waitForUserInput() {
  enterSleep(); // wait for watchdog interrupt or user interrupt
}


void enterSleep() {    
  digitalWrite(SLEEP_LED_OUT_PIN, HIGH);
  
  cli();
  
  if (isPwmActive()) {
    // We require Timer2 to stay active for PWM --> IDLE
    set_sleep_mode(SLEEP_MODE_IDLE);
  } else {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  }
  
  sleep_enable();
  sei();
  sleep_cpu();      // Controller waits for interrupt here
  sleep_disable();
  
//  if (sleeplessMillis() - start < 50) {
//    delay(50); // wait so we have a flashing LED on rapid short sleeps
//  }
  digitalWrite(SLEEP_LED_OUT_PIN, LOW);
//  delay(50); // wait so we have a flashing LED on rapidly successive sleeps
}

