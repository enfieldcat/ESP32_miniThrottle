ESP32 based JMRI miniThrottle
=============================
Development for model train throttle controller.

Status:
-------
Massively unstable and mostly untested (Mar 2022). Do not use this code:
* At best it will do something unexpected
* At worst it will damage something

You have been warned.

This project is on its early development stages and subject to change. Functions and structure may be added or removed without notice.

Description:
------------
Uses a esp32 module as the core for a JMRI (Java Model Railroad Interface) throttle using the WiThrotttle protocol.
It has some limited DCC++ support, to communicate directly to DCC++ without the need of a control station.
It can use one of several display types. The prototype tests against SSD106 and SSD1327 over I2C interface.
The supported display should be any supported by the lcdgfx library, although a minimum display width of 128 pixels is recommended.
Rotary encoder support is provided using the ESP32Encoder library.
The display and rotary encoder from the minimum configuration, but a membrane keypad is a highly recommended to enable seleting functions.
Several different keypad combinations are supported including 3x4, 4x4, 5x4 combinations.
Other optional hardware includes 3 position switch (reversing lever), 3v voltmeter (speedometer), slide potentiometer (thottle lever), various LED indicators (track power, bi-directional (trainset) mode, function +10 and function +20.

The thottle should allow locomotives, turnouts and routes to be controlled.
Advanced setup and diagnostics can be set and viewed using 115200 baud serial interface.

In trainset mode, the mid point of the potentiometer slider is stop, down is reverse speed, and up is forward speed. Or when using the encoder to adjust speed rotating past stop will reverse the direction of movement.


Future Works:
-------------
* testing and debugging, including testing on more display types
* Add documentation to https://conferre.cf
* Add configuration options to device menu.
* Support for protocols other than JMRI: SRCP, DCC++ (?)
* Support for consisting


Tested interfaces:
------------------
* JMRI: ESP_DCC https://www.instructables.com/DCC-Controller-2-Boards-1-PSU-No-Soldering/
* DCC++ : https://github.com/DccPlusPlus/BaseStation/wiki

It is expected to work but NOT TESTED using JRMI https://www.jmri.org/ and Digitrax LNWI (LocoNet WiFi Interface)/
