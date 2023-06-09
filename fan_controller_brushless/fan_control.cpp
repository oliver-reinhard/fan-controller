#include <Arduino.h> 
#include <avr/sleep.h>

#include <scheduler.h>
#include <wdt_scheduler.h>
#include <blink_task.h>
#include "log_io.h"
#include "fan_control.h"
#define MAX_TASK_GROUPS_OVERRIDE 5

const TaskGroup MODE_CHANGED_GROUP = 1;
const TaskGroup INTENSITY_CHANGED_GROUP = 2;
const TaskGroup SPEED_TRANSITION_GROUP = 3;
const TaskGroup INTERVAL_GROUP = 4;
const TaskGroup PAUSE_BLINK_GROUP = 5;
const TaskGroup TASK_GROUPS[MAX_TASK_GROUPS_OVERRIDE] = {MODE_CHANGED_GROUP, INTENSITY_CHANGED_GROUP, SPEED_TRANSITION_GROUP, INTERVAL_GROUP, PAUSE_BLINK_GROUP};

// forward declaration:
void handleStateTransition(Event event);

class FanScheduler : public WatchdogTimerBasedScheduler {
  public:
    FanScheduler(const TaskGroup groups[], const uint8_t groupCount) : WatchdogTimerBasedScheduler(groups, groupCount) { }
  
  protected:
    WdtSleepMode sleepMode() { 
      // cannot turn MCU off while fan is running at other than 100% duty cycle because timer is needed for PWM:
      return logicalIO()->isPwmActive() ? SLEEP_MODE_IDLE : SLEEP_MODE_PWR_DOWN;
    }
};

class ModeChangedTask : public AbstractTask {
  public:
    TaskGroup group() { return MODE_CHANGED_GROUP; }
    const char *name() { return "Mode"; }
    void action() {
      handleStateTransition(MODE_CHANGED);
    }
};

class IntensityChangedTask : public AbstractTask {
  public:
    TaskGroup group() { return INTENSITY_CHANGED_GROUP; }
    const char *name() { return "Intensity"; }
    void action() {
      handleStateTransition(INTENSITY_CHANGED);
    }
};

class IntervalModeTask : public BlinkTask {
  public:
    IntervalModeTask() : BlinkTask (INTERVAL_GROUP, 0 /* not used */) { };
    const char *name() { return "Interval"; }

   protected:
      virtual void onAction()  { 
        handleStateTransition(INTERVAL_PHASE_ENDED);
      }
      virtual void offAction() { 
        handleStateTransition(INTERVAL_PHASE_ENDED);
      }
};

// 
// Singleton instances
//
FanScheduler FAN_SCHEDULER = FanScheduler(TASK_GROUPS, MAX_TASK_GROUPS_OVERRIDE);

BlinkTask SPEED_TRANSITION_BLINKER = BlinkTask(SPEED_TRANSITION_GROUP, STATUS_LED_OUT_PIN, 6);
BlinkTask PAUSE_BLINKER = BlinkTask(PAUSE_BLINK_GROUP, STATUS_LED_OUT_PIN); // infinite (= runs until canceled)
IntervalModeTask INTERVAL_MODE_TASK = IntervalModeTask(); // infinite (= runs until canceled)
ModeChangedTask MODE_CHANGED_TASK = ModeChangedTask();
IntensityChangedTask INTENSITY_CHANGED_TASK = IntensityChangedTask();

volatile FanState fanState = FAN_OFF;          // current fan state

//
// FUNCTIONS
//
void controllerLoop() { FAN_SCHEDULER.loop();  }

//
// Used as interrupt handlers => becomes a task factory
//
void scheduleModeChangeTask() {
  AbstractTask* speedTransition = FAN_SCHEDULER.taskForGroup(SPEED_TRANSITION_GROUP);
  if (speedTransition != NULL) {
    // a speed transition is underway => process update when it's done
    FAN_SCHEDULER.scheduleTask(& MODE_CHANGED_TASK, speedTransition->timeToCompletion());
  } else {
    FAN_SCHEDULER.scheduleTaskNow(& MODE_CHANGED_TASK);
  }
}

//
// Used as interrupt handlers => becomes a task factory
//
void scheduleIntensityChangedTask() {
  AbstractTask* speedTransition = FAN_SCHEDULER.taskForGroup(SPEED_TRANSITION_GROUP);
  if (speedTransition != NULL) {
    // a speed transition is underway => process update when it's done
    FAN_SCHEDULER.scheduleTask(& INTENSITY_CHANGED_TASK, speedTransition->timeToCompletion());
  } else {
    FAN_SCHEDULER.scheduleTaskNow(& INTENSITY_CHANGED_TASK);
  }
}

// Applicable only in mode CONTINUOUS
FanSpeed mapToFanSpeed(FanIntensity intensity) {
  switch(intensity) {
    case INTENSITY_HIGH:    return SPEED_FULL;
    case INTENSITY_MEDIUM:  return SPEED_MEDIUM;
    default:                return SPEED_MIN;
  }
}

// Applicable only in mode INTERVAL
// Returns [s]
time16_s_t mapToIntervalPauseDuration(FanIntensity intensity) {
  switch(intensity) {
    case INTENSITY_HIGH:    return INTERVAL_PAUSE_SHORT_DURATION; // [s]
    case INTENSITY_MEDIUM:  return INTERVAL_PAUSE_MEDIUM_DURATION; // [s]
    default:                return INTERVAL_PAUSE_LONG_DURATION; // [s]
  }
}
  
