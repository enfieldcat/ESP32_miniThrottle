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



/*
 * internal fonts
 * Micro: digital_font5x7
 * Small: ssd1306xled_font6x8
 * Std:   ssd1306xled_font8x16
 * Large: free_calibri11x12
 * 
 * If defining additional fonts, list sources in .gitignore
 * In one or more font source #define CUSTOM_FONT
 * The source font should be listed in Ardiuno IDE before displayRoutines.ino
 */
#ifndef NODISPLAY
const char *fontLabel[] = {"Small 6x8", "Std 8x16"
#ifdef FONT_10x20
, "Medium 10x20"
#endif
#ifdef FONT_14x16
, "Wide 14x16"
#endif
#ifdef FONT_12x24
, "Large 12x24" 
#endif
#ifdef FONT_16x32
, "X Large 16x32"
#endif
#ifdef FONT_32x64
, "XX Large 32x64"
#endif
};
const uint8_t fontWidth[]  = { 6,8
#ifdef FONT_10x20
,10
#endif
#ifdef FONT_14x16
,14
#endif
#ifdef FONT_12x24
, 12
#endif
#ifdef FONT_16x32
, 16
#endif
#ifdef FONT_32x64
, 32
#endif
};
const uint8_t fontHeight[] = { 8,16
#ifdef FONT_10x20
,20
#endif
#ifdef FONT_14x16
,16
#endif
#ifdef FONT_12x24
, 24
#endif
#ifdef FONT_16x32
, 32
#endif
#ifdef FONT_32x64
, 64
#endif
};


void setupFonts()
{
  static uint8_t fontIndex = 1;
  uint8_t incrementer = 0;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setupFonts()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  #ifdef SCREENROTATE
  #if SCREENROTATE == 2
  display.getInterface().flipHorizontal (screenRotate);
  display.getInterface().flipVertical (screenRotate);
  #else
  display.getInterface().setRotation (screenRotate);
  #endif
  #endif
  screenWidth    = display.width();
  screenHeight   = display.height();
  fontIndex      = nvs_get_int ("fontIndex", 1);
  selFontWidth   = fontWidth[fontIndex];
  selFontHeight  = fontHeight[fontIndex];
  charsPerLine   = screenWidth  / selFontWidth;
  linesPerScreen = screenHeight / selFontHeight;
  if (fontIndex == incrementer++) display.setFixedFont(ssd1306xled_font6x8);
  else if (fontIndex == incrementer++) display.setFixedFont(ssd1306xled_font8x16);
#ifdef FONT_10x20
  else if (fontIndex == incrementer++) display.setFixedFont(font_10x20);
#endif
#ifdef FONT_14x16
  else if (fontIndex == incrementer++) display.setFixedFont(font_14x16);
#endif
#ifdef FONT_12x24
  else if (fontIndex == incrementer++) display.setFixedFont(font_12x24);
#endif
#ifdef FONT_16x32
  else if (fontIndex == incrementer++) display.setFixedFont(font_16x32);
#endif
  else display.setFixedFont(ssd1306xled_font8x16);

  display.fill(0x00);
  #ifdef STDCOLOR
  display.setColor (STDCOLOR);
  #endif
  #ifdef BACKCOLOR
  display.setBackground (BACKCOLOR);
  #endif
}


void mkLocoMenu()
{
  char *locoMenu[locomotiveCount+2];
  const char lastOpt[] = { "Enter loco ID" };
  uint8_t result = 255;
  uint8_t commandKey;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkLocoMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  for (uint8_t n=0; n<locomotiveCount; n++) locoMenu[n] = locoRoster[n].name;
  locoMenu[locomotiveCount] = (char*) lastOpt;
  locoMenu[locomotiveCount+1] = (char*) prevMenuOption;   // give option to go to previous menu
  while (result!=0 && trackPower
    #ifndef SERIALCTRL
    && client.connected()
    #endif
    ) {
    result = displayMenu ((const char**) locoMenu, locomotiveCount+2, lastLocoMenuOption);
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
          locoRoster[locomotiveCount].speed     = -1;
          locoRoster[locomotiveCount].steps     = 128;
          locoRoster[locomotiveCount].id        = tLoco;
          locoRoster[locomotiveCount].function  = 0;
          sprintf (locoRoster[locomotiveCount].name, "Loco-ID-%d", tLoco);
        }
      }
    }
    if (result > 0) {
      // Pass details on to CAB control / locomotive driver
      lastLocoMenuOption = result - 1;
      initialLoco  = lastLocoMenuOption;
      locomotiveDriver ();
      result = 255;
      while (xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(debounceTime)) == pdPASS) {}
    }
  }
}

