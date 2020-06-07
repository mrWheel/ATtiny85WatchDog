# ATtiny85WatchDog
Using an ATtiny85 as WatchDog for an other processor

The objective is to have a means to monitor activity on a master processor, for instance, an ESP8266 (or ESP32 or any
other type of processor).

![Screenshot 2020-06-07 at 11 51 43](https://user-images.githubusercontent.com/5585427/83965751-557d2300-a8b6-11ea-8b43-abaa05e172a1.png)

The master processor needs to send at least every 4500 milli second (msec) a "Keep Alive" pulse to the WatchDog 
[WDT-FEED] (the 3v3 signal is level-shifted by Q1 to [WDT] (PB2, DIL-7)). If the WatchDog does not receive this 
pulse in time it will make [ESP-RESET] (PB3, DIL-2) HIGH for 20 msec to reset the master processor.

![Screenshot 2020-06-07 at 11 51 16](https://user-images.githubusercontent.com/5585427/83965792-84939480-a8b6-11ea-80fe-8f711a88e029.png)

In the immage above you see a typical reset circuit for an ESP processor. [ESP-RESET] comes from the ATtiny85 (PB3, DIL-2). 
The Q3 MOSFET works as a level-shifter (if needed) and on a HIGH signal it will pull [RESET] down.

PB4 (DIL-3) on the ATtiny85 is used to control (in this case) relays but this is not needed for every implementation. 
PB0 (DIL-5) outputs a PWM signal to the [WDT-LED] (D2) which will, after a "Keep Alive" pulse lights bright (100%) 
and gradulay dimms to 0% approx. 500 msec before the circuit will reset the master processor.
