void processDccPacket (char *packet)
{
  if (strncmp (packet, "<t ", 3) == 0) dccSpeedChange (&packet[3]);
  else if (strncmp (packet, "PPA", 3) == 0) dccPowerChange (packet[3]);
  else if (strncmp (packet, "<p",  2) == 0) dccPowerChange (packet[2]);
  else if (strncmp (packet, "<* ", 3) == 0) dccComment (&packet[3]);
  else if (strncmp (packet, "<r",  2) == 0) dccCV (&packet[2]);
  else if (strncmp (packet, "<i",  2) == 0) dccInfo (&packet[2]);
}

void dccSpeedChange(char* speedSet)
{
  int dccRegister = 1;
  int dccSpeed = 0;
  int dccDirection = 0;
  int maxLocoArray = locomotiveCount + MAXCONSISTSIZE;
  char *token;
  char *remain = speedSet;

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
      locoRoster[ptr].speed = dccSpeed;
      if (dccDirection == 1) locoRoster[ptr].direction = FORWARD;
      else locoRoster[ptr].direction = REVERSE;
    }
  }
}

/*
 * Power on/off to track
 */
void dccPowerChange(char state)
{
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
  if (strlen(cv)>=sizeof(remServerDesc)) cv[sizeof(remServerDesc)-1] = '\0';
  strcpy (remServerDesc, cv);
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
    for (int n=0; n<numEntries; n++) {
      turnoutData[n].state = 'C';
      sprintf (turnoutData[n].sysName, "%d", (20+n));
      strcpy (turnoutData[n].userName, curData);
      curData = curData + 16;
      sprintf (commandBuffer, "<T %s %s>", turnoutData[n].sysName, curData);
      curData = curData + (2*NAMELENGTH);
      txPacket (commandBuffer);
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
    char *rawData  = (char*) nvs_extractStr ("turnout", numEntries, BUFFSIZE);
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
