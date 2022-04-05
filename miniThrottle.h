/*
 * Use this file to centralise any defines and includes
 */
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "lcdgfx.h"
#include <Keypad.h>
#include <ESP32Encoder.h>
#include <Preferences.h>
#include <esp_partition.h>

// Project identity and version
#define PRODUCTNAME "MiniThrottle" // Branding name
#define VERSION     "0.1a"         // Version string

// Select one display board type
//#define SSD1306
#define SSD1327

// Default settings
#define MYSSID  "DCC_ESP"
#define HOST    "withrottle.local"
#define PORT    12090
#define NAME    "mThrottle"

// Uncomment these to enable debug options on startup
// #define DELAYONSTART 20000
#define SHOWPACKETSONSTART 1

// Define application things
#define MAXFUNCTIONS   30   // only expect 0-29 supported
#define MAXCONSISTSIZE  4   // max number of locomotives to drive in one session
#define NAMELENGTH     32   // Length of names for locos, turnoutss, routes etc
#define SSIDLENGTH     33   // Length to permit SSIDs and passwords
#define SORTDATA        1   // 0 = unsorted, 1 = sorted lists of data

// Select one CPU board type, adjust pin mappings as most convienent
//#define WEMOS 1
//#define DEVKIT 1

// Define Network and buffer sizes
#define WIFINETS        4   // Count of number network credentials to store
#define BUFFSIZE      250   // Keyboard buffer size
#define NETWBUFFSIZE 1024   // Network packet size
#define JMRIMAXFIELDS  64   // Max fields per JMRI received packet
#define MAXSUBTOKEN     4   // Max sub tokens per JRMI field
#define DEBOUNCEMS     33   // millis to wait for switch / keypad debounce
#define INITWAIT        5   // seconds to wait for first packet from server to ID protocol
#define BUMPCOUNT     100   // re-evalute loco count every N times through loco control routine

// Divisor for converting uSeconds to Seconds
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

// DCC++ Values
#define CALLBACKNUM 10812
#define CALLBACKSUB 22112

// Define display things
#ifdef SSD1306
// #define SCREENINVERT
#define DISPLAYADDR  0x3c   // i2c display addr
#endif
#ifdef SSD1327
#define DISPLAYADDR  0x3c   // i2c display addr
#endif

/*
 * **********  KEYPAD CONFIG  *******************************************************************
 */
// Define keypad things, Define one type of keypad
//#define keynone
//#define key1x5
#define key3x4
//#define key4x4
//#define key4x5
//
//
#ifdef key4x5
#define COLCOUNT        4
#define ROWCOUNT        5
// F1, F2 = X and Y
// Arrow keys are "U"p, "D"own, "L"eft and "R"ight
// ESC = "E" and ENT = "S"ubmit
char keymap[ROWCOUNT][COLCOUNT] = {
  { 'L', '0', 'R', 'S' },
  { '7', '8', '9', 'E' },
  { '4', '5', '6', 'D' },
  { '1', '2', '3', 'U' },
  { 'X', 'Y', '#', '*' }
};
#endif
#ifdef key4x4
#define COLCOUNT        4
#define ROWCOUNT        4
char keymap[ROWCOUNT][COLCOUNT] = {
  { '*', '0', '#', 'S' },
  { '7', '8', '9', 'E' },
  { '4', '5', '6', 'Y' },
  { '1', '2', '3', 'X' }
};
#endif
#ifdef key3x4
#define COLCOUNT        3
#define ROWCOUNT        4
char keymap[ROWCOUNT][COLCOUNT] = {
  { '*', '0', '#' },
  { '7', '8', '9' },
  { '4', '5', '6' },
  { '1', '2', '3' }
};
#endif
#ifdef key1x5
#define COLCOUNT        5
#define ROWCOUNT        1
char keymap[ROWCOUNT][COLCOUNT] = {
  { '*', '#', 'S', 'E', 'X' }
};
#endif


