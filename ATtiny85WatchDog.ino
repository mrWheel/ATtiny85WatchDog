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
 * <---[RELAYS] (D4) PB4 3|      |6 PB1 (D1) [REL_LED] ->
 *                   GND 4|      |5 PB0 (D0) [PWM_LED] -> 
 *                        +------+
 *
 *  Boot sequence:
 *  ==============
 *  State       | REL_LED   | PWM_LED     | Remark
 *  ------------+-----------+-------------+------------------------------------
 *  Power On    | Blink     | Blink       | inversed from each other
 *              |           |             | Relays "Off"
 *  ------------+-----------+-------------+------------------------------------
 *  Init 1      | Off       | 0% to 100%  | repeat for 20 seconds
 *              |           |             | Relays "Off"
 *  ------------+-----------+-------------+------------------------------------
 *  Init 2      | On        |             | Relays "On"          
 *  ------------+-----------+-------------+------------------------------------
 *  Init 3      | On        | 100% to 0%  | repeat for 35 seconds 
 *              |           |             | Relays are "On" 
 *  ------------+-----------+-------------+------------------------------------
 *  Init 4      | On        | Off         | wait for 10 Feeds to pass
 *              |           |             | Relays "On"
 *  ------------+-----------+-------------+------------------------------------
 *  Normal      | On        | 100% to 0%  | Reset to 100% after Feed is  
 *  Operation   |           |             | received
 *              |           |             | Relays "On"
 *              |           +-------------+------------------------------------
 *              |           | Off         | If no feed within 1 second: 
 *              |           |             | "Alarm State" 
 *              |           |             | else: "Normal Operation"
 *              |           |             | Relays "On"
 *  ------------+-----------+-------------+------------------------------------
 *  Alarm State | Off       | Off         | Relays "Off"
 *              |           |             | Reset ESP8266
 *              |           |             | Restart ATtinyWatchDog to 
 *              |           |             | "Power On" state
 *  ------------+-----------+-------------+------------------------------------
 *  
*/

// https://github.com/GreyGnome/EnableInterrupt
#include <EnableInterrupt.h>

#define _PIN_PWM_LED        0       // GPIO-00 ==> DIL-5 ==> PB0
#define _PIN_INTERRUPT      2       // GPIO-02 ==> DIL-7 ==> PB2  / INT0
#define _PIN_RST_ESP        3       // GPIO-03 ==> DIL-2 ==> PB3
#define _PIN_RELAYS         4       // GPIO-04 ==> DIL-3 ==> PB4
#define _PIN_REL_LED        1       // GPIO-01 ==> DIL-6 ==> PB1

#define _HALF_A_SECOND    500       // half a second
#define _MAX_HALF_SECONDS  40       // Max 20 seconds elapse befoure WDT kicks in!

volatile uint8_t  WDcounter;
volatile bool     resetESP          = false;
volatile uint8_t  interrupsReceived = 0;
uint8_t           dutyCycle;

void interruptSR(void) 
{
    WDcounter = 0;
    interrupsReceived++;
    if (interrupsReceived > 10)
    {    
      interrupsReceived = 11;
      resetESP  = true;
    }
    
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
    pinMode(_PIN_PWM_LED,    OUTPUT);
    digitalWrite(_PIN_PWM_LED,  LOW);
    pinMode(_PIN_RST_ESP,    OUTPUT);
    digitalWrite(_PIN_RST_ESP,  LOW);
    pinMode(_PIN_RELAYS,     OUTPUT);
    digitalWrite(_PIN_RELAYS,   LOW); // begin met relays "off"
    pinMode(_PIN_REL_LED,    OUTPUT);

    for(int r=0; r<6; r++)
    {
      digitalWrite(_PIN_REL_LED, !digitalRead(_PIN_REL_LED));
      digitalWrite(_PIN_PWM_LED, !digitalRead(_PIN_REL_LED));
      delay(200);
    }
    digitalWrite(_PIN_REL_LED, LOW); // begin met relays-led "off"

    uint32_t startupDelay = millis() + 20000;
    while (millis() < startupDelay)
    {
      for(int i=-1; i<101; i++)
      {
        makePWM(i, 25);
        if (i<1) delay(50);
      }
    }
    digitalWrite(_PIN_RELAYS,  HIGH); // activate relays ("on")
    digitalWrite(_PIN_REL_LED, HIGH); // activate LED ("on")
    
    startupDelay = millis() + 35000;
    while (millis() < startupDelay)
    {
      for(int i=101; i>-1; i--)
      {
        makePWM(i, 25);
        if (i<1) delay(50);
      }
    }

    digitalWrite(_PIN_PWM_LED, LOW);

  //enableInterrupt(_PIN_INTERRUPT, interruptSR, RISING);
    enableInterrupt(_PIN_INTERRUPT, interruptSR, CHANGE);
    
  //digitalWrite(_PIN_PWM_LED, HIGH);

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
      if (resetESP) // get's true after 10 Feeds
      {
        dutyCycle = (100 / _MAX_HALF_SECONDS) * (_MAX_HALF_SECONDS - WDcounter);
      }
      else  // less than 10 feeds, so do not light PWM_LED
      {
        dutyCycle = 0;
      }
      digitalWrite(_PIN_RELAYS,  HIGH);
    }
    else  // WDcounter > _MAX_HALF_)SECONDS
    { 
      WDcounter = _MAX_HALF_SECONDS;
      //--- this should never happen, but if it does ..
      //--- disable relays ("off")
      digitalWrite(_PIN_RELAYS,  LOW);
      digitalWrite(_PIN_REL_LED, LOW);
      dutyCycle = 0;
      digitalWrite(_PIN_PWM_LED, LOW);
      if (resetESP)   // we only want to do this one time only!
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
