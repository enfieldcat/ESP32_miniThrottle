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



void processDccPacket (char *packet)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s processDccPacket(%s)\r\n", getTimeStamp(), packet);
    xSemaphoreGive(consoleSem);
  }

  // this is the vocab we understand
  if      (strncmp (packet, "<T ",  3) == 0) dccSpeedChange (&packet[3]);
  else if (strncmp (packet, "<l ",  3) == 0) dccLocoStatus  (&packet[3]);
  else if (strncmp (packet, "PPA",  3) == 0) dccPowerChange (packet[3]);
  else if (strncmp (packet, "<p",   2) == 0) dccPowerChange (packet[2]);
  else if (strncmp (packet, "<* ",  3) == 0) dccComment     (&packet[3]);
  else if (strncmp (packet, "<r",   2) == 0) dccCV          (&packet[2]);
  else if (strncmp (packet, "<i",   2) == 0) dccInfo        (&packet[2]);
  else if (strncmp (packet, "<H ",  3) == 0) dccAckTurnout  (&packet[3]);
  else if (strncmp (packet, "<O>",  3) == 0) dccAckTurnout  ();
  else if (strncmp (packet, "<jT ", 4) == 0) dccConfTurnout (&packet[4]);
  else if (strncmp (packet, "<jA ", 4) == 0) dccConfRoute   (&packet[4]);
  else if (strncmp (packet, "<jR ", 4) == 0) dccConfLoco    (&packet[4]);
}

/*
 * LocoStatus update
 * <l 31 1 231 33>
 * <l 313 0 128 1>
 * ID, status?, speed 0-255, functions bit map
 */
void dccLocoStatus (char* locoStatus)
{
  int16_t locoID = -9999;
  // int16_t locoState = -9999;
  int16_t  locoSpeed = -9999;
  uint16_t locoFunc = 9999;
  // uint8_t next, maxIdx, curIdx;
  uint8_t next = 0;
  uint8_t result = 0;
  char *ptr, *savePtr;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccLocoStatus(%s)\r\n", getTimeStamp(), locoStatus);
    xSemaphoreGive(consoleSem);
  }

  xQueueSend (dccAckQueue, &result, 0);   // Ack even if garbled.
  for (uint8_t n=(strlen(locoStatus)-1); n<0 && locoStatus[n]!=' ' && locoStatus[n]!='>'; n--) locoStatus[n] = '\0'; // truncate trailing chars
  ptr = strtok_r (locoStatus, " ", &savePtr);
  if (ptr!=NULL && strlen(ptr)>0) {
    locoID = util_str2int(locoStatus);
  }
  else {
    if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("dccLocoStatus: No loco ID\r\n");
      xSemaphoreGive(consoleSem);
    }
    return;
  }
  // Grab the status? And ignore it
  ptr = strtok_r (NULL, " ", &savePtr);
  /* if (ptr!=NULL && strlen(ptr)>0) {
    locoState = abs(util_str2int(ptr));
  }
  else {
    if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("dccLocoStatus: No loco state\r\n");
      xSemaphoreGive(consoleSem);
    }
    return;
  } */
  // Grab the speed 0 - 255
  ptr = strtok_r (NULL, " ", &savePtr);
  if (ptr!=NULL && strlen(ptr)>0) {
    locoSpeed = util_str2int(ptr);
  }
  else {
    if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("dccLocoStatus: No loco speed\r\n");
      xSemaphoreGive(consoleSem);
    }
    return;
  }
  // Grab the function setting
  ptr = strtok_r (NULL, " ", &savePtr);
  if (ptr!=NULL && strlen(ptr)>0) {
    locoFunc = util_str2int(ptr);
  }
  else {
    if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("dccLocoStatus: No function data\r\n");
      xSemaphoreGive(consoleSem);
    }
    return;
  }

  if (locoID != -9999) {
    for (next=0; next<locomotiveCount+MAXCONSISTSIZE && locoRoster[next].id!=locoID; next++);
    if (next==locomotiveCount+MAXCONSISTSIZE) return;
    if (locoFunc>=0 && locoFunc != 9999 && xSemaphoreTake(functionSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      uint32_t changes  = locoRoster[next].function ^ locoFunc;  // xor new state with current to detect changes
      if (changes>0) {
        locoRoster[next].function = locoFunc;
        funcChange = true;
      }
      xSemaphoreGive(functionSem);
      #ifdef RELAYPORT
      // If relaying and this is not one of our locally controlled locos...
      if ((!locoRoster[next].owned) && relayMode == WITHROTRELAY) {
        if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          uint8_t chk = relayCount;
          xSemaphoreGive(relaySvrSem);
          if (chk > 1) { // only send message if relay service running and initialised
            if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              uint32_t mask     = 1;
              uint16_t t_id     = locoRoster[next].id;
              int8_t t_type     = locoRoster[next].type;
              int8_t t_relayIdx = locoRoster[next].relayIdx;;
              char t_throttleNr = locoRoster[next].throttleNr;
              char outBuffer[40];
              uint8_t state;
              xSemaphoreGive(velociSem);
              for (uint8_t n=0; n<29; n++) {
                if ((mask&changes) > 0) {     // only relay changes
                  if ((mask&locoFunc) == 0) state = '0';
                  else state = '1';
                  sprintf (outBuffer, "M%cA%c%d<;>F%c%d\n\r", t_throttleNr, t_type, t_id, state, n);
                  reply2relayNode (&(remoteSys[t_relayIdx]), outBuffer);//  Send function status
                }
                mask <<= 1;
              }
            }
            else semFailed ("velociSem", __FILE__, __LINE__);
          }
        }
        else semFailed ("relaySvrSem", __FILE__, __LINE__);
      }
      #endif
    }
    else semFailed ("functionSem", __FILE__, __LINE__);
  }
  else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("Invalid loco ID in status update\r\n");
    xSemaphoreGive(consoleSem);
  }
}

