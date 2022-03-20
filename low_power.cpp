
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "low_power.h"
#include "fan_io.h"

typedef uint8_t watchdog_timeout_t;
  
const watchdog_timeout_t WATCHDOG_TIMEOUT = WDTO_4S; // see wdt.h
const time16_ms_t WATCHDOG_TIMEOUT_MS = WATCHDOG_TIMEOUT > 0 ? (16 << WATCHDOG_TIMEOUT) : 16; // [ms]

//volatile time32_ms_t lowPowerPhaseMillis = 0; // [ms]    // time incremented by watchdog if timers are asleep
volatile time32_ms_t systemMillis;
volatile time32_ms_t systemMillisCorrection = 4294967295 - 38000; // [ms]
//volatile time32_ms_t systemMillisCorrection = 0; // [ms]

void updateTime();

#if defined(__AVR_ATmega328P__)
  
  void configLowPower() {
    ADCSRA &= ~(1 << ADEN);
    power_adc_disable();
    power_spi_disable();
    #ifndef VERBOSE
      power_usart0_disable();
    #endif
    power_twi_disable();
    
//    power_timer0_disable(); // cannot disable, required for delay() function
    power_timer1_disable();
//    power_timer2_disable(); // cannot disable, required for PWM output

//  updateTime();
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

#endif

//
// FUNCTIONS
//
void enterSleep(watchdog_timeout_t timeout);
 
static inline watchdog_timeout_t mapToTimeout(time32_ms_t duration) {
  if (duration >= 4000) {
    return WDTO_4S;
  } else if (duration >= 1000) {
    return WDTO_1S;
  } else if (duration >= 120) {
    return WDTO_120MS;
  }
  return WDTO_15MS;
  
}

// returns true if interrupted
boolean delayInterruptible(time32_ms_t duration) { 
  time32_ms_t endAt = sleeplessMillis() + duration;
  duration32_ms_t remaining = duration;  // allow this to be negative!
  resetInputInterrupt();
  
  while (remaining > 0) {
    enterSleep(mapToTimeout(remaining));
//    invertStatusLED();  // use to debug watchdog / interrupt problems
    if (isInputInterrupt()) {
      return true;
    } 
    remaining = endAt - sleeplessMillis();
  }
  return false;
}

void waitForInterrupt() {
  enterSleep(WATCHDOG_TIMEOUT);
}

static inline time32_ms_t millisWithWrapAround() {
  time32_ms_t now = millis();
  if (systemMillis + systemMillisCorrection > now + systemMillisCorrection) {   // millis() has just "wrapped around" from UNIT_MAX to 0 (unit32 overflow)
    systemMillisCorrection = -now;  // make result of sleeplessMillis start from 0.
  }
  systemMillis = now;
  return now;
}

time32_ms_t sleeplessMillis() {
//  if (_WD_CONTROL_REG & _BV(WDE)) {  // watchdog is enabled
//    return systemMillis + systemMillisCorrection;
//  }
  return millisWithWrapAround() + systemMillisCorrection; // ensure time never jumps "back"
}

//void updateTime() {
//  time32_ms_t now = millisWithWrapAround();
//  if (now >= lowPowerPhaseMillis) {
//    lowPowerPhaseMillis = now;
//  } else {
//    systemMillisCorrection = lowPowerPhaseMillis - now;
//  } 
//}

ISR (WDT_vect) {
  // wake up MCU
  _WD_CONTROL_REG |= _BV(WDIE);  // do not delete this line --> watchdog would reset MCU at next interrupt
  systemMillisCorrection += WATCHDOG_TIMEOUT_MS;
   Serial.print("Watchdog: ");
    Serial.println(WATCHDOG_TIMEOUT_MS);
  #ifdef VERBOSE
    invertStatusLED();
    delay(50);
    invertStatusLED();
  #endif
}

void enterSleep(watchdog_timeout_t timeout) {    
  digitalWrite(SLEEP_LED_OUT_PIN, HIGH);
  time32_ms_t start = sleeplessMillis();
  
  cli();
  
  if (isPwmActive()) {
    // We require Timer2 to stay active for PWM --> IDLE
    set_sleep_mode(SLEEP_MODE_IDLE);
  } else {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
//    updateTime();
  }
  
  // setup a watchdog to wake MCU after the given timeout:
  wdt_enable(timeout); 
  _WD_CONTROL_REG |= _BV(WDIE);
  
  sleep_enable();
  sei();
  sleep_cpu();      // Controller waits for interrupt here
  sleep_disable();
  
  #if defined(__AVR_ATtiny85__)
    MCUSR &= ~_BV(WDRF); // see commend in ATtiny85 Datasheet, p.46, Note under "Bit 3 – WDE: Watchdog Enable" 
  #endif
  wdt_disable();
  
  if (sleeplessMillis() - start < 50) {
    delay(50); // wait so we have a flashing LED on rapid short sleeps
  }
  digitalWrite(SLEEP_LED_OUT_PIN, LOW);
  delay(50); // wait so we have a flashing LED on rapidly successive sleeps
}