#ifdef VERBOSE
  const char* fanStateName(FanState state) {
    switch (state) {
      case FAN_OFF: return "OFF";
      case FAN_ON: return "ON";
      case FAN_PAUSING: return "PAUSE";
      default: return "?";
    }
  }
  
  const char* eventName(Event event) {
    switch (event) {
      case EVENT_NONE: return "NONE";
      case MODE_CHANGED: return "Mode changed";
      case INTENSITY_CHANGED: return "Intensity changed";
      case INTERVAL_PHASE_ENDED: return "Phase ended";
      default: return "?";
    }
  }
#endif

void updateIntervalModeTaskPause() {
  duration16_s_t duration =  mapToIntervalPauseDuration(logicalIO()->fanIntensity());
  PAUSE_BLINKER.delays(INTERVAL_FAN_ON_DURATION*D_1S, duration*D_1S);
}
void animateTransition() {
  FAN_SCHEDULER.scheduleTaskNow(& SPEED_TRANSITION_BLINKER);
}

void fanOn(FanMode mode) {
  animateTransition();
  if (mode == MODE_CONTINUOUS) {
    logicalIO()->fanSpeed(mapToFanSpeed(logicalIO()->fanIntensity()));
  } else { /* getFanMode() == MODE_INTERVAL */
    logicalIO()->fanSpeed(SPEED_FULL);
  }
}

void fanOff() {
  animateTransition();
  logicalIO()->fanSpeed(SPEED_OFF);
}

void handleStateTransition(Event event) {
  if (event == EVENT_NONE) {
    return;
  }
  #ifdef VERBOSE
    FanState beforeState = fanState;
  #endif
  
  switch(fanState) {
    
    case FAN_OFF:
      switch(event) {
        case MODE_CHANGED: 
          if (logicalIO()->fanMode() == MODE_CONTINUOUS) {
            fanState = FAN_ON;
            fanOn(MODE_CONTINUOUS);
          } else if (logicalIO()->fanMode() == MODE_INTERVAL) {
            fanState = FAN_PAUSING;
            updateIntervalModeTaskPause();
            FAN_SCHEDULER.scheduleTaskNow(& INTERVAL_MODE_TASK);
          }
          break;
          
        case INTENSITY_CHANGED: break;
          // noop: new value has been recorded in getFanIntensity(), will take effect on next mode change (we are in OFF at this time)
        default: break;
      }
      break;
      
    case FAN_ON: 
      switch(event) {
        case MODE_CHANGED: 
          if (logicalIO()->fanMode() == MODE_CONTINUOUS) {
            FAN_SCHEDULER.cancelTask(& INTERVAL_MODE_TASK);
          } else if (logicalIO()->fanMode() == MODE_INTERVAL) {
            fanState = FAN_PAUSING;
            updateIntervalModeTaskPause();
            FAN_SCHEDULER.scheduleTaskNow(& INTERVAL_MODE_TASK);
          } else { // newMode == MODE_OFF
            FAN_SCHEDULER.cancelTask(& INTERVAL_MODE_TASK);
            fanState = FAN_OFF;
            fanOff();
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (logicalIO()->fanMode() == MODE_CONTINUOUS) {
            animateTransition();
            logicalIO()->fanSpeed(mapToFanSpeed(logicalIO()->fanIntensity()));
          } else if (logicalIO()->fanMode() == MODE_INTERVAL) {
            /// ANIMATE ???????????? => extra BLINKER
            updateIntervalModeTaskPause();
          }
          break;

        case INTERVAL_PHASE_ENDED:
          fanState = FAN_PAUSING;
          fanOff();
          break;
          
        default:
          break;
      }
      break;
      
    case FAN_PAUSING:
      switch(event) {
        case MODE_CHANGED:
          if (logicalIO()->fanMode() == MODE_OFF) {
            FAN_SCHEDULER.cancelTask(& INTERVAL_MODE_TASK);
            fanState = FAN_OFF;
          } else if (logicalIO()->fanMode() == MODE_CONTINUOUS) {
            FAN_SCHEDULER.cancelTask(& INTERVAL_MODE_TASK);
            fanState = FAN_ON;
            fanOn(MODE_CONTINUOUS);
          }
          break;
          
        case INTENSITY_CHANGED: 
          {
            SDuration originalDelay = INTERVAL_MODE_TASK.originalDelay();
            SDuration waited = originalDelay - (INTERVAL_MODE_TASK.dueTime() - now());
            updateIntervalModeTaskPause();
            if (waited > INTERVAL_MODE_TASK.offDuration()) { // pause is over
              FAN_SCHEDULER.rescheduleTask(& INTERVAL_MODE_TASK, 0); // = now
            } else {
              FAN_SCHEDULER.rescheduleTask(& INTERVAL_MODE_TASK, INTERVAL_MODE_TASK.offDuration() - waited);
            }
          }
          break;

        case INTERVAL_PHASE_ENDED:
          fanState = FAN_ON;
          fanOn(MODE_INTERVAL);
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

void initFanControl() {
  // Install input-change handlers (= assign function pointers)
  logicalIO()->modeChangedHandler = scheduleModeChangeTask;
  logicalIO()->intensityChangedHandler = scheduleIntensityChangedTask; 

  SPEED_TRANSITION_BLINKER.delays(D_500MS, D_500MS);
  PAUSE_BLINKER.delays(D_250MS, 30*D_1S);  // will be stopped at pause end
  updateIntervalModeTaskPause();

  logicalIO()->init();  // this causes the first tasks to be created for intensity and mode change

  if (logicalIO()->fanMode() != MODE_OFF) {
    handleStateTransition(MODE_CHANGED);
  }
}