void dccSpeedChange (char* speedSet)
{
  int16_t dccRegister = 1;
  int16_t dccSpeed = 0;
  int16_t dccDirection = 0;
  int16_t maxLocoArray = locomotiveCount + MAXCONSISTSIZE;
  char *token;
  char *remain = speedSet;
  uint8_t result = 15;
  uint8_t locoIndex = 0;
  #ifdef RELAYPORT
  uint16_t t_id;
  int8_t t_direction;
  int8_t t_type;
  int8_t t_relayIdx;
  char t_throttleNr;
  char outBuffer[40];
  bool t_owned = false;
  #endif

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccSpeedChange(%s)\r\n", getTimeStamp(), speedSet);
    xSemaphoreGive(consoleSem);
  }
  xQueueSend (dccAckQueue, &result, 0);
  if (xQueueReceive(dccLocoRefQueue, &locoIndex, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
    if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s dccSpeedChange failed to get associated LocoID from internal queue\r\n", getTimeStamp());
      xSemaphoreGive(consoleSem);
    }
    return;
  }
  int len = strlen (speedSet);
  while (speedSet[len-1]=='>' || speedSet[len-1]=='*' || speedSet[len-1]==' ') len--;
  speedSet[len] = '\0';
  token = strtok_r (remain, " ", &remain);
  dccRegister = util_str2int(token);
  token = strtok_r (remain, " ", &remain);
  dccSpeed = util_str2int(token);
  token = strtok_r (remain, " ", &remain);
  dccDirection = util_str2int(token);
  if (dccSpeed>127) dccSpeed = -1;
  if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    locoRoster[locoIndex].speed = (int16_t) dccSpeed;
    if (dccDirection == 1) locoRoster[locoIndex].direction = FORWARD;
    else locoRoster[locoIndex].direction = REVERSE;
    if (locoRoster[locoIndex].owned) speedChange = true;
    #ifdef RELAYPORT
    if (relayMode == WITHROTRELAY) {
      if (locoRoster[locoIndex].direction == REVERSE) t_direction = 0;
      else t_direction = 1;
      t_type = locoRoster[locoIndex].type;
      t_id = locoRoster[locoIndex].id;
      t_throttleNr = locoRoster[locoIndex].throttleNr;
      t_relayIdx   = locoRoster[locoIndex].relayIdx;
      t_owned      = locoRoster[locoIndex].owned;
    }
    #endif
    xSemaphoreGive(velociSem);
    #ifdef RELAYPORT
    if ((!t_owned) && relayMode == WITHROTRELAY && t_relayIdx<maxRelay) {
      if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        uint8_t chk = relayCount;
        xSemaphoreGive(relaySvrSem);
        if (chk > 1) { // only send message if relay service running and initialised
          sprintf (outBuffer, "M%cA%c%d<;>V%d\r\nM%cA%c%d<;>R%d\r\n", t_throttleNr, t_type, t_id, dccSpeed, t_throttleNr, t_type, t_id, t_direction);
          reply2relayNode (&(remoteSys[t_relayIdx]), outBuffer);
        }
      }
    }
    #endif
  }
  else semFailed ("velociSem", __FILE__, __LINE__);
}


void dccConfLoco (char *data)
{
// --> <JR>
// <-- <jR 3 10 150 1150>
// --> <JR 1150>
// <-- <jR 1150 "Steve the steamer" "//Snd On/*Whistle/*Whistle2/Brake/F5 Drain/Coal Shvl/Guard-Squeal/Loaded/Coastng/Injector>
  char *fld = NULL;
  char *locoName = NULL;
  char *funcName = NULL;
  uint16_t tptr = 0;
  uint16_t limit = strlen (data);
  uint16_t locoID = 65500;
  uint8_t result = 255;
  uint8_t fldCnt = 0;
  uint8_t totalFields = 1;
  uint8_t indexer = 255;
  bool isAllNumeric = true;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccConfLoco(%s)\r\n", getTimeStamp(), data);
    xSemaphoreGive(consoleSem);
  }
  if (limit < 3) return; // empty packet?
  for (uint16_t n=0; n<limit; n++) {
    if (data[n]==' ') totalFields++;
    if (isAllNumeric && data[n]!=' ' && data[n]!='>' && (data[n]<'0' || data[n]>'9')) isAllNumeric = false;
  }
  if (isAllNumeric) {
    if (dccExNumList != NULL) free (dccExNumList);
    dccExNumList = (uint16_t*) malloc (sizeof(uint16_t) * totalFields);
    dccExNumListSize = 0;
  }
  fld = data;
  for (uint16_t n=0; n<limit; n++) if (data[n]==' ' || data[n]=='>') {
    data[n] = '\0';
    if (isAllNumeric && strlen(fld)>0) {
      if (util_str_isa_int(fld)) dccExNumList[dccExNumListSize++] = util_str2int(fld);
      fld = &data[n+1];
    }
    else {
      if (fldCnt==0 && util_str_isa_int(fld)) {
        locoID = util_str2int(fld);
        if (xQueueReceive(dccOffsetQueue, &indexer, pdMS_TO_TICKS(TIMEOUT/4)) != pdPASS) { // wait for index number to use
          indexer = 255;
          n = limit;
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s New loco data found but ignored (loco %d)\r\n", getTimeStamp(), locoID);
            xSemaphoreGive(consoleSem);
          }
        }
        else {
          fldCnt++;
        }
      }
      if (fldCnt==1 && indexer!= 255) {
        bool isNew = true;
        fldCnt++;
        n++;
        if (indexer > locomotiveCount) indexer = locomotiveCount; // never go past end of table, some data may have skipped entry if if is for pre existing loco.
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          for (uint8_t chk=0; chk<locomotiveCount && isNew; chk++) {
            if (locoID == locoRoster[chk].id) isNew=false;
          }
          xSemaphoreGive(velociSem);
        }
        if (isNew) {
          while (n<limit && (data[n]==' ' || data[n]=='"')) n++;
          if (n<limit) {
            locoName = &data[n];
            while (n<limit && data[n]!='"') n++;
            data[n++] = '\0';
            if (strlen(locoName) > NAMELENGTH) locoName[NAMELENGTH]='\0';
          }
          while (n<limit && (data[n]==' ' || data[n]=='"')) n++;
          if (n<limit) {
            funcName = &data[n];
            while (n<limit && data[n]!='"') {
              if (data[n] == '/') data[n] = '~';
              n++;
            }
            data[n++] = '\0';
          }
          if (strlen(locoName) > 0) {
            if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              locoRoster[indexer].id = locoID;
              if (locoRoster[indexer].id > 127) locoRoster[indexer].type = 'L';
              else locoRoster[indexer].type = 'S';
              strcpy (locoRoster[indexer].name, locoName);
              locoRoster[indexer].direction     = STOP;
              locoRoster[indexer].speed         = 0;
              locoRoster[indexer].steps         = 128;
              locoRoster[indexer].owned         = false;
              locoRoster[indexer].function      = 0;
              locoRoster[indexer].throttleNr    = 255;
              locoRoster[indexer].relayIdx      = 255;
              locoRoster[indexer].functionLatch = 65535;
              if (strlen(funcName) > 0) {
                locoRoster[indexer].functionString = (char*) malloc (strlen(funcName));
                strcpy(locoRoster[indexer].functionString, funcName);
                }
              else locoRoster[indexer].functionString = NULL;
              locomotiveCount++;
              xSemaphoreGive(velociSem);
              if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("%s Imported DCC-Ex locomotive %s\r\n", getTimeStamp(), locoName);
                xSemaphoreGive(consoleSem);
              }
            }
          }
        }
      }
    }
  }
  xQueueSend (dccLocoRefQueue, &result, 0);
}

