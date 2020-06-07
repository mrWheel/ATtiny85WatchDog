# ATtiny85WatchDog
Using an ATtiny85 as WatchDog for an other processor

The objective is to have a means to monitor activity on a master processor, for instance, an ESP8266 (or ESP32 or any
other type of processor).

The master processor needs to send at least every 4500MS a "Keep Alive" pulse to the WatchDog. If the WatchDog does 
not receive this pulse in time it will make pin x HIGH for 20MS to reset the master processor.

