#ifndef FAN_CONTROL_H_INCLUDED
  #define FAN_CONTROL_H_INCLUDED
  
  #include <io_util.h>
  
  // const duration16_s_t INTERVAL_FAN_ON_DURATION = 300;         // [s]
  // const duration16_s_t INTERVAL_PAUSE_SHORT_DURATION = 60;     // [s]
  // const duration16_s_t INTERVAL_PAUSE_MEDIUM_DURATION = 600;   // [s]
  // const duration16_s_t INTERVAL_PAUSE_LONG_DURATION = 3600;    // [s]
  // const duration16_s_t INTERVAL_PAUSE_BLIP_PERIOD = 10;    // [s]
  const duration16_s_t INTERVAL_FAN_ON_DURATION = 30;         // [s]
  const duration16_s_t INTERVAL_PAUSE_SHORT_DURATION = 15;     // [s]
  const duration16_s_t INTERVAL_PAUSE_MEDIUM_DURATION = 30;   // [s]
  const duration16_s_t INTERVAL_PAUSE_LONG_DURATION = 60;    // [s]
  const duration16_s_t INTERVAL_PAUSE_BLIP_PERIOD = 10;    // [s]
  void controllerLoop();
  
  //
  // CONTROLLER STATES
  //
  typedef enum  {FAN_OFF, FAN_ON, FAN_PAUSING} FanState;
  typedef enum  {EVENT_NONE, MODE_CHANGED, INTENSITY_CHANGED, INTERVAL_PHASE_ENDED} Event;

  void initFanControl();

#endif