void dccConfTurnout (char *data)
{
  char *fld = NULL;
  uint16_t tptr = 0;
  uint16_t limit = strlen (data);
  uint16_t toID = 65500;
  uint8_t result = 255;
  uint8_t fldCnt = 0;
  uint8_t totalFields = 1;
  uint8_t indexer = 0;
  bool isAllNumeric = true;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccConfTurnout(%s)\r\n", getTimeStamp(), data);
    xSemaphoreGive(consoleSem);
  }
  if (limit < 3) return; // empty packet?
  for (uint16_t n=0; n<limit; n++) {
    if (data[n]==' ') totalFields++;
    if (isAllNumeric && data[n]!=' ' && data[n]!='>' && (data[n]<'0' || data[n]>'9')) isAllNumeric = false;
  }
  if (isAllNumeric) {
    if (dccExNumList != NULL) free (dccExNumList);
    dccExNumList = (uint16_t*) malloc (sizeof(uint16_t) * totalFields);
    dccExNumListSize = 0;
    toID = nvs_get_int("toOffset", 100);
  }
  fld = data;
  for (uint16_t n=0; n<limit; n++) if (data[n]==' ' || data[n]=='>') {
    data[n] = '\0';
    if (isAllNumeric && strlen(fld)>0) {
      if (util_str_isa_int(fld)) dccExNumList[dccExNumListSize] = util_str2int(fld);
      if (dccExNumList[dccExNumListSize] < toID) dccExNumListSize++;
      fld = &data[n+1];
    }
    else {
      if (fldCnt==0 && util_str_isa_int(fld)) toID = util_str2int(fld);
      if (fldCnt==1) {
        char tBuffer[SYSNAMELENGTH];
        bool isNew = true;
        sprintf (tBuffer, "%d", toID);
        if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          for (uint8_t chk=0; chk<turnoutCount && isNew; chk++) {
            if (strcmp(tBuffer, turnoutList[chk].sysName) == 0) isNew=false;
          }
          xSemaphoreGive(turnoutSem);
        }
        if (isNew && (fld[0] == 'C' || fld[0] == 'T' || fld[0] == 'X')) {
          result = fld[0];
          if (toID < nvs_get_int("toOffset", 100)) {
            if (xQueueReceive(dccOffsetQueue, &indexer, pdMS_TO_TICKS(TIMEOUT)) == pdPASS) { // wait for index number to use
              if (n<limit) n++;
              if (data[n] == '"' && n<limit) n++;
              fld = &data[n];
              while (n<limit && data[n]!='"') n++;
              if (data[n] == '"' && n<limit) data[n] = '\0';
              if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                sprintf (turnoutList[indexer].sysName, "%d", toID);
                if (strlen(fld) > 0) {
                  if (strlen(fld) > NAMELENGTH) fld[NAMELENGTH-1] = '\0';
                  strcpy(turnoutList[indexer].userName, fld);
                }
                else sprintf(turnoutList[indexer].userName, "DCC-Ex-%d", toID);
                switch (result) {
                  case 'C': turnoutList[indexer].state = '2'; break;;
                  case 'T': turnoutList[indexer].state = '4'; break;;
                  default : turnoutList[indexer].state = '1'; break;;
                }
                turnoutCount++;
                xSemaphoreGive(turnoutSem);
                if (debuglevel>0 && strlen(fld)>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                  Serial.printf ("%s Imported DCC-Ex turnout %s\r\n", getTimeStamp(), fld);
                  xSemaphoreGive(consoleSem);
                }
              }
            }
            else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("%s Timeout waiting for DCC-Ex turnout %s\r\n", getTimeStamp(), fld);
              xSemaphoreGive(consoleSem);
            }
          }
        }
      }
      fld = &data[n+1];
      fldCnt++;
    }
  }
  xQueueSend (dccTurnoutQueue, &result, 0);
}

