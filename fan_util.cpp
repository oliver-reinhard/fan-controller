#include "fan_util.h"
  
void configInput(pin_t pin) {
  pinMode(pin, INPUT);
}

void configInputWithPullup(pin_t pin) {
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH);              // Activate pull-up resistor on pin (input)
}

void configOutput(pin_t pin) {
  pinMode(pin, OUTPUT);
}
