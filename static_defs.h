/*
miniThrottle, A WiThrottle/DCC-Ex Throttle for model train control

MIT License

Copyright (c) [2021-2024] [Enfield Cat]

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



/*
 * Use this file to centralise any defines and includes
 * This file should contain non-customisable definitions
 */
// #include <stdio.h>
// #include <time.h>
// Used for user interfaces
#ifndef NODISPLAY
#include "lcdgfx.h"
#include <Keypad.h>
#ifdef ENCODE_UP
#include <ESP32Encoder.h>
#endif
#endif
// Used for hardware inspection
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <rom/rtc.h>
#include <esp_timer.h>
// Used for process / thread control
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// Used for NVS access and query
#include <Preferences.h>
#include <esp_partition.h>

// Project identity and version
#ifdef PRODUCTNAME
#undef PRODUCTNAME
#endif
#ifdef VERSION
#undef VERSION
#endif
#define PRODUCTNAME "miniThrottle" // Branding name
#define VERSION     "0.8a"         // Version string

// Use either WiFi or define additional pins for second serial port to connect directly to DCC-Ex (WiFi free)
// It is expected most users will want to use miniThrottle as a WiFi device.
// Comment out "#define USEWIFI" to use direct serial connection to DCC-Ex
#ifdef USEWIFI
#include <WiFi.h>
// #include <WiFiMulti.h>
#include <ESPmDNS.h>
#endif
// Geek feature prereqs
#ifdef SHOWPARTITIONS
#include <esp_partition.h>
#endif
// Sanity check of BACKLIGHTPIN - not available on some display types
#ifdef BACKLIGHTPIN
#ifdef SSD1306
#undef BACKLIGHTPIN
#endif
#ifdef SSD1327
#undef BACKLIGHTPIN
#endif
#endif

// NB: OTA update may have limited applicability
// Name of default certificate file used for Over The Air Update using https, say for a club standard build from the club web server
#ifdef OTAUPDATE
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#ifndef FILESUPPORT
#define FILESUPPORT
#endif
#endif

// Should a web interface start and how long should it be active for?
// Comment out => no web server, 0 => always running or lifetime in minutes
//#define WEBLIFETIME   0
#ifdef WEBLIFETIME
//extern "C" {
//#include "crypto/base64.h"
#include "mbedtls/base64.h"
//}
#define CSSFILE "/miniThrottle.css"
#ifndef FILESUPPORT
#define FILESUPPORT
#endif
#endif

// Local filesystem can potentially be used to store https certificates, command sequences or icons/images
#ifdef FILESUPPORT
#define DEFAULTCONF "/sampleConfig.cfg"
#define DEFAULTCOMMAND "/sampleCommand.cfg"
#define DEFAULTAUTO "/sampleAuto.run"
#ifdef USEWIFI
#define CERTFILE "/rootCACertificate"
#ifndef NOHTTPCLIENT
#include <HTTPClient.h>
#endif
#endif
#include <FS.h>
#include <SPIFFS.h>
#define WEB_BUFFER_SIZE 4098
#define FORMAT_SPIFFS_IF_FAILED true   // format unformatted partition
#endif


/* ****************************************************************************
 * 
 * Beyond this point there probably isn't anything too interesting as far as
 * application configuration goes.
 * 
 **************************************************************************** */

// Define application things
#define MAXFUNCTIONS   30   // only expect 0-29 supported
#define MAXCONSISTSIZE  8   // max number of locomotives to drive in one session
#define NAMELENGTH     32   // Length of names for locos, turnouts, routes etc
#define SYSNAMELENGTH  16   // Length of system names for locos, turnouts, routes etc
#define SSIDLENGTH     33   // Length to permit SSIDs and passwords
#define SORTDATA        1   // 0 = unsorted, 1 = sorted lists of data
#define MAXRTSTEPS     25   // Max number of steps / waypoints in a route.
#define MAXPARAMS  (MAXRTSTEPS*2)+3   // Maximum number of parameters a serial console comand can have, 43 => route of up to 20 switches

// Define Network and buffer sizes
#define WIFINETS        6   // Count of number network credentials to store
#define BUFFSIZE      250   // Keyboard buffer size
#define NETWBUFFSIZE 1400   // Network packet size
#define WITHROTMAXFIELDS  64   // Max fields per WiThrottle received packet
#define MAXSUBTOKEN     4   // Max sub tokens per WiThrottle field
#define DEBOUNCEMS     33   // millis to wait for switch / keypad debounce
#define INITWAIT        5   // seconds to wait for first packet from server to ID protocol
#define BUMPCOUNT     100   // re-evalute loco count every N times through loco control routine
#define TIMEOUT      2000   // Network timeout in milliseconds
#define APCHANNEL       6   // Default WiFi Channel for AP mode
#define MAXAPCLIENTS    8   // Max number of access point clients
#define DEFAPCLIENTS    4   // Default number of access point clients allowed
#define MAXDIAGCONNECT  2   // Maximum number of concurrent diagnotic clients

