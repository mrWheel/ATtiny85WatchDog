/*
 * ATtiny85 Watch Dog for an ESP8266/ESP32
 * 
 * Board            : "ATtiny25/45/85"
 * Chip             : "ATtiny85"
 * Clock            : "8MHz (internal)"
 * B.O.D.           : "Disabled"
 * Timer 1 Clock    : "CPU"
 * millis()/micros(): "Enabled"
 * 
 * #elif defined(__AVR_ATtiny85__)
 * ATMEL ATTINY85
 *                        +--\/--+
 *             RESET PB5 1|      |8 VCC
 * <--[RST_ESP] (D3) PB3 2|      |7 PB2 (D2) (INT0) <----
 * <---[RELAYS] (D4) PB4 3|      |6 PB1 (D1)
 *                   GND 4|      |5 PB0 (D0) [PWM_LED] -> 
 *                        +------+
*/

// https://github.com/GreyGnome/EnableInterrupt
#include <EnableInterrupt.h>

#define _PIN_PWM_LED        0       // DIL-5 ==> PB0
#define _PIN_INTERRUPT      2       // DIL-7 ==> PB2  / INT0
#define _PIN_RST_ESP        3       // DIL-2 ==> PB3
#define _PIN_RELAYS         4       // DIL-3 ==> PB4

#define _HALF_A_SECOND    500       // half a second
#define _MAX_HALF_SECONDS  10       // Max seconds elapse befoure WDT kicks in!

volatile uint8_t  WDcounter;
volatile bool     resetESP  = false;
uint8_t           dutyCycle;

void interruptSR(void) 
{
    WDcounter = 0;
    resetESP  = true;
    
}   // interruptSR()


void makePWM(int8_t dutyCycle, uint32_t durationMS)
{
  uint32_t  timeToGo = millis() + durationMS;
  
  if (dutyCycle > 95)  dutyCycle = 100;
  if (dutyCycle <  5)  dutyCycle =   0;
  
  uint32_t   offTime = 100 - dutyCycle;
  uint32_t   onTime  = 100 - offTime;
  
  //Serial.print("makePWM("); Serial.print(onTime); Serial.println(")");
  
  while (timeToGo > millis())
  {
    if (onTime  > 0)  digitalWrite(_PIN_PWM_LED, HIGH);
    else              digitalWrite(_PIN_PWM_LED, LOW);
    delayMicroseconds(onTime);
    if (offTime > 0)  digitalWrite(_PIN_PWM_LED, LOW);
    else              digitalWrite(_PIN_PWM_LED, HIGH);
    delayMicroseconds(offTime);
  }

} // makePWM()


void setup() 
{
    pinMode(_PIN_PWM_LED,   OUTPUT);
    digitalWrite(_PIN_PWM_LED, LOW);
    pinMode(_PIN_RST_ESP,   OUTPUT);
    digitalWrite(_PIN_RST_ESP, LOW);
    pinMode(_PIN_RELAYS,    OUTPUT);
    digitalWrite(_PIN_RELAYS,  LOW);
    
    for(int l=0; l<3; l++)
    {
      for(int i=-5; i<105; i++)
      {
        makePWM(i, 15);
      }
      for(int i=105; i>-5; i--)
      {
        makePWM(i, 15);
      }
    }
    
    delay(6000);   // give ESP time to (re)boot (one minute?)

    //enableInterrupt(_INT0, interruptSR, RISING);
    enableInterrupt(_PIN_INTERRUPT, interruptSR, RISING);
    
    digitalWrite(_PIN_PWM_LED, LOW);

    WDcounter = 0;
    dutyCycle = 0;

} // setup()


void loop() 
{
    if (WDcounter <= _MAX_HALF_SECONDS)
    {
        // counter = 0: (100/6) * (6 - 0) => 17 * 6 = 100
        // counter = 1: (100/6) * (6 - 1) => 17 * 5 =  83
        // counter = 2: (100/6) * (6 - 2) => 17 * 4 =  66
        // counter = 3: (100/6) * (6 - 3) => 17 * 3 =  50
        // counter = 4: (100/6) * (6 - 4) => 17 * 2 =  33
        // counter = 5: (100/6) * (6 - 5) => 17 * 1 =  17
      dutyCycle = (100 / _MAX_HALF_SECONDS) * (_MAX_HALF_SECONDS - WDcounter);
      digitalWrite(_PIN_RELAYS,  HIGH);
    }
    else  // WDcounter > _MAX_HALF_)SECONDS
    { 
      WDcounter = _MAX_HALF_SECONDS;
      digitalWrite(_PIN_RELAYS,  LOW);
      dutyCycle = 0;
      digitalWrite(_PIN_PWM_LED, LOW);
      if (resetESP)   // we only want to do this once!
      {
        disableInterrupt(_PIN_INTERRUPT);
        WDcounter = 0;
        digitalWrite(_PIN_RST_ESP, HIGH);
        delay(500);
        digitalWrite(_PIN_RST_ESP, LOW);
        resetESP = false;
        setup();
      }
    }

    makePWM(dutyCycle, _HALF_A_SECOND);
    WDcounter++;

} // loop()

/* eof */