void mkTurnoutMenu()
{
  char *switchMenu[turnoutCount + 1];
  char *stateMenu[] ={(char*)"Close", (char*)"Throw"};
  char operation = 'C';
  uint8_t result = 255;
  uint8_t reqState = 255;
  uint8_t option = 0;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkTurnoutMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (turnoutCount == 0 || turnoutStateCount == 0) {
    displayTempMessage ((char*)txtWarning, "No switches or switch states defined", true);
    return;
  }
  for (uint8_t n=0; n<turnoutCount; n++) switchMenu[n] = turnoutList[n].userName;
  switchMenu[turnoutCount] = (char*) prevMenuOption;
  //for (uint8_t n=0; n<turnoutStateCount; n++) stateMenu[n] = turnoutState[n].name;
  while (result!=0
    #ifndef SERIALCTRL
    && client.connected()
    #endif
    ) {
    result = displayMenu ((const char**) switchMenu, turnoutCount+1, lastSwitchMenuOption);
    if (result == (turnoutCount+1)) result = 0;  // give option to go to previous menu
    if (result > 0) {
      lastSwitchMenuOption = result - 1;
      if (turnoutList[result-1].state == turnoutState[0].state) option = 0;
      else option = 1;
      // reqState = displayMenu ((const char**) stateMenu, turnoutStateCount, option);
      reqState = displayMenu ((const char**) stateMenu, 2, option);
      if (reqState != 0) {
        if (reqState == 2) operation = 'T';
        setTurnout (result-1, operation);
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

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkRouteMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (routeCount == 0) {
    displayTempMessage ("Warning:", "No routes defined", true);
    return;
  }
  for (uint8_t n=0; n<routeCount; n++) routeMenu[n] = routeList[n].userName;
  routeMenu[routeCount] = (char*) prevMenuOption;
  while (result!=0
    #ifndef SERIALCTRL
    && client.connected()
    #endif
    ) {
    result = displayMenu ((const char**)routeMenu, routeCount+1, lastRouteMenuOption);
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

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkPowerMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (trackPower) option = 0;
  else option = 1;
  result = displayMenu(PowerOptions , 2, option);
  if (result != 0) setTrackPower (result);
}


void mkConfigMenu()
{
  const char *configMenu[] = {"Bidirectional Mode", "CPU Speed", "Font", "Info", "Protocol", "Restart", "Rotate Screen", "Server IP", "Server Port", "Speed Step", "Prev. Menu"};
  char *address;
  uint8_t reference[] = {1,2,3,4,5,6,7,8,9,10,11};
  uint8_t result = 1;
  char commandKey;
  char paramName[9];
  int portNum;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkConfigMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  #ifdef SERIALCTRL
  reference[4] = 200;  // protocol, serial => DCC-Ex direct connection
  reference[7] = 200;  // Server IP, not applicable for serial link
  reference[8] = 200;  // Server port.
  #endif
  #ifndef SCREENROTATE
  reference[6] = 200;  // Rotate screen
  #endif
  while (result != 0) {
    result = displayMenu (configMenu, reference, 11, (result-1));
    switch (result) {
      case 1:
        if (displayYesNo ("Bidirectional mode?")) {
          bidirectionalMode = true;
          nvs_put_int ("bidirectional", 1);
        }
        else {
          bidirectionalMode = false;
          nvs_put_int ("bidirectional", 0);
        }
        break;
      case 2:
        mkCpuSpeedMenu();
        break;
      case 3:
        mkFontMenu();
        break;
      case 4:
        displayInfo();
        xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(30000)); // 30 second timeout
        break;
      case 5:
        mkProtoPref();
        break;
      case 6:
        setDisconnected();
        displayTempMessage ("Restarting", "Press any key to reboot or wait 30 seconds", true);
        displayTempMessage ("Restarting", "Please wait", false);
        ESP.restart();
        break;
      case 7:
        mkRotateMenu();
        break;
      case 8:
        address = enterAddress ("Server IP Address:");
        if (strlen(address)>0) {
          portNum = 0;
          for (uint8_t lo=0; lo<strlen(address); lo++) if (address[lo]=='.') portNum++;
          if (portNum==3 && address[strlen(address)-1]!='.') {
            sprintf(paramName, "server_%d", (WIFINETS-1));
            nvs_put_string (paramName, address);
          }
          else {
            if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.print ("reject malformed address: ");
              Serial.println (address);
              xSemaphoreGive(consoleSem);
            }
          }
        }
        break;
      case 9:
        portNum = enterNumber("Server Port:");
        if (portNum>0) {
          sprintf (paramName, "port_%d", (WIFINETS-1));
          nvs_put_int (paramName, portNum);
        }
        break;
      case 10:
        mkSpeedStepMenu();
        break;
      default:
        result = 0;
        break;
    }
  }
}

void mkCVMenu()
{
  const char *cvMenu[] = {"Read Loco Addr", "Read CV Byte", "Write Loco Addr", "Write CV Byte", "Write CV Bit", "Prev. Menu"};
  static uint8_t lastCvMenuOption = 0;
  uint8_t result = 1;
  uint8_t bitval = 1;
  int16_t requestCV = 0;
  int16_t queryCV = 0;
  char message[64];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkCVMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (cmdProtocol == DCCEX) {
    while (result!=0
      #ifndef SERIALCTRL
      && client.connected()
      #endif
      ) {
      result = displayMenu (cvMenu, 6, lastCvMenuOption);
      if (result > 0) lastCvMenuOption = result - 1;
      switch (result) {
        case 1:
          while (xQueueReceive(cvQueue, &requestCV, pdMS_TO_TICKS(0)) == pdPASS); // clear the buffer of anything that got there
          getAddress();
          displayTempMessage (NULL, "Reading data", false);
          if (xQueueReceive(cvQueue, &requestCV, pdMS_TO_TICKS(30000)) == pdPASS) { // 30 second timeout
            if (requestCV >= 0) sprintf (message, "Address = %d", requestCV);
            else strcpy(message, "Error reading address");
            displayTempMessage (NULL, message, true);
          }
          else displayTempMessage ((char*)txtWarning, "Timed out waiting for address", true);
          break;
        case 2:
          while (xQueueReceive(cvQueue, &requestCV, pdMS_TO_TICKS(0)) == pdPASS); // clear the buffer of anything that got there
          queryCV = enterNumber("Enter CV to read (1-1024)");
          if (queryCV<1024) {
            getCV(queryCV);
            displayTempMessage (NULL, "Reading data", false);
            if (xQueueReceive(cvQueue, &requestCV, pdMS_TO_TICKS(30000)) == pdPASS) { // 30 second timeout
              if (requestCV >= 0) {
                display.clear();
                sprintf (message, "CV %d (0x%03x):", queryCV, queryCV);
                displayScreenLine (message, 0, false);
                sprintf (message, "Dec: %d", requestCV);
                displayScreenLine (message, 1, false);
                sprintf (message, "Hex: 0x%02x", requestCV);
                displayScreenLine (message, 2, false);
                if (requestCV < 256) {
                  sprintf (message, "Bin: %s", util_bin2str ((uint8_t)(requestCV & 0xff)));
                  displayScreenLine (message, 3, false);
                }
                xQueueReceive(keyboardQueue, &queryCV, pdMS_TO_TICKS(30000)); // 30 second timeout
              }
              else {
                sprintf (message, "Error reading CV %d", requestCV);
                displayTempMessage (NULL, message, true);
              }
            }
            else displayTempMessage ((char*)txtWarning, "Timed out reading CV", true);
          }
          else {
            displayTempMessage ((char*)txtWarning, "CV number should be less than 1024", true);
          }
          break;
        case 3:
          requestCV = enterNumber ("New loco address (1-10239)");
          if (requestCV > 0 && requestCV < 10240) {
            sprintf (message, "Set loco address to %d?", requestCV);
            if (displayYesNo (message)) {
              sprintf (message, "<W %d>", requestCV);
              txPacket (message);
            }
          }
          break;
        case 4:
          requestCV = enterNumber ("CV number (1-1024)");
          if (requestCV > 0 && requestCV <= 1024) {
            sprintf (message, "Value of CV %d (0-255)", requestCV);
            queryCV = enterNumber (message);
            if (queryCV>=0 && queryCV<=255) {
              sprintf (message, "Set CV %d = %d?", requestCV, queryCV);
              if (displayYesNo (message)) {
                sprintf (message, "<W %d %d %d %d>", requestCV, queryCV, CALLBACKNUM, CALLBACKSUB); 
                txPacket (message);
              }
            }
          }
          break;
        case 5:
          requestCV = enterNumber ("CV number (1-1024)");
          if (requestCV > 0 && requestCV <= 1024) {
            sprintf (message, "Bit number of CV %d (0-7)", requestCV);
            queryCV = enterNumber (message);
            if (queryCV>=0 && queryCV<8) {
              sprintf (message, "value of CV %d, bit %d (0-1)", requestCV, queryCV);
              bitval = enterNumber (message);
              if (bitval==0 || bitval == 1) {
                sprintf (message, "Set CV %d, bit %d = %d?", requestCV, queryCV, bitval);
                if (displayYesNo (message)) {
                  sprintf (message, "<W %d %d %d %d %d>", requestCV, queryCV, bitval, CALLBACKNUM, CALLBACKSUB); 
                  txPacket (message);
                }
              }
            }
          }
          break;
        default:
          result = 0;
         break;
      }
    }
  }
  else displayTempMessage ((char*)txtWarning, "CV programming requires use of DCC-Ex protocol", true);
}

void mkFontMenu()
{
  char fontMenu[sizeof(fontWidth) + 1][32];
  char *mp[sizeof(fontWidth) + 1];
  uint8_t result;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkFontMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  for (uint8_t n=0; n<sizeof(fontWidth); n++) sprintf (fontMenu[n], "%s (%dx%d chars)", fontLabel[n], (screenWidth/fontWidth[n]), (screenHeight/fontHeight[n]));
  sprintf (fontMenu[sizeof(fontWidth)], prevMenuOption);
  for (uint8_t n=0;n<sizeof(fontWidth) + 1; n++) mp[n] = (char*) &fontMenu[n];
  result = displayMenu ((const char**) mp, (sizeof(fontWidth) + 1), 1);
  if (result > 0 && result <= sizeof(fontWidth)) {
    result--;
    nvs_put_int ( "fontIndex", result);
    setupFonts();
  }
}


void mkSpeedStepMenu()
{
  char speedMenu[10][10];
  char *mp[10];
  uint8_t result;
  uint8_t speedStep = nvs_get_int ("speedStep", 4);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkSpeedStepMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  for (uint8_t n=0; n<9 ;n++) sprintf (speedMenu[n], "%d/click",  n+1);
  sprintf (speedMenu[9], prevMenuOption);
  for (uint8_t n=0; n<10; n++) mp[n] = (char*) &speedMenu[n];
  result = displayMenu ((const char **) mp, 10, speedStep-1);
  if (result > 0 && result <= 9) {
    nvs_put_int ( "speedStep", result);
  }
}


void mkCpuSpeedMenu()
{
  const char *cpuMenu[] = {"80MHz", "160MHz", "240MHz", "Prev. Menu"};
  uint8_t result;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkCpuSpeedMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  switch (nvs_get_int ("cpuspeed", 0)) {
    case 240:
      result = 2;
      break;
    case 160:
      result = 1;
      break;
    default:
      result = 0;
      break;
  }
  result = displayMenu (cpuMenu, 4, result);
  switch (result) {
    case 1:
      nvs_put_int ( "cpuspeed", 80);
      break;
    case 2:
      nvs_put_int ( "cpuspeed", 160);
      break;
    case 3:
      nvs_put_int ( "cpuspeed", 240);
      break;
  }
}


void mkProtoPref()
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkProtoPref()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  const char *protoMenu[] = {"WiThrottle", "DCC-Ex", "Prev. Menu"};
  int defProto = cmdProtocol - 1;
  if (defProto < 0) defProto = 0;
  uint8_t result = displayMenu (protoMenu, 3, defProto);
  if (result > 0 && result<3) {
    cmdProtocol = result;
    nvs_put_int ( "defaultProto", result);
  }
}

uint8_t mkCabMenu() // In CAB menu - Returns the count of owned locos
{
  const char *CABOptions[] = {"Add Loco", "Consist Reversal", "Remove Loco", "Routes", "Set Lead Loco", "Turnouts", "Return"} ;

  uint8_t result = 255;
  uint8_t option = 0;
  uint8_t retval = 0;
  uint8_t limit = locomotiveCount + MAXCONSISTSIZE;
  uint8_t reference[] = { 1,2,3,4,5,6,7};
  uint8_t menuMask = nvs_get_int("mainMenuMask", 0);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkCabMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  for (uint8_t n = 0; n<limit; n++) if (locoRoster[n].owned) retval++;
  if (turnoutCount == 0 || (menuMask&2) > 0) reference[3] = 200;  // Turnouts are array index 1 in baseMenu => mask 0x02
  if (routeCount == 0   || (menuMask&4) > 0) reference[5] = 200;  // Routes   are array index 2 in baseMenu => mask 0x04
  if (retval <= 1)      {reference[1] = 200; reference[4] = 200;  }
  retval = 0;
  result = displayMenu((const char**)CABOptions, reference , 7, 2);
  if (result == 1) {     // Add loco, should only show locos we don't yet own
    char *addOpts [locomotiveCount+1];
    for (uint8_t n = 0; n<locomotiveCount; n++) if (!locoRoster[n].owned) addOpts[option++] = locoRoster[n].name;
    addOpts[option++] = (char*) "Enter loco ID";
    addOpts[option++] = (char*) "Return";
    result = displayMenu ((const char**) addOpts, option, 0);
    if (result != 0 && result < option) {
      if (result < option -1) {
        option = 255;
        result--;
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
              displayTempMessage ((char*)txtWarning, "unable to add loco to roster", true);
            }
            else {
              // update the slot with our data
              locoRoster[option].id = tLoco;
              if (locoRoster[option].id < 128) locoRoster[option].type = 'S';
              else locoRoster[option].type = 'L';
              if (cmdProtocol==DCCEX) {
                locoRoster[option].owned = true;
                #ifdef RELAYPORT
                locoRoster[option].relayIdx = 240;  // "Magic number" => owned by serial link when in relaying context
                #endif
              }
              locoRoster[option].direction = STOP;
              locoRoster[option].speed     = -1;
              locoRoster[option].steps     = 128;
              locoRoster[option].id        = tLoco;
              locoRoster[option].function  = 0;
              locoRoster[option].reverseConsist = false;
              sprintf (locoRoster[option].name, "Loco-ID-%d", tLoco);
            }
          }
        }
      }
      if (option != 255) {
        // Now if we have a new loco, try to add it to our throttle
        char displayLine[40];
        locoRoster[option].steal = '?';
        locoRoster[option].direction = locoRoster[initialLoco].direction;
        locoRoster[option].speed = -1;
        if (cmdProtocol==WITHROT) {
          locoRoster[option].throttleNr = nextThrottle++;
          if (nextThrottle > 'Z') nextThrottle = '0';
          setLocoOwnership (option, true);
          while (cmdProtocol==WITHROT && locoRoster[option].steal == '?') delay (100);
          if (locoRoster[option].steal == 'Y') {
            sprintf (displayLine, "Steal loco %s?", locoRoster[option].name);
            if (displayYesNo (displayLine)) {
              setStealLoco (option);
            }
          }
        }
        else if (cmdProtocol==DCCEX) {
          locoRoster[option].steal = 'N';
          locoRoster[option].owned = true;
          locoRoster[option].reverseConsist = false;
          #ifdef RELAYPORT
          locoRoster[option].relayIdx = 240;
          #endif
        }
      }
    }
  }
  else if (result == 2) {
    mkSetReverseMenu();
  }
  else if (result == 3) {  // remove loco, should only list locos we own
    result = mkGetLocoMenu(0);
    if (result != 255) {
      setLocoOwnership (result, false);
      locoRoster[result].owned = false;
      #ifdef RELAYPORT
      locoRoster[result].relayIdx = 255;
      #endif
    }
  }
  else if (result == 4) {
    mkRouteMenu();
  }
  else if (result == 5) {
    mkSetLeadLocoMenu();
  }
  else if (result == 6) {
    mkTurnoutMenu();
  }
  for (uint8_t n = 0; n<limit; n++) if (locoRoster[n].owned) retval++;
  return (retval);
}