// Divisor for converting uSeconds to Seconds
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

// command history buffer size in bytes
#ifndef COMMAND_HISTORY
#define COMMAND_HISTORY 1024
#endif
// automation data
#define LABELSIZE 16

// DCC-Ex Values
#define CALLBACKNUM 10812
#define CALLBACKSUB 22112

// Function Mapping
// Option one from WiThrottle Doc, Option two from DigiTrax SDN144PS
//#define FUNCTNAMES "Headlight~Bell~Whistle~Short Whistle~Steam Release~FX5 Light~FX6 Light~Dimmer~Mute~Water Stop~Injectors~Brake Squeal~Coupler~~~~~~~~~~~~~~~~"
#define FUNCTNAMES "Lights~Bell~Horn/Whistle~Coupler Crash~Air Feature~Dyn Brake Fans~Notch Up/Blow Down~Crossing Gate Horn~Mute~Brake Squeal~Air Horn Seq~Greaser~Safety Blow Off~~~~~~~~~~~~~~~~"
#define FUNCTLATCH 483
#define FUNCTLEADONLY 225

// default model if not specified elsewhere
// https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/hw-reference/chip-series-comparison.html
//
// Primary target is ESP32 / ESP32-DOWD
// ESP32-C3: Encoder library won't work, issues with LCDGFX - may have limited application
// ESP32-S2: Untested
// ESP32-S3: Limited testing, seems OK
#define ESP32 1
#define ESP32C3 2
#define ESP32S2 3
#define ESP32S3 4
#ifndef ESPMODEL
#define ESPMODEL ESP32
#endif
#if ESPMODEL == ESP32
#define MAXPINS 40
#elif ESPMODEL == ESPS3
#define MAXPINS 49
#elif ESPMODEL == ESPS2
#define MAXPINS 47
#else
#define MAXPINS 22
#endif

/*
 * **********  ENUMERATIONS  *********************************************************************
 */
// enumerations
enum directionInd { FORWARD = 0,   STOP = 1,     REVERSE = 2, UNCHANGED = 3 };
enum ctrlProtocol { UNDEFINED = 0, WITHROT = 1,  DCCEX = 2 };
enum varTypes     { STRING = 0,    PASSWORD = 1, INTEGER = 2 };
enum dccPower     { BOTH = 0,      MAINONLY = 1, PROGONLY = 2, JOIN = 3 };
enum dccIntegrate { LOCALINV = 0,  DCCINV = 1,   BOTHINV = 2 };
enum wifiModes    { WIFISTA = 1,   WIFIAP = 2,   WIFIBOTH = 3 };
#ifdef RELAYPORT
enum relayTypes   { NORELAY = 0,  WITHROTRELAY = 1, DCCRELAY = 2 };
#endif

/*
 * ***********  STRUCTURES  ***********************************************************************
 */
// Structures
struct locomotive_s {
  char *functionString;           // function description string for imported loco from DCC-Ex
  uint32_t function;              // Bit Array of enabled functions
  uint32_t functionLatch;         // set if latching function
  uint16_t id;                    // DCC address
  int16_t speed;                  // 128 steps, -1, 0-126
  uint8_t throttleNr;             // Throttle Number
  uint8_t steps;                  // Steps to use when updating speed
  uint8_t relayIdx;               // If relayed, which relay
  int8_t direction;               // FORWARD, STOP or REVERSE
  char steal;                     // Is a steal required?
  char type;                      // Long or short addr. 127 and below should be short, 128 and above should be long
  char name[NAMELENGTH];          // name of loco
  bool owned;                     // Is loco owned by this throttle?
  bool reverseConsist;            // Is loco reversed in a consist?
};

struct turnoutState_s {
  uint8_t state;                  // numeric state value
  char name[SYSNAMELENGTH];       // human readible equivalent
};

struct turnout_s {
  uint8_t state;                  // numeric turnout state
  char sysName[SYSNAMELENGTH];    // DCC address / system name
  char userName[NAMELENGTH];      // human readible name
};

struct routeState_s {
  uint8_t state;                  // numeric state value
  char name[SYSNAMELENGTH];       // human readible equivalent
};

