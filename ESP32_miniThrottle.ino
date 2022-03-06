
/*
 * Reference Page: https://www.jmri.org/help/en/package/jmri/jmrit/withrottle/Protocol.shtml
 */
#include "miniThrottle.h"

// Initialize the OLED display using i2c interface, Adjust according to display device
// DisplaySSD1306_128x64_I2C display(-1); // or (-1,{busId, addr, scl, sda, frequency})
#ifdef SSD1306
DisplaySSD1306_128x64_I2C display (-1,{0, DISPLAYADDR, SCK_PIN, SDA_PIN, -1});
#else
#ifdef SSD1327
DisplaySSD1327_128x128_I2C display (-1,{0, DISPLAYADDR, SCK_PIN, SDA_PIN, -1});
#endif
#endif

// WiFi Server Definitions
WiFiClient client;
static SemaphoreHandle_t transmitSem = xSemaphoreCreateMutex();
static SemaphoreHandle_t displaySem  = xSemaphoreCreateMutex();
static QueueHandle_t keyboardQueue   = xQueueCreate (10, sizeof(char));   // Queue for keyboard type of events
static QueueHandle_t keyReleaseQueue = xQueueCreate (10, sizeof(char));   // Queue for keyboard release type of events
static struct locomotive_s   *locoRoster   = (struct locomotive_s*) malloc (sizeof(struct locomotive_s) * MAXCONSISTSIZE);
static struct turnoutState_s *turnoutState = NULL;
static struct turnout_s      *turnoutList  = NULL;
static struct routeState_s   *routeState   = NULL;
static struct route_s        *routeList    = NULL;
static int screenWidth      = 0;
static int screenHeight     = 0;
static int keepAliveTime    = 10;
static uint16_t initialLoco = 3;
static uint8_t locomotiveCount      = 0;
static uint8_t switchCount          = 0;
static uint8_t turnoutStateCount    = 0;
static uint8_t routeCount           = 0;
static uint8_t routeStateCount      = 0;
static uint8_t lastMainMenuOption   = 0;
static uint8_t lastLocoMenuOption   = 0;
static uint8_t lastSwitchMenuOption = 0;
static uint8_t lastRouteMenuOption  = 0;
static uint8_t charsPerLine;
static uint8_t linesPerScreen;
static uint8_t debounceTime = DEBOUNCEMS;
static uint8_t cmdProtocol = UNDEFINED;
static uint8_t nextThrottle = 'A';
static char ssid[SSIDLENGTH];
static char tname[SSIDLENGTH];
static char remServerType[10] = { "" };  // eg: JMRI
static char remServerDesc[32] = { "" };  // eg: JMRI My whizzbang server v 1.0.4
static char remServerNode[32] = { "" };  // eg: 192.168.6.1
static char lastMessage[40]   = { "" };  // eg: JMRI: address 'L23' not allowed as Long
static bool configHasChanged = false;
static bool showPackets      = false;
static bool showKeepAlive    = false;
static bool showKeypad       = false;
static bool trackPower       = false;
static bool refreshDisplay   = true;
static bool drivingLoco      = false;
static bool initialDataSent  = false;
static bool trainSetMode     = false;
static bool menuMode         = false;
static bool funcChange       = true;

const char prevMenuOption[] = { "Prev. Menu"};
const char *protoList[]     = { "Undefined", "JMRI", "DCC++" };

/*
 * Supported fonts
 * Micro: digital_font5x7
 * Small: ssd1306xled_font6x8
 * Std:   ssd1306xled_font8x16
 * Large: free_calibri11x12
 */
const char *fontLabel[] = {"Small", "Std", "Large", "Wide"};
const uint8_t fontWidth[]  = { 6,8,10,18 };
const uint8_t fontHeight[] = { 8,16,20,18 };
// selected values being:
static uint8_t selFontWidth  = 8;
static uint8_t selFontHeight = 16;


/*
 * Set up steps
 * * Initialise console connection
 * * Load basic config
 * * Start subtasks to handle I/O
 */
