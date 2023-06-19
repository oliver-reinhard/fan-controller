//#define F_CPU 1000000UL                  // ATmega 328: Defaults to 16 MHz

#include <io_util.h>
  
#if defined(__AVR_ATmega328P__)
  #define VERBOSE
  #include <debug.h>
#endif

#if defined(__AVR_ATmega328P__)
  const pin_t FAN_PWM_OUT_PIN = 9;              // PIN 9 — OC1A PWM signal @ 25 kHz!! DO NOT CHANGE PIN !! (PWM configuration is specific to Timer 1)
  const pin_t STATUS_LED_OUT_PIN = 8; 

#elif defined(__AVR_ATtiny85__)
  const pin_t FAN_PWM_OUT_PIN = PB1;            // OC1A PWM signal @ 25 kHz => MUST BE PB1!!!
  const pin_t STATUS_LED_OUT_PIN = PB0;
#endif 

void setup() {
  #ifdef VERBOSE
    // Setup Serial Monitor
    Serial.begin(38400);
    DEBUG("F_CPU: ", F_CPU);
  #endif

  configOutput(STATUS_LED_OUT_PIN);
  configPWM1();

  turnOnLED(STATUS_LED_OUT_PIN, 2000);
  delay(2000);

  // test_steady_increments();
  test_specific_duty_values();

  turnOnLED(STATUS_LED_OUT_PIN, 2000);
  #ifdef VERBOSE
    DEBUG("Done");
    Serial.println();
  #endif
}

void loop() { }

// -------------

void testCycle(uint8_t blinkTimes, pwm_duty_t value) {
    flashLED(STATUS_LED_OUT_PIN, blinkTimes);
    #ifdef VERBOSE
      DEBUG("Duty value", value);
    #endif
    setFanDutyCycle(value);
    delay(10000);
    setFanDutyCycle(0);
    delay(4000);
}

void test_steady_increments() {
  const uint8_t PCT_START = 4;
  const uint8_t PCT_END = 20;
  const uint8_t PCT_INCR = 2;
  const uint8_t STEPS = (PCT_END - PCT_START) / PCT_INCR + 1;

  for(uint8_t i=1; i<=STEPS; i++) {
    pwm_duty_t duty_value = (pwm_duty_t) (((uint16_t)PWM_DUTY_MAX) * (PCT_START + (i-1)*PCT_INCR) / 100);
    #ifdef VERBOSE
      uint8_t pct_value = PCT_START + (i-1) * PCT_INCR;
      Serial.print(pct_value);
      Serial.print("% -> ");
    #endif
    testCycle(i, duty_value);
  }
}

void test_specific_duty_values() {
  #if defined(__AVR_ATmega328P__)
    testCycle(1, 17); // => ORC1A = 21
    testCycle(2, 25); // => ORC1A = 31
    testCycle(3, PWM_DUTY_MAX);// => ORC1A = 320 (= MAX)
  #elif defined(__AVR_ATtiny85__)
    testCycle(1, 20); // => ORC1A = 3
    testCycle(2, 26); // => ORC1A = 4
    testCycle(3, PWM_DUTY_MAX); // => ORC1A = 40 (= MAX)
  #endif
}

//
// PWM / Timer1 scaling to 25 KHz
//
#if defined(__AVR_ATmega328P__)
  const uint16_t TIMER1_COUNT_TO = 320;    // count to this value (Timer 1 is 16 bit)

#elif defined(__AVR_ATtiny85__)
  #if (F_CPU == 1000000UL)
    // PWM frequency = 1 MHz / 1 / 40 = 25 kHz 
    const uint8_t TIMER1_COUNT_TO = 40;     // count to this value
  #else
    #error("F_CPU is undefined or its value is unknown")
  #endif
#endif

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
          | _BV(CS10); // prescaling_B

    TCNT1 = 0;  // Reset timer
    ICR1 = TIMER1_COUNT_TO; // TOP (= count to this value)
            
    // Set the PWM pins as output.
    configOutput(FAN_PWM_OUT_PIN); // = PIN 9 => OC1A
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
    
    // On Compare Match with OCR1A (counter == OCR1A): Clear the output line (-> LOW), set HIGH on $00
    TCCR1 |= _BV(COM1A1);
  
    // Configure PWM frequency:
    TCCR1 |= _BV(CS10);       // Prescale factor = 1
    OCR1C = TIMER1_COUNT_TO;  // Count 0,1,2..compare-match,0,1,2..compare-match, etc
  
    // Determines Duty Cycle: OCR1A / OCR1C e.g. value of 50 / 200 --> 25%,  value of 50 --> 0%
    OCR1A = 0;
    configOutput(FAN_PWM_OUT_PIN);  // PB1 => OC1A
  #endif
}


void setFanDutyCycle(pwm_duty_t value) {
  #if defined(__AVR_ATmega328P__)
    uint16_t scaled = (((uint32_t) value) * TIMER1_COUNT_TO /  PWM_DUTY_MAX);
    #ifdef VERBOSE
      DEBUG("  -> OCR1A", scaled);
    #endif
    OCR1A = scaled;

  #elif defined(__AVR_ATtiny85__)
    pwm_duty_t scaled = (((uint16_t) value) * TIMER1_COUNT_TO /  PWM_DUTY_MAX);
    #ifdef VERBOSE
      DEBUG("  -> OCR1A", scaled);
    #endif
    OCR1A = scaled;
  #endif
}