struct route_s {
  uint8_t state;                  // State of the route
  char sysName[SYSNAMELENGTH];    // System name of the route
  char userName[NAMELENGTH];      // User Name
  uint8_t turnoutNr[MAXRTSTEPS];  // turnout numbers
  uint8_t desiredSt[MAXRTSTEPS];  // desired state of turnout
};

#ifdef RELAYPORT
struct relayConnection_s {
  WiFiClient *client;             // Client data
  IPAddress address;              // remote IP
  uint64_t keepAlive;             // time of last packet
  uint32_t inPackets;             // count of packets in  - receive from remote side
  uint32_t outPackets;            // count of packets out - sent to remote side
  uint8_t  id;                    // serial number of this entry, used for reverse lookup
  char nodeName[NAMELENGTH];      // remote name
  bool active;                    // expected to be active and connected
};
#endif

// structure for holding base information on many NVS values
struct nvsVar_s {
  char *varName;
  uint8_t varType;
  int16_t varMin; 
  uint16_t varMax;
  int16_t numDefault;
  char *strDefault;
  char *varDesc;
};

// structure for holding pin assignments
struct pinVar_s {
  int8_t pinNr;
  char *pinDesc;
};

/*
 * structures used by automated processes
 */
#ifndef PROCTABLESIZE
#define PROCTABLESIZE 10
#endif
#ifndef PROCNAMELENGTH
#define PROCNAMELENGTH 29
#endif
#ifndef LIFO_SIZE
#define LIFO_SIZE 10
#endif
#ifndef REGISTERCOUNT
#define REGISTERCOUNT 10
#endif
// proc table structure
struct procTable_s {
  uint16_t id;
  uint8_t state;
  char filename[PROCNAMELENGTH];
};
enum procStates  { PROCFREE = 7 , PROCDIE = 13, PROCRUN = 17, PROCTRACE = 29 };
enum pinTypes    { DIN = 0, DOUT = 1, AIN = 2, AOUT = 3, PWM = 4 , RGB = 5};
// structure for holding jump table info
struct jumpTable_s {
  uint16_t jumpTo;
  char label[LABELSIZE];
};
// structure for tokenising key words and pointing to lines
struct lineTable_s {
  char* start;
  uint8_t token;
  uint8_t sec_token; // secondary tokens also ensure word alignment of pointers
  uint16_t param;
};
// structure for local Pins
#ifndef LOCALPINCNT
#define LOCALPINCNT 20
#endif
struct localpin_s {
  uint8_t pinNr;
  uint8_t assignment;
  uint8_t channel;
};
// structure of dcc-ex sensor data
#ifndef DCCSENSORCNT
#define DCCSENSORCNT 50
#endif
enum { SENSORON, SENSOROFF, SENSORUNKNOWN };
struct dccSensor_s {
  uint32_t id;
  uint8_t  value;
};
//                          0       1       2         3        4           5       6         7        8         9          10        11        12       13      14          15
const char* runTokens[] = {"rem ", "key ", "delay ", "goto ", "waitfor ", "exit", "runfg ", "runbg ", "power ", "route ", "throw ", "close ", "sleep ", "set ", "sendcmd ", "configpin " };

/*
 * Required by Throttle functionality to get loco, turnout and route data, but also useful debug tool for inspecting NVS
 * Inspired by https://github.com/Edzelf/ESP32-Show_nvs_keys/blob/master/Show_nvs_keys.ino
 * Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
 */
struct nvs_entry
{
  uint8_t  Ns ;         // Namespace ID
  uint8_t  Type ;       // Type of value
  uint8_t  Span ;       // Number of entries used for this item
  uint8_t  Rvs ;        // Reserved, should be 0xFF
  uint32_t CRC ;        // CRC
  char     Key[16] ;    // Key in Ascii
  union {
    uint64_t sixFour;
    uint32_t threeTwo[2];
    uint16_t oneSix[4];
    uint8_t  eight[8];
  } Data ;       // Data in entry 
} ;

struct nvs_page                                     // For nvs entries
{                                                   // 1 page is 4096 bytes
  uint32_t  State ;
  uint32_t  Seqnr ;
  
  uint32_t  Unused[5] ;
  uint32_t  CRC ;
  uint8_t   Bitmap[32] ;
  nvs_entry Entry[126] ;
} ;

uint8_t nvs_index_ref[] = { 0x01,      0x02,       0x04,       0x08,       0x11,     0x12,      0x14,      0x18,      0x21,     0x41,   0x42,       0x48,      0xff};
const char *nvs_descrip[]     = { "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "String", "Blob", "BlobData", "BlobIdx", "Unused" };
