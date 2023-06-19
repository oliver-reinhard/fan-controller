#include <Arduino.h> 
#include <avr/sleep.h>
#include <scheduler.h>
#include <wdt_scheduler.h>
#include <blink_task.h>
#include "log_io.h"
#include "fan_control.h"

const TaskGroup MODE_CHANGED_GROUP = 1;
const TaskGroup INTENSITY_CHANGED_GROUP = 2;
const TaskGroup SPEED_TRANSITION_GROUP = 3;
const TaskGroup INTERVAL_GROUP = 4;
const TaskGroup PAUSE_SHOW_ALIVE_GROUP = 5;
#define NUM_TASK_GROUPS 5

#if NUM_TASK_GROUPS > MAX_SCHEDULER_TASK_GROUPS
 #error("The static Scheduler task group limit is MAX_SCHEDULER_TASK_GROUPS")
#endif
const TaskGroup TASK_GROUPS[NUM_TASK_GROUPS] = {MODE_CHANGED_GROUP, INTENSITY_CHANGED_GROUP, SPEED_TRANSITION_GROUP, INTERVAL_GROUP, PAUSE_SHOW_ALIVE_GROUP};

// forward declaration:
void handleStateTransition(Event event);


class FanScheduler : public WatchdogTimerBasedScheduler {
  public:
    FanScheduler(const TaskGroup groups[], const uint8_t groupCount) : WatchdogTimerBasedScheduler(groups, groupCount) { }
  
  protected:
    bool stopTimersDuringSleep() { 
      // Timer1 is needed for PWM: while fan is running at other than 100% duty cycle => cannot turn MCU off 
      return ! logicalIO()->isPwmActive();
    }
    
    #if defined(__AVR_ATmega328P__)
      void wakingUp() { logicalIO()->wdtWakeupLEDBlip(); }
    #endif
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
    IntervalModeTask() : BlinkTask (INTERVAL_GROUP, 0 /* ledPin: value 0 is unused */) { };  // infinite (i.e. until canceled)
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
FanScheduler FAN_SCHEDULER = FanScheduler(TASK_GROUPS, NUM_TASK_GROUPS);

BlinkTask SPEED_TRANSITION_BLINKER = BlinkTask(SPEED_TRANSITION_GROUP, STATUS_LED_OUT_PIN, 5);
BlinkTask INTENTITY_CHANGED_FEEDBACK_BLINKER = BlinkTask(SPEED_TRANSITION_GROUP, STATUS_LED_OUT_PIN, 2);
BlinkTask PAUSE_SHOW_ALIVE = BlinkTask(PAUSE_SHOW_ALIVE_GROUP, STATUS_LED_OUT_PIN); // infinite (= runs until canceled)
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

void animateSpeedTransition() {
   FAN_SCHEDULER.scheduleTaskNow(& SPEED_TRANSITION_BLINKER);
}

void animateIntensityChange() {
  FAN_SCHEDULER.scheduleTaskNow(& INTENTITY_CHANGED_FEEDBACK_BLINKER);
}

void updateIntervalModeTaskPause() {
  duration16_s_t duration =  mapToIntervalPauseDuration(logicalIO()->fanIntensity());
  INTERVAL_MODE_TASK.delays(INTERVAL_FAN_ON_DURATION*D_1S, duration*D_1S);
}

void startIntervalModeNow() {
  updateIntervalModeTaskPause();
  FAN_SCHEDULER.scheduleTaskNow(& INTERVAL_MODE_TASK);
}

void endIntervalMode() {
  FAN_SCHEDULER.cancelTask(& INTERVAL_MODE_TASK);
  FAN_SCHEDULER.cancelTask(& PAUSE_SHOW_ALIVE);
}

void fanOn(FanMode mode) {
  FAN_SCHEDULER.cancelTask(& PAUSE_SHOW_ALIVE);
  animateSpeedTransition();
  if (mode == MODE_CONTINUOUS) {
    logicalIO()->fanSpeed(mapToFanSpeed(logicalIO()->fanIntensity()));
  } else { // getFanMode() == MODE_INTERVAL
    logicalIO()->fanSpeed(SPEED_FULL);
  }
}

void fanOff() {
  animateSpeedTransition();
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
            startIntervalModeNow(); // ==> Task will turn fan ON first
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
            endIntervalMode();
          } else if (logicalIO()->fanMode() == MODE_INTERVAL) {
            fanState = FAN_PAUSING;
            startIntervalModeNow();
          } else { // newMode == MODE_OFF
            endIntervalMode();
            fanState = FAN_OFF;
            fanOff();
          }
          break;
          
        case INTENSITY_CHANGED: 
          if (logicalIO()->fanMode() == MODE_CONTINUOUS) {
            animateSpeedTransition();
            logicalIO()->fanSpeed(mapToFanSpeed(logicalIO()->fanIntensity()));
          } else if (logicalIO()->fanMode() == MODE_INTERVAL) {
            updateIntervalModeTaskPause();
            animateIntensityChange();
          }
          break;

        case INTERVAL_PHASE_ENDED:
          fanState = FAN_PAUSING;
          fanOff();
          FAN_SCHEDULER.scheduleTask(& PAUSE_SHOW_ALIVE, PAUSE_SHOW_ALIVE.offDuration());  // blink for the first time after offDuration
          break;
          
        default:
          break;
      }
      break;
      
    case FAN_PAUSING:
      switch(event) {
        case MODE_CHANGED:
          if (logicalIO()->fanMode() == MODE_OFF) {
            endIntervalMode();
            fanState = FAN_OFF;
          } else if (logicalIO()->fanMode() == MODE_CONTINUOUS) {
            endIntervalMode();
            fanState = FAN_ON;
            fanOn(MODE_CONTINUOUS);
          }
          break;
          
        case INTENSITY_CHANGED: 
          {
            SDuration originalDelay = INTERVAL_MODE_TASK.originalDelay();
            SDuration waited = originalDelay - (INTERVAL_MODE_TASK.dueTime() - now());
            updateIntervalModeTaskPause();
            animateIntensityChange();
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
    Serial.flush();
  #endif
}

void initFanControl() {
  // Install input-change handlers (= assign function pointers)
  logicalIO()->modeChangedHandler = scheduleModeChangeTask;
  logicalIO()->intensityChangedHandler = scheduleIntensityChangedTask; 

  SPEED_TRANSITION_BLINKER.name("Speed");
  SPEED_TRANSITION_BLINKER.delays(D_500MS, D_500MS);
  INTENTITY_CHANGED_FEEDBACK_BLINKER.name("Intensity");
  INTENTITY_CHANGED_FEEDBACK_BLINKER.delays(D_250MS, D_250MS);
  PAUSE_SHOW_ALIVE.name("Blip");
  PAUSE_SHOW_ALIVE.delays(D_250MS, INTERVAL_PAUSE_BLIP_PERIOD*D_1S);  // will be stopped at pause end
  updateIntervalModeTaskPause();

  logicalIO()->init();  // this causes the first tasks to be created for intensity and mode change
}

