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

/* ************************************************************************\
 * run an automation - the initial purpose of this to give a tool for automated
 * testing via simulated keystrokes. Each automation becomes an instance of the
 * class and potentially we have multiple instances running.
 *
 * In time this may expand to allow custom controls to be created, eg, a button
 * on the control panel for headlights or horn, or an analogue volt-meter to
 * show water level or boiler pressure.
\************************************************************************ */

class runAutomation {

private:
  struct lineTable_s *lineTable = NULL;
  struct jumpTable_s *jumpTable = NULL;
  float lifoStack[LIFO_SIZE];
  float localRegister[REGISTERCOUNT];
  uint16_t currentLine = 0;
  uint16_t numberOfLines = 0;
  uint16_t jumptableSize = 0;
  uint16_t myProcID = 0;
  int8_t stackPtr = 0;
  bool runnableAuto = true;
  bool evalRGB = false;

  void push (float value)
  {
    if (stackPtr<LIFO_SIZE ) {
      lifoStack[stackPtr++] = value;
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s stack overflow in line %d\r\n", getTimeStamp(), currentLine+1);
        xSemaphoreGive(consoleSem);
      }
    }
  }


  float pop()
  {
    float value = 0.00;
    if (stackPtr>0) {
      value = lifoStack[--stackPtr];
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s stack underflow in line %d\r\n", getTimeStamp(), currentLine+1);
        xSemaphoreGive(consoleSem);
      }
    }
    return value;
  }

  float eval(char op)
  {
    float temp, tempa;

    switch( op ) {
      case '+':
        return (pop() + pop());
      case '*':
        return (pop() * pop());
      case '-':
        temp = pop();
        return (pop() - temp);
      case '/': {
        temp = pop();
        if (temp == 0.00) {
          pop ();
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s divide by zero error in line %d\r\n", getTimeStamp(), currentLine+1);
            xSemaphoreGive(consoleSem);
          }
          return (0.00);
        }
        return (pop() / temp);
      }
      case '%': {
        temp = pop();
        if (temp == 0.00) {
          pop ();
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s divide by zero error in line %d\r\n", getTimeStamp(), currentLine+1);
            xSemaphoreGive(consoleSem);
          }
          return (0.00);
        }
        return fmod(pop(), temp);
      }
      case '!': return (1 / pop());
      case 'q': return (cbrt(pop()));
      case 'h': return (hypot(pop(), pop()));
      case '^': temp = pop();
        return (pow (pop(), temp));
      case 'v':
        return (sqrt (pop()));
      case '&':
        temp = pop();
        if (pop()>0 && temp>0) return(1.00);
        return(0.00);
      case '|':
        temp = pop();
        if (pop()>0 || temp>0) return(1.00);
        return(0.00);
      case '=':
        temp = pop();
        if (temp == pop()) return(1.00);
        return(0.00);
      case '>':
        temp = pop();
        if (temp <= pop()) return(1.00);
        return(0.00);
      case '<':
        temp = pop();
        if (temp >= pop()) return(1.00);
        return(0.00);
      case '~':
        if (pop() > 0.5) return (0.00);
        else return(1.00);
      case 'a': return fabs(pop());
      case 's': return sin(pop());
      case 'c': return cos(pop());
      case 't': return tan(pop());
      case 'S': return asin(pop());
      case 'C': return acos(pop());
      case 'T': return atan(pop());
      case 'l': return log10f(pop());
      case 'n': return logf(pop());
      case 'i': temp = pop(); if (temp>0.00) return floor(temp); return ceil(temp);
      case 'I': temp = pop(); if (temp>0.00) return ceil(temp); return floor(temp);
      case 'x': temp = pop(); tempa = pop(); push (temp); return (tempa);
      case 'f': return (util_ftoc (pop()));
      case 'F': return (util_ctof (pop()));
      case 'r': return (util_rtod (pop()));
      case 'R': return (util_dtor (pop()));
    }
  }

  void doShowStack (char op)
  {
    uint8_t tPtr = 0;
    char msgBuffer[20];

    sprintf (msgBuffer, " %c ", op);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s", msgBuffer);
      xSemaphoreGive(consoleSem);
    }
    for (tPtr=0; tPtr<stackPtr; tPtr++) {
      sprintf (msgBuffer, " %9s", util_ftos(lifoStack[tPtr], 4));
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s", msgBuffer);
        xSemaphoreGive(consoleSem);
      }
    }
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("\r\n");
      xSemaphoreGive(consoleSem);
    }
  }

  int need (char op)
  {
    switch( op ) {
      case '+':
      case '*':
      case '-':
      case '/':
      case '%':
      case '^':
      case 'h':
      case 'x':
      case '&':
      case '|':
      case '=':
      case '<':
      case '>':
        return 2;
        break;
      case '~':
      case '!':
      case 'v':
      case 'q':
      case 'a':
      case 's':
      case 'c':
      case 't':
      case 'S':
      case 'C':
      case 'T':
      case 'l':
      case 'i':
      case 'I':
      case 'n':
      case 'f':
      case 'F':
      case 'R':
      case 'r':
        return 1;
        break;
      default:
        if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Invalid rpn operand in line %d\r\n", getTimeStamp(), currentLine+1);
          xSemaphoreGive(consoleSem);
        }
        return 127;
    }
  }

  int checknr (char* number)
  {
    for( ; *number; number++ )
      if((*number < '0' || *number > '9') && *number != '-' && *number != '.') return 0;
    return 1;
  }

  float getvar (char* varname)
  {
    float retval = 0.00;
    char varcopy[32];
    char *subvar;
    char *indexer;
    uint8_t subcnt = 0;
    uint8_t tptr = 0;
    uint8_t limit = strlen(varname);

    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s getvar(%s)\r\n", getTimeStamp(), varname);
      xSemaphoreGive(consoleSem);
    }
    // create copy of var and break into components
    if (strlen(varname) > sizeof(varcopy)) return (0.00);
    strcpy (varcopy, varname);
    for (;tptr < limit && varcopy[tptr]!='.'; tptr++);
    for (;tptr < limit && varcopy[tptr]=='.'; tptr++) varcopy[tptr] = '\0';
    if (tptr < limit) {
      subvar = &varcopy[tptr];
      subcnt++;
      for (;tptr < limit && varcopy[tptr]!='.'; tptr++);
      for (;tptr < limit && varcopy[tptr]=='.'; tptr++) varcopy[tptr] = '\0';
      if (tptr < limit) {
        indexer = &varcopy[tptr];
        subcnt++;
        for (;tptr < limit && varcopy[tptr]!='.'; tptr++);
        for (;tptr < limit && varcopy[tptr]=='.'; tptr++) varcopy[tptr] = '\0';
      }
    }
    // process variables with one part
    if (subcnt == 0) {
      if (strcmp (varname, "random") == 0) {
        retval = (float)rand()/(float)(RAND_MAX);
      }
      if (strcmp (varname, "connected") == 0) {
        #ifdef SERIALCTRL
        retval = 1;
        #else
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (wiCliConnected) retval = 1.00;
          xSemaphoreGive(shmSem);
        }
        #endif
      }
      else if (strcmp (varcopy, "trackpower") == 0){
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (trackPower) retval = 1.00;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "driving") == 0){
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (drivingLoco) retval = 1.00;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "potenabled") == 0){
        #ifdef POTTHROTPIN
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (drivingLoco && enablePot) retval = 1.00;
          xSemaphoreGive(shmSem);
        }
        #else
        retval = 0.00;
        #endif
      }
      else if (strcmp (varcopy, "bidirectional") == 0){
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (bidirectionalMode) retval = 1.00;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "accesspoint") == 0){
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (APrunning) retval = 1.00;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "lococount") == 0) {
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          retval = locomotiveCount;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "turnoutcount") == 0) {
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          retval = turnoutCount;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "turnoutstatecount") == 0) {
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          retval = turnoutStateCount;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "routecount") == 0) {
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          retval = routeCount;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "routestatecount") == 0) {
        if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          retval = routeStateCount;
          xSemaphoreGive(shmSem);
        }
      }
      else if (strcmp (varcopy, "protocol")     == 0) retval = cmdProtocol;
      else if (strcmp (varcopy, "freemem")      == 0) retval = ESP.getFreeHeap();
      else if (strcmp (varcopy, "minfree")      == 0) retval = ESP.getMinFreeHeap();
      else if (strcmp (varcopy, "memsize")      == 0) retval = ESP.getHeapSize();
      else if (strcmp (varcopy, "uptime")       == 0) retval = esp_timer_get_time() / (uS_TO_S_FACTOR * 60.0);
      else if (strcmp (varcopy, "cpufreq")      == 0) retval = ESP.getCpuFreqMHz();
      else if (strcmp (varcopy, "xtalfreq")     == 0) retval = getXtalFrequencyMhz();
      else if (strcmp (varcopy, "psram")        == 0) retval = ESP.getPsramSize();
      else if (strcmp (varcopy, "psramfree")    == 0) retval = ESP.getFreePsram();
      else if (strcmp (varcopy, "flashsize")    == 0) retval = (spi_flash_get_chip_size() / (1024 * 1024));
      #ifdef LED_BUILTIN
      else if (strcmp (varcopy, "ledbuiltin")   == 0) retval = LED_BUILTIN;
      #endif
      #ifdef RGB_BUILTIN
      else if (strcmp (varcopy, "rgbbuiltin")   == 0) retval = RGB_BUILTIN;
      #endif
      #ifdef RGB_BRIGHTNESS
      else if (strcmp (varcopy, "rgbbrightness") == 0) retval = RGB_BRIGHTNESS;
      #endif
      #ifndef NODISPLAY
      else if (strcmp (varcopy, "screenwidth")  == 0) retval = screenWidth;
      else if (strcmp (varcopy, "screenheight") == 0) retval = screenHeight;
      else if (strcmp (varcopy, "fontwidth")    == 0) retval = selFontWidth;
      else if (strcmp (varcopy, "fontheight")   == 0) retval = selFontHeight;
      #ifdef BACKLIGHTPIN
      else if (strcmp (varcopy, "backlight")    == 0) retval = backlightValue;
      #ifdef BLANKINGTIME
      else if (strcmp (varcopy, "blankingtime") == 0) retval = blankingTime;
      #endif
      #endif
      #endif
      else if (strcmp (varcopy, "leadloco") == 0) {
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          retval = initialLoco;
          xSemaphoreGive(velociSem);
        }
      }
      else if (strncmp (varcopy, "reg", 3) == 0 && varcopy[3]>='0' && varcopy[3]<='9' && varcopy[4] == '\0') {
        int8_t i = varcopy[3] - '0';
        retval = localRegister[i];
      }
      else if (strncmp (varcopy, "shreg", 5) == 0 && varcopy[5]>='0' && varcopy[5]<='9' && varcopy[6] == '\0') {
        int8_t i = varcopy[5] - '0';
        if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          retval = sharedRegister[i];
          xSemaphoreGive(procSem);
        }
      }
    }
    else if (strcmp (varcopy, "leadloco")==0) {
      if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (drivingLoco) {
          if (strcmp (subvar, "speed")==0) {
            if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              retval = locoRoster[initialLoco].speed;
              xSemaphoreGive(velociSem);
            }
          }
          else if (strcmp (subvar, "direction")==0) {
            if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              retval = locoRoster[initialLoco].direction;
              xSemaphoreGive(velociSem);
            }
          }
          else if (strcmp (subvar, "id")==0) {
            if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              retval = locoRoster[initialLoco].id;
              xSemaphoreGive(velociSem);
            }
          }
          else if (strcmp (subvar, "steps")==0) {
            if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              retval = locoRoster[initialLoco].steps;
              xSemaphoreGive(velociSem);
            }
          }
          else if (strcmp (subvar, "function")==0) {
            if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              retval = locoRoster[initialLoco].function;
              xSemaphoreGive(velociSem);
            }
            if (subcnt == 2 && util_str_isa_int (indexer)) {
              uint32_t mask = 1;
              uint32_t funcNumber = util_str2int(indexer);
              mask <<= funcNumber;
              funcNumber = retval;
              retval = funcNumber & mask;
              if (retval != 0) retval = 1;
            }
          }
        }
        xSemaphoreGive(shmSem);
      }
    }
    else if (strcmp (varcopy, "dccsensor")==0) {
      if (subcnt == 1 && util_str_isa_int (subvar)) {
        uint32_t id = util_str2int(subvar);
        bool notFound = true;
        if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          for (uint8_t n=0; notFound && n<DCCSENSORCNT; n++) if (dccSensorTable[n].id == id) {
            notFound = false;
            retval = dccSensorTable[n].value;
          }	
          xSemaphoreGive(procSem);
          if (notFound) retval = SENSORUNKNOWN;
        }
      }
    }
    else if (strcmp (varcopy, "localpin")==0) {
      if (subcnt == 1 && util_str_isa_int (subvar)) {
        uint32_t pinNr = util_str2int(subvar);
        bool notFound = true;
        if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          for (uint8_t n=0; notFound && n<LOCALPINCNT && localPinTable[n].pinNr<255; n++) if (localPinTable[n].pinNr == pinNr) {
            notFound = false;
            if (localPinTable[n].assignment == DIN) retval = digitalRead(pinNr);
            else if (localPinTable[n].assignment == AIN) {  // Average of 3 samples.
              for (uint8_t multi=0 ; multi<3; multi++) { retval += analogRead(pinNr); delay (10); }
              retval = retval / 3;
            }
            else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("%s Pin %d is not an input\r\n", getTimeStamp(), pinNr);
              xSemaphoreGive(consoleSem);
            }
          }
          xSemaphoreGive(procSem);
        }
      }
    }
    return (retval);
  }

  float calc( int argc, char** argv )
    {
      int   i;
      float temp;
      bool  showStack = false;
      char  msgBuffer[20];

      if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s getvar(%c, %x)\r\n", getTimeStamp(), argc, argv);
        xSemaphoreGive(consoleSem);
      }
      i = 0;
      stackPtr = 0;
      lifoStack[0] = 0.00;
      if (strcmp(argv[0], "rpn") == 0) {
        showStack = true;
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("Op   Older <-- Stack --> Newer\r\n");
          xSemaphoreGive(consoleSem);
        }
        i++;
      }
      for(; i < argc; i++ ) {
        char* token = argv[i];
        char* endptr;
        char op;

        if(strcmp(token, "-")!=0 && checknr( token ) ) {
          /* We have a valid number. */
          temp = util_str2float( token );
          push(temp);
          if (showStack) doShowStack ('.');
        } else if (strcmp (token, "e") == 0) {
          push (M_E);
          if (showStack) doShowStack ('#');
        } else if (strcmp (token, "pi") == 0) {
          push (M_PI);
          if (showStack) doShowStack ('#');
        } else if (strcmp (token, "g") == 0) {
          push (9.80665);
          if (showStack) doShowStack ('#');
        }
        else {
          if( strlen( token ) != 1 ) {
            temp = getvar (token);
            push (temp);
            if (showStack) doShowStack ('#');
          }
          /* We have an operand (hopefully) */
          else {
            op = token[0];
            if( stackPtr < need(op) ) {
              if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("Too few arguments on stack.\r\n");
                xSemaphoreGive(consoleSem);
              }
            }
            else {
              push(eval(op));
              if (showStack) doShowStack (op);
            }
          }
        }
      }
      if (evalRGB) {
        evalRGB = false;
        if( stackPtr != 3 ) {
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("Found %d arguments on stack, need 3 to set RGB.\r\n", stackPtr);
            xSemaphoreGive(consoleSem);
          }
        }
      }
      else {
        if( stackPtr > 1 ) {
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%d too many arguments on stack.\r\n", (stackPtr-1));
            xSemaphoreGive(consoleSem);
          }
        }
      }
      return (lifoStack[0]);
    }


  void runProcessLine (int lineNr)
  {
    char* buffer;
    char* inLine = lineTable[lineNr].start;
    char* instruction;
    char* param[MAXPARAMS];;
    uint16_t nparam;
    uint16_t lineLen = 0;
    uint16_t n =0;
    bool inSpace = true;
    char token = lineTable[lineNr].token;

    if (lineNr >= numberOfLines || token==250) return;
    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s runProcessLine(%d)\r\n", getTimeStamp(), lineNr);
      xSemaphoreGive(consoleSem);
    }
    // create a copy of the current line that we can parse
    for (; inLine[lineLen] != '\n' && inLine[lineLen] != '\r' && inLine[lineLen] != '\0'; lineLen++);
    buffer = (char*) malloc(lineLen+1);
    for (uint16_t j=0; j<=lineLen; j++) buffer[j] = inLine[j];
    // waste leading white space
    for (instruction = buffer; *instruction==' '; instruction++);
    if (*instruction != '#' && *instruction != ':') { // ignore comment lines
      // find end of first token
      for (n=0, nparam=0; n<lineLen && instruction[n]!= '\0' && nparam<MAXPARAMS; n++) {
        if (instruction[n]<=' ') {
          instruction[n] = '\0';
          inSpace = true;
        }
        else if (inSpace) {
          inSpace = false;
          param[nparam++] = (char*) &instruction[n];
        }
      }
      switch (token) {
        case 0 :   // printable remark - rem
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s %s\r\n", getTimeStamp(), &inLine[4]);
            xSemaphoreGive(consoleSem);
          }
          break;
        case 1 :   // place keypad entry in queue  - key
          if (nparam==2) {
            param[1][0]= util_menuKeySwap(param[1][0]);
            xQueueSend (keyboardQueue, param[1], 0);
          }
          break;
        case 2 :   // delay - time expressed in 1/1000ths of a second
        case 12:   // sleep - time expressed in seconds
          if (nparam==2 && util_str_isa_int (param[1])){
            uint64_t delayTime =  (util_str2int(param[1]));
            if (token == 2) delay (delayTime);
            else sleep (delayTime);
          }
          else if (nparam == 1) delay(1000);
          else {
            int result = calc (nparam-1, &param[1]);
            if (token == 2) delay (result);
            else sleep (result);
          }
          break;
        case 3 :   // goto
          if (jumpTable != NULL && nparam>1) {
            /*
            bool notFound = true;
            for (uint16_t j=0; notFound && j<jumptableSize; j++) {
              if (strcmp (jumpTable[j].label, param[1]) == 0) {
                notFound = false;
                if (nparam == 2 || calc (nparam-2, &param[2]) > 0.5) currentLine = jumpTable[j].jumpTo;
              }
            }
            if (notFound && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("%s Unrecognised label in line %d: %s\r\n", getTimeStamp(), lineNr+1, inLine);
              xSemaphoreGive(consoleSem);
            }
            */
            if (nparam == 2 || calc (nparam-2, &param[2]) > 0.5) currentLine = lineTable[lineNr].param;
          }
          break;
        case 4 :   // waitfor  - break from loop if killed during wait
          if (nparam>=2) {
            bool waitunset = true;
            while (waitunset && runnableAuto) {
              if (calc (nparam-1, &param[1]) > 0.5) waitunset = false;
              if (waitunset) delay (500);
              if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                if (procTable[myProcID].state == 19) runnableAuto = false;
                xSemaphoreGive(procSem);
              }
            }
          }
          else sleep(10);
          break;
        case 5 :   // exit
          if (strcmp(param[0], "exit") == 0) { // set unrunnable
            runnableAuto = false;	
          }
          break;
        case 6 : // call - wait for completion of script
        case 7 : // exec - run in background and keep going
          if (nparam == 2) {
            if (token == 6) runfg (param[1]);
            else runbg (param[1]);
          }
          else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s %s requires one parameter, a filename\r\n", getTimeStamp(), param[0]);
            xSemaphoreGive(consoleSem);
          }
          break;
        case 8:  // power on/off
          if (nparam == 2 && (strcmp(param[1],"on")==0 || strcmp(param[1],"off")==0)) {
            if (param[1][1]=='n') setTrackPower(1);
            else setTrackPower(2);
          }
          else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s %s requires one parameter, on or off\r\n", getTimeStamp(), param[0]);
            xSemaphoreGive(consoleSem);
          }
          break;
        case 9:   // route
          {
          bool notFound = true;
          int8_t selected = 0;
          if (nparam>0 && xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            for (int8_t routeNr=0; notFound && routeNr<routeCount; routeNr++) {
              if (strcasecmp(param[1], routeList[routeNr].userName) == 0) {
                notFound = false;
                selected = routeNr;
              }
            }
            xSemaphoreGive(routeSem);
          }
          if (notFound && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Route not found in line %d: %s\r\n", getTimeStamp(), lineNr+1, param[1]);
            xSemaphoreGive(consoleSem);
          }
          else if (selected >= 0 && selected < routeCount) setRoute(selected);
          }
          break;
        case 10: // Throw
        case 11: // Close
          {
          bool notFound = true;
          int8_t selected = 0;
          if (nparam>0 && xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            for (int8_t turnoutNr=0; notFound && turnoutNr<turnoutCount; turnoutNr++) {
              if (strcasecmp(param[1], turnoutList[turnoutNr].userName) == 0) {
                notFound = false;
                selected = turnoutNr;
              }
            }
            xSemaphoreGive(turnoutSem);
          }
          if (notFound && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Turnout not found in line %d: %s\r\n", getTimeStamp(), lineNr+1, param[1]);
            xSemaphoreGive(consoleSem);
          }
          else if (selected >= 0 && selected < turnoutCount) {
            if (token == 10) setTurnout (selected, 'T');
            else setTurnout (selected, 'C');
            }
          }
          break;
        case 13:    // set
          if (nparam>2) {
            if (strncmp (param[1], "reg", 3) == 0 && param[1][3]>='0' && param[1][3]<='9' && param[1][4] == '\0') {
              int8_t i = param[1][3] - '0';
              localRegister[i] =  calc (nparam-2, &param[2]);
            }
            else if (strncmp (param[1], "shreg", 3) == 0 && param[1][5]>='0' && param[1][5]<='9' && param[1][6] == '\0') {
              float temp = calc (nparam-2, &param[2]);
              int8_t i = param[1][5] - '0';
              if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                sharedRegister[i] = temp;
                xSemaphoreGive(procSem);
              }
            }
            else if (strcmp (param[1], "bidirectional") == 0) {
              float temp = calc (nparam-2, &param[2]);
              if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                if (temp<0.5) bidirectionalMode = false;
                else bidirectionalMode = true;
                xSemaphoreGive(shmSem);
              }
            }
            else if (strncmp (param[1], "localpin.", 9) == 0) {
              uint8_t pinNr = 0;
              uint8_t assignment = 0;
              uint8_t channel = 0;
              bool notFound = true;
              float temp = 0.00;
              if (util_str_isa_int(&param[1][9]))
                pinNr = util_str2int(&param[1][9]);
              else
                pinNr = ((int) getvar (&param[1][9])) & 255;
              if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                for (uint8_t i=0; notFound && localPinTable[i].pinNr<255; i++) if (pinNr == localPinTable[i].pinNr) {
                  assignment = localPinTable[i].assignment;
                  channel    = localPinTable[i].channel;
                  notFound   = false;
                } 
                xSemaphoreGive(procSem);
                if (assignment == RGB) evalRGB = true;
                temp = calc (nparam-2, &param[2]);
                if (notFound) {
                  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    Serial.printf ("%s Pin %d is not defined\r\n", getTimeStamp(), pinNr);
                    xSemaphoreGive(consoleSem);
                  }
                }
                else if (pinNr>0 || strcmp(&param[1][9], "0") == 0) switch (assignment) {
                case DOUT:
                  if (temp < 0.5) digitalWrite(pinNr, LOW);
                  else digitalWrite(pinNr, HIGH);
                  break;
                #if ESPMODEL == ESP32
                case AOUT:
                  dacWrite (pinNr, temp);
                  break;
                #elif ESPMODEL == ESP32C2
                case AOUT:
                  dacWrite (pinNr, temp);
                  break;
                #endif
                case PWM:
                  if (temp < 0) temp = 0;
                  else if (temp > 255) temp = 255;
                  ledcWrite(channel, temp);
                  break;
                #ifdef USENEOPIXEL
                case RGB:
                  if (stackPtr == 3)
                    neopixelWrite(pinNr, lifoStack[0], lifoStack[1], lifoStack[2]);
                  break;
                #endif
                default:
                  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    Serial.printf ("%s Pin %d is not an output\r\n", getTimeStamp(), pinNr);
                    xSemaphoreGive(consoleSem);
                  }
                }
              }
            }
          }
          break;
        case 14:   // sendcmd - send raw instruction to CS, not checking response
          if (strlen(lineTable[lineNr].start) > 8) {
            if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              showNextPacket = true;
              xSemaphoreGive(shmSem);
            }
            txPacket (&(lineTable[lineNr].start[8]));
          }
          break;
        case 15:   // configpin
          {
            uint16_t initialVal = 0;
            uint8_t function = 99;
            uint8_t pinNr = 255;
            if (nparam>2) {
              if (util_str_isa_int(param[1]))
                pinNr = util_str2int(param[1]);
              else
                pinNr = ((int) getvar (param[1])) & 255;
              if      (strcasecmp (param[2], "DIN")  == 0) function = DIN;
              else if (strcasecmp (param[2], "DOUT") == 0) function = DOUT;
              else if (strcasecmp (param[2], "AIN")  == 0) function = AIN;
              else if (strcasecmp (param[2], "AOUT") == 0) function = AOUT;
              else if (strcasecmp (param[2], "PWM")  == 0) function = PWM;
              else if (strcasecmp (param[2], "RGB")  == 0) function = RGB;
              else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("%s Unknown parameter %s for pin %d in line %d\r\n", getTimeStamp(), param[2], pinNr, lineNr+1);
                xSemaphoreGive(consoleSem);
                pinNr = 255;
              }
              if (nparam>3) {
                if (function == DIN || function == AIN) {
                  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    Serial.printf ("%s Ignoring redundant parameters for input pin in line %d\r\n", getTimeStamp(), lineNr+1);
                    xSemaphoreGive(consoleSem);
                  }
                }
                else {
                  if (function == RGB) evalRGB = true;
                  initialVal = calc (nparam-3, &param[3]);
                  }
               }
              if (pinNr!=255 && (pinNr!=0 || strcmp (param[1], "0") == 0)) autoInitPin (pinNr, function, initialVal);
              else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("%s Cannot assign %d pin in line %d\r\n", getTimeStamp(), pinNr, lineNr+1);
                xSemaphoreGive(consoleSem);
              }
            }
          }
          break;
        default:   // unknown instruction
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Syntax error in line %d: %s\r\n", getTimeStamp(), lineNr+1, inLine);
            xSemaphoreGive(consoleSem);
          }
          break;
      }
    }
    free(buffer);
  }


  void autoInitPin (uint8_t pinNr, uint8_t function, uint16_t initVal)
  {
    bool ok = true;
    uint8_t i = 0;
    char* typeLabel[] = {"DIN", "DOUT", "AIN", "AOUT", "PWM", "RGB" };
    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s autoInitPin(%d, %d, %d)\r\n", getTimeStamp(), pinNr, function, initVal);
      xSemaphoreGive(consoleSem);
    }
    // Sanity check parameters
    if (pinNr >= MAXPINS
      #ifdef LED_BUILTIN
      && pinNr != LED_BUILTIN
      #endif
      ) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Pin %d out of range\r\n", getTimeStamp(), pinNr);
        xSemaphoreGive(consoleSem);
      }
      return;
    }
    if (function > RGB) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Pin %d assigned invalid function\r\n", getTimeStamp(), pinNr);
        xSemaphoreGive(consoleSem);
      }
      return;
    }
    // https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/hw-reference/chip-series-comparison.html
    #if ESPMODEL == ESP32
    if (function == AIN  && pinNr < 32) ok = false;
    if (function == AOUT && (pinNr < 25 || pinNr > 26)) ok = false;
    if ((function == PWM || function == DOUT || function == RGB)  &&  pinNr > 33) ok = false;
    #elif ESPMODEL == ESP32S2
    if (function == AIN  && pinNr > 10) ok = false;
    if (function == DOUT && pinNr > 45) ok = false;
    if (function == AOUT && (pinNr < 17 || pinNr > 18)) ok = false;
    if ((function == PWM || function == DOUT || function == RGB)  &&  pinNr > 45) ok = false;
    #elif ESPMODEL == ESP32S3
    if (function == AIN  && (pinNr < 3 || pinNr > 10)) ok = false;
    if (function == AOUT) ok = false;
    #else
    if (function == AIN  && pinNr > 4) ok = false;
    if (function == AOUT) ok = false;
    #endif
    if (!ok) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Pin %d out of range for %s\r\n", getTimeStamp(), pinNr, typeLabel[function]);
        xSemaphoreGive(consoleSem);
      }
      return;
    }
    // first check against hard coded pins
    for (i=0; i<(sizeof(pinVars)/sizeof(pinVar_s)); i++) {
      if (pinNr == pinVars[i].pinNr) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Pin %d already in use by %s\r\n", getTimeStamp(), pinNr, pinVars[i].pinDesc);
          xSemaphoreGive(consoleSem);
        }
        return;
      }
    }
    // now check against previously defined pins
    for (i=0; i<LOCALPINCNT && localPinTable[i].pinNr<255; i++) {
      if (pinNr == localPinTable[i].pinNr) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Pin %d already in use as %s\r\n", getTimeStamp(), pinNr, typeLabel[localPinTable[i].assignment]);
          xSemaphoreGive(consoleSem);
        }
        return;
      }
    }
    if (i<LOCALPINCNT) {
      if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        localPinTable[i].pinNr = pinNr;
        localPinTable[i].assignment = function;
        xSemaphoreGive(procSem);
        switch (function) {
        case DIN:
          pinMode(pinNr, INPUT);
          break;
        case DOUT:
          pinMode(pinNr, OUTPUT);
          if (initVal == 0) digitalWrite(pinNr, LOW);
          else digitalWrite(pinNr, HIGH);
          break;
        case AIN:
          analogReadResolution(10);
          adcAttachPin(pinNr);
          analogSetPinAttenuation(pinNr, ADC_11db);  // param 2 = attenuation, range 0-3 sets FSD: 0:ADC_0db=800mV, 1:ADC_2_5db=1.1V, 2:ADC_6db=1.35V, 3:ADC_11db=2.6V
          break;
#if ESPMODEL == ESP32
        case AOUT:
          dacWrite (pinNr, initVal);
          break;
#elif ESPMODEL == ESP32C2
        case AOUT:
          dacWrite (pinNr, initVal);
          break;
#endif
        case PWM:
          if (ledChannel<15) {
            ledChannel++;
            if (initVal>255) initVal = 255;
            localPinTable[i].channel = ledChannel;
            ledcSetup(ledChannel, 5000, 8);
            ledcAttachPin(pinNr, ledChannel);
            ledcWrite(ledChannel, initVal);
          }
          break;
        #ifdef USENEOPIXEL
        case RGB:
          if (stackPtr == 3)
            neopixelWrite(pinNr, lifoStack[0], lifoStack[1], lifoStack[2]);
          break;
        #endif
        }
      }
    }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s No more slots available to define custom I/O\r\n", getTimeStamp());
      xSemaphoreGive(consoleSem);
    }
  }
  

  void runLoadFile (uint8_t indexer)
  {
    char *automationData = NULL;
    char *tPtr = NULL;
    char *fileName;
    int sizeOfFile;
    uint16_t lineNr = 0;
    uint16_t labelNr = 0;
    bool trace = false;

    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s runLoadFile(%d)\r\n", getTimeStamp(), indexer);
      xSemaphoreGive(consoleSem);
    }
    myProcID = indexer;
    if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      fileName = procTable[indexer].filename;
      xSemaphoreGive(procSem);
    }
    automationData = util_loadFile(SPIFFS, fileName, &sizeOfFile);
    // count number of lines and labels in file
    if (automationData!=NULL) {
      for (uint8_t i; i<REGISTERCOUNT; i++) localRegister[i] = 0.00;
      for (int n=0; n<sizeOfFile; ) {
        while ((automationData[n]=='\n' || automationData[n]=='\r' || automationData[n]== ' ' || automationData[n]== '\0') && n<sizeOfFile) {
          automationData[n++]='\0';
        }
        numberOfLines++;
        if (n<sizeOfFile) {	
          if (automationData[n]==':') jumptableSize++;  // Is leading char a Label marker?
          while (automationData[n]!='\0' && automationData[n]!='\n' && automationData[n]!='\r' && n<sizeOfFile) n++;
        }
      }
      // now build table of lines and jump table
      {
        // Create a jump table and clear contents
        uint32_t mallocSize = 1+(jumptableSize*sizeof(jumpTable_s));
        if (mallocSize > 0) {
          jumpTable = (jumpTable_s*) malloc (mallocSize);
          for (uint32_t z=0; z<mallocSize; z++) ((char*)jumpTable)[z]='\0';
        }
        // Create a line table and clear contents
        mallocSize = 1+(numberOfLines*sizeof(lineTable_s));
        if (mallocSize > 0) {
          lineTable = (lineTable_s*) malloc (mallocSize);
          for (uint32_t z=0; z<mallocSize; z++) ((char*)lineTable)[z]='\0';
        }
        if (lineTable != NULL) {
          for (int n=0; n<sizeOfFile; n++) {
            // Split the automation into lines starting with text
            while (automationData[n] == '\0' && n<sizeOfFile) n++;  // move to start of text
            lineTable[lineNr].start = &(automationData[n]);         // save table of line starts
            // propulate the jump table if it is a jump
            if (automationData[n] == ':') {
                if (strlen(&automationData[n]) >= LABELSIZE) {      // truncate oversized labels
                int j = n + (LABELSIZE - 1);
                while (automationData[j]!='\0' && j<sizeOfFile) automationData[j++]='\0';
              }
              jumpTable[labelNr].jumpTo = lineNr;                   // save jump table data
              strcpy (jumpTable[labelNr++].label, &(automationData[n]));
            }
            // find the token of each line
            lineTable[lineNr].token = 250;
            for (uint8_t z=0; z<sizeof(runTokens)/sizeof(char*) && lineTable[lineNr].token==250; z++) {
              tPtr = (char*) runTokens[z];
              if (strlen(lineTable[lineNr].start)>=strlen(tPtr) && strncmp(lineTable[lineNr].start, tPtr, strlen(tPtr)) == 0)
                lineTable[lineNr].token = z;
            }
            if (lineTable[lineNr].token == 250 && lineTable[lineNr].start[0] != ':' && lineTable[lineNr].start[0] != '#') {
              runnableAuto = false;
              if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("%s Unrecognised keyword in line %d, automation is unrunnable\r\n", getTimeStamp(), (lineNr+1));
                xSemaphoreGive(consoleSem);
              }
            }
            while (automationData[n] != '\0' && n<sizeOfFile) n++;  // move to end of text
            lineNr++;
          }
          // Scan for jumps and update the line they should jump to
          // wait to the tables are built and do on a second pass, so we know where to find forward jumps.
          for (uint16_t n=0; n<numberOfLines; n++) if (lineTable[n].token == 3) {
            uint8_t z=0;
            bool notFound = true;
            char myLabel[LABELSIZE];
            char *tPtr = lineTable[n].start + 5;
            while (*tPtr == ' ' && *tPtr != '\0') tPtr++;
            while (*tPtr != ' ' && *tPtr != '\0') {
              myLabel[z] = *tPtr;
              z++;
              tPtr++;
            }
            myLabel[z] = '\0';
            for (uint16_t j=0; notFound && j<jumptableSize; j++) {
              if (strcmp (jumpTable[j].label, myLabel) == 0) {
                notFound = false;
                lineTable[n].param = jumpTable[j].jumpTo;
              }
            }
            if (notFound) {
              runnableAuto = false;
              if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("%s Label %s references nothing in line %d, automation is unrunnable\r\n", getTimeStamp(), myLabel, (n+1));
                xSemaphoreGive(consoleSem);
              }
            }
          }
          // Now ready to run
          //
          //mt_dump ((char*)automationData, sizeOfFile);
          //mt_dump ((char*)lineTable, (numberOfLines*sizeof(lineTable_s)));
          //mt_dump ((char*)jumpTable, (jumptableSize*sizeof(jumpTable_s)));
          while (currentLine < numberOfLines && runnableAuto) {
            // check for tracing
            tPtr = lineTable[currentLine].start;
            if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              if (procTable[indexer].state == 29) trace = true;
              else trace = false;
              xSemaphoreGive(procSem);
            }
            if (trace) {
              if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("%3d %s\r\n", currentLine+1, tPtr);
                xSemaphoreGive(consoleSem);
              }
            }
            // run if action required
            if (tPtr[0]!=':' && tPtr[0]!='#' && tPtr[0]!='\0') runProcessLine(currentLine);
            currentLine++;
            // check for kill
            if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              if (procTable[indexer].state == 19) runnableAuto = false;
              xSemaphoreGive(procSem);
            }
          }
          free (lineTable);
        }
      if (jumpTable!=NULL) free (jumpTable);
      }
    }
  free(automationData);
  if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    procTable[indexer].state = 11;
    xSemaphoreGive(procSem);
    }
  }

  static void runbgThread(void *pvParameters)
  {
    uint8_t indexer = 0;
    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s runbgThread(%s)\r\n", getTimeStamp(), (char*) pvParameters);
      xSemaphoreGive(consoleSem);
    }
    indexer = allocateProc((char*) pvParameters);
    if (indexer<PROCTABLESIZE) (new runAutomation)->runLoadFile (indexer);
    vTaskDelete( NULL );
  }

  static uint8_t allocateProc(char *fileName)
  {
  static uint16_t lastProc = 0;
  uint8_t ptr = 0;
  
  lastProc+=9;
  if (lastProc>10000) lastProc = 1;
  if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    for (int8_t n=0; n<PROCTABLESIZE; n++) if (procTable[n].state != 11 && lastProc == procTable[n].id) {
      lastProc+=9;
      n=-1;
      }
    for (ptr=0; ptr<PROCTABLESIZE && procTable[ptr].state != 11; ptr++);
    if (ptr > PROCTABLESIZE) ptr = 255;
    else {
      procTable[ptr].state = 17;
      procTable[ptr].id = lastProc;
      if (strlen(fileName)>sizeof (procTable[0].filename)) fileName[sizeof(procTable[0].filename)-1] = '\0';
      strcpy (procTable[ptr].filename, fileName);
      }
    xSemaphoreGive(procSem);
    }
  else ptr = 255;
  return (ptr);
  }

  static void setProcState (char *param, uint8_t state)
  {
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setProcState(%s, %d)\r\n", getTimeStamp(), param, state);
    xSemaphoreGive(consoleSem);
  }
  if (util_str_isa_int (param)) {
    uint16_t value = util_str2int(param);
    if (value < 10000 && xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      for (uint8_t n=0; n<PROCTABLESIZE; n++) if (procTable[n].state != 11 && procTable[n].id == value) {
        if (state == 29 && procTable[n].state == state) procTable[n].state = 17;
        else procTable[n].state = state;
        }
      xSemaphoreGive(procSem);
      }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("procss id should be less than 10000\r\n");
      xSemaphoreGive(consoleSem);
      }
    }
  else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("procss id should be numeric and less than 10000\r\n");
      xSemaphoreGive(consoleSem);
      }
  }
