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
    { (char*)"tname",          STRING,     4, sizeof(tname),            0,       (char*) NAME, (char*)"Device Name" },
    { (char*)"cpuspeed",       INTEGER,    0,           240,            0,       (char*)   "", (char*)"CPU Speed" },
    { (char*)"inventoryLoco",  INTEGER,    0,             2,            0,       (char*)   "", (char*)"DCC_Ex loco inventory"},
    { (char*)"inventoryTurn",  INTEGER,    0,             2,            0,       (char*)   "", (char*)"DCC_Ex turnouts inventory"},
    { (char*)"inventoryRout",  INTEGER,    0,             2,            0,       (char*)   "", (char*)"DCC_Ex routes inventory"},
    { (char*)"diagPort",       INTEGER,   10,         65500,           23,       (char*)   "", (char*)"Diagnostic port"},
    { (char*)"mainMenuMask",   INTEGER,    0,            15,            0,       (char*)   "", (char*)"Main Menu Mask"},
    #ifndef SERIALCTRL
    { (char*)"clientTimeout",   INTEGER,   20,        120000,         5000,       (char*)   "", (char*)"WiFi client timeout"},
    #endif
    //{ (char*)"disableCabMenu", INTEGER,    0,             1,            0,       (char*)   "", (char*)"Disable Cab menu"},
    #ifdef DELAYONSTART
    { (char*)"delayOnStart",   INTEGER,    0,           120, DELAYONSTART,       (char*)   "", (char*)"Start up delay in seconds" },
    #endif
    #ifndef NODISPLAY
    { (char*)"screenRotate",   INTEGER,    0,             3,            0,       (char*)   "", (char*)"Screen Rotation" },
    { (char*)"fontIndex",      INTEGER,    0,  wwwFontWidth,            1,       (char*)   "", (char*)"Font ID" },
    { (char*)"backlightValue", INTEGER,  100,           255,          200,       (char*)   "", (char*)"Screen Brightness" },
    { (char*)"warnSpeed",      INTEGER,   50,           101,           90,       (char*)   "", (char*)"Warning Speed" },
    { (char*)"allMenuItems",   INTEGER,    0,             1,            0,       (char*)   "", (char*)"Display all menu items" },
    { (char*)"menuWrap",       INTEGER,    0,             1,            0,       (char*)   "", (char*)"Menus wrap around" },
    { (char*)"noPwrTurnouts",  INTEGER,    0,             1,            0,       (char*)   "", (char*)"turnouts work without track power" },
    #ifdef SCREENSAVER
    { (char*)"screenSaver",    INTEGER,    0,           600,  SCREENSAVER,       (char*)   "", (char*)"Screen Saver Timeout" },
    #endif
    #endif
    { (char*)"debounceTime",   INTEGER,   10,           100,   DEBOUNCEMS,       (char*)   "", (char*)"Debounce mS" },
    { (char*)"detentCount",    INTEGER,    1,            10,            2,       (char*)   "", (char*)"Detent Count" },
    { (char*)"buttonStop",     INTEGER,    0,             1,            0,       (char*)   "", (char*)"Encoder Button Stop" },
    { (char*)"speedStep",      INTEGER,    1,            20,            4,       (char*)   "", (char*)"Speed Change Step" },
    { (char*)"sortData",       INTEGER,    0,             1,            1,       (char*)   "", (char*)"Enable Sorting" },
    { (char*)"clockFormat",    INTEGER,    0,             2,            0,       (char*)   "", (char*)"Fast Clock Format" },
    { (char*)"bidirectional",  INTEGER,    0,             1,            0,       (char*)   "", (char*)"standard/bidirectional mode" },
    #ifdef BRAKEPRESPIN
    { (char*)"brakeup",        INTEGER,    1,            10,            1,       (char*)   "", (char*)"Brake Up Rate" },
    { (char*)"brakedown",      INTEGER,    1,           100,           20,       (char*)   "", (char*)"Brake Down Rate" },
    #endif
    { (char*)"funcOverlay",    STRING,    16,            31,            0, (char*) FUNCOVERLAY, (char*)"Function Overlay" },
    { (char*)"routeDelay",     INTEGER,    0, ((sizeof(routeDelay)/sizeof(uint16_t))-1), 2, (char*)"", (char*)"Delay between Route Steps" },
    #ifdef OTAUPDATE
    { (char*)"ota_url",        STRING,    16,           120,            0,  (char*) OTAUPDATE, (char*)"OTA update URL" },
    #endif
    { (char*)"mdns",           INTEGER,    0,             1,            1,       (char*)   "", (char*)"mDNS Search Endabled" },
    { (char*)"defaultProto",   INTEGER, WITHROT,      DCCEX,      WITHROT,        (char*)  "", (char*)"Preferred Protocol" },
    { (char*)"toOffset",       INTEGER,    1,          1000,          100,       (char*)   "", (char*)"turnout numbering starts at" },
    #ifdef RELAYPORT
    { (char*)"maxRelay",       INTEGER,    0,      MAXRELAY,   MAXRELAY/2,       (char*)   "", (char*)"Max nodes to relay" },
    { (char*)"relayMode",      INTEGER,    0,             2,            1,       (char*)   "", (char*)"Relay mode" },
    { (char*)"relayPort",      INTEGER,    1,         65534,    RELAYPORT,       (char*)   "", (char*)"Relay port" },
    { (char*)"fastclock2dcc",  INTEGER,    0,             1,            0,       (char*)   "", (char*)"send fastclock to dcc-ex" },
    #ifdef FC_HOUR
    { (char*)"fc_hour",        INTEGER,    0,            23,      FC_HOUR,       (char*)   "", (char*)"fastclock hour" },
    #endif
    #ifdef FC_MIN
    { (char*)"fc_min",         INTEGER,    0,            59,       FC_MIN,       (char*)   "", (char*)"fastclock minute" },
    #endif
    #ifdef FC_RATE
    { (char*)"fc_rate",        INTEGER,    0,          1000,      FC_RATE,       (char*)   "", (char*)"fastclock speed-up rate" },
    #endif
    #endif
    #ifdef WEBLIFETIME
    { (char*)"webPort",        INTEGER,    1,         65534,      WEBPORT,       (char*)   "", (char*)"Web server port"},
    { (char*)"webuser",        STRING,     4,            16,            0,   (char*)  WEBUSER, (char*)"Web Admin Name" },
    { (char*)"webpass",        STRING,     4,            16,            0,   (char*)  WEBPASS, (char*)"Web Admin Password" },
    { (char*)"webTimeOut",     INTEGER,    0,           600,  WEBLIFETIME,       (char*)   "", (char*)"Web Server Timeout" },
    { (char*)"webStatus",      INTEGER,    0,             2,            0,       (char*)   "", (char*)"Device descript at start or end of status page" },
    { (char*)"webPwrSwitch",   INTEGER,    0,             1,            1,       (char*)   "", (char*)"Power on/off from web page" },
    #endif
    #ifdef WEBCACHE
    { (char*)"cacheTimeout",   INTEGER,    5,          1440,     WEBCACHE,       (char*)   "", (char*)"Static web content cache time"},
    #endif
    #ifdef WEBREFRESH
    { (char*)"webRefresh",     INTEGER,    0,          3600,   WEBREFRESH,       (char*)   "", (char*)"Status page refresh time" },
    #endif
    { (char*)"dccPower",       INTEGER,    0,          JOIN,         BOTH,       (char*)   "", (char*)"Outputs to enable on power-on" },
    { (char*)"dccRtError",     INTEGER,    0,             1,            0,       (char*)   "", (char*)"Stop route setup on error" },
    { (char*)"dccRmLoco",      INTEGER,    0,             1,            0,       (char*)   "", (char*)"Delete locos on DCC-Ex when not in use" },
    { (char*)"staConnect",     INTEGER,    0,             3,            2,       (char*)   "", (char*)"Wifi Station selection criteria" },
    { (char*)"APname",         STRING,     4,            32,            0,      (char*)  NAME, (char*)"Access point name" },
    { (char*)"APpass",         STRING,     4,            32,            0,     (char*) "none", (char*)"Access point password" },
    { (char*)"apChannel",      INTEGER,    1,            13,            6,       (char*)   "", (char*)"Access point channel" },
    { (char*)"apClients",      INTEGER,    1,             8,            4,       (char*)   "", (char*)"Max access point clients" }
  };



  /* **********************************************************************\
   *
   * Define an array of hardware config, so we can check for HW clashes
   *
  \********************************************************************** */
  struct pinVar_s pinVars[] = {
    #ifdef DCCTX
    { DCCTX,        (char*)"DCC - Tx" },
    #endif
    #ifdef DCCRX
    { DCCRX,        (char*)"DCC - Rx" },
    #endif
    #ifdef SDA_PIN
    { SDA_PIN,      (char*)"I2C - SDA" },
    #endif
    #ifdef SCK_PIN
    { SCK_PIN,      (char*)"I2C - SCK" },
    #endif
    #ifdef SPI_RESET
    { SPI_RESET,    (char*)"SPI - Reset" },
    #endif
    #ifdef SPI_CS
    { SPI_CS,       (char*)"SPI - CS" },
    #endif
    #ifdef SPI_DC
    { SPI_DC,       (char*)"SPI - DC" },
    #endif
    #ifdef SPI_SCL
    { SPI_SCL,      (char*)"SPI - Clock/SCL" },
    #endif
    #ifdef SPI_SDA
    { SPI_SDA,      (char*)"SPI - Data/SDA" },
    #endif
    #ifdef BACKLIGHTPIN
    { BACKLIGHTPIN, (char*)"Backlight" },
    #ifdef BACKLIGHTREF
    { BACKLIGHTREF, (char*)"Backlight Ref" },
    #endif
    #endif
    #ifdef ENCODE_UP
    { ENCODE_UP,    (char*)"Encoder Up" },
    #endif
    #ifdef ENCODE_DN
    { ENCODE_DN,    (char*)"Encoder Down" },
    #endif
    #ifdef ENCODE_SW
    { ENCODE_SW,    (char*)"Encoder Switch" },
    #endif
    #ifdef DIRFWDPIN
    { DIRFWDPIN,    (char*)"Forward Switch" },
    #endif
    #ifdef DIRREVPIN
    { DIRRWVPIN,    (char*)"Reverse Switch" },
    #endif
    #ifdef TRACKPWR
    { TRACKPWR,     (char*)"Trk Power LED" },
    #endif
    #ifdef TRACKPWRINV
    { TRACKPWRINV,  (char*)"Trk Power LED" },
    #endif
    #ifdef TRAINSETLEN
    { TRAINSETPIN,  (char*)"Bidirectional LED" },
    #endif
    #ifdef F1LED
    { F1LED,        (char*)"Func 10x LED" },
    #endif
    #ifdef F2LED
    { F2LED,        (char*)"Func 20x LED" },
    #endif
    #ifdef SPEEDOPIN
    { SPEEDOPIN,    (char*)"Speedometer Out" },
    #endif
    #ifdef BRAKEPRESPIN
    { BRAKEPRESPIN, (char*)"Brake Pres Out" },
    #endif
    #ifdef POTTHROTPIN
    { POTTHROTPIN,  (char*)"Throttle Poten" },
    #endif
    #ifndef keynone
    #endif
    { 1,            (char*)"Console - Tx" },
    { 3,            (char*)"Console - Rx" }
  }; 