/*
 * **********  PIN ASSIGNMENTS  ******************************************************************
 *
 * NB: pins 34 and above are read only, do not use them for outputs, assign to switches and rotary encoders
 * 
 * ADC is used for Potentiometer throttle (Pot-Throt / POTTHROTPIN)
 *     ADC1_CH0 - GPIO36
 *     ADC1_CH1 - GPIO37
 *     ADC1_CH2 - GPIO38
 *     ADC1_CH3 - GPIO39
 *     ADC1_CH4 - GPIO32
 *     ADC1_CH5 - GPIO33
 *     ADC1_CH6 - GPIO34
 *     ADC1_CH7 - GPIO35
 * ADC2 not usable with WIFI enabled.
 * 
 * DAC is used to drive 3v voltmeter speedo
 * DAC pins are limited to 25 and 26
 * 
 */

// Define Pin Assignments
// Manually check before compiling that pins are not duplicated
#define SDA_PIN        22    // i2c SDA pin, normally 21, built-in display uses 5 <-- test val 22
#define SCK_PIN        23    // i2c SCK pin, normally 22, built-in display uses 4 <-- test val 23
// define either of TRACKPWR or TRACKPWRINV or neither. do not set both
#define TRACKPWR       02   // track power indicator, 2 = devkit on-board, 5 = lolin on-board, 16 (inverted) for module w battery
//#define TRACKPWRINV    16   // Same as TRACKPWR, but set as inverted - Define either TRACKPWR or TRACKPWRINV or neither but not both
#define ENCODE_UP      36   // encoder up bounce
#define ENCODE_DN      39   // encoder down bounce
#define ENCODE_SW      34   // encoder switch
#define DIRFWDPIN      32   // Direction sw spdt center off LOW active
#define DIRREVPIN      33   // Direction sw spdt center off LOW active
#define POTTHROTPIN    35   // Potentiometer throttle, if defined do not leave to float
#ifdef key4x5
#define MEMBR_COLS     21,19,18,05
#define MEMBR_ROWS     17,16,04,26,15
#endif
#ifdef key4x4
#define MEMBR_COLS     21,19,18,05
#define MEMBR_ROWS     17,16,04,15
#endif
#ifdef key3x4
#define MEMBR_COLS     17,16,04
#define MEMBR_ROWS     05,18,19,21
#endif
#ifdef key1x5
#define MEMBR_COLS     15,26,23,17,05
#define MEMBR_ROWS     17
#endif
// To configure a speedometer, use one of the 2 DAC pins to drive a 3v voltmeter
#define SPEEDOPIN      25
// To enable trainset mode indicator
#define TRAINSETLED    27
// To enable function key indicators
#define F1LED          14
#define F2LED          12


/*
 * **********  ENUMERATIONS  *********************************************************************
 */
// enumerations
enum directionInd { FORWARD = 0, STOP = 1, REVERSE = 2, UNCHANGED = 9 };
enum ctrlProtocol { UNDEFINED = 0, JMRI = 1, DCCPLUS = 2 };

/*
 * ***********  STRUCTURES  ***********************************************************************
 */
// Structures
struct locomotive_s {
  uint16_t id;                    // DCC address
  char type;                      // Long or short addr. 127 and below should be short, 128 and above should be long
  uint8_t throttleNr;             // Throttle Number
  int8_t direction;               // FORWARD, STOP or REVERSE
  int8_t speed;                   // 128 steps, -1, 0-126
  uint8_t steps;                  // Steps to use when updating speed
  bool owned;                     // Is loco owned y this throttle?
  char steal;                     // Is a steal required?
  char name[NAMELENGTH];          // name of loco
  uint32_t function;              // Bit Array of enabled functions
};

struct turnoutState_s {
  uint8_t state;                  // numeric state value
  char name[NAMELENGTH];          // human readible equivalent
};

struct turnout_s {
  uint8_t state;                  // numeric turnout state
  char sysName[NAMELENGTH];       // DCC address / system name
  char userName[NAMELENGTH];      // human readible name
};

struct routeState_s {
  uint8_t state;                  // numeric state value
  char name[NAMELENGTH];          // human readible equivalent
};

struct route_s {
  uint8_t state;                  // State of the route
  char sysName[NAMELENGTH];       // System name of the route
  char userName[NAMELENGTH];      // User Name
};

/*
 * Not required by Throttle, but useful debug tool for inspecting NVS
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
char *nvs_descrip[]     = { "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "String", "Blob", "BlobData", "BlobIdx", "Unused" };