public:

static void runbg(char *fileName)
  {
  static uint16_t serial=0;
  char threadName[12];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s runbg(%s)\r\n", getTimeStamp(), fileName);
    xSemaphoreGive(consoleSem);
  }
  sprintf (threadName, "runbg_%05d", serial++);
  if (serial > 65000) serial = 1;
  xTaskCreate(runbgThread, threadName, 8192, fileName, 4, NULL);
  delay (500); // Set up time allowance - copy *fileName before the calling routine destroys it
  }

static void runfg(char *fileName)
  {
  uint8_t indexer = 0;
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s runfg(%s)\r\n", getTimeStamp(), fileName);
    xSemaphoreGive(consoleSem);
  }
  indexer = allocateProc(fileName);
  if (indexer<PROCTABLESIZE) (new runAutomation)->runLoadFile (indexer);
  }

static void listProcs()
  {
  uint8_t count=0;
  uint8_t stateType;
  char *stateDesc[] = {"Running", "Stopping", "Tracing"};
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s listProcs()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    for (uint8_t n=0; n<PROCTABLESIZE; n++) if (procTable[n].state != 11) {
      if (count==0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Automation processes found:\r\n", getTimeStamp());
        Serial.printf ("+-------+----------+----------------------------------+\r\n");
        Serial.printf ("| %5s | %8s | %-32s |\r\n", "ID", "STATE", "NAME");
        Serial.printf ("+-------+----------+----------------------------------+\r\n");
        xSemaphoreGive(consoleSem);
        }
      if (procTable[n].state == 17) stateType= 0;
      else if (procTable[n].state == 29) stateType= 0;
      else stateType = 1;
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("| %5d | %8s | %-32s |\r\n", procTable[n].id, stateDesc[stateType], procTable[n].filename);
        xSemaphoreGive(consoleSem);
        }
      count++;
      }
    xSemaphoreGive(procSem);
    }
  if (count>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("+-------+----------+----------------------------------+\r\n");
      xSemaphoreGive(consoleSem);
      }
  else if (count==0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s No automation processes found.\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
    }
  }

  static void  traceProc (char *param)
  {
    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s killProc(%s)\r\n", getTimeStamp(), param);
      xSemaphoreGive(consoleSem);
    }
    setProcState(param, 29);
  }

  static void killProc (char *param)
  {
    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s killProc(%s)\r\n", getTimeStamp(), param);
      xSemaphoreGive(consoleSem);
    }
    setProcState(param, 19);
  }

  static void dumpRegisters()
  {
    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s dumpRegisters()\r\n", getTimeStamp());
      xSemaphoreGive(consoleSem);
    }
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("\r\nShared Registers\r\n");
      Serial.printf ("Register      Value\r\n");
      xSemaphoreGive(consoleSem);
    }
    if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      for (uint8_t n; n<REGISTERCOUNT; n++) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf (" shreg%d   %9.4f\r\n", n, sharedRegister[n]);
          xSemaphoreGive(consoleSem);
        }
      }
      xSemaphoreGive(procSem);
    }
    delay(50);
    if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (dccSensorTable[0].id<40000) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("\r\nDCC-EX Sensors\r\n");
          Serial.printf ("Sensor-ID     Value\r\n");
          xSemaphoreGive(consoleSem);
        }
        for (uint8_t n=0;n<DCCSENSORCNT && dccSensorTable[n].id<40000; n++) {
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("   %6d %9.4f\r\n", dccSensorTable[n].id, dccSensorTable[n].value);
            xSemaphoreGive(consoleSem);
          }
        }
      }
      xSemaphoreGive(procSem);
    }
    delay(50);
    if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (localPinTable[0].pinNr<255) {
        char* typeLabel[] = {"DIN", "DOUT", "AIN", "AOUT", "PWM" };
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("\r\nAssigned Pins\r\n");
          Serial.printf ("Pin-Nr Type\r\n");
          xSemaphoreGive(consoleSem);
        }
        for (uint8_t n=0;n<LOCALPINCNT && localPinTable[n].pinNr<255; n++) {
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("   %3d %s\r\n", localPinTable[n].pinNr, typeLabel[localPinTable[n].assignment]);
            xSemaphoreGive(consoleSem);
          }
        }
      }
      xSemaphoreGive(procSem);
    }
  }

  float rpn (int argc, char** argv)
  {
    for (uint8_t i; i<REGISTERCOUNT; i++) localRegister[i] = 0.00;
    float result = calc(argc, argv);
    return (result);
  }
};


void runInitialAuto()
{
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Starting auto.run file\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  runAutomation::runbg ("/auto.run");
}