void setup()  {
  Serial.begin(115200);
  for (uint8_t n=0; n<MAXCONSISTSIZE; n++) {
    strcpy (locoRoster[n].name, "Void");
    locoRoster[n].owned = false;
  }
  // load initial settings from Non Volatile Storage (NVS)
  nvs_init();
  nvs_get_string ("tname", tname, NAME, sizeof(tname));
  debounceTime = nvs_get_int ("debounceTime", DEBOUNCEMS);
  if (nvs_get_int ("trainSetMode", 0) == 1) trainSetMode = true;;
  #ifdef SHOWPACKETSONSTART
  showPackets = true;
  #endif
  // Also change CPU speed before starting wireless comms
  int cpuSpeed = nvs_get_int ("cpuspeed", 0);
  if (cpuSpeed > 0) {
    if (cpuSpeed < 80) cpuSpeed = 80; 
    Serial.print ("Setting CPU speed to ");
    Serial.print (cpuSpeed);
    Serial.println ("MHz");
    setCpuFrequencyMhz (cpuSpeed);
  }
  // Configure I/O pins
  #ifdef TRACKPWR
  pinMode(TRACKPWR, OUTPUT); digitalWrite(TRACKPWR, LOW);
  #endif
  #ifdef TRACKPWRINV
  pinMode(TRACKPWRINV, OUTPUT); digitalWrite(TRACKPWRINV, HIGH);
  #endif
  #ifdef F1LED
  pinMode(F1LED, OUTPUT); digitalWrite(F1LED, LOW);
  #endif
  #ifdef F2LED
  pinMode(F2LED, OUTPUT); digitalWrite(F2LED, LOW);
  #endif
  #ifdef TRAINSETLED
  pinMode(TRAINSETLED, OUTPUT);
  if (trainSetMode) digitalWrite(TRAINSETLED, HIGH);
  else digitalWrite(TRAINSETLED, LOW);
  #endif
  // Print a diagnostic to the console, Prior to starting tasks no semaphore required
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   (PRODUCTNAME);
    Serial.print   (" ");
    Serial.println (VERSION);
    Serial.print   ("Compile time: ");
    Serial.print   (__DATE__);
    Serial.print   (" ");
    Serial.println (__TIME__);
    Serial.print   ("Throttle Name: ");
    Serial.println (tname);
    xSemaphoreGive(displaySem);
  }
  // Use tasks to process various input and output streams
  xTaskCreate(serialConsole, "serialConsole", 8192, NULL, 4, NULL);
  #ifdef DELAYONSTART
  Serial.println ("Delay before starting network services");
  delay (DELAYONSTART);  // Start console but not any network services before the delay
  Serial.println ("Starting network services");
  #endif
  xTaskCreate(connectionManager, "connectionMgr", 8192, NULL, 4, NULL);
  xTaskCreate(keepAlive, "keepAlive", 2048, NULL, 4, NULL);
  #ifdef DELAYONSTART
  Serial.println ("Network services started, starting user interface");
  #endif
  #ifndef keynone
  xTaskCreate(keypadMonitor, "keypadMonitor", 2048, NULL, 4, NULL);
  #endif
  xTaskCreate(switchMonitor, "switchMonitor", 2048, NULL, 4, NULL);
}

/*
 * Use the main loop to update the display
 * To avoid a corrupted display only update the display from a single thread
 * Try to avoid writing over the edge of the screen.
 */
void loop()
{
  uint8_t connectState = 99;
  uint8_t menuId = 0;
  uint8_t change = 0;
  uint8_t answer;
  char commandKey;
  char commandStr[2];
  char *baseMenu[] = { "Locomotives", "Turnouts", "Routes", "Track Power", "Configuration", "Info", "Restart" };
  
  display.begin();
  #ifdef SCREENINVERT
  #ifdef SSD1306
  display.flipHorizontal();
  display.flipVertical();
  #endif
  #endif
  screenWidth    = display.width();
  screenHeight   = display.height();
  setupFonts();
/*
  #ifdef SSD1306
  display.setFixedFont( ssd1306xled_font8x16 );
  #else
  #ifdef SSD1327
  display.setFixedFont( ssd1306xled_font8x16 );
  #endif
  #endif
  */
  while (xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(debounceTime)) == pdPASS) {} // clear keyboard buffer
  while (true) {
    if (!client.connected()) {
      uint8_t tState;
      uint8_t answer;

      if (tState < 3 && xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(debounceTime)) == pdPASS) mkConfigMenu();
      if (WiFi.status() == WL_CONNECTED) tState = 2;
      else tState = 1;
      if (tState != connectState) {
        connectState = tState;
        display.clear();
        display.printFixed(0, 0, "Name:", STYLE_NORMAL);
        display.printFixed((6 * selFontWidth), 0, tname, STYLE_BOLD);
        display.printFixed(0, (selFontWidth+selFontHeight), "WiFi: ", STYLE_NORMAL);
        if (WiFi.status() == WL_CONNECTED) {
          display.printFixed((6 * selFontWidth),(selFontWidth+selFontHeight), ssid, STYLE_BOLD);
          display.printFixed(0, 2*(selFontWidth+selFontHeight), "Svr:  ", STYLE_NORMAL);
          display.printFixed((6 * selFontWidth),2*(selFontWidth+selFontHeight), "Connecting", STYLE_BOLD);
        }
        else display.printFixed((6 * selFontWidth),(selFontWidth+selFontHeight), "Connecting", STYLE_BOLD);
      }
    }
    else {
      if (connectState < 3) {
        displayTempMessage ("Connected", "Id. Protocol", false);
        connectState = 3;
        menuId = 0;
        for (uint8_t n=0; n<INITWAIT && cmdProtocol==UNDEFINED; n++) {
          delay (1000); // Pause on initialisation
        }
        while (xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(debounceTime)) == pdPASS) {} // clear keyboard buffer
      }
      while (client.connected()) {
        answer = displayMenu (baseMenu, 7, lastMainMenuOption);
        if (answer > 0) lastMainMenuOption = answer - 1;
        switch (answer) {
          case 1:
            if (trackPower) mkLocoMenu ();
            else displayTempMessage ("Warning:", "Track power off", true);
            break;
          case 2:
            if (trackPower) mkSwitchMenu ();
            else displayTempMessage ("Warning:", "Track power off", true);
            break;
          case 3:
            mkRouteMenu();
            break;
          case 4:
            mkPowerMenu();
            break;
          case 5:
            mkConfigMenu();
            break;
          case 6:
            displayInfo();
            xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(30000)); // 30 second timeout
            break;
          case 7: 
            setDisconnected();
            displayTempMessage ("Rebooting", "Press any key to reboot or wait 30 seconds", true);
            ESP.restart();
            break;
        }
      }
    }
    delay (debounceTime);
  }
  vTaskDelete( NULL );
}
