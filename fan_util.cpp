#include "fan_util.h"
  
void configInput(uint8_t pin) {
  pinMode(pin, INPUT);
}

void configInputWithPullup(uint8_t pin) {
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH);              // Activate pull-up resistor on pin (input)
}

void configOutput(uint8_t pin) {
  pinMode(pin, OUTPUT);
}