void dccConfRoute (char *data)
{
  const char *prefix[] = {"", "auto - "};
  char *fld = NULL;
  uint8_t *rtSet;
  uint16_t tptr = 0;
  uint16_t limit = strlen (data);
  uint16_t rtID = 65500;
  uint8_t result = 255;
  uint8_t fldCnt = 0;
  uint8_t totalFields = 1;
  uint8_t indexer = 0;
  uint8_t prefixID = 0;
  bool isAllNumeric = true;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccConfRoute(%s)\r\n", getTimeStamp(), data);
    xSemaphoreGive(consoleSem);
  }
  if (limit < 3) return; // empty packet?
  for (uint16_t n=0; n<limit; n++) {
    if (data[n]==' ') totalFields++;
    if (isAllNumeric && data[n]!=' ' && data[n]!='>' && (data[n]<'0' || data[n]>'9')) isAllNumeric = false;
  }
  if (isAllNumeric) {
    if (dccExNumList != NULL) free (dccExNumList);
    dccExNumList = (uint16_t*) malloc (sizeof(uint16_t) * totalFields);
    dccExNumListSize = 0;
    rtID = nvs_get_int("toOffset", 100);
  }
  fld = data;
  for (uint16_t n=0; n<limit; n++) if (data[n]==' ' || data[n]=='>') {
    data[n] = '\0';
    if (isAllNumeric) {
      if (util_str_isa_int(fld)) dccExNumList[dccExNumListSize] = util_str2int(fld);
      if (dccExNumList[dccExNumListSize] < rtID) dccExNumListSize++;
      fld = &data[n+1];
    }
    else {
      if (fldCnt==0 && util_str_isa_int(fld)) rtID = util_str2int(fld);
      if (fldCnt==1) {
        char tBuffer[SYSNAMELENGTH];
        bool isNew = true;
        sprintf (tBuffer, "%d", rtID);
        if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          for (uint8_t chk=0; chk<routeCount && isNew; chk++) {
            if (strcmp(tBuffer, routeList[chk].sysName) == 0) isNew=false;
          }
          xSemaphoreGive(routeSem);
        }
        if (isNew && (fld[0] == 'A' || fld[0] == 'R')) {
          if (fld[0] == 'A') prefixID = 1;
          else prefixID = 0;
          result = fld[0];
          if (rtID < nvs_get_int("toOffset", 100)) {
            if (xQueueReceive(dccOffsetQueue, &indexer, pdMS_TO_TICKS(TIMEOUT)) == pdPASS) { // wait for index number to use
              if (n<limit) n++;
              if (data[n] == '"' && n<limit) n++;
              fld = &data[n];
              while (n<limit && data[n]!='"') n++;
              if (data[n] == '"' && n<limit) data[n] = '\0';
              if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                sprintf (routeList[indexer].sysName, "%d", rtID);
                if (strlen(fld) > 0) {
                  if (prefixID == 0) {
                    if (strlen(fld) > NAMELENGTH) fld[NAMELENGTH-1] = '\0';
                  }
                  else {
                    if (strlen(fld) > NAMELENGTH-7) fld[NAMELENGTH-8] = '\0';
                  }
                  sprintf(routeList[indexer].userName, "%s%s", prefix[prefixID], fld);
                }
                else sprintf(routeList[indexer].userName, "%sDCC-Ex-%d", prefix[prefixID], rtID);
                rtSet = &routeList[indexer].turnoutNr[0];
                rtSet[0] = 220;
                rtSet[1] = 255;
                rtSet = &routeList[indexer].desiredSt[0];
                rtSet[0] = 220;
                rtSet[1] = 255;
                routeList[indexer].state = '4';
                routeCount++;
                xSemaphoreGive(routeSem);
                if (debuglevel>0 && strlen(fld)>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                  Serial.printf ("%s Imported DCC-Ex route %s\r\n", getTimeStamp(), fld);
                  xSemaphoreGive(consoleSem);
                }
              }
            }
            else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("%s Timeout waiting for DCC-Ex route %s\r\n", getTimeStamp(), fld);
              xSemaphoreGive(consoleSem);
            }
          }
        }
      }
      fld = &data[n+1];
      fldCnt++;
    }
  }
  xQueueSend (dccRouteQueue, &result, 0);
}

/*
 * Ack turnout packet
 */
void dccAckTurnout (char *ack)
{
  uint8_t result = 255;
  uint8_t state = 'C';
  #ifdef RELAYPORT
  char outBuffer[32];
  #endif
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccAckTurnout(%s)\r\n", getTimeStamp(), ack);
    xSemaphoreGive(consoleSem);
  }
  xQueueSend (dccAckQueue, &result, 0);
  for (uint8_t n=0; n<strlen(ack); n++) if (ack[n]==' ') {
    ack[n]='\0';
    result = ack[n+1];
    if (result == '1') {
      result = '4';
      state = 'T';
    }
    else result = '2';
  }
  for (uint8_t n=0; n<turnoutCount; n++) if (strcmp (ack, turnoutList[n].sysName) == 0) {
    if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      turnoutList[n].state = result;
      xSemaphoreGive(turnoutSem);
      #ifdef RELAYPORT
      sprintf (outBuffer, "PTA%c%s\r\n", result, ack);
      relay2WiThrot (outBuffer);
      #endif
      invalidateRoutes (n, state);
      n = turnoutCount;
    }
  }
}

void dccAckTurnout ()
{
  char result = 15;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccAckTurnout()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  xQueueSend (dccAckQueue, &result, 0);
}

/*
 * Power on/off to track
 */
