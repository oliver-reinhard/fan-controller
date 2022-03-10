#define F_CPU 1000000UL                  // Defaults to 1 MHz
//#define F_CPU 128000UL                  // Defaults to 1 MHz

//#include <util/delay.h>
  
#include "fan_io.h"
#include "fan_control.h"
#include "low_power.h"

//
//  #define VERBOSE --> see fan_io.h
//

const time32_ms_t INTERVAL_FAN_ON_DURATION_MS = (time32_ms_t) INTERVAL_FAN_ON_DURATION * 1000; // [ms]

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
    Serial.begin(74880);
    
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
  time32_ms_t now = _millis();
//  Serial.print("loop: _millis: ");
//    Serial.println(now);

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
        int32_t remaingingPhaseDuration = INTERVAL_FAN_ON_DURATION_MS - (now - getIntervalPhaseBeginTime());  // can be < 0
  Serial.print("remaingingPhaseDuration: ");
    Serial.println(remaingingPhaseDuration);
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
  }
}


ISR (INT0_vect) {       // Interrupt service routine for INT0 on PB2
  debounceSwitch();
  handleModeChange();
}

ISR (PCINT0_vect) {       // Interrupt service routine for Pin Change Interrupt Request 0
  debounceSwitch();
  handleModeChange();
}

ISR (PCINT2_vect) {       // Interrupt service routine for Pin Change Interrupt Request 2
  debounceSwitch();
  if (updateFanIntensityFromInputPins()) {
    interruptCause = INTENSITY_CHANGED;
    handleStateTransition(INTENSITY_CHANGED);
  }
}

static inline void debounceSwitch() {
  delay(SWITCH_DEBOUNCE_WAIT_MS);
}
