//#define F_CPU 1000000UL                  // ATmega 328: Defaults to 16 MHz
//#define F_CPU 128000UL                  // Defaults to 16 MHz

#include <io_util.h>
#include <debug.h>
#include "phys_io.h"
#include "log_io.h"
#include "fan_control.h"

//
//  #define VERBOSE --> see phys_io.h
//  #define SCHEDULER_VERBOSE --> scheduler.h

void setup() {
  #ifdef VERBOSE
    // Setup Serial Monitor
    Serial.begin(38400);
    
    Serial.print("F_CPU: ");
    Serial.println(F_CPU);
    Serial.print("Fan out max: ");
    Serial.println(ANALOG_OUT_MAX);
    Serial.print("Fan out low threshold: ");
    Serial.println(FAN_LOW_THRESHOLD_DUTY_VALUE);
    Serial.flush();
    #define USART0_SERIAL USART0_ON
  #else
    #define USART0_SERIAL USART0_OFF
  #endif

  configPhysicalIO();
  turnOnLED(STATUS_LED_OUT_PIN, 1500);
  delay(500);

  initFanControl();

  controllerLoop(); // infinite 
}

void loop() { }
