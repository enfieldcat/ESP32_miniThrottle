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
 * Seprate locomotive control from other functions
 */
#ifndef NODISPLAY
void locomotiveDriver()
{
  const char *dirString[] = {">>", "||", "<<", "??"};
  const char *trainModeString[] = {"Std", "<->"};
  uint32_t throt_time = 36;
  int16_t t_speed = 0;
  int16_t t_steps = 128;
  uint8_t locoCount = 1;
  uint8_t lineNr = 0;
  uint8_t speedLine = 0;
  uint8_t funcLine = 0;
  uint8_t graphLine = 255;
  uint8_t functPrefix = 0;
  uint8_t m;
  uint8_t speedStep = 4;
  uint8_t bumpCount = 0;
  int16_t warnSpeed = nvs_get_int ("warnSpeed", 90);
  int16_t calcSpeed = 0;
  uint8_t calcDirection = STOP;
  uint8_t t_direction = STOP;
  int8_t speedPosition = 0;
  char commandChar = 'Z';
  char releaseChar = 'Z';
  char displayLine[65];
  char potVal[10];
  char funVal[4];
  bool inFunct = false;
  bool dirChange = false;
  bool brakeFlag = true;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s locomotiveDriver()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }

  potVal[0] = '\0';
  funVal[0] = '\0';
  speedStep = nvs_get_int ("speedStep", 4);
  while (xQueueReceive(keyReleaseQueue, &releaseChar, pdMS_TO_TICKS(debounceTime)) == pdPASS); // clear release queue
  drivingLoco = true;
  nextThrottle = 'A';
  locoRoster[initialLoco].steal = '?';
  locoRoster[initialLoco].throttleNr = nextThrottle++;
  setLocoOwnership (initialLoco, true);
  if (cmdProtocol == WITHROT) {
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
    #ifdef RELAYPORT
    locoRoster[initialLoco].relayIdx = 240;
    #endif
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
      uint8_t dispColorFlag = 'S';
      refreshDisplay = false;
      display.clear();
      lineNr = 0;
      // display all controlled locos, but leave at least 1 status line
      for (uint8_t n=0; n<maxLocoArray && lineNr<(linesPerScreen-2); n++) if (locoRoster[n].owned) {
        if (n == initialLoco) dispColorFlag = 'L';
        else if (locoRoster[n].reverseConsist) dispColorFlag = 'R';
        else dispColorFlag = 'I';
        displayScreenLine (locoRoster[n].name, lineNr++, dispColorFlag, true);
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
    if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (fc_time != throt_time) {
        throt_time = fc_time;
        speedChange = true;
      }
      xSemaphoreGive(fastClockSem);
    }
    if (speedChange && speedLine<linesPerScreen) {
      speedChange = false;
      // Show speed
      if (bidirectionalMode) m = 1;
      else m = 0;
      #ifdef POTTHROTPIN
      if (enablePot) strcpy (potVal, "Pot");
      else if (throt_time != 36) {
        timeFormat (potVal, throt_time);
      }
      else potVal[0] = '\0';
      #else
      if (throt_time != 36) {
        timeFormat (potVal, throt_time);
      }
      else potVal[0] = '\0';
      #endif
      if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
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
      else semFailed ("velociSem", __FILE__, __LINE__);
      if (speedLine > linesPerScreen-4 || speedLine == (graphLine-1)) {
        if (strlen (potVal)>5) potVal[5]='\0';
        if (brakeFlag) {
          sprintf (displayLine, "%sSTOP %-5s %3s",   dirString[calcDirection], potVal, trainModeString[m]);
        }
        else {
          if (locoRoster[initialLoco].steps > 0) {
            sprintf (displayLine, "%s%3d%% %-5s %3s", dirString[calcDirection], calcSpeed, potVal, trainModeString[m]);
          }
          else {
            sprintf (displayLine, "%s %3d %-5s %3s",   dirString[calcDirection], calcSpeed, potVal, trainModeString[m]);
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
        else strcpy (funVal, "  ");
        // else funVal[0] = '\0';
        if (charsPerLine > 5) {
          if (charsPerLine < 12) {
            sprintf (dispTemplate, "%%-5s%%%ds", charsPerLine-5);
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
        speedPosition = strlen(displayLine);
        #ifdef SCALEFONT
        if (speedPosition < (charsPerLine>>1)) {
          speedPosition = ((charsPerLine>>1) - speedPosition) * selFontWidth;
          display.printFixedN(speedPosition, ((speedLine+1)*selFontHeight), displayLine, STYLE_NORMAL, 1);
        }
        else {
        #endif
        speedPosition = (((charsPerLine - speedPosition) * selFontWidth) >> 1);
        display.printFixed(speedPosition, ((speedLine+1)*selFontHeight), displayLine, STYLE_NORMAL);
        #ifdef SCALEFONT
        }
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
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          xpos = ((locoRoster[initialLoco].speed * (screenWidth-4)) / (locoRoster[initialLoco].steps-2)) + 2;
          xSemaphoreGive(velociSem);
        } 
        else semFailed ("velociSem", __FILE__, __LINE__);
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
      if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        uint16_t speedo = (locoRoster[initialLoco].speed * 255) / locoRoster[initialLoco].steps;
        dacWrite (SPEEDOPIN, speedo);
        xSemaphoreGive(velociSem);
      }
      else semFailed ("velociSem", __FILE__, __LINE__);
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
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            dirChange = false;
            t_speed = locoRoster[initialLoco].speed;
            t_direction = locoRoster[initialLoco].direction;
            t_steps = locoRoster[initialLoco].steps;
            xSemaphoreGive(velociSem);
            if (bidirectionalMode && t_direction != FORWARD) {
              if (t_speed > 0) {                                 // Up => decrease reverse speed
                t_speed = t_speed - speedStep;
                if (t_speed < 0) t_speed = 0;
                #ifdef BRAKEPRESPIN
                brakedown(speedStep);
                #endif
              }
              else if (t_direction == REVERSE && t_speed != -1) { // stoped already change direction
                if (cmdProtocol == DCCEX) t_direction = STOP;     // move from REVERSE to STOP
                else t_direction = FORWARD;                       //   or from REVERSE to FORWARD
                dirChange = true;
                t_speed = -1;
              }
              else {
                t_direction = FORWARD;                            // move from STOP to FORWARD
                dirChange = true;
                t_speed = 1;                                      // put into forward motion.
              }
              speedChange = true;
            }
            else if (t_speed < (t_steps - 2)) {                   // FORWARD or not bidirectional
              if (t_direction == STOP) {                          // if stopped default direction is forward
                dirChange = true;
                t_direction = FORWARD;
              }
              speedChange = true;
              t_speed = t_speed + speedStep;
              if (t_speed > (t_steps - 2)) {
                t_speed = t_steps - 2;
              }
            }
            // now set increased speed for all our controlled locos, based on initial loco speed
            for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
              setLocoSpeed (n, t_speed, t_direction);
              if (dirChange) {
                setLocoDirection (n, t_direction);
              }
            }
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
          break;
        case 'D':  // Increase reversedness
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            dirChange = false;
            t_speed = locoRoster[initialLoco].speed;
            t_direction = locoRoster[initialLoco].direction;
            t_steps = locoRoster[initialLoco].steps;
            xSemaphoreGive(velociSem);
            // In bidirectional mode down is an increase of speed if in reverse
            if (bidirectionalMode && t_direction != FORWARD) {
              // if in bidirectional and direction is not FORWARD, then special handling is required
              if (t_direction == STOP) {              // If in STOP, then "down" places us in reverse
                t_direction = REVERSE;
                t_speed = -1;
                speedChange = true;
                dirChange = true;
              }
              else if (t_speed < (t_steps - 2)) {     // bidirectional && REVERSE, make go faster reverse
                speedChange = true;
                t_speed = t_speed + speedStep;
                if (t_speed > (t_steps - 2)) {
                  t_speed = t_steps - 2;
                }
              }
            }
            else if (t_speed > 0) {                   // not bidirectional and speed > 0
              speedChange = true;
              t_speed = t_speed - speedStep;
              if (t_speed < 0) t_speed = 0;
              #ifdef BRAKEPRESPIN
              brakedown(speedStep);
              #endif
            }
            else {
              t_speed = -1;
              // if (cmdProtocol == DCCEX) t_direction = STOP;
            }
            if (t_speed <= 0) {                       // Down to zero speed, direction state may change
              if (cmdProtocol == DCCEX && t_direction != STOP && t_speed != -1) {
                // t_direction = STOP;     // move to STOP
                t_speed = -1;
                dirChange = true;
              }
              else if (bidirectionalMode && t_direction != REVERSE) {
                t_direction = REVERSE;
                t_speed = 1;
                dirChange = true;
              }
            }
            // now set reduced speed for all our controlled locos, based on initial loco speed
            for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
              setLocoSpeed (n, t_speed, t_direction);
              if (dirChange) {
                setLocoDirection (n, t_direction);
              }
            }
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
          break;
        case 'B':
          speedChange = true;
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
            setLocoSpeed (n, -1, STOP);
          }
          #ifdef BRAKEPRESPIN
          brakedown(128);
          #endif
          break;
        case 'L':  // Left / Right = Reverse / Forward
        case 'R':
          uint8_t intended_dir;
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            t_direction = locoRoster[initialLoco].direction;
            xSemaphoreGive(velociSem);
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
          if (commandChar == 'L') intended_dir = REVERSE;
          else intended_dir = FORWARD;
          for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
            if (t_direction != intended_dir) {
              speedChange = true;
              setLocoDirection (n, intended_dir);
              setLocoSpeed (n, 0, intended_dir);
            }
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
            if (nvs_get_int("disableCabMenu", 0) == 0) locoCount = mkCabMenu();
            else {  //Only expect the initialLoco to be controlled if Cab  Menu is disabled
              setLocoOwnership (initialLoco, false);
              locoRoster[initialLoco].owned = false;
              #ifdef RELAYPORT
              locoRoster[initialLoco].relayIdx = 255;
              #endif
              locoCount = 0;
            }
          }
          else {
            if (commandChar == '#' || nvs_get_int ("buttonStop", 1) == 0) {
              displayTempMessage ("Warning:", "Cannot enter Cab menu when speed > 0", true);
            }
            else {
              speedChange = true;
              #ifdef BRAKEPRESPIN
              brakedown(128);
              #endif
              for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
                setLocoSpeed (n, 0, STOP);
              }
              if (showPackets && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.println ("Executed button-stop");
                xSemaphoreGive(consoleSem);
              }
            }
          }
          refreshDisplay = true;
          delay (20);
          break;
        }
      if (commandChar >= '0' && commandChar <= '9') {
        if (xSemaphoreTake(functionSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          // refreshDisplay = true;  // force change to top lines of display
          funcChange = true;
          functPrefix += (commandChar - '0');
          uint16_t mask = 1 << functPrefix;
          inFunct = true;
          if ((mask&defaultLeadVal) > 0) {
            setLocoFunction (initialLoco, functPrefix, true);
          }
          else for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
            setLocoFunction (n, functPrefix, true);
          }
          xSemaphoreGive(functionSem);
          if (speedLine <= linesPerScreen-4) speedChange = true; // update function flag in display
        }
        else semFailed ("velociSem", __FILE__, __LINE__);
        //if (cmdProtocol!=WITHROT {
        //  functPrefix = 0;
        //  inFunct = false;
        //}
        #ifdef F1LED
        digitalWrite(F1LED, LOW);
        #endif
        #ifdef F2LED
        digitalWrite(F2LED, LOW);
        #endif
      }
    }
    if (inFunct && xQueueReceive(keyReleaseQueue, &releaseChar, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
      if (xSemaphoreTake(functionSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        speedChange = true;  // force change to top lines of display
        inFunct = false;
        for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) setLocoFunction (n, functPrefix, false);
        functPrefix = 0;
        xSemaphoreGive(functionSem);
      }
      else semFailed ("functionSem", __FILE__, __LINE__);
    }
    // Don't escape if speed is non zero
    if (commandChar == 'E') {
      for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned && locoRoster[n].speed > 0) {
        commandChar = 'Z';
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("No escape permitted, loco speed not zero: %s = %d", locoRoster[n].name, locoRoster[n].speed);
          xSemaphoreGive(consoleSem);
        }
      }
    }
  }
  for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
    setLocoOwnership (n, false);
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (cmdProtocol==DCCEX) {
        locoRoster[n].owned = false;
        locoRoster[n].reverseConsist = false;
      }
      #ifdef RELAYPORT
      if (locoRoster[n].relayIdx == 240) locoRoster[n].relayIdx = 255;
      #endif
      xSemaphoreGive(velociSem);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
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
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Ending locoDriver session\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
}

