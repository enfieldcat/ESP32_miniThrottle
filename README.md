ESP32 based WiThrottle (miniThrottle)
=====================================
Development for model train throttle controller.
It uses either WiThrottle or DCC-Ex protocols to connect to a control station over WiFi.
The system has 2 levels of configuration:
1. Hardcoded: eg to define I/O pins of the processor or which display type to support. These settings are not expected to change over the lifetime of the miniThrottle.
2. Run time: eg to change what WiFi password to use when the WiFi Network changes. Run time settings can be set via eith a web interface or by connecting to the serial port.

If using DCC-Ex a direct serial connection (WiFi-Free) connection is possible.
Such a direct serial connection may provide sufficient control for small layouts with a panel mounted throttle, especially if it is configured to act as a relay.
The unit supports up to 8 concurrent connections when relaying. ie total of relay, web and diagnostic connections.

Status:
-------
More documentation at https://camelthorn.cloud/miniThrottle/miniIntro.php

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
The web interface allows most run time settings to be set.
Advanced setup and diagnostics can be set and viewed using 115200 baud serial interface, this is generally the USB interface on most Esp32 modules.

In bidirectional mode, the mid point of the potentiometer slider is stop, down is reverse speed, and up is forward speed. Or when using the encoder to adjust speed rotating past stop will reverse the direction of movement.

Configuration:
--------------
On initial start the unit will run in both WiFi AP and Station modes.
This allows the user to connect via the AP to http://192/168.4.1 to perform the initial set up - default credentials are admin:mysecret.
Alternatively use a serial terminal to connect to a "console" which will allow WiFi networks to be defined. Multiple networks can be defined.
The serial terminal can also be used to define the locomotive roster and turnouts when using a direct connection to DCC-Ex.

The serial terminal to console runs at 115200, 8 bits, no parity, 1 stop bit. Line terminator is line feed (LF) character.
Run "help summary" at prompt to get list of configuration commands.

If using serial direct to DCC-Ex a second serial port is opened for this and also runs at 115200 bits per second.

For hardware configuration and setting of default values, edit miniThrottle.h - these should not change over time.
Configurable settings are stored in Non Volatile memory.
