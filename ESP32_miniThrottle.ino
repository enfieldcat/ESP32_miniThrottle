
/*
 * Reference Page: https://www.jmri.org/help/en/package/jmri/jmrit/withrottle/Protocol.shtml
 */
#include "miniThrottle.h"

// Initialize the OLED display using i2c interface, Adjust according to display device
// DisplaySSD1306_128x64_I2C display(-1); // or (-1,{busId, addr, scl, sda, frequency})
#ifdef SSD1306
DisplaySSD1306_128x64_I2C display (-1,{0, DISPLAYADDR, SCK_PIN, SDA_PIN, -1});
#endif
#ifdef SSD1327
DisplaySSD1327_128x128_I2C display (-1,{0, DISPLAYADDR, SCK_PIN, SDA_PIN, -1});
#endif
#ifdef ST7735
// params {reset, {busid, cs, dc, freq, scl, sca}}
// reset may be -1 if not used, otherwise -1 => defaults
DisplayST7735_128x160x16_SPI display(SPI_RESET,{-1, SPI_CS, SPI_DC, 0, SPI_SCL, SPI_SDA});
#endif
#ifdef ST7789
DisplayST7789_135x240x16_SPI display(SPI_RESET,{-1, SPI_CS, SPI_DC, 0, SPI_SCL, SPI_SDA});
#endif
#ifdef ILI9341
DisplayILI9341_240x320x16_SPI display(SPI_RESET,{-1, SPI_CS, SPI_DC, 0, SPI_SCL, SPI_SDA});
#endif

