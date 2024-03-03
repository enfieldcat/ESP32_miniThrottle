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
  uint16_t currentLine = 0;
  uint16_t numberOfLines = 0;
  uint16_t jumptableSize = 0;
  bool runnableAuto = true;

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
          else delay(1000);
          break;
        case 3 :   // goto
          if (jumpTable != NULL && nparam>1) {
            bool notFound = true;
            for (uint16_t j=0; notFound && j<jumptableSize; j++) {
              if (strcmp (jumpTable[j].label, param[1]) == 0) {
                notFound = false;
                currentLine = jumpTable[j].jumpTo;
              }
            }
            if (notFound && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("%s Unrecognised label in line %d: %s\r\n", getTimeStamp(), lineNr+1, inLine);
              xSemaphoreGive(consoleSem);
            }
          }
          break;
        case 4 :   // waitfor
          if (nparam==2) {
            bool waitunset = true;
            if (strcmp (param[1], "trackpower") == 0){
              while (waitunset) {
                if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                  if (trackPower) waitunset = false;
                  xSemaphoreGive(shmSem);
                }
                if (waitunset) delay (500);
              }
            }
            else if (strcmp (param[1], "driving") == 0){
              while (waitunset) {
                if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                  if (drivingLoco) waitunset = false;
                  xSemaphoreGive(shmSem);
                }
                if (waitunset) delay (500);
              }
            }
            else if (strcmp (param[1], "connected") == 0){
              #ifndef SERIALCTRL
              while (waitunset) {
                if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                  if (wiCliConnected) waitunset = false;
                  xSemaphoreGive(shmSem);
                }
                if (waitunset) delay (500);
              }
              #endif
            }
            else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("%s Unrecognised waitfor in line %d: %s\r\n", getTimeStamp(), lineNr+1, inLine);
              xSemaphoreGive(consoleSem);
              waitunset = false;
            }
          }
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
        default:
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Syntax error in line %d: %s\r\n", getTimeStamp(), lineNr+1, inLine);
            xSemaphoreGive(consoleSem);
          }
          break;
      }
    }
    free(buffer);
  }
  

  void runLoadFile (uint8_t indexer)
  {
    char *automationData = NULL;
    char *tPtr = NULL;
    char *fileName;
    int sizeOfFile;
    uint16_t lineNr = 0;
    uint16_t labelNr = 0;

    if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s runLoadFile(%d)\r\n", getTimeStamp(), indexer);
      xSemaphoreGive(consoleSem);
    }
    if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      fileName = procTable[indexer].filename;
      xSemaphoreGive(procSem);
    }
    automationData = util_loadFile(SPIFFS, fileName, &sizeOfFile);
    // count number of lines and labels in file
    if (automationData!=NULL) {
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
        uint32_t mallocSize = 1+(jumptableSize*sizeof(jumpTable_s));
        if (mallocSize > 0) {
          jumpTable = (jumpTable_s*) malloc (mallocSize);
          for (uint32_t z=0; z<mallocSize; z++) ((char*)jumpTable)[z]='\0';
        }
        mallocSize = 1+(numberOfLines*sizeof(lineTable_s));
        if (mallocSize > 0) {
          lineTable = (lineTable_s*) malloc (mallocSize);
          for (uint32_t z=0; z<mallocSize; z++) ((char*)lineTable)[z]='\0';
        }
        if (lineTable != NULL) {
          for (int n=0; n<sizeOfFile; n++) {
            while (automationData[n] == '\0' && n<sizeOfFile) n++;  // move to start of text
            lineTable[lineNr].start = &(automationData[n]);         // save table of line starts
            if (automationData[n] == ':') {
                if (strlen(&automationData[n]) >= LABELSIZE) {      // truncate oversized labels
                int j = n + (LABELSIZE - 1);
                while (automationData[j]!='\0' && j<sizeOfFile) automationData[j++]='\0';
              }
              jumpTable[labelNr].jumpTo = lineNr;                   // save jump table data
              strcpy (jumpTable[labelNr++].label, &(automationData[n]));
            }
            lineTable[lineNr].token = 250;
            for (uint8_t z=0; z<sizeof(runTokens)/sizeof(char*) && lineTable[lineNr].token==250; z++) {
              tPtr = (char*) runTokens[z];
              if (strlen(lineTable[lineNr].start)>=strlen(tPtr) && strncmp(lineTable[lineNr].start, tPtr, strlen(tPtr)) == 0)
                lineTable[lineNr].token = z;
            }
            while (automationData[n] != '\0' && n<sizeOfFile) n++;  // move to end of text
            lineNr++;
          }
          //mt_dump ((char*)automationData, sizeOfFile);
          //mt_dump ((char*)lineTable, (numberOfLines*sizeof(lineTable_s)));
          //mt_dump ((char*)jumpTable, (jumptableSize*sizeof(jumpTable_s)));
          while (currentLine < numberOfLines && runnableAuto) {
            tPtr = lineTable[currentLine].start;
            if (tPtr[0]!=':' && tPtr[0]!='#' && tPtr[0]!='\0') runProcessLine(currentLine);
            currentLine++;
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
  char *stateDesc[] = {"Running", "Stopping"};
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

  static void killProc (char *param)
  {
  if (util_str_isa_int (param)) {
    uint16_t value = util_str2int(param);
    if (value < 10000 && xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      for (uint8_t n=0; n<PROCTABLESIZE; n++) if (procTable[n].state != 11 && procTable[n].id == value) {
        procTable[n].state = 19;
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

};


void runInitialAuto()
{
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Starting auto.run file\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  runAutomation::runbg ("/auto.run");
}