// for cab menu, select a controlled loco
// variation 0 exclude lead loco
// variation 1 include lead loco
// return loco index in roster
uint8_t mkGetLocoMenu (uint8_t variation)
{
  uint8_t limit = locomotiveCount + MAXCONSISTSIZE;
  uint8_t retVal=255;
  uint8_t count = 0;
  uint8_t option = 0;
  uint8_t leadIndex = 0;

  for (uint8_t n = 0; n<limit; n++) if (locoRoster[n].owned) count++;
  char *selectOpts[count+1];
  uint8_t reference[count+1];
  for (uint8_t n = 0; n<limit; n++) if (locoRoster[n].owned && (variation == 1 || n != initialLoco || count == 1)) {
    if (n == initialLoco) leadIndex = option;
    reference[option] = n;
    selectOpts[option++] = locoRoster[n].name;
  }
  reference[option] = 255;
  selectOpts[option++] = (char*) "Return";
  count = displayMenu ((const char**) selectOpts, option, leadIndex);
  if (count > 0 && count < option) {
    retVal = reference[count-1];
  }
  return(retVal);
}

void mkSetReverseMenu()
{
  uint8_t switchLoco = mkGetLocoMenu(1);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkSetReverseMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (switchLoco != 255 && switchLoco != initialLoco) {
    int16_t speed = 0;
    uint8_t direction = 0;
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (locoRoster[switchLoco].reverseConsist) locoRoster[switchLoco].reverseConsist = false;
      else locoRoster[switchLoco].reverseConsist = true;
      direction = locoRoster[switchLoco].direction;
      speed = locoRoster[switchLoco].speed;
      xSemaphoreGive(velociSem);
      setLocoDirection (switchLoco, direction);
      setLocoSpeed (switchLoco, speed, direction);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
}

void mkSetLeadLocoMenu()
{
  uint8_t newLead = mkGetLocoMenu(1);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkSetLeadLocoMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (newLead != 255 && newLead != initialLoco) {
    const uint32_t maxMask = 1<<31;
    uint32_t mask = 1;
    uint32_t oldFuncs;
    uint32_t tval;
    uint8_t oldLead = initialLoco;
    initialLoco = newLead;
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      oldFuncs = locoRoster[oldLead].function;
      xSemaphoreGive(velociSem);
      // now have to swap any slected lead loco only functions
      for (uint8_t functPrefix=0; mask<maxMask; mask<<=1, functPrefix++) {
        tval = oldFuncs & mask;
        if (tval > 0) {
          setLocoFunction (oldLead, functPrefix, false);
          setLocoFunction (newLead, functPrefix, true);
        }
      }
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
}

void mkRotateMenu()
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkRotateMenu()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  #ifndef SCREENROTATE
  displayTempMessage ("Warning:", "Screen rotation not supported on this screen type", true);
  #else
  #if SCREENROTATE == 2
  uint8_t opts = 3;
  const char *rotateMenu[] = {"0 deg (Normal)", "180 deg (Invert)", "Prev. Menu"};
  #else
  uint8_t opts = 5;
  const char *rotateMenu[] = {"0 deg (Normal)", "90 deg Right", "180 deg (Invert)", "90 deg Left", "Prev. Menu"};
  #endif
  uint8_t result = displayMenu (rotateMenu, opts, 0);
  if (result > 0 && result < opts) {
    result--;
    screenRotate = result;
    nvs_put_int ("screenRotate", result);
    setupFonts();
  }
  #endif
}

void displayInfo()
{
  char outData[80];
  uint8_t lineNr = 0;
  static uint8_t currentOpt = 0;
  
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s displayInfo()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  display.clear();
  if (currentOpt == 0) {
    currentOpt++;
    sprintf (outData, "%s %s", PRODUCTNAME, VERSION);
    displayScreenLine (outData, lineNr++, false);
    if (lineNr >= linesPerScreen) return;
  }
  if (currentOpt == 1) {
    currentOpt++;
    sprintf (outData, "Name: %s", tname);
    displayScreenLine (outData, lineNr++, false);
    if (lineNr >= linesPerScreen) return;
  }
  if (currentOpt == 2) {
    currentOpt++;
    if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      uint32_t tint = fc_time;
      xSemaphoreGive(fastClockSem);
      if (fc_time != 36) {
        char tString[10];
        timeFormat (tString, tint);
        sprintf (outData, "Time: %s", tString);
        displayScreenLine (outData, lineNr++, false);
        if (lineNr >= linesPerScreen) return;
      }
    }
  }
  if (currentOpt == 3) {
    currentOpt++;
    if (ssid[0]!='\0') {
      IPAddress ip = WiFi.localIP();
      if (charsPerLine < 20)
        sprintf (outData, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      else
        sprintf (outData, "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      displayScreenLine (outData, lineNr++, false);
      if (lineNr >= linesPerScreen) return;
      sprintf (outData, "SSID: %s", ssid);
      displayScreenLine (outData, lineNr++, false);
    }
    if (lineNr >= linesPerScreen) return;
  }
  if (currentOpt == 4) {
    currentOpt++;
    sprintf (outData, "Proto: %s", protoList[cmdProtocol]);
    displayScreenLine (outData, lineNr++, false);
    if (lineNr >= linesPerScreen) return;
  }
  if (currentOpt == 5) {
    currentOpt++;
    #ifdef RELAYPORT
    sprintf (outData, "Relay: %s", relayTypeList[relayMode]);
    displayScreenLine (outData, lineNr++, false);
    if (lineNr >= linesPerScreen) return;
    #endif
  }
  if (currentOpt == 6) {
    currentOpt++;
    #ifdef SERIALCTRL
    displayScreenLine ("Svr: Serial Connect", lineNr++, false);
    #else
    if (strlen (remServerNode) > 0) {
      sprintf (outData, "Svr: %s", remServerNode);
      displayScreenLine (outData, lineNr++, false);
    }
    #endif
    if (lineNr >= linesPerScreen) return;
  }
  if (currentOpt == 7) {
    currentOpt++;
    for (uint8_t n=0; n<4; n++) if (dccLCD[n][0] != '\0') {
      if (charsPerLine<=16) sprintf (outData, "*%s", dccLCD[n]);
      else sprintf (outData, "LCD%d: %s", n, dccLCD[n]);
      displayScreenLine (outData, lineNr++, false);
      if (lineNr >= linesPerScreen) return;
    }
  }
  if (currentOpt == 8) {
    currentOpt++;
    if (strlen (remServerDesc) > 0) {
      sprintf (outData, "Desc: %s", remServerDesc);
      displayScreenLine (outData, lineNr++, false);
      if (lineNr >= linesPerScreen) return;
    }
  }
  if (currentOpt == 9) {
    currentOpt++;
    if (strlen (remServerType) > 0) {
      sprintf (outData, "Type: %s", remServerType);
      displayScreenLine (outData, lineNr++, false);
      if (lineNr >= linesPerScreen) return;
    }
  }
  if (currentOpt == 10) {
    currentOpt++;
    if (strlen (lastMessage) > 0) {
      sprintf (outData, "Msg: %s", lastMessage);
      displayScreenLine (outData, lineNr++, false);
      if (lineNr >= linesPerScreen) return;
    }
  }
  // Got thus far ... so back to opt zero;
  currentOpt = 0;
  if (lineNr==0) displayInfo(); // try again if we displayed nothing
}


//
// display menu after weeding out code-200 items not for display
//
uint8_t displayMenu (const char **menuItems, uint8_t *menuIndex, uint8_t itemCount, uint8_t selectedItem)
{
  uint8_t retVal = -1;
  uint8_t cnt = 0;
  uint8_t newIndex[itemCount];
  uint8_t newSelection = 0;
  char *newItems[itemCount];
  bool allMenuItems = false;

  if (nvs_get_int ("allMenuItems", 0) == 1) allMenuItems = true;
  for (uint8_t n = 0; n<itemCount; n++) if (allMenuItems || menuIndex[n]!=200) {
    newIndex[cnt]= menuIndex[n];
    newItems[cnt]= (char*) menuItems[n];
    if (n == selectedItem) newSelection = cnt;
    cnt++;
  }
  retVal = displayMenu ((const char **)newItems, cnt, newSelection);
  if (retVal > 0) retVal = newIndex[retVal-1];
  return (retVal);
}


//
// display all menu items and return result
//
uint8_t displayMenu (const char **menuItems, uint8_t itemCount, uint8_t selectedItem)
{
  uint8_t currentItem = selectedItem;
  uint8_t displayLine;
  uint8_t exitCode = 255;
  uint8_t menuWrap = nvs_get_int ("menuWrap", 0);
  char commandKey;
  bool hasChanged = true;
  bool lineChanged[itemCount];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s displayMenu(%x, %d, %d)\r\n", getTimeStamp(), menuItems, itemCount, selectedItem);
    xSemaphoreGive(consoleSem);
  }
  for (uint8_t n=0; n<itemCount; n++) lineChanged[n] = true;
  if (selectedItem > itemCount) selectedItem = 0;
  display.clear();
  menuMode = true;
  while (exitCode == 255) {
    if (hasChanged) {
      hasChanged = false;
      if (currentItem >= (linesPerScreen-1)) {
        displayLine = currentItem - (linesPerScreen-1);
        for (uint8_t n=0; n<itemCount; n++) lineChanged[n] = true;
      }
      else displayLine = 0;
      for (uint8_t n=0; n<linesPerScreen && displayLine < itemCount; n++, displayLine++) if (lineChanged[displayLine]) {
        displayScreenLine (menuItems[displayLine], n, displayLine==currentItem);
        lineChanged[displayLine] = false;
      }
    }
    if (xQueueReceive(keyboardQueue, &commandKey, pdMS_TO_TICKS(10)) == pdPASS) {
      #ifdef SCREENSAVER
      if (xSemaphoreTake(screenSvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (inScreenSaver) {
          ledcWrite(0, backlightValue);
          inScreenSaver = false;
          if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s SCREEN: Blanking ended\r\n", getTimeStamp());
            xSemaphoreGive(consoleSem);
          }
        }
        screenActTime = esp_timer_get_time();
        xSemaphoreGive(screenSvrSem);
      }
      else semFailed ("screenSvrSem", __FILE__, __LINE__);
      #endif
      if (commandKey == 'S' || commandKey == 'R') {
        exitCode = currentItem + 1;
      }
      else if (commandKey == 'E' || commandKey == 'L') {
        exitCode = 0;
      }
      else if (commandKey == 'D') {
        if (currentItem > 0) {
          lineChanged[currentItem] = true;
          currentItem--;
          lineChanged[currentItem] = true;
          hasChanged = true;
        }
        else if (menuWrap == 1) {
          lineChanged[currentItem] = true;
          currentItem = itemCount - 1;
          lineChanged[currentItem] = true;
          hasChanged = true;
        }
      }
      else if (commandKey == 'U') {
        if (currentItem < (itemCount-1)) {
          lineChanged[currentItem] = true;
          currentItem++;
          lineChanged[currentItem] = true;
          hasChanged = true;
        }
        else if (menuWrap == 1) {
          if (itemCount < linesPerScreen) {
            lineChanged[currentItem] = true;
            currentItem = 0;
            lineChanged[currentItem] = true;
          }
          else {
            for (uint8_t j=0; j<itemCount; j++) lineChanged[j] = true;
            currentItem = 0;
          }
          hasChanged = true;
        }
      }
    }
    else {
      delay (debounceTime);
      #ifdef SCREENSAVER
      if (xSemaphoreTake(screenSvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if ((!inScreenSaver) && blankingTime>0 && (esp_timer_get_time() - screenActTime) > blankingTime) {
          ledcWrite(0, 0);
          inScreenSaver = true;
          if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s SCREEN: Blanking started\r\n", getTimeStamp());
            xSemaphoreGive(consoleSem);
          }
          inScreenSaver = true;
        }
        xSemaphoreGive(screenSvrSem);
      }
      else semFailed ("screenSvrSem", __FILE__, __LINE__);
      #endif
    }
  }
  menuMode = false;
  if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (exitCode==0) Serial.printf ("%s MENU: No selection / Back One Level\r\n", getTimeStamp());
    else Serial.printf ("%s MENU: Option %d: %s\r\n", getTimeStamp(), exitCode, menuItems[(exitCode-1)]);
    xSemaphoreGive(consoleSem);
  }
  return (exitCode);
}