void dccPowerChange(char state)
{
  uint8_t result = 0;

  if (state != '0' && state != '1') return;
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccPowerChange(%c)\r\n", getTimeStamp(), state);
    xSemaphoreGive(consoleSem);
  }
  xQueueSend (dccAckQueue, &result, 0);
  if (state == '1') {
    trackPower = true;
    #ifdef TRACKPWR
    digitalWrite(TRACKPWR, HIGH);
    #endif
    #ifdef TRACKPWRINV
    digitalWrite(TRACKPWRINV, LOW);
    #endif
  }
  else {
    trackPower = false;
    #ifdef TRACKPWR
    digitalWrite(TRACKPWR, LOW);  // Off or unknown
    #endif
    #ifdef TRACKPWRINV
    digitalWrite(TRACKPWRINV, HIGH);  // Off or unknown
    #endif
  }
  #ifdef RELAYPORT
  char   outBuffer[8];
  sprintf (outBuffer, "PPA%c\r\n", state);
  relay2WiThrot (outBuffer);
  #endif
}

/*
 * Accept a comment from DCC
 */
void dccComment(char* comment)
{
  char *tPtr;
  int len = strlen (comment);
  uint8_t result = 0;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccComment(%s)\r\n", getTimeStamp(), comment);
    xSemaphoreGive(consoleSem);
  }
  xQueueSend (dccAckQueue, &result, 0);
  // drop trailing chars
  while (comment[len-1]=='>' || comment[len-1]=='*' || comment[len-1]==' ') len--;
  if (len>(sizeof(lastMessage)-1)) len=sizeof(lastMessage)-1;
  comment[len] = '\0';
  // drop LCDx: leader
  if (strncmp (comment, "LCD", 3) == 0) {
    if (comment[3]>='0' && comment[3]<='3') {
      tPtr = &comment[5];
      if (strlen(tPtr) > 20) tPtr[20] = '\0';
      result = comment[3] - '0';
      strcpy (dccLCD[result], tPtr);
      result = 0;
    }
  }
  else {
  len = 0;
    while (comment[len]!=':' && len<strlen(comment)) len++;
    if (comment[len] == ':') len++;
    else len = 0;
    // copy to lastMessage buffer
    strcpy (lastMessage, &comment[len]);
  }
}

/*
 * Take a CV value and place it on a queue for display processing
 * <r10812|22112|56 -1>
 * <r -1>
 */
void dccCV(char* cv)
{
  int16_t result;
  char *start = NULL;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccCV(%s)\r\n", getTimeStamp(), cv);
    xSemaphoreGive(consoleSem);
  }
  if (cv[0] == ' ') start = cv + 1;
  else {
    for (uint8_t n=strlen(cv)-1; n>0 && start==NULL; n--) if (!((cv[n]<='9' && cv[n]>='0') || cv[n]=='>' || cv[n]=='-')) start = cv + n + 1;
  }
  for (uint8_t n=0; n<strlen(start); n++) if (start[n]==' ' || start[n]=='>') start[n]='\0';
  result = util_str2int(start);
  xQueueSend (cvQueue, &result, 0);
}

/*
 * Copy info string
 */
void dccInfo(char *cv)
{
  uint8_t result = 0;
  int len = strlen (cv);
  
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccInfo(%s)\r\n", getTimeStamp(), cv);
    xSemaphoreGive(consoleSem);
  }
  while (cv[len-1]=='>' || cv[len-1]=='*' || cv[len-1]==' ') len--;
  cv[len] = '\0';
  if (strlen(cv)>=sizeof(remServerDesc)) cv[sizeof(remServerDesc)-1] = '\0';
  strcpy (remServerDesc, cv);
  xQueueSend (dccAckQueue, &result, 0);
}

/*
 * Populate locomotive structure for DCC-Ex
 * Not really about receiving packets, but primes memory for use
 */
