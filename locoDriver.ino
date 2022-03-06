/*
 * Seprate locomotive control from other functions
 */
void locomotiveDriver()
{
  const char *dirString[] = {">>", "||", "<<"};
  const char *trainModeString[] = {"Std", "<->"};
  uint8_t locoCount = 1;
  uint8_t lineNr = 0;
  uint8_t speedLine = 0;
  uint8_t funcLine = 0;
  uint8_t graphLine = 255;
  uint8_t functPrefix = 0;
  uint8_t m;
  uint8_t speedStep = 4;
  uint8_t bumpCount = 0;
  int16_t targetSpeed = 0;
  bool speedChange = false;
  bool dirChange = false;
  char commandChar = 'Z';
  char releaseChar = 'Z';
  char displayLine[40];
  bool inFunct = false;

  speedStep = nvs_get_int ("speedStep", 4);
  while (xQueueReceive(keyReleaseQueue, &releaseChar, pdMS_TO_TICKS(debounceTime)) == pdPASS); // clear release queue
  drivingLoco = true;
  nextThrottle = 'A';
  targetSpeed = 0;
  locoRoster[initialLoco].steal = '?';
  locoRoster[initialLoco].direction = STOP;
  locoRoster[initialLoco].speed = 0;
  locoRoster[initialLoco].throttleNr = nextThrottle++;
  setLocoOwnership (initialLoco, true);
  // Serial.println ("Wait for steal flag");
  while (locoRoster[initialLoco].steal == '?') delay (100);
  if (locoRoster[initialLoco].steal == 'Y') {
    sprintf (displayLine, "Steal loco %s?", locoRoster[initialLoco].name);
    if (displayYesNo (displayLine)) {
      // Serial.println (displayLine);
      setStealLoco (initialLoco);
    }
    else locoCount = 0;
  }
  // else Serial.println ("No steal");
  int maxLocoArray = locomotiveCount + MAXCONSISTSIZE;
  refreshDisplay = true;
  while (commandChar != 'E' && locoCount > 0) {  // Until escaped out of driving mode
    if (bumpCount > BUMPCOUNT) {
      uint8_t count = 0;
      for (uint8_t n = 0; n<maxLocoArray; n++) if (locoRoster[n].owned) count++;
      if (count != locoCount) locoCount = count;
    }
    else bumpCount++;
    if (speedLine == 0) refreshDisplay = true;
    if (refreshDisplay) {
      refreshDisplay = false;
      display.clear();
      lineNr = 0;
      for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
        // sprintf (displayLine, "Loco: %s", locoRoster[n].name);
        displayScreenLine (locoRoster[n].name, lineNr++, true);
      }
      speedLine = lineNr++;
      if (lineNr+1<linesPerScreen) {
        graphLine = lineNr++;
        funcLine = lineNr++;
      }
      else {
        graphLine = 0;
        if (lineNr<linesPerScreen) funcLine = lineNr++;
        else funcLine = 0;
      }
      speedChange = true;
      funcChange = true;
    }
    if ((m==0 && trainSetMode) || (m==1 && !trainSetMode)) speedChange = true;
    if (speedChange && speedLine<linesPerScreen) {
      speedChange = false;
      // Show speed
      if (trainSetMode) m = 1;
      else m = 0;
      if (locoRoster[initialLoco].steps > 0)
        sprintf (displayLine, "%s %3d%% %8s", dirString[locoRoster[initialLoco].direction], ((locoRoster[initialLoco].speed * 100) / locoRoster[initialLoco].steps), trainModeString[m]);
      else
        sprintf (displayLine, "%s %3d %9s", dirString[locoRoster[initialLoco].direction], locoRoster[initialLoco].speed, trainModeString[m]);
      displayScreenLine (displayLine, speedLine, false);
      // Display graph line if available
      if (graphLine>0 && locoRoster[initialLoco].steps>0) {
        uint8_t temp = graphLine*selFontHeight;
        uint8_t xpos = ((locoRoster[initialLoco].speed * (screenWidth-4)) / (locoRoster[initialLoco].steps-1)) + 2;
        uint16_t oldColor = display.getColor();
        display.drawRect(0, temp,   screenWidth-1, temp + (selFontHeight-1));
        // display.setColor (RGB_COLOR8 (128,128,128));
        display.fillRect(1, temp+2, xpos         , temp + (selFontHeight-3));
        display.setColor (0);
        display.fillRect( xpos+1 ,temp+1, screenWidth-2, temp + (selFontHeight-2));
        display.setColor (oldColor);
      }
      // update speedo DAC, 3v voltmeter
      #ifdef SPEEDOPIN
      uint16_t speedo = (locoRoster[initialLoco].speed * 255) / locoRoster[initialLoco].steps;
      dacWrite (SPEEDOPIN, speedo);
      #endif
    }
    if (funcChange && funcLine>0) {
      for (uint8_t n=0; n<maxLocoArray && funcChange; n++) if (locoRoster[n].owned) {
        funcChange = false;    
        displayFunctions (funcLine, locoRoster[n].function);
      }
    }
    //
    // When adjusting speed, multiple units may get out of step with each other for
    // various reasons, eg missed packets on network. So always use the initial loco
    // as the authoritative speed reference.
    //
    if (xQueueReceive(keyboardQueue, &commandChar, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
      switch (commandChar) {
        case 'U':  // Increase forwardness. 
          if (trainSetMode && locoRoster[initialLoco].direction != FORWARD) {
            if (locoRoster[initialLoco].speed > 0) {
              locoRoster[initialLoco].speed = locoRoster[initialLoco].speed - speedStep;
              if (locoRoster[initialLoco].speed < 0) locoRoster[initialLoco].speed = 0;
              setLocoSpeed (initialLoco, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
            }
            else if (locoRoster[initialLoco].direction == REVERSE) {
              locoRoster[initialLoco].direction = STOP;     // move from REVERSE to STOP
            }
            else {
              locoRoster[initialLoco].direction = FORWARD;  // move from STOP to FORWARD
              setLocoSpeed (initialLoco, 0, FORWARD);
              setLocoDirection (initialLoco, FORWARD);
              dirChange = true;
            }
            speedChange = true;
          }
          else if (locoRoster[initialLoco].speed < (locoRoster[initialLoco].steps - 1)) {
            speedChange = true;
            locoRoster[initialLoco].speed = locoRoster[initialLoco].speed + speedStep;
            if (locoRoster[initialLoco].speed > (locoRoster[initialLoco].steps - 1)) {
              locoRoster[initialLoco].speed = locoRoster[initialLoco].steps - 1;
            }
            setLocoSpeed (initialLoco, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
          }
          // now set increased speed for other controlled locos, based on initial loco speed
          targetSpeed = locoRoster[initialLoco].speed;
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned && n != initialLoco) {
            setLocoSpeed (n, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
            if (dirChange) {
              setLocoDirection (n, locoRoster[initialLoco].direction);
            }
          }
          dirChange = false;
          break;
        case 'D':  // Increase reversedness
          // In trainset mode down is an increase of sepped if in reverse
          if (trainSetMode && locoRoster[initialLoco].direction != FORWARD) {
            // if in trainsetmode and direction is not FORWARD, then special handling is required
            if (locoRoster[initialLoco].direction == STOP) {
              locoRoster[initialLoco].direction = REVERSE;
              speedChange = true;
              setLocoSpeed (initialLoco, 0, REVERSE);
              setLocoDirection (initialLoco, REVERSE);
              dirChange = true;
            }
            else if (locoRoster[initialLoco].speed < (locoRoster[initialLoco].steps - 1)) {
              speedChange = true;
              locoRoster[initialLoco].speed = locoRoster[initialLoco].speed + speedStep;
              if (locoRoster[initialLoco].speed > (locoRoster[initialLoco].steps - 1)) {
                locoRoster[initialLoco].speed = locoRoster[initialLoco].steps - 1;
              }
              setLocoSpeed (initialLoco, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
            }
          }
          else if (locoRoster[initialLoco].speed > 0) { // FORWARD and speed > 0
            speedChange = true;
            locoRoster[initialLoco].speed = locoRoster[initialLoco].speed - speedStep;
            if (locoRoster[initialLoco].speed < 0) locoRoster[initialLoco].speed = 0;
            setLocoSpeed (initialLoco, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
          }
          else if (trainSetMode) {            // FORWARD and speed <= 0
            locoRoster[initialLoco].direction = STOP;
            setLocoSpeed (initialLoco, 0, locoRoster[initialLoco].direction);
            speedChange = true;
          }
          // now set reduced speed for other controlled locos, based on initial loco speed
          targetSpeed = locoRoster[initialLoco].speed;
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned && n != initialLoco) {
            setLocoSpeed (n, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
            if (dirChange) {
              setLocoDirection (n, locoRoster[initialLoco].direction);
            }
          }
          dirChange = false;
          break;
        case 'B':
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
            if (locoRoster[n].speed > 0) {
              locoRoster[n].speed = 0;
              speedChange = true;
              setLocoSpeed (n, 0, locoRoster[n].direction);
            }
          }
          break;
        case 'L':  // Left / Right = Reverse / Forward
        case 'R':
          uint8_t intended_dir;
          if (commandChar == 'L') intended_dir = REVERSE;
          else intended_dir = FORWARD;
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
            if (locoRoster[n].direction != intended_dir) {
              if (locoRoster[n].speed > 0) {
                locoRoster[n].speed = 0;
              }
              speedChange = true;
              locoRoster[n].direction = intended_dir;
              setLocoDirection (n, intended_dir);
              setLocoSpeed (n, 0, locoRoster[n].direction);
            }
          }
          break;
        case 'X':
          if (functPrefix == 10) functPrefix = 0;
          else functPrefix = 10;
          #ifdef F1LED
          if (functPrefix == 10) digitalWrite(F1LED, HIGH);
          else digitalWrite(F1LED, LOW);
          #endif
          #ifdef F2LED
          digitalWrite(F2LED, LOW);
          #endif
          break;
        case 'Y':
          if (functPrefix == 20) functPrefix = 0;
          else functPrefix = 20;
          #ifdef F1LED
          digitalWrite(F1LED, LOW);
          #endif
          #ifdef F2LED
          if (functPrefix == 20) digitalWrite(F2LED, HIGH);
          else digitalWrite(F2LED, LOW);
          #endif
          break;
        case '#':
          uint8_t check = 0;
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned && locoRoster[n].speed>0) check++;
          if (check == 0) {
            locoCount = mkCabMenu();
          }
          else displayTempMessage ("Warning:", "Cannot enter Cab menu when speed > 0", true);
          refreshDisplay = true;
          break;
        }
      if (commandChar >= '0' && commandChar <= '9') {
        functPrefix += (commandChar - '0');
        inFunct = true;
        //funcChange = true;
        for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) setLocoFunction (n, functPrefix, true);
        #ifdef F1LED
        digitalWrite(F1LED, LOW);
        #endif
        #ifdef F2LED
        digitalWrite(F2LED, LOW);
        #endif
      }
    }
    if (inFunct && xQueueReceive(keyReleaseQueue, &releaseChar, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
      inFunct = false;
      //funcChange = true;
      for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) setLocoFunction (n, functPrefix, false);
      functPrefix = 0;
    }
    // Don't escape if speed is non zero
    if (commandChar == 'E') {
      for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned && locoRoster[n].speed != 0) {
        commandChar = 'Z';
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          Serial.print ("No escape permitted, loco speed not zero: ");
          Serial.print (locoRoster[n].name);
          Serial.print (" = ");
          Serial.println (locoRoster[n].speed);
          xSemaphoreGive(displaySem);
        }
      }
    }
  }
  for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
    setLocoOwnership (n, false);
  }
  drivingLoco = false;
}


void displayFunctions (uint8_t lineNr, uint32_t functions)
{
  uint8_t limit = charsPerLine;
  uint8_t y;
  uint32_t mask = 0x01;
  bool set;
  const char fTemplate[] = { "*1234567890123456789012345678" };
  char dis[2];

  dis[1] = '\0';
  if (limit > 28) limit = 29;
  y = lineNr*selFontHeight;
  for (uint8_t n=0; n<limit; n++) {
    if (functions & mask) {
      set = true;
      display.invertColors();
    }
    else {
      set = false;
    }
    dis[0] = fTemplate[n];
    display.printFixed((n*selFontWidth), y, dis, STYLE_NORMAL);
    if (set) display.invertColors();
    mask<<=1;
  }
}
