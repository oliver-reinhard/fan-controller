//#define F_CPU 1000000UL                  // ATmega 328: Defaults to 16 MHz
//#define F_CPU 128000UL                  // Defaults to 16 MHz

#include "fan_io.h"
#include "low_power.h"
#include "wdt_time.h"
#include "fan_control.h"

//
//  #define VERBOSE --> see fan_io.h
//

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
  configWatchdogTime();

  delay(2000);
  flashLED(STATUS_LED_OUT_PIN, 6);
  delay(2000);
  
  initFanControl();

  #ifdef VERBOSE
    // Setup Serial Monitor
    Serial.begin(38400);
    
    Serial.print("F_CPU: ");
    Serial.println(F_CPU);
    Serial.print("Fan out max: ");
    Serial.println(ANALOG_OUT_MAX);
    Serial.print("Fan out low threshold: ");
    Serial.println(FAN_OUT_LOW_THRESHOLD);
    #define USART0_SERIAL USART0_ON
  #else
    #define USART0_SERIAL USART0_OFF
  #endif
}


void loop() {
  time32_s_t now = wdtTime_s();
  // Serial.print("loop: time : ");
  // Serial.println(now);

  switch(getFanState()) {
    case FAN_OFF:
      waitForUserInput();  // blocking wait
      break;
      
    case FAN_SPEEDING_UP:
      // When transistioning from OFF or STEADY, duty value is set to minimum -> let fan speed up to this first -> delay first
      if (! delayInterruptible_millis(SPEED_TRANSITION_CYCLE_DURATION_MS)) {
        speedUp();
      }
      break;
      
    case FAN_STEADY:
    {
      if (getFanMode() == MODE_INTERVAL) {
        // sleep until active phase is over
        duration16_s_t remainingPhaseDuration = INTERVAL_FAN_ON_DURATION - (now - getIntervalPhaseBeginTime());  // can be < 0
        // Serial.print("remainingPhaseDuration: ");
        // Serial.print(remainingPhaseDuration);
        // Serial.print(", now: ");
        // Serial.print(now);
        // Serial.print(", getIntervalPhaseBeginTime: ");
        // Serial.println(getIntervalPhaseBeginTime());
        // Serial.flush();
        if (remainingPhaseDuration > 0) {
          delayInterruptible_seconds(remainingPhaseDuration);
        } else {
          handleStateTransition(INTERVAL_PHASE_ENDED);
        }
      } else {
        waitForUserInput();
      }
    }
    break;
      
    case FAN_SLOWING_DOWN:
      slowDown();
      delayInterruptible_millis(SPEED_TRANSITION_CYCLE_DURATION_MS);
      break;
      
    case FAN_PAUSING:
    {
      // sleep until next LED flash or until pause is over (whatever will happen first)
      duration16_s_t remainingPhaseDuration = getIntervalPauseDuration() - (now - getIntervalPhaseBeginTime());  // can be < 0
        // Serial.print("remainingPhaseDuration: ");
        // Serial.print(remainingPhaseDuration);
        // Serial.print(", now: ");
        // Serial.print(now);
        // Serial.print(", getIntervalPhaseBeginTime: ");
        // Serial.println(getIntervalPhaseBeginTime());
      if (remainingPhaseDuration > 0) {
        duration16_s_t remaingingBlipDelay = INTERVAL_PAUSE_BLIP_OFF_DURATION_S - (now - getLastPauseBlipTime());  // can be < 0
          // Serial.print("remaingingBlipDelay: ");
          // Serial.print(remaingingBlipDelay);
          // Serial.print(", getLastPauseBlipTime: ");
          // Serial.println(getLastPauseBlipTime());
          // Serial.flush();
        if (remaingingBlipDelay < 0) {
          remaingingBlipDelay = 0;
        }
        if (remainingPhaseDuration <= remaingingBlipDelay) {
          delayInterruptible_seconds(remainingPhaseDuration);
        } else {
          if( ! delayInterruptible_seconds(remaingingBlipDelay)) {
            showPauseBlip();
            resetPauseBlip();
          }
        }
      } else {
        handleStateTransition(INTERVAL_PHASE_ENDED);
      }
    }
    break;

    default:
      // Serial.println("loop: default case");
      break;
  }
  // Serial.println("end loop");
}