void dccPopulateLoco()
{
  struct locomotive_s *locoData = NULL;
  int numEntries = 0;
  int totalEntries = 0;
  int limit;
  uint8_t offset = 0;
  uint8_t reqState = 0;
  char *cPtr;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccPopulateLoco()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  // Get counts of things we want info about
  if (inventoryLoco != DCCINV) {
    numEntries = nvs_count ("loco", "String");
    totalEntries += numEntries;
  }
  if (inventoryLoco != LOCALINV) {
    while (xQueueReceive(dccLocoRefQueue, &reqState, 0) == pdPASS) {} // clear ack Queue
    txPacket ("<JR>");
    // wait for ack
    if (xQueueReceive(dccLocoRefQueue, &reqState, pdMS_TO_TICKS(TIMEOUT)) != pdPASS) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Warning: No response for querying Ex-RAIL loco count\r\n",  getTimeStamp());
        xSemaphoreGive(consoleSem);
      }
    }
    totalEntries += dccExNumListSize; 
    while (xQueueReceive (dccOffsetQueue, &offset, 0));
  }
  // allocate storage for locos
  limit = (totalEntries + MAXCONSISTSIZE) * sizeof(struct locomotive_s);
  locoData = (struct locomotive_s*) malloc (limit);
  cPtr = (char*) locoData;
  for (int n=0; n<limit; n++) cPtr[n] = '\0';
  if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("Total locomotives allocated: %d (%d bytes)\r\n", totalEntries, limit);
    xSemaphoreGive(consoleSem);
  }
  // get locally defined locomotives
  if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("loco", numEntries, NAMELENGTH);
    char *curData;
    char labelName[16];	
    curData = rawData;
    for (int n=0; n<numEntries; n++) {
      locoData[n].id = util_str2int (curData);
      curData = curData + 16;
      if (locoData[n].id > 127) locoData[n].type = 'L';
      else locoData[n].type = 'S';
      strcpy (locoData[n].name, curData);
      if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Loading locomotive %s\r\n", getTimeStamp(), curData);
        xSemaphoreGive(consoleSem);
      }
      curData = curData + NAMELENGTH;
      locoData[n].functionString  = NULL;
      locoData[n].direction = STOP;
      locoData[n].speed     = 0;
      locoData[n].steps     = 128;
      locoData[n].owned     = false;
      locoData[n].function  = 0;
      locoData[n].throttleNr= 255;
      locoData[n].relayIdx  = 255;
      sprintf (labelName, "latch%d", n);
      locoData[n].functionLatch = nvs_get_int (labelName, 65535);
    }
    if (rawData != NULL) {
      free (rawData);
    }
    // switch loaded data to live
    if (locoData != NULL) {
      if (locoRoster != NULL) free (locoRoster);
      locoRoster = locoData;
      locomotiveCount = numEntries;
    }
    if (dccExNumListSize == 0) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Info: No DCC-Ex defined locomotives loaded\r\n",  getTimeStamp());
        xSemaphoreGive(consoleSem);
      }
    }
    else if (dccExNumListSize > 0) {
      int16_t locoID;
      offset = numEntries;
      char cmdBuffer[16];
      bool isNew;
      for (uint8_t n=0; n<dccExNumListSize; n++) {
        locoID = dccExNumList[n];
        isNew = true;
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          for (uint8_t chk=0; chk<locomotiveCount && isNew; chk++) {
            if (locoID == locoRoster[chk].id) isNew=false;
          }
          xSemaphoreGive(velociSem);
        }
        if (isNew) {
          xQueueSend (dccOffsetQueue, &offset, 0);
          offset++;
          sprintf (cmdBuffer, "<JR %d>", locoID);
          txPacket (cmdBuffer);
          // wait for ack
          if (xQueueReceive(dccLocoRefQueue, &reqState, pdMS_TO_TICKS(TIMEOUT)) != pdPASS) { // wait for ack
            if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("%s Warning: No response for querying Ex-RAIL loco %d\r\n",  getTimeStamp(), locoID);
              xSemaphoreGive(consoleSem);
            }
          }
        }
      }
      delay (500);  // extra delay (shouldn't be required) for packet processing to complete
    }
    // configure ad-hoc entry space
    if (locoRoster != NULL) for (uint8_t tokenPtr=locomotiveCount, n=0; n<MAXCONSISTSIZE; tokenPtr++, n++) {
      sprintf (locoRoster[tokenPtr].name, "ad-hoc-%d", n+1);
      locoRoster[tokenPtr].direction = STOP;
      locoRoster[tokenPtr].speed     = 0;
      locoRoster[tokenPtr].steps     = 128;
      locoRoster[tokenPtr].id        = 0;
      locoRoster[tokenPtr].owned     = false;
      locoRoster[tokenPtr].function  = 0;
      locoRoster[tokenPtr].throttleNr= 255;
      locoRoster[tokenPtr].relayIdx  = 255;
      locoRoster[tokenPtr].functionLatch = 65535;
      locoRoster[tokenPtr].functionString  = NULL;
    }
  }
}