void displayScreenLine (const char *menuItem, uint8_t lineNr, bool inverted)
{
  char linedata[charsPerLine+1];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    char *boolind = (char*)"false";
    if (inverted) boolind = (char*) "true";
    Serial.printf ("%s displayScreenLine(%s, %d, %s)\r\n", getTimeStamp(), menuItem, lineNr, boolind);
    xSemaphoreGive(consoleSem);
  }
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
  if (inverted) {
    #ifdef INVERTCOLOR
    display.setColor (INVERTCOLOR);
    #endif
    #ifdef BACKCOLOR
    display.setBackground (BACKCOLOR);
    #endif
    display.invertColors();
  }
  display.printFixed(0, (lineNr*selFontHeight), linedata, STYLE_NORMAL);
  if (inverted) {
    display.invertColors();
    #ifdef  STDCOLOR
    display.setColor (STDCOLOR);
    #endif
    #ifdef BACKCOLOR
    display.setBackground (BACKCOLOR);
    #endif
  }
}

void displayScreenLine (const char *menuItem, uint8_t lineNr, char function, bool inverted)
{
  char linedata[charsPerLine+1];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    char *boolind = (char*)"false";
    if (inverted) boolind = (char*) "true";
    Serial.printf ("%s displayScreenLine(%s, %d, %c)\r\n", getTimeStamp(), menuItem, lineNr, function);
    xSemaphoreGive(consoleSem);
  }
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
 
  #ifdef COLORDISPLAY
  switch (function) {
    #ifdef LEADCOLOR
      case 'L':
        display.setColor (LEADCOLOR);
        break;
    #endif
    #ifdef REVERSCOLOR
      case 'R':
        display.setColor (REVERSCOLOR);
        break;
    #endif
    #ifdef INVERTCOLOR
      case 'I':
        display.setColor (INVERTCOLOR);
        break;
    #endif
    #ifdef INPUTCOLOR
      case 'K':
        display.setColor (INPUTCOLOR);
        break;
    #endif
    #ifdef WARNCOLOR
      case 'W':
        display.setColor (WARNCOLOR);
        break;
    #endif
    #ifdef  STDCOLOR
      default:
        display.setColor (STDCOLOR);
        break;
    #endif
  }
  #ifdef BACKCOLOR
  display.setBackground (BACKCOLOR);
  #endif
  #endif
  if (inverted) {
    display.invertColors();
  }
  display.printFixed(0, (lineNr*selFontHeight), linedata, STYLE_NORMAL);
  if (inverted) {
    display.invertColors();
  }
  #ifdef  STDCOLOR
  display.setColor (STDCOLOR);
  #endif
  #ifdef BACKCOLOR
  display.setBackground (BACKCOLOR);
  #endif
}

