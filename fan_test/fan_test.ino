//#define F_CPU 1000000UL                  // ATmega 328: Defaults to 16 MHz
//#define F_CPU 128000UL                  // Defaults to 16 MHz

#include "io_util.h"
//
  
#if defined(__AVR_ATmega328P__)
  #define VERBOSE
#endif

#if defined(__AVR_ATmega328P__)
  const pin_t FAN_PWM_OUT_PIN = 9;              // PIN 9 — OC1A PWM signal !! DO NOT CHANGE PIN !! (PWM configuration is specific to Timer 1)
  const pin_t STATUS_LED_OUT_PIN = 8;           // PD0 - digital out; is on when fan is of, blinks during transitioning 

#elif defined(__AVR_ATtiny85__)
  const pin_t FAN_POWER_ON_OUT_PIN = PB5;       // Fan power: MOSFET on/off (some fans don't stop at PWM duty cycle = 0%)
  const pin_t FAN_PWM_OUT_PIN = PB1;            // PWM signal @ 25 kHz
#endif 

//
// PWM / Timer1 scaling to 25 KHz
//
#if defined(__AVR_ATmega328P__)
  const uint8_t TIMER1_PRESCALER = 1;      // divide by 1
  const uint16_t TIMER1_COUNT_TO = 320;    // count to this value (Timer 1 is 16 bit)

  const pwm_duty_t PWM_DUTY_MAX = 255;

#elif defined(__AVR_ATtiny85__)
  #if (F_CPU == 1000000UL)
    // PWM frequency = 1 MHz / 1 / 40 = 25 kHz 
    const uint8_t TIMER1_PRESCALER = 1;     // divide by 1
    const uint8_t TIMER1_COUNT_TO = 160;    // count to this value
  #elif #if (F_CPU == 128000UL)
    // PWM frequency = 128 kHz / 1 / 5 = 25.6 kHz 
    const uint8_t TIMER1_PRESCALER = 1;     // divide by 1
    const uint8_t TIMER1_COUNT_TO = 5;      // count to this value
  #else
    #error("F_CPU is undefined or its value is unknown")
  #endif
  
  const uint8_t PWM_DUTY_MAX = TIMER1_COUNT_TO;
#endif

//
// SETUP
//
void setup() {
  configOutput(STATUS_LED_OUT_PIN);
  configPWM1();

  delay(1000);
  turnOnLED(STATUS_LED_OUT_PIN, 2000);
  delay(2000);

  #ifdef VERBOSE
    // Setup Serial Monitor
    Serial.begin(38400);
    
    Serial.print("F_CPU: ");
    Serial.println(F_CPU);
  #endif

  // test_steady_increments();
  test_specific_duty_values();

  turnOnLED(STATUS_LED_OUT_PIN, 2000);
  #ifdef VERBOSE
    Serial.print("Done");
    // Serial.print(" -> ");
    // Serial.print(PCT_INCR);
    // Serial.println("%");
    // setFanDutyCycle(PWM_DUTY_INCR);
    Serial.println();
  #endif
}

void loop() {
}

// -------------

void testCycle(uint8_t blinkTimes, pwm_duty_t value) {
    flashLED(STATUS_LED_OUT_PIN, blinkTimes);
    #ifdef VERBOSE
      Serial.println(value);
    #endif
    setFanDutyCycle(value);
    delay(10000);
    setFanDutyCycle(0);
    delay(4000);
}

void test_steady_increments() {
  const uint8_t PCT_START = 5;
  const uint8_t PCT_END = 10;
  const uint8_t PCT_INCR = 1;
  const uint8_t STEPS = (PCT_END - PCT_START) / PCT_INCR + 1;
  const pwm_duty_t PWM_DUTY_START = (pwm_duty_t) (((uint16_t)PWM_DUTY_MAX) * PCT_START / 100);
  const pwm_duty_t PWM_DUTY_INCR = (pwm_duty_t) (((uint16_t)PWM_DUTY_MAX) * PCT_INCR / 100);

  for(uint8_t i=1; i<=STEPS; i++) {
    uint8_t pct_value = PCT_START + (i-1) * PCT_INCR;
    pwm_duty_t duty_value = PWM_DUTY_START + (i-1) * PWM_DUTY_INCR;
    #ifdef VERBOSE
      Serial.print(pct_value);
      Serial.print("% -> ");
      Serial.println(duty_value);
    #endif
    testCycle(i, duty_value);
  }
}

void test_specific_duty_values() {
  testCycle(1, 20);
  testCycle(2, 40);
  testCycle(3, PWM_DUTY_MAX);
}