void dccPopulateTurnout()
{
  struct turnout_s *turnoutData = NULL;
  char *cPtr;
  int numEntries = 0;      // number of entries in NVS
  int totalEntries = 0;    // total number of entries including DCC-Ex / Ex-RAIL
  uint8_t offset = 0;
  uint8_t reqState;
  bool mustDefine = false;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccPopulateTurnout()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  // Configure turnout states array
  if (turnoutState != NULL) free (turnoutState);
  turnoutState = (struct turnoutState_s*) malloc (4 * sizeof(struct turnoutState_s));
  cPtr = (char*) turnoutState;
  for (int n=0; n<(4 * sizeof(struct turnoutState_s)); n++) cPtr[n] = '\0';
  turnoutState[0].state = '2';
  turnoutState[1].state = '4';
  turnoutState[2].state = '1';
  turnoutState[3].state = '8';
  strcpy (turnoutState[0].name, "Closed");
  strcpy (turnoutState[1].name, "Thrown");
  strcpy (turnoutState[2].name, "Unknown");
  strcpy (turnoutState[3].name, "Failed");
  turnoutStateCount = 4;
  // Get counts of things we want info about
  if (inventoryTurn != DCCINV) {
    numEntries = nvs_count ("turnout", "String");
    totalEntries += numEntries;
  }
  if (inventoryTurn != LOCALINV) {
    while (xQueueReceive(dccTurnoutQueue, &reqState, 0) == pdPASS) {} // clear ack Queue
    txPacket ("<JT>");
    // wait for ack
    if (xQueueReceive(dccTurnoutQueue, &reqState, pdMS_TO_TICKS(TIMEOUT)) != pdPASS) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Warning: No response for querying Ex-RAIL turnout count\r\n",  getTimeStamp());
        xSemaphoreGive(consoleSem);
      }
    }
    totalEntries += dccExNumListSize; 
    while (xQueueReceive (dccOffsetQueue, &offset, 0));
  }
  // Create array to hold all that stuff
  if (totalEntries > 0) {
    int limit = totalEntries * sizeof(struct turnout_s);
    turnoutData = (struct turnout_s*) malloc (limit);
    if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Total Turnouts allocated: %d (%d bytes)\r\n", totalEntries, limit);
      xSemaphoreGive(consoleSem);
    }
    cPtr = (char*) turnoutData;
    for (int n=0; n<limit; n++) cPtr[n] = '\0';
  }
  // Read in the local info
  if (numEntries == 0) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Info: No in-throttle defined turnouts loaded\r\n",  getTimeStamp());
      xSemaphoreGive(consoleSem);
    }
  }
  else if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("turnout", numEntries, 2*NAMELENGTH);
    char *curData;
    char commandBuffer[3*NAMELENGTH]; 
    uint8_t offset = nvs_get_int("toOffset", 100);

    if (rawData != NULL) {
      curData = rawData;
      for (int n=0; n<numEntries; n++) {
        turnoutData[n].state = '1';
        sprintf (turnoutData[n].sysName, "%d", (offset+n));
        strcpy (turnoutData[n].userName, curData);
        if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Loading turnout %s\r\n", getTimeStamp(), curData);
          xSemaphoreGive(consoleSem);
        }
        curData = curData + 16;
        curData = curData + (2*NAMELENGTH);
        }
      // Sort and renumber - keep all miniThrottles consistent for same set of data even if stored differently
      sortTurnout(turnoutData, numEntries);
      for (int n=0; n<numEntries; n++) {
        sprintf (turnoutData[n].sysName, "%d", (offset+n));
      }
      // Now test for existence, and define if not already there
      curData = rawData;
      for (int n=0; n<numEntries; n++) {
      sprintf (commandBuffer, "<JT %s>", turnoutData[n].sysName);
      while (xQueueReceive(dccTurnoutQueue, &reqState, 0) == pdPASS) {} // clear ack Queue
      txPacket (commandBuffer);
      mustDefine = false;
      if (xQueueReceive(dccTurnoutQueue, &reqState, pdMS_TO_TICKS(TIMEOUT)) != pdPASS) {
        // wait for ack
        mustDefine = true;
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Warning: No response for testing turnout existence\r\n", getTimeStamp());
        xSemaphoreGive(consoleSem);
        }
      }
      else {
        if (reqState == 'C') turnoutData[n].state = '2';
        else if (reqState == 'T') turnoutData[n].state = '4';
        else mustDefine = true;
        if (debuglevel>0 && (reqState=='C' || reqState=='T') && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Turnout T%d exists, state reported as %c\r\n", getTimeStamp(), (offset+n), reqState);
          xSemaphoreGive(consoleSem);
        }
      }
      curData = curData + 16;
      sprintf (commandBuffer, "<T %s %s>", turnoutData[n].sysName, curData);
      if (mustDefine) {
        if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Define turnout T%d as %s\r\n", getTimeStamp(), (offset+n), curData);
          xSemaphoreGive(consoleSem);
        }
        while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS) {} // clear ack Queue
        txPacket (commandBuffer);
        if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(TIMEOUT)) != pdPASS) {
          // wait for ack
          turnoutData[n].state = '8';
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Warning: No response for defining turnout, status = Failed\r\n",  getTimeStamp());
            xSemaphoreGive(consoleSem);
          }
        }
      }
      curData = curData + (2*NAMELENGTH);
    }
    if (rawData != NULL) free (rawData);
  }
  }
  if (turnoutData != NULL) {
    if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (turnoutList != NULL) free (turnoutList);
      turnoutList = turnoutData;
      turnoutCount = numEntries;
      xSemaphoreGive(turnoutSem);
    }
    // Now that we have the structure in place, lets append to it by querying DCC-Ex if required
    // OR note there is nothing to load
    if (dccExNumListSize == 0) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Info: No DCC-Ex defined turnouts loaded\r\n",  getTimeStamp());
        xSemaphoreGive(consoleSem);
      }
    }
    else if (dccExNumListSize > 0) {
      char commandBuffer[3*NAMELENGTH]; 
      int16_t offsetLimit = nvs_get_int("toOffset", 100);
      bool isNew = false;

      offset = numEntries;
      for (uint8_t n=0; n<dccExNumListSize; n++) {
        // If below offset threshold
        if (dccExNumList[n] < offsetLimit) {
          isNew = true;
          sprintf (commandBuffer, "%d", dccExNumList[n]);
          if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            for (uint8_t chk=0; chk<turnoutCount && isNew; chk++) {
              if (strcmp(commandBuffer, turnoutList[chk].sysName) == 0) isNew=false;
            }
            xSemaphoreGive(turnoutSem);
          }
          if (isNew) {
            xQueueSend (dccOffsetQueue, &offset, 0);
            offset++;
            sprintf (commandBuffer, "<JT %d>", dccExNumList[n]);
            txPacket (commandBuffer);
            // wait for ack
            if (xQueueReceive(dccTurnoutQueue, &reqState, pdMS_TO_TICKS(TIMEOUT)) != pdPASS) {
              if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("%s Warning: No response for querying Ex-RAIL turnout definition\r\n", getTimeStamp());
                xSemaphoreGive(consoleSem);
              }
            }
          }
        }
      }
    delay (500);    // arbitrary wait period for any associated threads to complete
    }
  }
}


