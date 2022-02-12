ESP32 based JMRI miniThrottle
=============================

Status:
-------
Massively unstable and mostly untested (Feb 2022)

Description:
------------
Uses a esp32 module as the core for a JMRI (Java Model Railroad Interface) throttle using the WiThrotttle protocol.
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
* testing and debugging
* Add documentation to https://conferre.cf
* Support for protocols other than JMRI: SRCP, DCC++ (?)
* Support for consisting