void configPWM1() {
  #if defined(__AVR_ATmega328P__)
    // No specific PWM frequency --> default = 490 Hz

    // Configure Timer 1 for PWM @ 25 kHz.
    // Source: https://www.arduined.eu/arduino-pwm-pc-fan-control/
    //
    // Undo the configuration done by the Arduino core library:
    TCCR1A = 0; 
    TCCR1B = 0;

    // Waveform Generator Mode (WGM):
    // -> See Table 15-5 of ATmega328P Datasheet
    // - 4 bits, distributed across TCCR1A and TCCR1B 
    // - Set to mode #10: "PWM, phase correct, 9-bit, TOP = ICR1" 
    //   | WGM13 | WGM12 | WGM11 | WGM10 |
    //   |   1   |   0   |   1   |   0   |

    // Compare Output Mode for chanlels A / B (COM)
    // !!! Channels A and B have NOTHING TO DO WITH CONTROL REGISTERS A and B !!!
    // -> See Table 15-4 of ATmega328P Datasheet (this table applies due to the WGM13 bit)
    // - Set to "Clear OC1A/OC1B on compare match when up-counting."
    // | COM1A1 | COM1A0 | COM1B1 | COM1B0 | 
    // |   1    |   0    |    1   |   0    |

    // Prescaler / Clock Select (CS)
    // - 3 bits
    // - Set to 1 (no prescaling)
    //   | CS12 | CS11 | CS10 | 
    //   |  0   |  0   |   1  |
  
    // Configure Timer/Counter1 Control Register A (TCCR1A) 
    // | COM1A1 | COM1A0 | COM1B1 | COM1B0 |  -  |  -  | WGM11 | WGM10 |
    // |   1    |   0    |    1   |   0    |  0  |  0  |   1   |   0   |
    TCCR1A = _BV(COM1A1)
          | _BV(COM1B1)
          | _BV(WGM11);

    // Configure Timer/Counter1 Control Register B (TCCR1B) 
    // - Input Capture Noise Canceler (ICNC)
    // - Input Capture Edge Select (ICNS)
    // | ICNC1 |  ICES1 |  -  | WGM13 | WGM12 | CS12 | CS11 | CS10 | 
    // |   0   |    0   |  0  |   1   |   0   |  0   |  0   |   1  |
    TCCR1B = _BV(WGM13)  
          | _BV(CS10);

    TCNT1 = 0;  // Reset timer
    ICR1 = TIMER1_COUNT_TO; // TOP (= count to this value)
            
    // Set the PWM pins as output.
    configOutput(9); // OC1A
    // configOutput(10);  // OC1B
  
  #elif defined(__AVR_ATtiny85__)
    // Configure Timer/Counter1 Control Register 1 (TCR1) 
    // | CTC1 | PWM1A | COM1A | CS |
    // |  1   |  1    |  2    | 4  |  ->  #bits
    //
    // CTC1 - Clear Timer/Counter on Compare Match: When set (==1), TCC1 is reset to $00 in the CPU clock cycle after a compare match with OCR1C register value.
    // PWM1A - Pulse Width Modulator A Enable: When set (==1), enables PWM mode based on comparator OCR1A in TC1 and the counter value is reset to $00 in the CPU clock cycle after a compare match with OCR1C register value.
    // COM1A - Comparator A Output Mode: determines output-pin action following a compare match with compare register A (OCR1A) in TC1
    // CS - Clock Select Bits: defines the prescaling factor of TC1
  
    // Clear all TCCR1 bits:
    TCCR1 &= B00000000;      // Clear 
  
    // Clear Timer/Counter on Compare Match: count from 0, 1, 2 .. OCR1C, 0, 1, 2 .. ORC1C, etc
    TCCR1 |= _BV(CTC1);
    
    // Enable PWM A based on OCR1A
    TCCR1 |= _BV(PWM1A);
    
    // On Compare Match with OCR1A (counter == OCR1A): Clear the output line (-> LOW), set on $00
    TCCR1 |= _BV(COM1A1);
  
    // Configure PWM frequency:
    TCCR1 |= TIMER1_PRESCALER;  // Prescale factor
    OCR1C = TIMER1_COUNT_TO;    // Count 0,1,2..compare-match,0,1,2..compare-match, etc
  
    // Determines Duty Cycle: OCR1A / OCR1C e.g. value of 50 / 200 --> 25%,  value of 50 --> 0%
    OCR1A = 0;
  #endif
}


void setFanDutyCycle(pwm_duty_t value) {
  #if defined(__AVR_ATmega328P__)
    analogWrite(FAN_PWM_OUT_PIN, value); // Send PWM signal

  #elif defined(__AVR_ATtiny85__)
    OCR1A = value;
  #endif
}