void dccPopulateRoutes()
{
  struct route_s *rtData = NULL;
  char *cPtr;
  char recvData;
  int numEntries = 0;
  int totalEntries =0;
  uint8_t offset = 0;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccPopulateRoutes()\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }

  if (routeState != NULL) free (routeState);
  routeState = (struct routeState_s*) malloc (4 * sizeof(struct routeState_s));
  cPtr = (char*) routeState;
  for (int n=0; n<(4 * sizeof(struct routeState_s)); n++) cPtr[n] = '\0';
  routeState[0].state = '2';
  strcpy (routeState[0].name, "Active");
  routeState[1].state = '4';
  strcpy (routeState[1].name, "Inactive");
  routeState[2].state = '8';
  strcpy (routeState[2].name, "Inconsistent");
  routeState[3].state = '0';
  strcpy (routeState[3].name, "Unknown");
  routeStateCount = 4;

  if (inventoryRout != DCCINV) {
    numEntries = nvs_count ("route", "String");
    totalEntries += numEntries;
  }
  if (inventoryRout != LOCALINV) {
    while (xQueueReceive(dccRouteQueue, &recvData, 0) == pdPASS) {} // clear ack Queue
    txPacket ("<JA>");
    // wait for ack
    if (xQueueReceive(dccRouteQueue, &recvData, pdMS_TO_TICKS(TIMEOUT)) != pdPASS) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Warning: No response for querying Ex-RAIL route count\r\n",  getTimeStamp());
        xSemaphoreGive(consoleSem);
      }
    }
    totalEntries += dccExNumListSize; 
    while (xQueueReceive (dccOffsetQueue, &offset, 0));
  }
  if (totalEntries > 0) {
    int limit = totalEntries * sizeof(struct route_s);
    rtData = (struct route_s*) malloc (limit);
    if (debuglevel>1 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Total Routes allocated: %d (%d bytes)\r\n", totalEntries, limit);
      xSemaphoreGive(consoleSem);
    }
    cPtr = (char*) rtData;
    for (int n=0; n<limit; n++) cPtr[n] = '\0';
  }
  if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("route", numEntries, BUFFSIZE);
    char *curData;
    char *turnoutData;
    uint16_t limit  = 0;
    uint16_t ptr    = 0;  // byte offset pointer
    uint16_t wPtr   = 0;  // word pointer
    uint8_t  tCnt   = 0;  // turnout counter
    uint8_t  tVal   = 0;

    offset = nvs_get_int("toOffset", 100);
    curData = rawData;
    for (int n=0; n<numEntries; n++) {
      // Populate base data of each route
      rtData[n].state = '4';
      sprintf (rtData[n].sysName, "R%02d", offset+n);
      strcpy  (rtData[n].userName, curData);
      // populate details of turnouts and desired states.
      // for 10 routes of upto 25 steps each, this is only 500 bytes of memory if stored using turnout index and desired state
      turnoutData = curData + 16;
      ptr = 0;
      tCnt = 0;
      if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Loading route %s\r\n", getTimeStamp(), curData);
        xSemaphoreGive(consoleSem);
      }
      limit = strlen (turnoutData);
      while (turnoutData[ptr]!=0 && tCnt<MAXRTSTEPS && ptr<limit) {
        while (turnoutData[ptr] == ' ' && turnoutData[ptr]!='\0') ptr++;
        wPtr = ptr;
        while (turnoutData[ptr] != ' ' && turnoutData[ptr]!='\0') ptr++;   // skip to white space
        while (turnoutData[ptr] == ' ' && turnoutData[ptr]!='\0') {
          turnoutData[ptr] = '\0';
          ptr++;
        }
        tVal = 200;  // 255 => end of route, 200 => MIA / removed turnout.
        for (uint8_t n=0; n<turnoutCount && tVal == 200; n++) if (strcasecmp(turnoutList[n].userName, &turnoutData[wPtr]) == 0) {
          tVal = n;
        }
        rtData[n].turnoutNr[tCnt] = tVal;  // 255 => end of route, 200 => MIA / removed turnout.
        if (tVal == 200) {
          if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s dccPopulateRoutes: route %s, ERROR: turnout %s not found or removed?\r\n", getTimeStamp(), curData, &turnoutData[wPtr]);
            xSemaphoreGive(consoleSem);
          }
        }
        turnoutData[ptr] = toupper(turnoutData[ptr]);
        if (turnoutData[ptr] == 'T' || turnoutData[ptr] == 'C') {
          rtData[n].desiredSt[tCnt] = turnoutData[ptr];
        }
        else {
          rtData[n].desiredSt[tCnt] = 200;
          if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s dccPopulateRoutes: State of route %s, switch %s should be T (for thrown) or C (for closed).\r\n", getTimeStamp(), curData, &turnoutData[wPtr]);
            xSemaphoreGive(consoleSem);
          }
        }
        tCnt++;
        ptr++;
      }
      while (tCnt < MAXRTSTEPS) {
        rtData[n].turnoutNr[tCnt] = 255;
        rtData[n].desiredSt[tCnt] = 255;
        tCnt++;
      }
      curData = curData + (16 + BUFFSIZE);
    }
    if (rawData != NULL) {
      free (rawData);
    }
  }
  if (rtData != NULL) {
    if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (routeList != NULL) free (routeList);
      routeList = rtData;
      routeCount = numEntries;
      xSemaphoreGive(routeSem);
      // Now that we have the structure in place, lets append to it by querying DCC-Ex if required
      // OR note there is nothing to load
      if (dccExNumListSize == 0) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Info: No DCC-Ex defined routes/automations loaded\r\n",  getTimeStamp());
          xSemaphoreGive(consoleSem);
        }
      }
      else if (dccExNumListSize > 0) {
        char commandBuffer[3*NAMELENGTH]; 
        uint16_t offsetLimit = nvs_get_int("toOffset", 100);
        bool isNew = false;
        offset = numEntries;
        for (uint8_t n=0; n<dccExNumListSize; n++) {
          if (dccExNumList[n] < offsetLimit) {
            isNew = true;
            sprintf (commandBuffer, "%d", dccExNumList[n]);
            if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              for (uint8_t chk=0; chk<routeCount && isNew; chk++) {
                if (strcmp(commandBuffer, routeList[chk].sysName) == 0) isNew=false;
              }
              xSemaphoreGive(routeSem);
            }
            if (isNew) {
              // If below offset threshold, add the array offset to feedback queue, then query
              xQueueSend (dccOffsetQueue, &offset, 0);
              offset++;
              sprintf (commandBuffer, "<JA %d>", dccExNumList[n]);
              txPacket (commandBuffer);
              // wait for ack
              if (xQueueReceive(dccRouteQueue, &recvData, pdMS_TO_TICKS(TIMEOUT)) != pdPASS) {
                if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                  Serial.printf ("%s Warning: No response for querying Ex-RAIL routes definition\r\n", getTimeStamp());
                  xSemaphoreGive(consoleSem);
                }
              }
            }
          }
        }
      }
    }
  }
}

