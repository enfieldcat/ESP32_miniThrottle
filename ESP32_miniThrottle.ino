
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
static struct locomotive_s   *locoRoster  = (struct locomotive_s*) malloc (sizeof(struct locomotive_s) * MAXCONSISTSIZE);
static struct turnoutState_s *switchState = NULL;
static struct turnout_s      *switchList  = NULL;
static struct routeState_s   *routeState  = NULL;
static struct route_s        *routeList   = NULL;
static int screenWidth      = 0;
static int screenHeight     = 0;
static int keepAliveTime    = 10;
static uint16_t initialLoco = 3;
static uint8_t locomotiveCount      = 0;
static uint8_t switchCount          = 0;
static uint8_t switchStateCount     = 0;
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
static char ssid[33];
static char tname[33];
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
  xTaskCreate(keypadMonitor, "keypadMonitor", 2048, NULL, 4, NULL);
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
  char *baseMenu[] = { "Locomotives", "Switches", "Routes", "Track Power", "Info", "Restart" };
  
  display.begin();
  #ifdef SCREENINVERT
  #ifdef SSD1306
  display.flipHorizontal();
  display.flipVertical();
  #endif
  #endif
  screenWidth    = display.width();
  screenHeight   = display.height();
  charsPerLine   = screenWidth  / FONTWIDTH;
  linesPerScreen = screenHeight / FONTHEIGHT;
  #ifdef SSD1306
  display.setFixedFont( ssd1306xled_font8x16 );
  #else
  #ifdef SSD1327
  display.setFixedFont( ssd1306xled_font8x16 );
  #endif
  #endif
  while (true) {
    if (!client.connected()) {
      uint8_t tState;
      uint8_t answer;

      if (WiFi.status() == WL_CONNECTED) tState = 2;
      else tState = 1;
      if (tState != connectState) {
        connectState = tState;
        display.clear();
        display.printFixed(0, 0, "Name:", STYLE_NORMAL);
        display.printFixed((6 * FONTWIDTH), 0, tname, STYLE_BOLD);
        display.printFixed(0, (FONTWIDTH+FONTHEIGHT), "WiFi: ", STYLE_NORMAL);
        if (WiFi.status() == WL_CONNECTED) {
          display.printFixed((6 * FONTWIDTH),(FONTWIDTH+FONTHEIGHT), ssid, STYLE_BOLD);
          display.printFixed(0, 2*(FONTWIDTH+FONTHEIGHT), "Svr:  ", STYLE_NORMAL);
          display.printFixed((6 * FONTWIDTH),2*(FONTWIDTH+FONTHEIGHT), "Connecting", STYLE_BOLD);
        }
        else display.printFixed((6 * FONTWIDTH),(FONTWIDTH+FONTHEIGHT), "Connecting", STYLE_BOLD);
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
        answer = displayMenu (baseMenu, 6, lastMainMenuOption);
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
            displayInfo();
            xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(30000)); // 30 second timeout
            break;
          case 6: 
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

void mkLocoMenu()
{
  char *locoMenu[locomotiveCount+2];
  const char lastOpt[] = { "Enter loco ID" };
  uint8_t result = 255;

  for (uint8_t n=0; n<locomotiveCount; n++) locoMenu[n] = locoRoster[n].name;
  locoMenu[locomotiveCount] = (char*) lastOpt;
  locoMenu[locomotiveCount+1] = (char*) prevMenuOption;   // give option to go to previous menu
  while (result!=0) {
    result = displayMenu (locoMenu, locomotiveCount+2, lastLocoMenuOption);
    if (result == (locomotiveCount + 2)) result = 0;
    else if (result == (locomotiveCount + 1)) {
      // Deal with ad-hoc entry of locomotive
      int tLoco = enterNumber("Enter Loco ID:");
      if (tLoco <= 0) result = 0;
      else {
        // search if this exists in table
        uint8_t maxLocoArray = locomotiveCount + MAXCONSISTSIZE;
        uint8_t tcntr = 0;
        for (;tcntr<maxLocoArray && locoRoster[tcntr].id!= tLoco; tcntr++);
        if (tcntr<maxLocoArray) result = tcntr + 1;
        else {
          if (tLoco < 128) locoRoster[locomotiveCount].type = 'S';
          else             locoRoster[locomotiveCount].type = 'L';
          locoRoster[locomotiveCount].direction = STOP;
          locoRoster[locomotiveCount].speed     = 0;
          locoRoster[locomotiveCount].steps     = 128;
          locoRoster[locomotiveCount].id        = tLoco;
          locoRoster[locomotiveCount].function  = 0;
        }
      }
    }
    if (result > 0) {
      // Pass details on to CAB control / locomotive driver
      lastLocoMenuOption = result - 1;
      initialLoco  = lastLocoMenuOption;
      locomotiveDriver ();
      result = 255;
    }
  }
}

void mkSwitchMenu()
{
  char *switchMenu[switchCount + 1];
  char *stateMenu[switchStateCount];
  uint8_t result = 255;
  uint8_t reqState = 255;
  uint8_t option = 0;

  if (switchCount == 0 || switchStateCount == 0) {
    displayTempMessage ("Warning:", "No switches or switch states defined", true);
    return;
  }
  for (uint8_t n=0; n<switchCount; n++) switchMenu[n] = switchList[n].userName;
  switchMenu[switchCount] = (char*) prevMenuOption;
  for (uint8_t n=0; n<switchStateCount; n++) stateMenu[n] = switchState[n].name;
  while (result!=0) {
    result = displayMenu (switchMenu, switchCount+1, lastSwitchMenuOption);
    if (result == (switchCount+1)) result = 0;  // give option to go to previous menu
    if (result > 0) {
      lastSwitchMenuOption = result - 1;
      if (switchList[result-1].state == switchState[0].state) option = 0;
      else option = 1;
      reqState = displayMenu (stateMenu, switchStateCount, option);
      if (reqState != 0) {
        setTurnout (result-1, reqState-1);
      }
      result = 255;
    }
  }
}


void mkRouteMenu()
{
  char *routeMenu[routeCount + 1];
  uint8_t result = 255;
  uint8_t option = 0;

  if (routeCount == 0) {
    displayTempMessage ("Warning:", "No routes defined", true);
    return;
  }
  for (uint8_t n=0; n<routeCount; n++) routeMenu[n] = routeList[n].userName;
  routeMenu[switchCount] = (char*) prevMenuOption;
  while (result!=0) {
    result = displayMenu (routeMenu, routeCount+1, lastRouteMenuOption);
    if (result == (routeCount+1)) result = 0;  // give option to go to previous menu
    if (result > 0) {
      lastRouteMenuOption = result - 1;
      setRoute (lastRouteMenuOption);
    }
  }
}

void mkPowerMenu()
{
  const char *PowerOptions[] = {"Track Power On", "Track Power Off"};
  uint8_t result = 255;
  uint8_t option = 0;

//  while (result != 0) {  // expected usage is is to return to main menuol 
    if (trackPower) option = 0;
    else option = 1;
    result = displayMenu((char**)PowerOptions , 2, option);
    if (result != 0) setTrackPower (result);
//  }
}


uint8_t mkCabMenu() // In CAB menu - Returns the count of owned locos
{
  const char *CABOptions[] = {"Add Loco", "Remove Loco", "Return"} ;

  uint8_t result = 255;
  uint8_t option = 0;
  uint8_t retval = 0;
  uint8_t limit = locomotiveCount + MAXCONSISTSIZE;

  result = displayMenu((char**)CABOptions , 3, 2);
  if (result == 1) {     // Add loco, should only show locos we don't yet own
    char *addOpts [locomotiveCount+1];
    for (uint8_t n = 0; n<locomotiveCount; n++) if (!locoRoster[n].owned) addOpts[option++] = locoRoster[n].name;
    addOpts[option++] = (const char*) "Enter loco ID";
    addOpts[option++] = (const char*) "Return";
    result = displayMenu ((char**) addOpts, option, 0);
    if (result != 0 && result < option) {
      if (result < option -1) {
        option = 255;
        for (uint8_t n = 0; option == 255 && n < limit; n++) {
          if ((!locoRoster[n].owned) && strcmp (locoRoster[n].name, addOpts[result])==0) option = n;
        }
      }
      else {
        // User will enter a number
        int tLoco = enterNumber("Enter Loco ID:");
        if (tLoco <= 0) result = 255;
        else {
          // check if the entered number is on the roster already
          for (option=0; option<locomotiveCount && locoRoster[option].id != tLoco; option++);
          if (option == locomotiveCount) {
            // Check for next available table slot or error
            for (option=locomotiveCount; option<limit && !locoRoster[option].owned; option++);
            if (option == limit) {
              option = 255;
              displayTempMessage ("Warning:", "unable to add loco to roster", true);
            }
            else {
              // update the slot with our data
              locoRoster[option].id = tLoco;
              if (locoRoster[option].id < 128) locoRoster[option].type = 'S';
              else locoRoster[option].type = 'L';
            }
          }
        }
      }
      if (option != 255) {
        // Now if we have a new loco, try to add it to our throttle
        char displayLine[40];
        locoRoster[option].steal = '?';
        locoRoster[option].direction = locoRoster[initialLoco].direction;
        locoRoster[option].speed = 0;
        locoRoster[option].throttleNr = nextThrottle++;
        if (nextThrottle > 'Z') nextThrottle = '0';
        setLocoOwnership (option, true);
        while (locoRoster[option].steal == '?') delay (100);
        if (locoRoster[option].steal == 'Y') {
          sprintf (displayLine, "Steal loco %s?", locoRoster[option].name);
          if (displayYesNo (displayLine)) {
            setStealLoco (option);
          }
        }
      }
    }
  }
  else if (result == 2) {  // remove loco, should only list locos we own
    uint8_t count = 0;
    for (uint8_t n = 0; n<limit; n++) if (locoRoster[n].owned) count++;
    char *removeOpts[count+1];
    for (uint8_t n = 0; n<limit; n++) if (locoRoster[n].owned && (n != initialLoco || count == 1)) {
      removeOpts[option++] = locoRoster[n].name;
    }
    removeOpts[option++] = (const char*) "Return";
    result = displayMenu ((char**) removeOpts, option, 0);
    if (result != 0 && result != option) {
      result--;
      for (uint8_t n=0; n<limit; n++) {
        if (locoRoster[n].owned && strcmp (locoRoster[n].name, removeOpts[result]) == 0) {
          setLocoOwnership (n, false);
          locoRoster[n].owned = false;
        }
      }
    }
  }
  for (uint8_t n = 0; n<limit; n++) if (locoRoster[n].owned) retval++;
  return (retval);
}

void displayInfo()
{
  const char *protocolName[] = { "Unknown", "JMRI", "SRCP" };
  char outData[50];
  uint8_t lineNr = 0;
  
  display.clear();
  sprintf (outData, "Name: %s", tname);
  displayScreenLine (outData, lineNr++, false);
  sprintf (outData, "SSID: %s", ssid);
  displayScreenLine (outData, lineNr++, false);
  sprintf (outData, "Proto: %s", protocolName[cmdProtocol]);
  displayScreenLine (outData, lineNr++, false);
  if (lineNr >= linesPerScreen) return;
  if (strlen (remServerNode) > 0) {
    sprintf (outData, "Svr: %s", remServerNode);
    displayScreenLine (outData, lineNr++, false);
  }
  if (strlen (remServerDesc) > 0) {
    sprintf (outData, "Desc: %s", remServerDesc);
    displayScreenLine (outData, lineNr++, false);
  }
  if (strlen (remServerType) > 0) {
    sprintf (outData, "Type: %s", remServerType);
    displayScreenLine (outData, lineNr++, false);
  }
  if (strlen (lastMessage) > 0) {
    sprintf (outData, "Msg: %s", lastMessage);
    displayScreenLine (outData, lineNr++, false);
  }
}

//
uint8_t displayMenu (char **menuItems, uint8_t itemCount, uint8_t selectedItem)
{
  uint8_t currentItem = selectedItem;
  uint8_t displayLine;
  uint8_t exitCode = 255;
  char commandKey;
  bool hasChanged = true;

  if (selectedItem > itemCount) selectedItem = 0;
  display.clear();
  menuMode = true;
  while (exitCode == 255) {
    if (hasChanged) {
      hasChanged = false;
      if (currentItem > (linesPerScreen-1)) displayLine = currentItem - (linesPerScreen-1);
      else displayLine = 0;
      for (uint8_t n=0; n<linesPerScreen && displayLine < itemCount; n++, displayLine++) {
        displayScreenLine (menuItems[displayLine], n, displayLine==currentItem);
      }
    }
    if (xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(10)) == pdPASS) {
      if (commandKey == 'S' || commandKey == 'R') {
        exitCode = currentItem + 1;
      }
      else if (commandKey == 'E' || commandKey == 'L') {
        exitCode = 0;
      }
      else if (commandKey == 'D') {
        if (currentItem > 0) {
          currentItem--;
          hasChanged = true;
        }
      }
      else if (commandKey == 'U') {
        if (currentItem < (itemCount-1)) {
          currentItem++;
          hasChanged = true;
        }
      }
    }
    else delay (debounceTime);
  }
  menuMode = false;
  return (exitCode);
}


void displayScreenLine (char *menuItem, uint8_t lineNr, bool inverted)
{
  char linedata[charsPerLine+1];

  if (lineNr >= linesPerScreen) return;
  // create a right padded line with the data to be displayed
  if (strlen (menuItem) > charsPerLine) {
    strncpy (linedata, menuItem, charsPerLine);
    linedata[sizeof(linedata)-1] = '\0';
  }
  else {
    strcpy (linedata, menuItem);
    for (uint8_t n=strlen(linedata); n<charsPerLine; n++) linedata[n]=' ';
  }
  linedata[charsPerLine] = '\0';
  if (inverted) display.invertColors();
  display.printFixed(0, (lineNr*FONTHEIGHT), linedata, STYLE_NORMAL);
  if (inverted) display.invertColors();
}


uint8_t displayTempMessage (char *header, char *message, bool wait4response)
{
  uint8_t lineNr = 0;
  uint8_t offset = 0;
  char lineData[charsPerLine+1];
  char *charPtr = message;

  display.clear();
  if (header!=NULL) displayScreenLine (header, lineNr++, true);
  if (message!=NULL) {  // break message into chunks less than a line's length and display them.
    while (strlen(charPtr) > charsPerLine) {
      offset = charsPerLine;
      while (offset>0 && charPtr[offset] != ' ') offset--;
      if (offset == 0) {
        strncpy (lineData, charPtr, charsPerLine);
        charPtr += charsPerLine;
        lineData[charsPerLine] = '\0';
      }
      else {
        offset++;
        strncpy (lineData, charPtr, offset);
        charPtr += offset;
        lineData[offset] = '\0';
      }
      displayScreenLine (lineData, lineNr++, false);
    }
    displayScreenLine (charPtr, lineNr, false);
  }
  // wait for user response or time out
  if (wait4response) {
    xQueueReceive(keyboardQueue, &offset, pdMS_TO_TICKS(30000)); // 30 second timeout
  }
  return (lineNr);
}

bool displayYesNo (char *question)
{
  uint8_t baseLine = displayTempMessage (NULL, question, false);
  uint8_t command = 255;
  bool option = true;

  menuMode = true;
  while (command != 'S' && command != 'R' && command != 'L') {  // Select or left right to accept answer
    if (command == 255 || xQueueReceive(keyboardQueue, &command, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
      displayScreenLine (" Yes", baseLine,    option);
      displayScreenLine (" No",  baseLine+1, !option);
      if (command == 255) command = 254;
      else if (command == 'U' || command == 'D') option = !option; // up down to move between answers
    }
  }
  menuMode = false;
  return (option);
}

int enterNumber(char *prompt)
{
  int retVal = 0;
  char *tptr;
  char inKey = 'Z';
  char dispNum[8];
  char inBuffer[7];
  uint8_t charPosition = 0;
  uint8_t x, y;

  x = 4 * FONTWIDTH;
  y = 1.5 * FONTHEIGHT;
  display.clear();
  displayTempMessage (NULL, prompt, false);
  while (inKey != 'S' && inKey != 'E' && inKey != '#' && inKey != '*') {
    inBuffer[charPosition] = '\0';
    if (inBuffer[0] == '\0') retVal = 0;
    else retVal = strtol (inBuffer, &tptr, 10);
    sprintf (dispNum, "%d ", retVal);
    display.printFixed (x, y, dispNum, STYLE_NORMAL);
    if (xQueueReceive(keyboardQueue, &inKey, pdMS_TO_TICKS(30000)) == pdPASS) {
      if (charPosition < (sizeof (inBuffer) - 1) && inKey >= '0' && inKey <= '9') {
        inBuffer[charPosition++] = inKey;
      }
      if ((inKey == 'L' || inKey == 'R') && charPosition>0) charPosition--;
    }
    else inKey = 'E';
  }
  if (inKey == 'E') retVal = -1;
  return (retVal);
}
