ESP32 based JMRI miniThrottle
=============================
Development for model train throttle controller.
It uses either JRMI or DCC++ protocols to conect to a control station.
The default is configurable.

Status:
-------
More of code is tested and debugged (April 2022), but still a work in progress. Use at own risk:
* At best it might do something unexpected
* At worst it will damage something

You have been warned.

This project is on its early development stages and subject to change. Functions and structure may be added or removed without notice.

Description:
------------
Uses a esp32 module as the core for a JMRI (Java Model Railroad Interface), or DCC++ throttle using the WiThrotttle protocol.
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

Configuration:
--------------
Use a serial terminal to connect to a "console" which will allow WiFi networks to be defined. Up to 4 networks can be defined.
The serial terminal can also be used to define the locomotive roster and turnouts when using a direct connection to DCC++.

The serial terminal runs at 115200, 8 bits, no parity, 1 stop bit. Line terminator is line feed (LF) character.
Run "help summary" at prompt to get list of configuration commands.

For hardware configuration and setting of default values, edit miniThrottle.h - these should not change over time.
Configurable settings are stored in Non Volatile memory.

Future Works:
-------------
* testing and debugging, including testing on more display types
* Add documentation to https://conferre.cf
* Improve configuration options to device menu.
* Support for protocols other than JMRI: SRCP, DCC++ (?)
* Support for consisting


Tested interfaces:
------------------
* JMRI: ESP_DCC https://www.instructables.com/DCC-Controller-2-Boards-1-PSU-No-Soldering/
* DCC++ : https://github.com/DccPlusPlus/BaseStation/wiki

It is expected to work but NOT TESTED using JRMI https://www.jmri.org/ and Digitrax LNWI (LocoNet WiFi Interface)/