#ifdef USEWIFI
// WiFi Server Definitions
WiFiClient client;
#else
static HardwareSerial serial_dev(1);
#endif
static SemaphoreHandle_t transmitSem = xSemaphoreCreateMutex();
static SemaphoreHandle_t displaySem  = xSemaphoreCreateMutex();
static SemaphoreHandle_t velociSem   = xSemaphoreCreateMutex();
static SemaphoreHandle_t functionSem = xSemaphoreCreateMutex();
static SemaphoreHandle_t turnoutSem  = xSemaphoreCreateMutex();
static QueueHandle_t cvQueue         = xQueueCreate (2, sizeof(int16_t)); // Queue for querying CV Values
static QueueHandle_t keyboardQueue   = xQueueCreate (2,  sizeof(char));   // Queue for keyboard type of events
static QueueHandle_t keyReleaseQueue = xQueueCreate (2,  sizeof(char));   // Queue for keyboard release type of events
static QueueHandle_t dccAckQueue     = xQueueCreate (10, sizeof(char));   // Queue for dcc updates, avoid flooding of WiFi
static struct locomotive_s   *locoRoster   = (struct locomotive_s*) malloc (sizeof(struct locomotive_s) * MAXCONSISTSIZE);
static struct turnoutState_s *turnoutState = NULL;
static struct turnout_s      *turnoutList  = NULL;
static struct routeState_s   *routeState   = NULL;
static struct route_s        *routeList    = NULL;
static int screenWidth      = 0;
static int screenHeight     = 0;
static int keepAliveTime    = 10;
#ifdef BRAKEPRESPIN
static int brakePres        = 0;
#endif
const  uint16_t routeDelay[] = {0, 500, 1000, 2000, 3000, 4000};
static uint16_t initialLoco = 3;
static uint8_t locomotiveCount      = 0;
static uint8_t turnoutCount         = 0;
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
static uint8_t cmdProtocol  = UNDEFINED;
static uint8_t nextThrottle = 'A';
static uint8_t screenRotate = 0;
#ifdef BACKLIGHTPIN
static uint16_t backlightValue = 200;
#endif
static char ssid[SSIDLENGTH];
static char tname[SSIDLENGTH];
static char remServerType[10] = { "" };  // eg: JMRI
static char remServerDesc[64] = { "" };  // eg: JMRI My whizzbang server v 1.0.4
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
static bool bidirectionalMode= false;
static bool menuMode         = false;
static bool funcChange       = true;
static bool speedChange      = false;
#ifdef POTTHROTPIN
static bool enablePot        = true;
#endif
#ifdef FILESUPPORT
static fs::File writeFile;
static bool writingFile      = false;
const  char* rootCACertificate  = NULL;
#ifdef CERTFILE
const  char* defaultCertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFFjCCAv6gAwIBAgIRAJErCErPDBinU/bWLiWnX1owDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjAwOTA0MDAwMDAw\n" \
"WhcNMjUwOTE1MTYwMDAwWjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n" \
"RW5jcnlwdDELMAkGA1UEAxMCUjMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n" \
"AoIBAQC7AhUozPaglNMPEuyNVZLD+ILxmaZ6QoinXSaqtSu5xUyxr45r+XXIo9cP\n" \
"R5QUVTVXjJ6oojkZ9YI8QqlObvU7wy7bjcCwXPNZOOftz2nwWgsbvsCUJCWH+jdx\n" \
"sxPnHKzhm+/b5DtFUkWWqcFTzjTIUu61ru2P3mBw4qVUq7ZtDpelQDRrK9O8Zutm\n" \
"NHz6a4uPVymZ+DAXXbpyb/uBxa3Shlg9F8fnCbvxK/eG3MHacV3URuPMrSXBiLxg\n" \
"Z3Vms/EY96Jc5lP/Ooi2R6X/ExjqmAl3P51T+c8B5fWmcBcUr2Ok/5mzk53cU6cG\n" \
"/kiFHaFpriV1uxPMUgP17VGhi9sVAgMBAAGjggEIMIIBBDAOBgNVHQ8BAf8EBAMC\n" \
"AYYwHQYDVR0lBBYwFAYIKwYBBQUHAwIGCCsGAQUFBwMBMBIGA1UdEwEB/wQIMAYB\n" \
"Af8CAQAwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYfr52LFMLGMB8GA1UdIwQYMBaA\n" \
"FHm0WeZ7tuXkAXOACIjIGlj26ZtuMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcw\n" \
"AoYWaHR0cDovL3gxLmkubGVuY3Iub3JnLzAnBgNVHR8EIDAeMBygGqAYhhZodHRw\n" \
"Oi8veDEuYy5sZW5jci5vcmcvMCIGA1UdIAQbMBkwCAYGZ4EMAQIBMA0GCysGAQQB\n" \
"gt8TAQEBMA0GCSqGSIb3DQEBCwUAA4ICAQCFyk5HPqP3hUSFvNVneLKYY611TR6W\n" \
"PTNlclQtgaDqw+34IL9fzLdwALduO/ZelN7kIJ+m74uyA+eitRY8kc607TkC53wl\n" \
"ikfmZW4/RvTZ8M6UK+5UzhK8jCdLuMGYL6KvzXGRSgi3yLgjewQtCPkIVz6D2QQz\n" \
"CkcheAmCJ8MqyJu5zlzyZMjAvnnAT45tRAxekrsu94sQ4egdRCnbWSDtY7kh+BIm\n" \
"lJNXoB1lBMEKIq4QDUOXoRgffuDghje1WrG9ML+Hbisq/yFOGwXD9RiX8F6sw6W4\n" \
"avAuvDszue5L3sz85K+EC4Y/wFVDNvZo4TYXao6Z0f+lQKc0t8DQYzk1OXVu8rp2\n" \
"yJMC6alLbBfODALZvYH7n7do1AZls4I9d1P4jnkDrQoxB3UqQ9hVl3LEKQ73xF1O\n" \
"yK5GhDDX8oVfGKF5u+decIsH4YaTw7mP3GFxJSqv3+0lUFJoi5Lc5da149p90Ids\n" \
"hCExroL1+7mryIkXPeFM5TgO9r0rvZaBFOvV2z0gp35Z0+L4WPlbuEjN/lxPFin+\n" \
"HlUjr8gRsI3qfJOQFy/9rKIJR0Y/8Omwt/8oTWgy1mdeHmmjk7j1nYsvC9JSQ6Zv\n" \
"MldlTTKB3zhThV1+XWYp6rjd5JW1zbVWEkLNxE7GJThEUG3szgBVGP7pSWTUTsqX\n" \
"nLRbwHOoq7hHwg==\n" \
"-----END CERTIFICATE-----\n" ;
#endif
#endif

const char prevMenuOption[] = { "Prev. Menu"};
const char *protoList[]     = { "Undefined", "JMRI", "DCC++" };

// selected font sizes being:
static uint8_t selFontWidth  = 8;
static uint8_t selFontHeight = 16;

const char txtWarning[] = { "Warning" };


/*
 * Set up steps
 * * Initialise console connection
 * * Load basic config
 * * Start subtasks to handle I/O
 */
