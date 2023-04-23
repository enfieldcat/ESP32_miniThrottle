ESP32 based WiThrottle (miniThrottle)
=====================================
Development for model train throttle controller.
It uses either WiThrttle or DCC-Ex protocols to connect to a control station over WiFi.
The default is configurable.
If using DCC-Ex a direct serial connection (WiFi-Free) connection is possible. This may provide sufficient control for small layouts with a panel mounted throttle.

Status:
-------
More documentation at https://conferre.cf/miniThrottle/miniIntro.php

Description:
------------
Uses a esp32 module as the core for a WiThrottle (JMRI / Java Model Railroad Interface), or DCC-Ex throttle using the WiThrotttle protocol.
It can use one of several display types.
The supported display should be any supported by the lcdgfx library, although a minimum display width of 128 pixels is recommended.
Rotary encoder support is provided using the ESP32Encoder library.
The display and rotary encoder form the minimum configuration, but a membrane keypad is a highly recommended to enable seleting functions.
Several different keypad combinations are supported including 3x4, 4x4, 5x4 combinations.
Other optional hardware includes 3 position switch (reversing lever), 3v voltmeter (speedometer), slide potentiometer (thottle lever), various LED indicators (track power, bidirectional (trainset) mode, function +10 and function +20.

The thottle allows locomotives, turnouts and routes to be controlled.
Advanced setup and diagnostics can be set and viewed using 115200 baud serial interface, this is generally the USB interface on most Esp32 modules.

In bidirectional mode, the mid point of the potentiometer slider is stop, down is reverse speed, and up is forward speed. Or when using the encoder to adjust speed rotating past stop will reverse the direction of movement.

Configuration:
--------------
Use a serial terminal to connect to a "console" which will allow WiFi networks to be defined. Multiple networks can be defined.
The serial terminal can also be used to define the locomotive roster and turnouts when using a direct connection to DCC-Ex.

The serial terminal to console runs at 115200, 8 bits, no parity, 1 stop bit. Line terminator is line feed (LF) character.
Run "help summary" at prompt to get list of configuration commands.

If using serial direct to DCC-Ex a second serial port is opened for this and also runs at 115200 bits per second.

For hardware configuration and setting of default values, edit miniThrottle.h - these should not change over time.
Configurable settings are stored in Non Volatile memory.


Tested interfaces:
------------------
* WiThrottle: ESP_DCC https://www.instructables.com/DCC-Controller-2-Boards-1-PSU-No-Soldering/
* DCC-Ex : https://github.com/DccPlusPlus/BaseStation/wiki
* Itself in relay mode

It is expected to work but NOT TESTED using JMRI https://www.jmri.org/ and Digitrax LNWI (LocoNet WiFi Interface)/