// wait for up to 30 seconds to read CV data
int16_t wait4cv()
{
  int16_t retVal = -1;
  
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s wait4cv()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  display.clear();
  displayScreenLine ("Wait for data", 1, false);
  if (xQueueReceive(cvQueue, &retVal, pdMS_TO_TICKS(30000)) != pdPASS) {
    retVal = -1;
  }
  return (retVal);
}


uint8_t displayTempMessage (const char *header, const char *message, bool wait4response)
{
  uint8_t lineNr = 0;
  uint8_t offset = 0;
  char lineData[charsPerLine+1];
  char *charPtr = (char *) message;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    char *boolind = (char*)"false";
    if (wait4response) boolind = (char*) "true";
    if (header!=NULL && message!=NULL) Serial.printf ("%s displayTempMessage(%s, %s, %s)\r\n", getTimeStamp(), header, message, boolind);
    else if (header!=NULL)  Serial.printf ("%s displayTempMessage(%s, NULL, %s)\r\n", getTimeStamp(), header, boolind);
    else if (message!=NULL) Serial.printf ("%s displayTempMessage(NULL, %s, %s)\r\n", getTimeStamp(), message, boolind);
    xSemaphoreGive(consoleSem);
  }
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

bool displayYesNo (const char *question)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s displayYesNo(%s)\r\n", getTimeStamp(), question);
    xSemaphoreGive(consoleSem);
  }

  uint8_t baseLine = displayTempMessage (NULL, question, false) + 1;
  uint8_t command = 255;
  bool option = true;

  menuMode = true;
  while (command != 'S' && command != 'R' && command != 'L') {  // Select or left right to accept answer
    if (command == 255 || xQueueReceive(keyboardQueue, &command, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
      if (command == 255) command = 254;
      else if (command == 'U' || command == 'D') option = !option; // up down to move between answers
      displayScreenLine (" Yes", baseLine,    option);
      displayScreenLine (" No",  baseLine+1, !option);
    }
  }
  menuMode = false;
  if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s PROMPT: %s\r\n   YES/NO: ", getTimeStamp(), question);
    if (option) Serial.printf ("Yes\r\n");
    else Serial.printf ("No\r\n");
    xSemaphoreGive(consoleSem);
  }
  return (option);
}