void setup()  {
  Serial.begin(115200);
  delay (1000);
  for (uint8_t n=0; n<MAXCONSISTSIZE; n++) {
    strcpy (locoRoster[n].name, "Void");
    locoRoster[n].owned = false;
  }
  // load initial settings from Non Volatile Storage (NVS)
  nvs_init();
  nvs_get_string ("tname", tname, NAME, sizeof(tname));
  // Print a diagnostic to the console, Prior to starting tasks no semaphore required
  Serial.print   ("Software Vers: ");
  Serial.print   (PRODUCTNAME);
  Serial.print   (" ");
  Serial.println (VERSION);
  Serial.print   ("Compile time:  ");
  Serial.print   (__DATE__);
  Serial.print   (" ");
  Serial.println (__TIME__);
  Serial.print   ("Display Type:  ");
  Serial.println (DISPLAY);
  Serial.print   ("Throttle Name: ");
  Serial.println (tname);
  debounceTime = nvs_get_int ("debounceTime", DEBOUNCEMS);
  screenRotate = nvs_get_int ("screenRotate", 0);
  if (nvs_get_int ("bidirectional", 0) == 1) bidirectionalMode = true;
  #ifdef SHOWPACKETSONSTART
  showPackets = true;
  #endif
  // Also change CPU speed before starting wireless comms
  int cpuSpeed = nvs_get_int ("cpuspeed", 0);
  if (cpuSpeed > 0) {
    #ifdef USEWIFI
    if (cpuSpeed < 80) cpuSpeed = 80; 
    #endif
    Serial.print ("Setting CPU speed to ");
    Serial.print (cpuSpeed);
    Serial.println ("MHz");
    setCpuFrequencyMhz (cpuSpeed);
  }
  // Configure I/O pins
  // Track power indicator
  #ifdef TRACKPWR
  pinMode(TRACKPWR, OUTPUT);
  digitalWrite(TRACKPWR, LOW);
  #endif
  #ifdef TRACKPWRINV
  pinMode(TRACKPWRINV, OUTPUT);
  digitalWrite(TRACKPWRINV, HIGH);
  #endif
  // function key indicators off
  #ifdef F1LED
  pinMode(F1LED, OUTPUT);
  digitalWrite(F1LED, LOW);
  #endif
  #ifdef F2LED
  pinMode(F2LED, OUTPUT);
  digitalWrite(F2LED, LOW);
  #endif
  // trainset mode indicator
  #ifdef TRAINSETLED
  pinMode(TRAINSETLED, OUTPUT);
  if (bidirectionalMode) digitalWrite(TRAINSETLED, HIGH);
  else digitalWrite(TRAINSETLED, LOW);
  #endif
  // Read a backlight reference ADC pin and set backlight PWM using this
  #ifdef BACKLIGHTPIN
  pinMode(BACKLIGHTPIN, OUTPUT);
  #ifdef BACKLIGHTREF
  analogReadResolution(10);
  adcAttachPin(BACKLIGHTREF);
  analogSetPinAttenuation(BACKLIGHTREF, ADC_11db);  // param 2 = attenuation, range 0-3 sets FSD: 0:ADC_0db=800mV, 1:ADC_2_5db=1.1V, 2:ADC_6db=1.35V, 3:ADC_11db=2.6V
  backlightValue = (analogRead(BACKLIGHTREF))>>2;
  #else
  // digitalWrite (BACKLIGHTPIN, 1);
  backlightValue = nvs_get_int ("backlightValue", 200);
  #endif
  ledcSetup(0, 50, 8);
  ledcAttachPin(BACKLIGHTPIN, 0);
  ledcWrite(0, backlightValue);
  // analogWrite(BACKLIGHTPIN, backlightValue);
  #endif
  // Set speedometer initial position
  #ifdef SPEEDOPIN
  dacWrite (SPEEDOPIN, 0);
  #endif
  // Set brake initial position
  #ifdef BRAKEPRESPIN
  dacWrite (BRAKEPRESPIN, 0);
  #endif

  // Check filesystem for cert and icon storage
  #ifdef FILESUPPORT
    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Serial.println("SPIFFS Mount Failed: will attempt to format and mount.");
    // spiffsAvailable = false;
    delay (1000);
  }
  // else spiffsAvailable = true;
  #ifdef CERTFILE
  defaultCertExists(SPIFFS);
  #endif
  #endif
  // Use tasks to process various input and output streams
  // micro controller has enough memory, that stack sizes can be generously allocated to avoid stack overflows
  xTaskCreate(serialConsole, "serialConsole", 8192, NULL, 4, NULL);
  #ifdef DELAYONSTART
  Serial.println ("Delay before starting network services");
  delay (DELAYONSTART);  // Start console but not any network services before the delay
  Serial.println ("Starting network services");
  #endif
  #ifdef USEWIFI
  xTaskCreate(connectionManager, "connectionMgr", 8192, NULL, 4, NULL);
  xTaskCreate(keepAlive, "keepAlive", 2048, NULL, 4, NULL);
  #else
  connectionManager();
  #endif
  #ifdef DELAYONSTART
  Serial.println ("Network services started, starting user interface");
  #endif
  #ifdef keynone
  Serial.println ("No keypad defined");
  #else
  xTaskCreate(keypadMonitor, "keypadMonitor", 2048, NULL, 4, NULL);
  #endif
  xTaskCreate(switchMonitor, "switchMonitor", 2048, NULL, 4, NULL);
  // xTaskCreate(displayMain,   "displayMain",   8192, NULL, 4, NULL);
}

