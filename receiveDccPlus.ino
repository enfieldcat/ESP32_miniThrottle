void processDccPacket (char *packet)
{
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
 * ID, void?, speed 0-255, functions bit map
 */
void dccLocoStatus (char* locoStatus)
{
  int16_t locoID = -9999;
  int16_t locoVoid = -9999;
  int16_t  locoSpeed = -9999;
  uint16_t locoFunc = 9999;
  uint8_t next, maxIdx, curIdx;
  uint8_t result = 0;
  char *ptr;

  xQueueSend (dccAckQueue, &result, 0);   // Ack even if garbled.
  maxIdx = strlen(locoStatus);
  for (uint8_t n=(strlen(locoStatus)-1); n<0 && locoStatus[n]!=' ' && locoStatus[n]!='>'; n--) locoStatus[n] = '\0'; // truncate trailing chars
  for (next=0; locoStatus[next]!=' ' && next<strlen(locoStatus); next++);  // extract loco id
  locoStatus[next++] = '\0';
  curIdx = next;
  if (ptr!=NULL && strlen(ptr)>0) {
    locoID = util_str2int(locoStatus);
  }
  else return;
  ptr = locoStatus + next;
  for (next=0; ptr[next]!=' ' && next<strlen(ptr); next++);  // extract loco void
  curIdx = curIdx + next + 1;
  if (curIdx > maxIdx) return;
  ptr[next++] = '\0';
  if (ptr!=NULL && strlen(ptr)>0) {
    locoVoid = util_str2int(ptr);
  }
  else return;
  ptr = ptr + next;
  for (next=0; ptr[next]!=' ' && next<strlen(ptr); next++);  // extract loco speed
  curIdx = curIdx + next + 1;
  if (curIdx > maxIdx) return;
  ptr[next++] = '\0';
  if (ptr!=NULL && strlen(ptr)>0) {
    locoSpeed = util_str2int(ptr);
  }
  else return;
  ptr = ptr + next;
  for (next=0; ptr[next]!=' ' && next<strlen(ptr); next++);  // extract loco functions
  curIdx = curIdx + next + 1;
  if (curIdx > maxIdx) return;
  ptr[next++] = '\0';
  if (ptr!=NULL && strlen(ptr)>0) {
    locoFunc = util_str2int(ptr);
  }
  else return;
  if (locoID != -9999) {
    for (next=0; next<locomotiveCount+MAXCONSISTSIZE && locoRoster[next].id!=locoID; next++);
    if (next==locomotiveCount+MAXCONSISTSIZE) return;
    if (locoFunc>=0 && locoFunc != 9999 && xSemaphoreTake(functionSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      locoRoster[next].function = locoFunc;
      xSemaphoreGive(functionSem);
    }
    if (locoSpeed>=-1 && locoSpeed <256 && xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      if (locoRoster[next].steps > 126) locoRoster[next].speed = (int16_t) ((locoSpeed >> 1) -1);
      xSemaphoreGive(velociSem);
    }
  }
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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

  xQueueSend (dccAckQueue, &result, 0);
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
  for (uint8_t ptr=0; ptr<maxLocoArray; ptr++) {
    if (locoRoster[ptr].owned) {
      if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        locoRoster[ptr].speed = (int16_t) dccSpeed;
        if (dccDirection == 1) locoRoster[ptr].direction = FORWARD;
        else locoRoster[ptr].direction = REVERSE;
        xSemaphoreGive(velociSem);
      }
      speedChange = true;
    }
  }
}

/*
 * Ack turnout packet
 */
void dccAckTurnout (char *ack)
{
  uint8_t result = 255;
  xQueueSend (dccAckQueue, &result, 0);
  for (uint8_t n=0; n<strlen(ack); n++) if (ack[n]==' ') {
    ack[n]='\0';
    result = ack[n+1];
    if (result == '1') result = 'T';
    else result = 'C';
  }
  for (uint8_t n=0; n<turnoutCount; n++) if (strcmp (ack, turnoutList[n].sysName) == 0) {
    turnoutList[n].state = result;
    n = turnoutCount;
  }
}

void dccAckTurnout ()
{
  char result = 15;
  xQueueSend (dccAckQueue, &result, 0);
}

/*
 * Power on/off to track
 */
void dccPowerChange(char state)
{
  uint8_t result = 0;

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
}

/*
 * Accept a comment from DCC
 */
void dccComment(char* comment)
{
  int len = strlen (comment);
  uint8_t result = 0;

  xQueueSend (dccAckQueue, &result, 0);
  // drop trailing chars
  while (comment[len-1]=='>' || comment[len-1]=='*' || comment[len-1]==' ') len--;
  if (len>(sizeof(lastMessage)-1)) len=sizeof(lastMessage)-1;
  comment[len] = '\0';
  // drop LCDx: leader
  len = 0;
  while (comment[len]!=':' && len<strlen(comment)) len++;
  if (comment[len] == ':') len++;
  else len = 0;
  // copy to lastMessage buffer
  strcpy (lastMessage, &comment[len]);
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
  
  if (strlen(cv)>=sizeof(remServerDesc)) cv[sizeof(remServerDesc)-1] = '\0';
  strcpy (remServerDesc, cv);
  xQueueSend (dccAckQueue, &result, 0);
}

/*
 * Populate locomotive structure for DCC++
 * Not really about receiving packets, but primes memory for use
 */
void dccPopulateLoco()
{
  int numEntries = nvs_count ("loco", "String");

  if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("loco", numEntries, NAMELENGTH);
    char *curData;
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
    }
  }
}

