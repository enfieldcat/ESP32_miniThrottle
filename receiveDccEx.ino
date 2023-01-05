void processDccPacket (char *packet)
{
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s processDccPacket(%s)\r\n", getTimeStamp(), packet);
    xSemaphoreGive(displaySem);
  }

  if (strncmp (packet, "<T ", 3) == 0) dccSpeedChange (&packet[3]);
  else if (strncmp (packet, "<l ", 3) == 0) dccLocoStatus  (&packet[3]);
  else if (strncmp (packet, "PPA", 3) == 0) dccPowerChange (packet[3]);
  else if (strncmp (packet, "<p",  2) == 0) dccPowerChange (packet[2]);
  else if (strncmp (packet, "<* ", 3) == 0) dccComment (&packet[3]);
  else if (strncmp (packet, "<r",  2) == 0) dccCV (&packet[2]);
  else if (strncmp (packet, "<i",  2) == 0) dccInfo (&packet[2]);
  else if (strncmp (packet, "<H ", 3) == 0) dccAckTurnout (&packet[3]);
  else if (strncmp (packet, "<O>", 3) == 0) dccAckTurnout ();
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

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccLocoStatus(%s)\r\n", getTimeStamp(), locoStatus);
    xSemaphoreGive(displaySem);
  }

  xQueueSend (dccAckQueue, &result, 0);   // Ack even if garbled.
  for (uint8_t n=(strlen(locoStatus)-1); n<0 && locoStatus[n]!=' ' && locoStatus[n]!='>'; n--) locoStatus[n] = '\0'; // truncate trailing chars
  ptr = strtok_r (locoStatus, " ", &savePtr);
  if (ptr!=NULL && strlen(ptr)>0) {
    locoID = util_str2int(locoStatus);
  }
  else {
    if (debuglevel>1 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("dccLocoStatus: No loco ID\r\n");
      xSemaphoreGive(displaySem);
    }
    return;
  }
  // Grab the status? And ignore it
  ptr = strtok_r (NULL, " ", &savePtr);
  /* if (ptr!=NULL && strlen(ptr)>0) {
    locoState = abs(util_str2int(ptr));
  }
  else {
    if (debuglevel>1 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("dccLocoStatus: No loco state\r\n");
      xSemaphoreGive(displaySem);
    }
    return;
  } */
  // Grab the speed 0 - 255
  ptr = strtok_r (NULL, " ", &savePtr);
  if (ptr!=NULL && strlen(ptr)>0) {
    locoSpeed = util_str2int(ptr);
  }
  else {
    if (debuglevel>1 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("dccLocoStatus: No loco speed\r\n");
      xSemaphoreGive(displaySem);
    }
    return;
  }
  // Grab the function setting
  ptr = strtok_r (NULL, " ", &savePtr);
  if (ptr!=NULL && strlen(ptr)>0) {
    locoFunc = util_str2int(ptr);
  }
  else {
    if (debuglevel>1 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("dccLocoStatus: No function data\r\n");
      xSemaphoreGive(displaySem);
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
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("Invalid loco ID in status update\r\n");
    xSemaphoreGive(displaySem);
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

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccSpeedChange(%s)\r\n", getTimeStamp(), speedSet);
    xSemaphoreGive(displaySem);
  }
  xQueueSend (dccAckQueue, &result, 0);
  if (xQueueReceive(dccLocoRefQueue, &locoIndex, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
    if (debuglevel>0 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s dccSpeedChange failed to get associated LocoID from internal queue\r\n", getTimeStamp());
      xSemaphoreGive(displaySem);
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
    if (t_owned) {
      speedChange = true;
    }
    else {
      if (relayMode == WITHROTRELAY && t_relayIdx<maxRelay) {
        if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          uint8_t chk = relayCount;
          xSemaphoreGive(relaySvrSem);
          if (chk > 1) { // only send message if relay service running and initialised
            sprintf (outBuffer, "M%cA%c%d<;>V%d\r\nM%cA%c%d<;>R%d\r\n", t_throttleNr, t_type, t_id, dccSpeed, t_throttleNr, t_type, t_id, t_direction);
            reply2relayNode (&(remoteSys[t_relayIdx]), outBuffer);
          }
        }
      }
    }
    #else
    speedChange = true;
    #endif
  }
  else semFailed ("velociSem", __FILE__, __LINE__);
}

/*
 * Ack turnout packet
 */
void dccAckTurnout (char *ack)
{
  uint8_t result = 255;
  #ifdef RELAYPORT
  char outBuffer[32];
  #endif
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccAckTurnout(%s)\r\n", getTimeStamp(), ack);
    xSemaphoreGive(displaySem);
  }
  xQueueSend (dccAckQueue, &result, 0);
  //if (ack[0]=='8' && ack[1]=='4') {   // DCC-Ex may bodge our name and change letters to numbers, Turnout names start 'T'
  //  ack[0]='T';
  //  for (uint8_t n=1; n<strlen(ack); n++) ack[n] = ack[n+1];
  //}
  for (uint8_t n=0; n<strlen(ack); n++) if (ack[n]==' ') {
    ack[n]='\0';
    result = ack[n+1];
    if (result == '1') result = '4';
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
      n = turnoutCount;
    }
  }
}

