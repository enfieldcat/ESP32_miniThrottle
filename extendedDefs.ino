/*
miniThrottle, A WiThrottle/DCC-Ex Throttle for model train control

MIT License

Copyright (c) [2021-2023] [Enfield Cat]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/



  /* **********************************************************************\
   *
   * Extended vars, file name sequence should be after display routine
   *                 but before Web, NVS or serial console
   *                 since Arduino compiles in alpha numeric seq except for projet name.ino
   *
  \********************************************************************** */
  /* **********************************************************************\
   *
   * Define variables stored in NVS
   * struct nvsVar_s {
   *   char *varName;
   *   uint8_t varType;
   *   int16_t varMin;
   *   uint16_t varMax;
   *   int16_t numDefault;
   *   char *strDefault;
   *   char *varDesc;
   * };
   *
  \********************************************************************** */
  #ifdef NODISPLAY
  uint16_t wwwFontWidth = 0;
  #else
  uint16_t wwwFontWidth = sizeof(fontWidth);
  #endif
  struct nvsVar_s nvsVars[] = {
    { "tname",          STRING,     4, sizeof(tname),            0,        NAME, "Device Name" },
    { "cpuspeed",       INTEGER,    0,           240,            0,          "", "CPU Speed" },
    { "inventoryLoco",  INTEGER,    0,             2,            0,          "", "DCC_Ex loco inventory"},
    { "inventoryTurn",  INTEGER,    0,             2,            0,          "", "DCC_Ex turnouts inventory"},
    { "inventoryRout",  INTEGER,    0,             2,            0,          "", "DCC_Ex routes inventory"},
    { "diagPort",       INTEGER,   10,         65500,           23,          "", "Diagnostic port"},
    // { "diagPortEnable", INTEGER,    0,             1,            0,          "", "Diagnostic port enabled"},
    #ifdef DELAYONSTART
    { "delayOnStart",   INTEGER,    0,           120, DELAYONSTART,          "", "Start up delay in seconds" },
    #endif
    #ifndef NODISPLAY
    { "screenRotate",   INTEGER,    0,             3,            0,          "", "Screen Rotation" },
    { "fontIndex",      INTEGER,    0,  wwwFontWidth,            1,          "", "Font ID" },
    { "backlightValue", INTEGER,  100,           255,          200,          "", "Screen Brightness" },
    { "warnSpeed",      INTEGER,   50,           101,           90,          "", "Warning Speed" },
    { "allMenuItems",   INTEGER,    0,             1,            0,          "", "Display all menu items" },
    { "menuWrap",       INTEGER,    0,             1,            0,          "", "Menus wrap around" },
    { "noPwrTurnouts",  INTEGER,    0,             1,            0,          "", "turnouts work without track power" },
    #ifdef SCREENSAVER
    { "screenSaver",    INTEGER,    0,           600,  SCREENSAVER,          "", "Screen Saver Timeout" },
    #endif
    #endif
    { "debounceTime",   INTEGER,   10,           100,   DEBOUNCEMS,          "", "Debounce mS" },
    { "detentCount",    INTEGER,    1,            10,            2,          "", "Detent Count" },
    { "buttonStop",     INTEGER,    0,             1,            0,          "", "Encoder Button Stop" },
    { "speedStep",      INTEGER,    1,            20,            4,          "", "Speed Change Step" },
    { "sortData",       INTEGER,    0,             1,            1,          "", "Enable Sorting" },
    { "clockFormat",    INTEGER,    0,             2,            0,          "", "Fast Clock Format" },
    { "bidirectional",  INTEGER,    0,             1,            0,          "", "standard/bidirectional mode" },
    #ifdef BRAKEPRESPIN
    { "brakeup",        INTEGER,    1,            10,            1,          "", "Brake Up Rate" },
    { "brakedown",      INTEGER,    1,           100,           20,          "", "Brake Down Rate" },
    #endif
    { "funcOverlay",    STRING,    16,            31,            0, FUNCOVERLAY, "Function Overlay" },
    { "routeDelay",     INTEGER,    0, (sizeof(routeDelay)/sizeof(uint16_t))-1, 2, "", "Delay between Route Steps" },
    #ifdef OTAUPDATE
    { "ota_url",        STRING,    16,           120,            0,   OTAUPDATE, "OTA update URL" },
    #endif
    { "mdns",           INTEGER,    0,             1,            1,          "", "mDNS Search Endabled" },
    { "defaultProto",   INTEGER, WITHROT,      DCCEX,      WITHROT,          "", "Preferred Protocol" },
    { "toOffset",       INTEGER,    1,          1000,          100,          "", "turnout numbering starts at" },
    #ifdef RELAYPORT
    { "maxRelay",       INTEGER,    0,      MAXRELAY,   MAXRELAY/2,          "", "Max nodes to relay" },
    { "relayMode",      INTEGER,    0,             2,            1,          "", "Relay mode" },
    { "relayPort",      INTEGER,    1,         65534,    RELAYPORT,          "", "Relay port" },
    { "fastclock2dcc",  INTEGER,    0,             1,            0,          "", "send fastclock to dcc-ex" },
    #ifdef FC_HOUR
    { "fc_hour",        INTEGER,    0,            23,      FC_HOUR,          "", "fastclock hour" },
    #endif
    #ifdef FC_MIN
    { "fc_min",         INTEGER,    0,            59,       FC_MIN,          "", "fastclock minute" },
    #endif
    #ifdef FC_RATE
    { "fc_rate",        INTEGER,    0,          1000,      FC_RATE,          "", "fastclock speed-up rate" },
    #endif
    #endif
    #ifdef WEBLIFETIME
    { "webPort",        INTEGER,    1,         65534,      WEBPORT,          "", "Web server port"},
    { "webuser",        STRING,     4,            16,            0,     WEBUSER, "Web Admin Name" },
    { "webpass",        STRING,     4,            16,            0,     WEBPASS, "Web Admin Password" },
    { "webTimeOut",     INTEGER,    0,           600,  WEBLIFETIME,          "", "Web Server Timeout" },
    { "webStatus",      INTEGER,    0,             2,            0,          "", "Device descript at start or end of status page" },
    { "webPwrSwitch",   INTEGER,    0,             1,            1,          "", "Power on/off from web page" },
    #endif
    #ifdef WEBCACHE
    { "cacheTimeout",   INTEGER,    5,          1440,     WEBCACHE,          "", "Static web content cache time"},
    #endif
    #ifdef WEBREFRESH
    { "webRefresh",     INTEGER,    0,          3600,   WEBREFRESH,          "", "Status page refresh time" },
    #endif
    { "dccPower",       INTEGER,    0,          JOIN,         BOTH,          "", "Outputs to enable on power-on" },
    { "dccRtError",     INTEGER,    0,             1,            0,          "", "Stop route setup on error" },
    { "dccRmLoco",      INTEGER,    0,             1,            0,          "", "Delete locos on DCC-Ex when not in use" },
    { "staConnect",     INTEGER,    0,             3,            2,          "", "Wifi Station selection criteria" },
    { "APname",         STRING,     4,            32,            0,        NAME, "Access point name" },
    { "APpass",         STRING,     4,            32,            0,      "none", "Access point password" },
    { "apChannel",      INTEGER,    1,            13,            6,          "", "Access point channel" },
    { "apClients",      INTEGER,    1,             8,            4,          "", "Max access point clients" }
  };



  /* **********************************************************************\
   *
   * Define an array of hardware config, so we can check for HW clashes
   *
  \********************************************************************** */
  struct pinVar_s pinVars[] = {
    #ifdef DCCTX
    { DCCTX,        "DCC - Tx" },
    #endif
    #ifdef DCCRX
    { DCCRX,        "DCC - Rx" },
    #endif
    #ifdef SDA_PIN
    { SDA_PIN,      "I2C - SDA" },
    #endif
    #ifdef SCK_PIN
    { SCK_PIN,      "I2C - SCK" },
    #endif
    #ifdef SPI_RESET
    { SPI_RESET,    "SPI - Reset" },
    #endif
    #ifdef SPI_CS
    { SPI_CS,       "SPI - CS" },
    #endif
    #ifdef SPI_DC
    { SPI_DC,       "SPI - DC" },
    #endif
    #ifdef SPI_SCL
    { SPI_SCL,      "SPI - Clock/SCL" },
    #endif
    #ifdef SPI_SDA
    { SPI_SDA,      "SPI - Data/SDA" },
    #endif
    #ifdef BACKLIGHTPIN
    { BACKLIGHTPIN, "Backlight" },
    #ifdef BACKLIGHTREF
    { BACKLIGHTREF, "Backlight Ref" },
    #endif
    #endif
    #ifdef ENCODE_UP
    { ENCODE_UP,    "Encoder Up" },
    #endif
    #ifdef ENCODE_DN
    { ENCODE_DN,    "Encoder Down" },
    #endif
    #ifdef ENCODE_SW
    { ENCODE_SW,    "Encoder Switch" },
    #endif
    #ifdef DIRFWDPIN
    { DIRFWDPIN,    "Forward Switch" },
    #endif
    #ifdef DIRREVPIN
    { DIRRWVPIN,    "Reverse Switch" },
    #endif
    #ifdef TRACKPWR
    { TRACKPWR,     "Trk Power LED" },
    #endif
    #ifdef TRACKPWRINV
    { TRACKPWRINV,  "Trk Power LED" },
    #endif
    #ifdef TRAINSETLEN
    { TRAINSETPIN,  "Bidirectional LED" },
    #endif
    #ifdef F1LED
    { F1LED,        "Func 10x LED" },
    #endif
    #ifdef F2LED
    { F2LED,        "Func 20x LED" },
    #endif
    #ifdef SPEEDOPIN
    { SPEEDOPIN,    "Speedometer Out" },
    #endif
    #ifdef BRAKEPRESPIN
    { BRAKEPRESPIN, "Brake Pres Out" },
    #endif
    #ifdef POTTHROTPIN
    { POTTHROTPIN,  "Throttle Poten" },
    #endif
    #ifndef keynone
    #endif
    { 1,            "Console - Tx" },
    { 3,            "Console - Rx" }
  }; 