int enterNumber(const char *prompt)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s enterNumber(%s)\r\n", getTimeStamp(), prompt);
    xSemaphoreGive(consoleSem);
  }
  #ifdef keynone
  displayTempMessage ("Warning:", "Cannot enter number without a keypad", true);
  return (-1);
  #else
  int retVal = 0;
  char *tptr;
  char inKey = 'Z';
  char dispNum[8];
  char inBuffer[7];
  uint16_t charPosition = 0; // use uint16_t - some displays will have more than 256 pixels width/height
  uint16_t x, y;

  x = 4 * selFontWidth;
  display.clear();
  y = (displayTempMessage (NULL, prompt, false)) + (1.5 * selFontHeight);
  #ifdef INPUTCOLOR
  display.setColor (INPUTCOLOR);
  #endif
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
      else if ((inKey == 'L' || inKey == 'R') && charPosition>0) charPosition--;
    }
    else inKey = 'E';
  }
  #ifdef STDCOLOR
  display.setColor (STDCOLOR);
  #endif
  if (inKey == 'E') retVal = -1;
  if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s PROMPT: %s\r\n   NUMBER: ", getTimeStamp(), prompt);
    if (retVal<0) Serial.printf ("No selection / Back One Level\r\n");
    else Serial.printf ("Value %d\r\n", retVal);
    xSemaphoreGive(consoleSem);
  }
  return (retVal);
  #endif
}

