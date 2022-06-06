/*
 * Seprate locomotive control from other functions
 */
void locomotiveDriver()
{
  const char *dirString[] = {">>", "||", "<<", "??"};
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
  int16_t warnSpeed = nvs_get_int ("warnSpeed", 90);
  int16_t calcSpeed = 0;
  uint8_t calcDirection = STOP;
  char commandChar = 'Z';
  char releaseChar = 'Z';
  char displayLine[65];
  char potVal[4];
  char funVal[4];
  bool inFunct = false;
  bool dirChange = false;
  bool brakeFlag = true;

  potVal[0] = '\0';
  funVal[0] = '\0';
  speedStep = nvs_get_int ("speedStep", 4);
  while (xQueueReceive(keyReleaseQueue, &releaseChar, pdMS_TO_TICKS(debounceTime)) == pdPASS); // clear release queue
  drivingLoco = true;
  nextThrottle = 'A';
  targetSpeed = 0;
  locoRoster[initialLoco].steal = '?';
  locoRoster[initialLoco].throttleNr = nextThrottle++;
  setLocoOwnership (initialLoco, true);
  if (cmdProtocol == JMRI) {
    while (locoRoster[initialLoco].steal == '?') delay (100);
    if (locoRoster[initialLoco].steal == 'Y') {
      sprintf (displayLine, "Steal loco %s?", locoRoster[initialLoco].name);
      if (displayYesNo (displayLine)) {
        setStealLoco (initialLoco);
      }
      else locoCount = 0;
    }
  }
  else {
    locoRoster[initialLoco].steal = 'N';
    locoRoster[initialLoco].owned = true;
  }
  if (locoCount > 0) {
    locoRoster[initialLoco].direction = STOP;
    locoRoster[initialLoco].speed = 0;
    #ifdef BRAKEPRESPIN
    brakePres = 0;
    #endif
  }
  // else Serial.println ("No steal");
  int maxLocoArray = locomotiveCount + MAXCONSISTSIZE;
  refreshDisplay = true;
  speedChange = true;
  while (commandChar != 'E' && locoCount > 0 && trackPower) {  // Until escaped out of driving mode
    if (bumpCount > BUMPCOUNT) {
      uint8_t count = 0;
      for (uint8_t n = 0; n<maxLocoArray; n++) if (locoRoster[n].owned) count++;
      if (count != locoCount) locoCount = count;
    }
    else bumpCount++;
    #ifdef BRAKEPRESPIN
    if (bumpCount & 0x01 == 0x01) brakeup ();
    #endif
    if (speedLine == 0) refreshDisplay = true;
    if (refreshDisplay) {
      refreshDisplay = false;
      display.clear();
      lineNr = 0;
      // display all controlled locos, but leave at least 1 status line
      for (uint8_t n=0; n<maxLocoArray && lineNr<(linesPerScreen-2); n++) if (locoRoster[n].owned) {
        // sprintf (displayLine, "Loco: %s", locoRoster[n].name);
        displayScreenLine (locoRoster[n].name, lineNr++, true);
      }
      speedLine = lineNr++;
      if (speedLine < linesPerScreen-4) {
        graphLine = lineNr+2;
        lineNr += 3;
      }
      else if (lineNr+1<linesPerScreen) {
        graphLine = lineNr++;
      }
      else graphLine = 0;
      funcLine = lineNr;
      if (graphLine >= linesPerScreen) graphLine = 0;
      if (funcLine  >= linesPerScreen) funcLine  = 0;
      if (funcLine  <  linesPerScreen-1) funcLine++;
      speedChange = true;
      funcChange = true;
    }
    if ((m==0 && bidirectionalMode) || (m==1 && !bidirectionalMode)) speedChange = true;
    if (speedChange && speedLine<linesPerScreen) {
      speedChange = false;
      // Show speed
      if (bidirectionalMode) m = 1;
      else m = 0;
      #ifdef POTTHROTPIN
      if (enablePot) strcpy (potVal, "Pot");
      else potVal[0] = '\0';
      #endif
      if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        calcDirection = locoRoster[initialLoco].direction;
        if (locoRoster[initialLoco].speed == -1) {
          calcSpeed = 0;
          brakeFlag = true;
        }
        else {
          brakeFlag = false;
          if (locoRoster[initialLoco].steps > 0) {
            calcSpeed = (locoRoster[initialLoco].speed * 100) / (locoRoster[initialLoco].steps - 2);
          }
          else {
            calcSpeed = locoRoster[initialLoco].speed;
          }
        }
        xSemaphoreGive(velociSem);
      }
      if (speedLine > linesPerScreen-4 || speedLine == (graphLine-1)) {
        if (brakeFlag) {
          sprintf (displayLine, "%s BRAKE %3s %3s",   dirString[calcDirection], potVal, trainModeString[m]);
        }
        else {
          if (locoRoster[initialLoco].steps > 0) {
            sprintf (displayLine, "%s %3d%% %3s %4s", dirString[calcDirection], calcSpeed, potVal, trainModeString[m]);
          }
          else {
            sprintf (displayLine, "%s %3d %3s %5s",   dirString[calcDirection], calcSpeed, potVal, trainModeString[m]);
          }
        }
        #ifdef WARNCOLOR
        if (calcSpeed >= warnSpeed) display.setColor (WARNCOLOR);
        #endif
        displayScreenLine (displayLine, speedLine, false);
        #ifdef WARNCOLOR
        if (calcSpeed >= warnSpeed) display.setColor (STDCOLOR);
        #endif
      }
      else {
        char dispTemplate[16];
        if (functPrefix > 9) {
          if (functPrefix > 19) strcpy (funVal, "F2");
          else strcpy (funVal, "F1");
        }
        else funVal[0] = '\0';
        if (charsPerLine > 5) {
          if (charsPerLine < 10) {
            sprintf (dispTemplate, "%%3s %%%ds", charsPerLine-4);
            sprintf (displayLine, dispTemplate, potVal, trainModeString[m]);
          }
          else {
            uint8_t spacer = ((charsPerLine / 3) << 1) - 3;
            sprintf (dispTemplate, "%%2s %%%ds%%%ds", spacer, (charsPerLine - (spacer+3)));
            sprintf (displayLine, dispTemplate, funVal, potVal, trainModeString[m]);
          }
          displayScreenLine (displayLine, speedLine, false);
        }
        else {
          displayScreenLine ((char*) trainModeString[m], speedLine, false);
        }
        if (brakeFlag) {
          strcpy (displayLine, " BRAKE ");
        }
        else {
          if (locoRoster[initialLoco].steps > 0) {
            sprintf (displayLine, "%s %3d%%", dirString[calcDirection], calcSpeed);
          }
          else {
            sprintf (displayLine, "%s %3d",   dirString[calcDirection], calcSpeed);
          }
        }
        // Can be annoying to change text color, default to only change graph colour if present
        //#ifdef WARNCOLOR
        //if (calcSpeed >= warnSpeed) display.setColor (WARNCOLOR);
        //#endif
        #ifdef SCALEFONT
        uint8_t speedPosition = ((charsPerLine>>1) - strlen(displayLine)) * selFontWidth;
        display.printFixedN(speedPosition, ((speedLine+1)*selFontHeight), displayLine, STYLE_NORMAL, 1);
        #else
        uint8_t speedPosition = (((charsPerLine - strlen(displayLine)) * selFontWidth) >> 1);
        display.printFixed(speedPosition, ((speedLine+1)*selFontHeight), displayLine, STYLE_NORMAL);
        #endif
        #ifdef WARNCOLOR
        if (calcSpeed >= warnSpeed) display.setColor (STDCOLOR);
        #endif
      }
      // Display graph line if available
      if (graphLine>0 && locoRoster[initialLoco].steps>0 && graphLine>speedLine) {
        int16_t ypos = graphLine*selFontHeight;
        int16_t xpos = 0;
        int16_t barHeight = selFontHeight - 1;
        uint16_t oldColor = display.getColor();
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          xpos = ((locoRoster[initialLoco].speed * (screenWidth-4)) / (locoRoster[initialLoco].steps-2)) + 2;
          xSemaphoreGive(velociSem);
        }
        if (funcLine > 0 && (funcLine - graphLine) > 1) barHeight = (selFontHeight << 1) - 1;
        if (xpos<0) xpos = 1;
        #ifdef SPEEDBORDER
        display.setColor (SPEEDBORDER);
        #endif
        display.drawRect(0, ypos,   screenWidth-1, ypos + barHeight);
        #ifdef SPEEDBAR
        #ifdef WARNCOLOR
        if (calcSpeed >= warnSpeed) display.setColor (WARNCOLOR);
        else
        #endif
        display.setColor (SPEEDBAR);
        #endif
        display.fillRect(1, ypos+2, xpos         , ypos + (barHeight-2));
        display.setColor (0);
        display.fillRect( xpos+1 ,ypos+1, screenWidth-2, ypos + (barHeight-2));
        display.setColor (oldColor);
        #ifdef BACKCOLOR
        display.setBackground (BACKCOLOR);
        #endif
      }
      // update speedo DAC, 3v voltmeter
      #ifdef SPEEDOPIN
      if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        uint16_t speedo = (locoRoster[initialLoco].speed * 255) / locoRoster[initialLoco].steps;
        dacWrite (SPEEDOPIN, speedo);
        xSemaphoreGive(velociSem);
      }
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
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            dirChange = false;
            if (bidirectionalMode && locoRoster[initialLoco].direction != FORWARD) {
              if (locoRoster[initialLoco].speed > 0) {
                locoRoster[initialLoco].speed = locoRoster[initialLoco].speed - speedStep;
                if (locoRoster[initialLoco].speed < 0) locoRoster[initialLoco].speed = 0;
                setLocoSpeed (initialLoco, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
                #ifdef BRAKEPRESPIN
                brakedown(1);
                #endif
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
            else if (locoRoster[initialLoco].speed < (locoRoster[initialLoco].steps - 2)) {
              speedChange = true;
              locoRoster[initialLoco].speed = locoRoster[initialLoco].speed + speedStep;
              if (locoRoster[initialLoco].speed > (locoRoster[initialLoco].steps - 2)) {
                locoRoster[initialLoco].speed = locoRoster[initialLoco].steps - 2;
              }
              setLocoSpeed (initialLoco, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
            }
            // now set increased speed for other controlled locos, based on initial loco speed
            targetSpeed = locoRoster[initialLoco].speed;
            for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned && n != initialLoco) {
              setLocoSpeed (n, targetSpeed, locoRoster[initialLoco].direction);
              if (dirChange) {
                setLocoDirection (n, locoRoster[initialLoco].direction);
              }
            }
            xSemaphoreGive(velociSem);
          }
          break;
        case 'D':  // Increase reversedness
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            dirChange = false;
            // In bidirectional mode down is an increase of speed if in reverse
            if (bidirectionalMode && locoRoster[initialLoco].direction != FORWARD) {
              // if in bidirectional and direction is not FORWARD, then special handling is required
              if (locoRoster[initialLoco].direction == STOP) {
                locoRoster[initialLoco].direction = REVERSE;
                speedChange = true;
                setLocoSpeed (initialLoco, 0, REVERSE);
                setLocoDirection (initialLoco, REVERSE);
                dirChange = true;
              }
              else if (locoRoster[initialLoco].speed < (locoRoster[initialLoco].steps - 2)) {
                speedChange = true;
                locoRoster[initialLoco].speed = locoRoster[initialLoco].speed + speedStep;
                if (locoRoster[initialLoco].speed > (locoRoster[initialLoco].steps - 2)) {
                  locoRoster[initialLoco].speed = locoRoster[initialLoco].steps - 2;
                }
                setLocoSpeed (initialLoco, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
              }
            }
            else if (locoRoster[initialLoco].speed > 0) { // FORWARD and speed > 0
              speedChange = true;
              locoRoster[initialLoco].speed = locoRoster[initialLoco].speed - speedStep;
              if (locoRoster[initialLoco].speed < 0) locoRoster[initialLoco].speed = 0;
              setLocoSpeed (initialLoco, locoRoster[initialLoco].speed, locoRoster[initialLoco].direction);
              #ifdef BRAKEPRESPIN
              brakedown(1);
              #endif
            }
            else if (bidirectionalMode) {            // FORWARD and speed <= 0
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
            xSemaphoreGive(velociSem);
          }
          break;
        case 'B':
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
              if (locoRoster[n].speed > 0) {
                locoRoster[n].speed = 0;
                speedChange = true;
              }
              setLocoSpeed (n, -1, locoRoster[n].direction);
            }
            xSemaphoreGive(velociSem);
          }
          break;
        case 'L':  // Left / Right = Reverse / Forward
        case 'R':
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
            xSemaphoreGive(velociSem);
          }
          break;
        case 'X':
          if (functPrefix == 10) functPrefix = 0;
          else functPrefix = 10;
          if (speedLine <= linesPerScreen-4) speedChange = true; // update function flag in display
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
          if (speedLine <= linesPerScreen-4) speedChange = true; // update function flag in display
          #ifdef F1LED
          digitalWrite(F1LED, LOW);
          #endif
          #ifdef F2LED
          if (functPrefix == 20) digitalWrite(F2LED, HIGH);
          else digitalWrite(F2LED, LOW);
          #endif
          break;
        case '#':
        case 'S':
          uint8_t check = 0;
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned && locoRoster[n].speed>0) check++;
          if (check == 0) {
            locoCount = mkCabMenu();
          }
          else {
            if (commandChar == '#' || nvs_get_int ("buttonStop", 1) == 0) displayTempMessage ("Warning:", "Cannot enter Cab menu when speed > 0", true);
            else {
              speedChange = true;
              targetSpeed = 0;
              #ifdef BRAKEPRESPIN
              brakedown(locoRoster[initialLoco].speed);
              #endif
              for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
                setLocoSpeed (n, targetSpeed, locoRoster[initialLoco].direction);
              }
              if (showPackets && xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
                Serial.println ("Executed button-stop");
                xSemaphoreGive(displaySem);
              }
            }
          }
          refreshDisplay = true;
          break;
        }
      if (commandChar >= '0' && commandChar <= '9') {
        if (xSemaphoreTake(functionSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          funcChange = true;
          functPrefix += (commandChar - '0');
          uint16_t mask = 1 << functPrefix;
          inFunct = true;
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
            if (cmdProtocol==JMRI) setLocoFunction (n, functPrefix, true);
            else if ((mask & locoRoster[n].function) == 0) { // set if not yet set
              locoRoster[n].function = locoRoster[n].function | mask;
              setLocoFunction (n, functPrefix, true);
            }
            else {  // unset function
              locoRoster[n].function = locoRoster[n].function & (~mask);
              setLocoFunction (n, functPrefix, false);
            }
          }
          xSemaphoreGive(functionSem);
          if (speedLine <= linesPerScreen-4) speedChange = true; // update function flag in display
        }
        if (cmdProtocol!=JMRI) {
          functPrefix = 0;
          inFunct = false;
        }
        #ifdef F1LED
        digitalWrite(F1LED, LOW);
        #endif
        #ifdef F2LED
        digitalWrite(F2LED, LOW);
        #endif
      }
    }
    if (inFunct && xQueueReceive(keyReleaseQueue, &releaseChar, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
      if (xSemaphoreTake(functionSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        funcChange = true;
        inFunct = false;
        for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) setLocoFunction (n, functPrefix, false);
        functPrefix = 0;
        xSemaphoreGive(functionSem);
      }
    }
    // Don't escape if speed is non zero
    if (commandChar == 'E') {
      for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned && locoRoster[n].speed > 0) {
        commandChar = 'Z';
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          Serial.printf ("No escape permitted, loco speed not zero: %s = %d", locoRoster[n].name, locoRoster[n].speed);
          xSemaphoreGive(displaySem);
        }
      }
    }
  }
  for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
    setLocoOwnership (n, false);
    if (cmdProtocol==DCCPLUS) locoRoster[n].owned = false;
  }
  #ifdef BRAKEPRESPIN
  while (brakePres > 5) {
    brakePres = brakePres - 5;
    dacWrite (BRAKEPRESPIN, brakePres);
    delay(10);
  }
  brakePres = 0;
  dacWrite (BRAKEPRESPIN, 0);
  #endif
  drivingLoco = false;
}

#ifdef BRAKEPRESPIN
void brakeup ()
{
  static int brakeup = nvs_get_int ("brakeup", 1);

  if (brakePres < 255) {
    brakePres += brakeup;
    if (brakePres > 255) brakePres = 255;
    dacWrite (BRAKEPRESPIN, brakePres);
  }
}

void brakedown (int steps)
{
  static int brakedown = nvs_get_int ("brakedown", 20);

  if (brakedown < 1) brakedown = 1;
  if (brakePres > 0) {
    int mods = steps * (brakePres / brakedown);
    if (mods == 0) {
      mods = brakedown / 10;
      if (mods == 0) mods = 1;
    }
    brakePres = brakePres - mods;
    if (brakePres < 0) brakePres = 0;
  }
}
#endif

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
  if (lineNr < (linesPerScreen - 1)) y += 2;  // add extra spacing
  
  #ifdef FUNCCOLOR
  display.setColor (FUNCCOLOR);
  #endif
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
  #ifdef STDCOLOR
  display.setColor (STDCOLOR);
  #endif
  #ifdef BACKCOLOR
  display.setBackground (BACKCOLOR);
  #endif
}
