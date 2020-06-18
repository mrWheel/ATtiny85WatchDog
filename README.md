# ATtiny85WatchDog
Using an ATtiny85 as WatchDog for an other processor

The objective is to have a means to monitor activity on a master processor, for instance, an ESP8266 (or ESP32 or any
other type of processor).

![Screenshot 2020-06-07 at 11 51 43](https://user-images.githubusercontent.com/5585427/83965751-557d2300-a8b6-11ea-8b43-abaa05e172a1.png)

The master processor needs to send at least every `_MAX_HALF_SECONDS` seconds a "Keep Alive" pulse to the WatchDog 
[WDT-FEED] (the 3v3 signal is level-shifted by Q1 to [WDT] (PB2, DIL-7)). If the WatchDog does not receive this 
pulse in time it will make [ESP-RESET] (PB3, DIL-2) HIGH for 500 msec to reset the master processor. In the Firmware PB1 is available as OUTPUT pin for an extra LED (REL_LED)

![Screenshot 2020-06-07 at 11 51 16](https://user-images.githubusercontent.com/5585427/83965792-84939480-a8b6-11ea-80fe-8f711a88e029.png)

In the image above you see a typical reset circuit for an ESP processor. [ESP-RESET] comes from the ATtiny85 (PB3, DIL-2). 
The Q3 MOSFET works as a level-shifter (if needed) and on a HIGH signal it will pull [RESET] down.

PB4 (DIL-3) on the ATtiny85 is used to control (in this case) relays but this is not needed for every implementation. 
PB0 (DIL-5) outputs a signal to the [WDT-LED] (D2) which will, after every received "Keep Alive" pulse go "HIGH" for 
500 milliseconds). Two seconds before the circuit will reset the master processor the PWM_LED will blink rapidly.

<pre>
 ATMEL ATTINY85
                         +--\/--+
              RESET PB5 1|      |8 VCC
  <--[RST_ESP] (D3) PB3 2|      |7 PB2 (D2) (INT0) <-----
  <---[RELAYS] (D4) PB4 3|      |6 PB1 (D1) [REL_LED] -->
                    GND 4|      |5 PB0 (D0) [SGNL_LED] -> 
                         +------+
 
   Boot sequence:
   ==============
   State       | REL_LED | SGNL_LED      | Remark
   ------------+---------+---------------+------------------------------------
   Power On    | Blink   | Blink         | inversed from each other
               |         |               | Relays "Off"
   ------------+---------+---------------+------------------------------------
   Init 1      | Off     | 900 MS On     | repeat for 10 seconds
               |         | 100 MS Off    | Relays "Off"
   ------------+---------+---------------+------------------------------------
   Init 2      | On      |               | Relays "On"          
   ------------+---------+---------------+------------------------------------
   Init 3      | On      | 100 MS On     | repeat for 20 seconds 
               |         | 900 MS Off    | Relays are "On" 
   ------------+---------+---------------+------------------------------------
   Init 4      | On      | 500 MS On     | wait for 10 Feeds to pass
               |         | for every     | Relays "On" 
               |         | Feed received | 
   ------------+---------+---------------+------------------------------------
   Normal      | On      | 500 MS On     | Relays "On"  
   Operation   |         | for every     | 
               |         | feed received | 
               |         +---------------+------------------------------------
               |         | Blink fast    | If no feed within 2 seconds: 
               |         |               | "Alarm State" 
               |         |               | else: "Normal Operation"
               |         |               | Relays "On"
   ------------+---------+---------------+------------------------------------
   Alarm State | Off     | Off           | Relays "Off"
               |         |               | Reset ESP8266
               |         |               | Restart ATtinyWatchDog to 
               |         |               | "Power On" state
   ------------+---------+---------------+------------------------------------
</pre>  
