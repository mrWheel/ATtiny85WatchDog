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
 * Arduino IDE version 1.8.12
 * 
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
 *              |         |               | Next: "Fase 1"
 *  ------------+---------+---------------+------------------------------------
 *  Fase 1      | Off     | 900 MS On     | repeat for 25 seconds
 *              |         | 100 MS Off    | Relays "Off"
 *              |         |               | Next: "Fase 2"
 *  ------------+---------+---------------+------------------------------------
 *  Fase 2      | Off     | 500 MS On     | wait for 3 Feeds to pass
 *              |         | for every     | Relays "Off" 
 *              |         | Feed received | Next: "Fase 3"
 *  ------------+---------+---------------+------------------------------------
 *  Fase 3      | Off     | Blink Fast!   | wait for 3 Feeds to pass
 *              |         | for 2 seconds | Relays "Off" 
 *              |         |               | Next: "Normal Operation"
 *  ------------+---------+---------------+------------------------------------
 *  Normal      | On      | 500 MS On     | Relays "On"  
 *  Operation   |         | for every     | 
 *              |         | feed received | 
 *              |         +---------------+------------------------------------
 *              |         | Blink fast    | If no feed within 2 seconds: 
 *              |         |               | Next: "Alarm State" 
 *              |         |               | else: "Normal Operation"
 *              |         |               | Relays "On"
 *  ------------+---------+---------------+------------------------------------
 *  Alarm State | Off     | Off           | Relays "Off"
 *              |         |               | Reset ESP8266
 *              |         |               | Restart ATtinyWatchDog to 
 *              |         |               | Next: "Power On" state
 *  ------------+---------+---------------+------------------------------------
 *  
*/

// https://github.com/GreyGnome/EnableInterrupt
#include <EnableInterrupt.h>

#define _PIN_SGNL_LED         0       // GPIO-00 ==> DIL-5 ==> PB0
#define _PIN_INTERRUPT        2       // GPIO-02 ==> DIL-7 ==> PB2  / INT0
#define _PIN_RST_ESP          3       // GPIO-03 ==> DIL-2 ==> PB3
#define _PIN_RELAYS           4       // GPIO-04 ==> DIL-3 ==> PB4
#define _PIN_REL_LED          1       // GPIO-01 ==> DIL-6 ==> PB1

#define _HALF_A_SECOND      500       // half a second
#define _MAX_HALF_SECONDS    40       // Max 20 seconds elapse befoure WDT kicks in!
#define _STARTUP_TIME     25000       // 25 seconden
#define _GLOW_TIME          500       // MilliSeconds

volatile bool receivedInterrupt = false;
uint8_t  WDcounter;
uint8_t  feedsReceived = 0;
uint32_t blinkTimer;

//----------------------------------------------------------------
void interruptSR(void) 
{
  receivedInterrupt = true;
    
}   // interruptSR()


//----------------------------------------------------------------
void blinkSgnlLed(uint16_t onMS, uint16_t offMS, uint32_t durationMS)
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
  
} // blinkSgnlLed()


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

    blinkSgnlLed(100, 900, _STARTUP_TIME);  // 25 seconds?
    digitalWrite(_PIN_SGNL_LED, LOW);

  //enableInterrupt(_PIN_INTERRUPT, interruptSR, RISING);
    enableInterrupt(_PIN_INTERRUPT, interruptSR, CHANGE);

    receivedInterrupt = false;
    WDcounter         = 0;
    feedsReceived     = 0;

} // setup()


//----------------------------------------------------------------
void loop() 
{
  if (receivedInterrupt)
  {
    receivedInterrupt = false;
    WDcounter         = 0;
    feedsReceived++;
    if (feedsReceived >= 3)
    {    
      //--- WDT is armed from now on
      if (feedsReceived == 3)
      {
        blinkSgnlLed(50, 200, 2000);
      }
      feedsReceived = 4;
      digitalWrite(_PIN_RELAYS,   HIGH);
      digitalWrite(_PIN_REL_LED,  HIGH);
    }
    digitalWrite(_PIN_SGNL_LED, HIGH);
    blinkTimer = millis() + _GLOW_TIME;
  }

  if (WDcounter <= _MAX_HALF_SECONDS)
  {
    if (millis() > blinkTimer)
    {
      digitalWrite(_PIN_SGNL_LED, LOW);
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
    disableInterrupt(_PIN_INTERRUPT);
    //--- now reset main processor
    digitalWrite(_PIN_RST_ESP, HIGH);
    delay(500);
    digitalWrite(_PIN_RST_ESP,  LOW);
    setup();
  }

  //--- almost at the max time without a feed ..
  if (WDcounter > (_MAX_HALF_SECONDS - 4))
  {
    blinkSgnlLed(100, 100, _HALF_A_SECOND);
  }
  else
  {
    delayMS(_HALF_A_SECOND);
  }
  
  WDcounter++;

} // loop()

/* eof */