void dccAckTurnout ()
{
  char result = 15;

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccAckTurnout()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }
  xQueueSend (dccAckQueue, &result, 0);
}

/*
 * Power on/off to track
 */
void dccPowerChange(char state)
{
  uint8_t result = 0;

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccPowerChange(%c)\r\n", getTimeStamp(), state);
    xSemaphoreGive(displaySem);
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

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccComment(%s)\r\n", getTimeStamp(), comment);
    xSemaphoreGive(displaySem);
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

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccCV(%s)\r\n", getTimeStamp(), cv);
    xSemaphoreGive(displaySem);
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
  
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccInfo(%s)\r\n", getTimeStamp(), cv);
    xSemaphoreGive(displaySem);
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
  int numEntries = nvs_count ("loco", "String");

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccPopulateLoco()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }
  if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("loco", numEntries, NAMELENGTH);
    char *curData;
    char labelName[16];
    struct locomotive_s *locoData = (struct locomotive_s*) malloc ((MAXCONSISTSIZE + numEntries) * sizeof(struct locomotive_s));

    curData = rawData;
    for (int n=0; n<numEntries; n++) {
      locoData[n].id = util_str2int (curData);
      curData = curData + 16;
      if (locoData[n].id > 127) locoData[n].type = 'L';
      else locoData[n].type = 'S';
      strcpy (locoData[n].name, curData);
      curData = curData + NAMELENGTH;
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
      if (locoRoster != NULL) free (locoRoster);
      locoRoster = locoData;
      locomotiveCount = numEntries;
    }
    if (locoRoster != NULL) for (uint8_t tokenPtr=numEntries, n=0; n<MAXCONSISTSIZE; tokenPtr++, n++) {
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
    }
  }
}

void dccPopulateTurnout()
{
  int numEntries = nvs_count ("turnout", "String");
  uint8_t reqState;

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccPopulateTurnout()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }
  if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("turnout", numEntries, 2*NAMELENGTH);
    char *curData;
    struct turnout_s *turnoutData = (struct turnout_s*) malloc (numEntries * sizeof(struct turnout_s));
    char commandBuffer[3*NAMELENGTH]; 
    
    if (turnoutState != NULL) free (turnoutState);
    turnoutState = (struct turnoutState_s*) malloc (4 * sizeof(struct turnoutState_s));
    turnoutState[0].state = '2';
    turnoutState[1].state = '4';
    turnoutState[2].state = '1';
    turnoutState[3].state = '8';
    strcpy (turnoutState[0].name, "Closed");
    strcpy (turnoutState[1].name, "Thrown");
    strcpy (turnoutState[2].name, "Unknown");
    strcpy (turnoutState[3].name, "Failed");
    turnoutStateCount = 4;

    curData = rawData;
    while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS) {} // clear ack Queue
    for (int n=0; n<numEntries; n++) {
      turnoutData[n].state = '1';
      sprintf (turnoutData[n].sysName, "%02d", (1+n));
      strcpy (turnoutData[n].userName, curData);
      curData = curData + 16;
      sprintf (commandBuffer, "<T %s %s>", turnoutData[n].sysName, curData);
      curData = curData + (2*NAMELENGTH);
      txPacket (commandBuffer);
      if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(TIMEOUT*3)) != pdPASS) { // wait for ack - wait longer than usual
        // wait for ack
        turnoutData[n].state = '8';
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.println ("Warning: No response for defining turnout, status = Failed");
          xSemaphoreGive(displaySem);
        }
      }
    }
    if (rawData != NULL) {
      free (rawData);
      if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (turnoutList != NULL) free (turnoutList);
        turnoutList = turnoutData;
        turnoutCount = numEntries;
        xSemaphoreGive(turnoutSem);
      }
    }
  }
}


void dccPopulateRoutes()
{
  int numEntries = nvs_count ("route", "String");

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dccPopulateRoutes()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }
  if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("route", numEntries, BUFFSIZE);
    char *curData;
    struct route_s *rData = (struct route_s*) malloc (numEntries * sizeof(struct route_s));

    if (routeState != NULL) free (routeState);
    routeState = (struct routeState_s*) malloc (4 * sizeof(struct routeState_s));
    routeState[0].state = '2';
    strcpy (routeState[0].name, "Active");
    routeState[1].state = '4';
    strcpy (routeState[1].name, "Inactive");
    routeState[2].state = '8';
    strcpy (routeState[2].name, "Inconsistent");
    routeState[3].state = '0';
    strcpy (routeState[3].name, "Unknown");
    routeStateCount = 4;

    curData = rawData;
    for (int n=0; n<numEntries; n++) {
      rData[n].state = '4';
      sprintf (rData[n].sysName, "R%02d", 1+n);
      strcpy  (rData[n].userName, curData);
      curData = curData + (16 + BUFFSIZE);
    }
    if (rawData != NULL) {
      free (rawData);
      if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (routeList != NULL) free (routeList);
        routeList = rData;
        routeCount = numEntries;
        xSemaphoreGive(routeSem);
      }
    }
  }
}
