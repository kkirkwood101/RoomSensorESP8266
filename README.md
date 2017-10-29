# RoomSensorESP8266

This package transmits data from 3 sensors attached to an ESP8266 processor to a central website. The code developed for the ESP8266 is developed on the Arduino IDE. The Website uses PHP and MySQL to store the data. The ESP8266 assumes a wifi connection is available to make the network connection. Electrical connection schematic is contained in file schematic, database table schema is found in schema. 
Sensors attached to the ESP8266
Dht22 (am2303)
Passive infrared sensor (PIR)
Reed Switch (for door open close monitoring)
Normal duty cycle for the processor is 30 minutes after which it will read all sensors and transmit Temp, humidity, motion, and door open/close status to the web server. In the event motion is detected or the door status changes a data packet will be sent instantly with all 4 readings.
the processor also computes the dewpoint given temp and humidity, as well as includes the name of the wifi access point aloong with the signal strength and includes that in the transmission.
