#include <avr/wdt.h>
#include <avr/power.h>

#include "time.h"


typedef uint8_t watchdog_timeout_t;
  
const watchdog_timeout_t WATCHDOG_TIMEOUT = WDTO_1S;  // see wdt.h
const time32_s_t WATCHDOG_TIMEOUT_S = 1;              // [s], depends on previous constant

volatile time32_s_t  time_s = 0;


void configTime() {   
  cli();                  // Stop interrupts
  // Setup a watchdog to wake MCU after the given timeout:
  wdt_enable(WATCHDOG_TIMEOUT); 
  _WD_CONTROL_REG |= _BV(WDIE);
  
  #if defined(__AVR_ATtiny85__)
    MCUSR &= ~_BV(WDRF); // see commend in ATtiny85 Datasheet, p.46, Note under "Bit 3 – WDE: Watchdog Enable" 
  #endif
  
  enableArduinoTimer0();
  sei();
}

ISR (WDT_vect) {
  // wake up MCU
  _WD_CONTROL_REG |= _BV(WDIE);  // do not delete this line --> watchdog would reset MCU at next interrupt
  time_s += WATCHDOG_TIMEOUT_S;
}


time32_s_t _time_s() {
  // Copied from millis() implementation in https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/wiring.c
  time32_s_t sec;
  uint8_t oldSREG = SREG;

  // disable interrupts while we read time_s or we might get an
  // inconsistent value (e.g. in the middle of a write to systemSeconds)
  cli();
  sec= time_s;
  SREG = oldSREG;
  return sec;
}
  

void enableArduinoTimer0() {
  TCNT0   = 0;
  TIMSK0 |= _BV(TOIE0);             // Enable overflow interrupt
  power_timer0_enable();
}

void disableArduinoTimer0() {
  TIMSK0 &= ~ _BV(TOIE0);           // Disable overflow interrupt
  power_timer0_disable();
}




//
// TIMER2 = interruptible timer at millisecond precision, only active when in action
//
  
const uint16_t TIMER2_PRESCALER = 1024;
const uint16_t TIMER2_TICKS_PER_CYCLE = 256;

//
// F_CPU / 1000 == CPU clock ticks per millisecond
// x == prescaled clock ticks per millisecond
//
//    F_CPU / 1000 = TIMER2_PRESCALER * x
//    x = F_CPU / TIMER2_PRESCALER / 1000 
//
const uint16_t TIMER2_TICKS_PER_SECOND =  F_CPU / TIMER2_PRESCALER;                    // = ticks per 1000 milliseconds
const uint16_t TIMER2_CYCLES_PER_SECOND =  TIMER2_TICKS_PER_SECOND / TIMER2_TICKS_PER_CYCLE;
const uint16_t TIMER2_MILLIS_PER_CYCLE =  1000L * TIMER2_TICKS_PER_CYCLE / TIMER2_TICKS_PER_SECOND;  // = 1000 / (TIMER2_TICKS_PER_SECOND / TIMER2_TICKS_PER_CYCLE)
// Increase precision for cycle computation --> multiply by 10:
const uint16_t TIMER2_MILLIS_PER_CYCLE_x10 =  10L * 1000L * TIMER2_TICKS_PER_CYCLE / TIMER2_TICKS_PER_SECOND;  
const uint16_t TIMER2_TICKS_PER_MILLI =  TIMER2_TICKS_PER_CYCLE / TIMER2_MILLIS_PER_CYCLE;

const uint16_t TIMER2_MILLIS_ERROR_PER_SECOND =  1000 - TIMER2_CYCLES_PER_SECOND * TIMER2_MILLIS_PER_CYCLE_x10 / 10;

volatile uint16_t timer2_ovfCycles;
volatile uint8_t  timer2_ctcRemainingCount;
volatile boolean  timer2_active = false;


void startTimer2_OVF(uint16_t cycles);
void startTimer2_CTC(uint8_t top);
void stopTimer2();

/*
 * Note: 16-bit range (up to 65 seconds)
 */
void setTimer2_millis(time16_ms_t ms) {
  uint32_t ms_x10 = ms * 10;
  uint16_t ovfCycles = (ms_x10 / TIMER2_MILLIS_PER_CYCLE_x10);
  uint8_t  ctcRemainingCount = (ms_x10 % TIMER2_MILLIS_PER_CYCLE_x10) * TIMER2_TICKS_PER_CYCLE / TIMER2_MILLIS_PER_CYCLE_x10;
  
  if (timer2_ovfCycles > 0) {
    startTimer2_OVF(timer2_ovfCycles);
    timer2_ctcRemainingCount = ctcRemainingCount;
  } else {
    startTimer2_CTC(ctcRemainingCount);
  }
}

/*
 * Note: 16-bit range (up to 18 hours)
 */
void setTimer2_seconds(time16_s_t sec) {
  uint16_t ovfCycles = sec * TIMER2_CYCLES_PER_SECOND;
  uint8_t  ctcRemainingCount = 0;
  
  if (timer2_ovfCycles > 0) {
    startTimer2_OVF(ovfCycles);
  } else {
    stopTimer2();
  }
}

boolean timer2CountingDown() {
  return timer2_active;
}

// Generates overflow interrupts:
void startTimer2_OVF(uint16_t cycles) {
  timer2_ovfCycles = cycles;
  TCNT2  = 0;
  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);     // Clear & set prescaler = 1024
  TCCR2A  = _BV(WGM21);                           // Set CTC mode
  TCCR2A = 0B00000000;                            // Clear --> normal mode --> sets OVF flag
  TIFR2 |= _BV(TOV2);                             // Clear OVF interrupt flag (= set to 1)
  TIMSK2 = _BV(TOIE2);                            // Enable OVF interrupt, disable others
  
  power_timer2_enable();
}

/* 
 * Clear Timer on Compare Match (CTC) mode.
 * Generates compare interrupts.
*/
void startTimer2_CTC(uint8_t top) {
  TCNT2  = 0;
  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);     // Clear & set prescaler = 1024
  TCCR2A  = _BV(WGM21);                           // Set CTC mode
  OCR2A   = top;
  TIFR2 |= _BV(OCF2A);                            // Clear CTC interrupt flag (= set to 1)
  TIMSK2 = _BV(OCIE2A);                           // Enable CTC interrupt, disable others
  timer2_active = true;
}

void stopTimer2() {
  timer2_active = false;
  TCCR2B = 0B00000000;          // Stop timer by removing clock source
  TIMSK2 = 0B00000000;          // Disable all interrupts
  power_timer2_disable();
}

ISR (TIMER2_OVF_vect) {
  if (timer2_ovfCycles > 1) {
    timer2_ovfCycles--;
  } else {
    startTimer2_CTC(timer2_ctcRemainingCount);
  }
}

ISR (TIMER2_COMPA_vect) { 
  stopTimer2();
}