/*
 * Main loop is used to run display
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
  const char *baseMenu[]  = { "Locomotives", "Turnouts", "Routes", "Track Power", "CV Programming", "Configuration" };
  const char txtNoPower[] = { "Track power off. Turn power on to enable function." };

  delay (250);
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print ("Start device display (");
    #ifdef DISPLAY
    Serial.print (DISPLAY);
    Serial.print (", ");
    #endif
    #ifdef SCREENROTATE
    Serial.print (SCREENROTATE);
    Serial.print (" way rotatable, ");
    #endif
    #ifdef SCALEFONT
    Serial.print (" scalable speed indic, ");
    #endif
    #ifdef COLORDISPLAY
    Serial.println ("color)");
    #else
    Serial.println ("monochrome)");
    #endif
    xSemaphoreGive(displaySem);
  }
  display.begin();
  setupFonts();

  while (xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(debounceTime)) == pdPASS) {} // clear keyboard buffer
  while (true) {
    #ifdef USEWIFI
    if (!client.connected()) {
      uint8_t tState;
      uint8_t answer;

      if (tState < 3 && xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
        mkConfigMenu();
        connectState = 99;
      }
      if (WiFi.status() == WL_CONNECTED) tState = 2;
      else tState = 1;
      if (tState != connectState) {
        uint8_t lineNr = 0;
        char outData[SSIDLENGTH + 10];
        connectState = tState;
        display.clear();
        displayScreenLine ("No Connection", lineNr++, true);
        sprintf (outData, "Name: %s", tname);
        displayScreenLine (outData, lineNr++, false);
        if (WiFi.status() == WL_CONNECTED) {
          if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(20000)) == pdTRUE) {
            uint8_t waitLoop = 0;
            while (strlen (ssid) == 0 && waitLoop++ < 100) {
              xSemaphoreGive(transmitSem);
              delay (100);
              xSemaphoreTake(transmitSem, pdMS_TO_TICKS(20000));
            }
          }
          xSemaphoreGive(transmitSem);
          sprintf (outData, "WiFi: %s", ssid);
          displayScreenLine (outData, lineNr++, false);
          displayScreenLine ("Svr:  Connecting", lineNr++, false);
        }
        else {
          displayScreenLine ("Wifi: Connecting", lineNr++, false);
        }
      }
    }
    else {
      if (connectState < 3) {
        displayTempMessage ("Connected", "Waiting for response.", false);
        connectState = 3;
        menuId = 0;
        for (uint8_t n=0; n<INITWAIT && cmdProtocol==UNDEFINED; n++) {
          delay (1000); // Pause on initialisation
        }
        while (xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(debounceTime)) == pdPASS) {} // clear keyboard buffer
      }
      while (client.connected()) {
        #else
        // handling for Serial connection
    if (true) {
      while (true) {
        #endif
        answer = displayMenu ((char**)baseMenu, 6, lastMainMenuOption);
        if (answer > 0) lastMainMenuOption = answer - 1;
        switch (answer) {
          case 1:
            if (trackPower) mkLocoMenu ();
            else displayTempMessage ((char*)txtWarning, (char*)txtNoPower, true);
            break;
          case 2:
            if (trackPower) mkTurnoutMenu ();
            else displayTempMessage ((char*)txtWarning, (char*)txtNoPower, true);
            break;
          case 3:
            if (trackPower) mkRouteMenu();
            else displayTempMessage ((char*)txtWarning, (char*)txtNoPower, true);
            break;
          case 4:
            mkPowerMenu();
            break;
          case 5:
            if (trackPower) mkCVMenu ();
            else displayTempMessage ((char*)txtWarning, (char*)txtNoPower, true);
            break;
          case 6:
            mkConfigMenu();
            break;
        }
      }
    }
    delay (debounceTime);
  }
  vTaskDelete( NULL );
}