char* enterAddress(const char *prompt)
{
  static char retVal[16];
  char displayVal[17];
  uint16_t x,y;
  uint16_t tVal = 0;
  uint8_t tPtr = 0;
  uint8_t dotCount = 0;
  char inKey = 'Z';
  
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s enterAddress(%s)\r\n", getTimeStamp(), prompt);
    xSemaphoreGive(consoleSem);
  }
  x = 0;
  display.clear();
  y = displayTempMessage (NULL, prompt, false) + (2 * selFontHeight);
  #ifdef INPUTCOLOR
  display.setColor (INPUTCOLOR);
  #endif
  while (inKey != 'S' && inKey != 'E' && inKey != '#' && tPtr<sizeof(retVal)) {
    retVal[tPtr] = '\0';
    sprintf (displayVal, "%s ", retVal);
    display.printFixed (x, y, displayVal, STYLE_NORMAL);
    if (xQueueReceive(keyboardQueue, &inKey, pdMS_TO_TICKS(30000)) == pdPASS) {
      if (tPtr < (sizeof (retVal) - 1) && inKey >= '0' && inKey <= '9' && (tVal+(inKey-'0'))<256) {
        retVal[tPtr++] = inKey;
        tVal = (tVal+(inKey-'0')) * 10;
        if (tVal>255 && dotCount<3) {
          retVal[tPtr++] = '.';
          dotCount++;
          tVal = 0;
        }
      }
      else if ((inKey == '*' || inKey == 'P' || inKey == 'X' || inKey == 'Y') && tPtr > 0 && retVal[tPtr-1] != '.' && dotCount < 3) {
        // NB: if running in potthrot mode the * key may present as P, either way make this a dot 
        retVal[tPtr++] = '.';
        dotCount++;
        tVal = 0;
      }
      else if ((inKey == 'L' || inKey == 'R') && tPtr>0) {
        tPtr--;
        if (retVal[tPtr]=='.') {
          tPtr--;
          dotCount--;
        }
        else tVal = tVal/10;
      }
    }
    else inKey = 'E';
  }
  #ifdef STDCOLOR
  display.setColor (STDCOLOR);
  #endif
  if (inKey == 'E') retVal[0] = '\0';
  else retVal[tPtr] = '\0';
  if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s PROMPT: %s\r\n   ADDRESS: %s\r\n", getTimeStamp(), prompt, retVal);
    xSemaphoreGive(consoleSem);
  }
  return (retVal);
}
#endif