#ifdef BRAKEPRESPIN
void brakeup ()
{
  static int brakeup = nvs_get_int ("brakeup", 1);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s brakeup()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }

  if (brakePres < 255) {
    brakePres += brakeup;
    if (brakePres > 255) brakePres = 255;
    dacWrite (BRAKEPRESPIN, brakePres);
  }
}

void brakedown (int steps)
{
  static int brakedown = nvs_get_int ("brakedown", 20);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s brakedown(%d)\r\n", getTimeStamp(), steps);
    xSemaphoreGive(consoleSem);
  }

  if (brakedown < 1) brakedown = 1;
  if (brakePres > 0) {
    int mods = steps * (brakePres / brakedown);
    if (mods == 0) {
      mods = brakedown / 10;
      if (mods == 0) mods = 1;
    }
    brakePres = brakePres - mods;
    if (brakePres < 0) brakePres = 0;
    if (brakePres < 255) {
      dacWrite (BRAKEPRESPIN, brakePres);
    }
  }
}
#endif

void displayFunctions (uint8_t lineNr, uint32_t functions)
{
  uint8_t limit = charsPerLine;
  uint8_t y;
  uint32_t mask = 0x01;
  bool set;
  char fTemplate[32];
  char dis[2];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s displayFunctions(%d, %d)\r\n", getTimeStamp(), lineNr, functions);
    xSemaphoreGive(consoleSem);
  }

  nvs_get_string ("funcOverlay", fTemplate, FUNCOVERLAY, sizeof(fTemplate));
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
#endif // NODISPLAY
