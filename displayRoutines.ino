void setupFonts()
{
  static uint8_t fontIndex = 1;

  fontIndex      = nvs_get_int ("fontIndex", 1);
  selFontWidth   = fontWidth[fontIndex];
  selFontHeight  = fontHeight[fontIndex];
  charsPerLine   = screenWidth  / selFontWidth;
  linesPerScreen = screenHeight / selFontHeight;
  switch (fontIndex) {
    case 0:
      display.setFixedFont(ssd1306xled_font6x8);
      break;
    case 1:
      display.setFixedFont(ssd1306xled_font8x16);
      break;
    case 2:
      display.setFixedFont(font_10x20);
      break;
    case 3:
      display.setFixedFont(font_18x18);
      break;
  }
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
  char *stateMenu[turnoutStateCount];
  uint8_t result = 255;
  uint8_t reqState = 255;
  uint8_t option = 0;

  if (switchCount == 0 || turnoutStateCount == 0) {
    displayTempMessage ("Warning:", "No switches or switch states defined", true);
    return;
  }
  for (uint8_t n=0; n<switchCount; n++) switchMenu[n] = turnoutList[n].userName;
  switchMenu[switchCount] = (char*) prevMenuOption;
  for (uint8_t n=0; n<turnoutStateCount; n++) stateMenu[n] = turnoutState[n].name;
  while (result!=0) {
    result = displayMenu (switchMenu, switchCount+1, lastSwitchMenuOption);
    if (result == (switchCount+1)) result = 0;  // give option to go to previous menu
    if (result > 0) {
      lastSwitchMenuOption = result - 1;
      if (turnoutList[result-1].state == turnoutState[0].state) option = 0;
      else option = 1;
      reqState = displayMenu (stateMenu, turnoutStateCount, option);
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


void mkConfigMenu()
{
  const char *configMenu[] = {"Speed Step", "Font", "CPU Speed", "Protocol", "Prev. Menu"};
  uint8_t result = 1;

  while (result != 0) {
    result = displayMenu ((char**) configMenu, 5, (result-1));
    switch (result) {
      case 1:
        mkSpeedStepMenu();
        break;
      case 2:
        mkFontMenu();
        break;
      case 3:
        mkCpuSpeedMenu();
        break;
      case 4:
        mkProtoPref();
        break;
      default:
        result = 0;
        break;
    }
  }
}

void mkFontMenu()
{
  char fontMenu[sizeof(fontWidth) + 1][32];
  char *mp[sizeof(fontWidth) + 1];
  uint8_t result;

  for (uint8_t n=0; n<sizeof(fontWidth); n++) sprintf (fontMenu[n], "%s (%dx%d chars)", fontLabel[n], (screenWidth/fontWidth[n]), (screenHeight/fontHeight[n]));
  sprintf (fontMenu[sizeof(fontWidth)], prevMenuOption);
  for (uint8_t n=0;n<sizeof(fontWidth) + 1; n++) mp[n] = (char*) &fontMenu[n];
  result = displayMenu (mp, (sizeof(fontWidth) + 1), 2);
  if (result > 0 && result <= sizeof(fontWidth)) {
    result--;
    nvs_put_int ("fontIndex", result);
    setupFonts();
  }
}


void mkSpeedStepMenu()
{
  char speedMenu[10][10];
  char *mp[10];
  uint8_t result;
  uint8_t speedStep = nvs_get_int ("speedStep", 4);

  for (uint8_t n=0; n<9 ;n++) sprintf (speedMenu[n], "%d/click",  n+1);
  sprintf (speedMenu[9], prevMenuOption);
  for (uint8_t n=0; n<10; n++) mp[n] = (char*) &speedMenu[n];
  result = displayMenu (mp, 10, speedStep-1);
  if (result > 0 && result <= 9) {
    nvs_put_int ("speedStep", result);
  }
}


void mkCpuSpeedMenu()
{
  char *cpuMenu[] = {"80MHz", "160MHz", "240MHz", "Prev. Menu"};
  uint8_t result;

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
      nvs_put_int ("cpuspeed", 80);
      break;
    case 2:
      nvs_put_int ("cpuspeed", 160);
      break;
    case 3:
      nvs_put_int ("cpuspeed", 240);
      break;
  }
}


void mkProtoPref()
{
  char *protoMenu[] = {"JMRI", "DCC++", "Prev. Menu"};
  uint8_t result = displayMenu (protoMenu, 3, (cmdProtocol-1));
  if (result > 0 && result<3) {
    cmdProtocol = result;
    nvs_put_int ("defaultProto", result);
  }
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
  display.printFixed(0, (lineNr*selFontHeight), linedata, STYLE_NORMAL);
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

  x = 4 * selFontWidth;
  y = 1.5 * selFontHeight;
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
