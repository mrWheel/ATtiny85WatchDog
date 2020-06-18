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
 * <--[RST_ESP] (D3) PB3 2|      |7 PB2 (D2) (INT0) <-----
 * <---[RELAYS] (D4) PB4 3|      |6 PB1 (D1) [REL_LED] -->
 *                   GND 4|      |5 PB0 (D0) [SGNL_LED] -> 
 *                        +------+
 *
 *  Boot sequence:
 *  ==============
 *  State       | REL_LED | SGNL_LED      | Remark
 *  ------------+---------+---------------+------------------------------------
 *  Power On    | Blink   | Blink         | inversed from each other
 *              |         |               | Relays "Off"
 *  ------------+---------+---------------+------------------------------------
 *  Init 1      | Off     | 900 MS On     | repeat for 10 seconds
 *              |         | 100 MS Off    | Relays "Off"
 *  ------------+---------+---------------+------------------------------------
 *  Init 2      | On      |               | Relays "On"          
 *  ------------+---------+---------------+------------------------------------
 *  Init 3      | On      | 100 MS On     | repeat for 20 seconds 
 *              |         | 900 MS Off    | Relays are "On" 
 *  ------------+---------+---------------+------------------------------------
 *  Init 4      | On      | 500 MS On     | wait for 10 Feeds to pass
 *              |         | for every     | Relays "On" 
 *              |         | Feed received | 
 *  ------------+---------+---------------+------------------------------------
 *  Normal      | On      | 500 MS On     | Relays "On"  
 *  Operation   |         | for every     | 
 *              |         | feed received | 
 *              |         +---------------+------------------------------------
 *              |         | Blink fast    | If no feed within 2 seconds: 
 *              |         |               | "Alarm State" 
 *              |         |               | else: "Normal Operation"
 *              |         |               | Relays "On"
 *  ------------+---------+---------------+------------------------------------
 *  Alarm State | Off     | Off           | Relays "Off"
 *              |         |               | Reset ESP8266
 *              |         |               | Restart ATtinyWatchDog to 
 *              |         |               | "Power On" state
 *  ------------+---------+---------------+------------------------------------
 *  
*/

// https://github.com/GreyGnome/EnableInterrupt
#include <EnableInterrupt.h>

#define _PIN_SGNL_LED       0       // GPIO-00 ==> DIL-5 ==> PB0
#define _PIN_INTERRUPT      2       // GPIO-02 ==> DIL-7 ==> PB2  / INT0
#define _PIN_RST_ESP        3       // GPIO-03 ==> DIL-2 ==> PB3
#define _PIN_RELAYS         4       // GPIO-04 ==> DIL-3 ==> PB4
#define _PIN_REL_LED        1       // GPIO-01 ==> DIL-6 ==> PB1

#define _HALF_A_SECOND    500       // half a second
#define _MAX_HALF_SECONDS  40       // Max 20 seconds elapse befoure WDT kicks in!
#define _GLOW_TIME        500       // MilliSeconds

volatile uint8_t  WDcounter;
volatile bool     resetESP          = false;
volatile uint8_t  interrupsReceived = 0;
volatile uint32_t blinkTimer;

//----------------------------------------------------------------
void interruptSR(void) 
{
    WDcounter = 0;
    interrupsReceived++;
    if (interrupsReceived > 10)
    {    
      interrupsReceived = 11;
      resetESP  = true;
    }
    digitalWrite(_PIN_SGNL_LED, HIGH);
    blinkTimer = millis() + _GLOW_TIME;
    
}   // interruptSR()


//----------------------------------------------------------------
void blinkLed(uint16_t onMS, uint16_t offMS, uint32_t durationMS)
{
  uint32_t  timeToGo = millis() + durationMS;
  
  while (timeToGo > millis())
  {
    digitalWrite(_PIN_SGNL_LED, HIGH);
    delay(onMS);
    digitalWrite(_PIN_SGNL_LED, LOW);
    delay(offMS);
  }
  digitalWrite(_PIN_SGNL_LED, LOW);
  
} // blinkLed()


//----------------------------------------------------------------
void delayMS(uint32_t durationMS)
{
  uint32_t  timeToGo = millis() + durationMS;
  
  while (timeToGo > millis())
  {
    if (millis() > blinkTimer)
    {
      digitalWrite(_PIN_SGNL_LED, LOW);
    }
  }

} // delayMS()


//----------------------------------------------------------------
void setup() 
{
    pinMode(_PIN_SGNL_LED,    OUTPUT);
    digitalWrite(_PIN_SGNL_LED, LOW);
    pinMode(_PIN_RST_ESP,    OUTPUT);
    digitalWrite(_PIN_RST_ESP,  LOW);
    pinMode(_PIN_RELAYS,     OUTPUT);
    digitalWrite(_PIN_RELAYS,   LOW); // begin met relays "off"
    pinMode(_PIN_REL_LED,    OUTPUT);

    for(int r=0; r<10; r++)
    {
      digitalWrite(_PIN_REL_LED,  !digitalRead(_PIN_REL_LED));
      digitalWrite(_PIN_SGNL_LED, !digitalRead(_PIN_REL_LED));
      delay(200);
    }
    digitalWrite(_PIN_REL_LED, LOW); // begin met relays-led "off"

    blinkLed(100, 900, 10000);
    digitalWrite(_PIN_RELAYS,  HIGH); // activate relays ("on")
    digitalWrite(_PIN_REL_LED, HIGH); // activate LED ("on")
    
    blinkLed(900, 100, 20000);
    digitalWrite(_PIN_SGNL_LED, LOW);

  //enableInterrupt(_PIN_INTERRUPT, interruptSR, RISING);
    enableInterrupt(_PIN_INTERRUPT, interruptSR, CHANGE);

    WDcounter = 0;

} // setup()


//----------------------------------------------------------------
void loop() 
{
  if (WDcounter <= 1)
  {
    digitalWrite(_PIN_SGNL_LED, HIGH);
    blinkTimer = millis() + _GLOW_TIME;
  }
  
  if (WDcounter <= _MAX_HALF_SECONDS)
  {
    if (resetESP) // get's true after 10 Feeds
    {
      if (millis() > blinkTimer)
      {
        digitalWrite(_PIN_SGNL_LED, LOW);
      }
    }
    else  // less than 10 feeds, so do not light PWM_LED
    {
      digitalWrite(_PIN_RELAYS,  HIGH);
    }
  }
  else  // WDcounter > _MAX_HALF_SECONDS
  { 
    WDcounter = _MAX_HALF_SECONDS;
    //--- this should never happen, but if it does ..
    //--- disable relays ("off")
    digitalWrite(_PIN_RELAYS,   LOW);
    digitalWrite(_PIN_REL_LED,  LOW);
    digitalWrite(_PIN_SGNL_LED, LOW);
    if (resetESP)   // we want to do this one time only!
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

  if (WDcounter > (_MAX_HALF_SECONDS - 4))
  {
    blinkLed(100, 100, _HALF_A_SECOND);
  }
  else
  {
    delayMS(_HALF_A_SECOND);
  }
  WDcounter++;

} // loop()

/* eof */