void dccPopulateTurnout()
{
  int numEntries = nvs_count ("turnout", "String");
  uint8_t reqState;

  if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("turnout", numEntries, 2*NAMELENGTH);
    char *curData;
    struct turnout_s *turnoutData = (struct turnout_s*) malloc (numEntries * sizeof(struct turnout_s));
    char commandBuffer[3*NAMELENGTH]; 
    
    if (turnoutState != NULL) free (turnoutState);
    turnoutState = (struct turnoutState_s*) malloc (2 * sizeof(struct turnoutState_s));
    turnoutState[0].state = 'C';
    turnoutState[1].state = 'T';
    strcpy (turnoutState[0].name, "Closed");
    strcpy (turnoutState[1].name, "Thrown");
    turnoutStateCount = 2;

    curData = rawData;
    while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS) {} // clear ack Queue
    for (int n=0; n<numEntries; n++) {
      turnoutData[n].state = 'C';
      sprintf (turnoutData[n].sysName, "%d", (20+n));
      strcpy (turnoutData[n].userName, curData);
      curData = curData + 16;
      sprintf (commandBuffer, "<T %s %s>", turnoutData[n].sysName, curData);
      curData = curData + (2*NAMELENGTH);
      txPacket (commandBuffer);
      if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(5000)) != pdPASS) { // wait for ack - wait longer than usual
        // wait for ack
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          Serial.println ("Warning: No response for defining turnout");
          xSemaphoreGive(displaySem);
        }
      }
    }
    if (rawData != NULL) {
      free (rawData);
      if (turnoutList != NULL) free (turnoutList);
      turnoutList = turnoutData;
      turnoutCount = numEntries;
    }
  }
}


void dccPopulateRoutes()
{
  int numEntries = nvs_count ("route", "String");

  if (numEntries > 0) {
    char *rawData  = (char*) nvs_extractStr ("route", numEntries, BUFFSIZE);
    char *curData;
    struct route_s *rData = (struct route_s*) malloc (numEntries * sizeof(struct route_s));

    if (routeState != NULL) free (routeState);
    routeState = (struct routeState_s*) malloc (sizeof(struct routeState_s));
    routeState[0].state = 'V';
    strcpy (routeState[0].name, "Valid");
    routeStateCount = 1;

    curData = rawData;
    for (int n=0; n<numEntries; n++) {
      rData[n].state = 'V';
      sprintf (rData[n].sysName, "r%d", 100+n);
      strcpy  (rData[n].userName, curData);
      curData = curData + (16 + BUFFSIZE);
    }
    if (rawData != NULL) {
      free (rawData);
      if (routeList != NULL) free (routeList);
      routeList = rData;
      routeCount = numEntries;
    }
  }
}
