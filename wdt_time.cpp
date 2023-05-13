#include <avr/wdt.h>
#include <avr/power.h>

#include "wdt_time.h"


typedef uint8_t watchdog_timeout_t;
  
const watchdog_timeout_t WATCHDOG_TIMEOUT = WDTO_1S;  // see wdt.h
const time32_s_t WATCHDOG_TIMEOUT_S = 1;              // [s], depends on previous constant

volatile time32_s_t  time_s = 0;


void configWatchdogTime() {   
  cli();                  // Stop interrupts
  // Setup a watchdog to wake MCU after the given timeout:
  wdt_enable(WATCHDOG_TIMEOUT); 
  _WD_CONTROL_REG |= _BV(WDIE);
  
  #if defined(__AVR_ATtiny85__)
    MCUSR &= ~_BV(WDRF); // see comment in ATtiny85 Datasheet, p.46, Note under "Bit 3 – WDE: Watchdog Enable" 
  #endif

  sei();
}

ISR (WDT_vect) {
  // wake up MCU
  _WD_CONTROL_REG |= _BV(WDIE);  // do not delete this line --> watchdog would reset MCU at next interrupt
  time_s += WATCHDOG_TIMEOUT_S;
}


time32_s_t wdtTime_s() {
  // Copied from millis() implementation in https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/wiring.c
  time32_s_t sec;
  uint8_t oldSREG = SREG;

  // disable interrupts while we read time_s or we might get an inconsistent value (e.g. in the middle of a write to systemSeconds)
  cli();
  sec  = time_s;
  SREG = oldSREG;
  sei();
  return sec;
}

  
void enableArduinoTimer0() {
  #if defined(__AVR_ATtiny85__)
    TCNT0   = 0;
    TIMSK0 |= _BV(TOIE0);             // Enable overflow interrupt
  #endif
  power_timer0_enable();
}

void disableArduinoTimer0() {
  #if defined(__AVR_ATtiny85__)
    TIMSK0 &= ~ _BV(TOIE0);           // Disable overflow interrupt
  #endif
  power_timer0_disable();
}