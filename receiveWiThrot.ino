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



// Keep a large statically allocated token table for major tokens
static char *majToken[WITHROTMAXFIELDS];
static uint8_t tokenTally;


void processWiThrotPacket (char *packet)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s processWiThrotPacket(%s)\r\n", getTimeStamp(), packet);
    xSemaphoreGive(consoleSem);
  }

  if (strncmp (packet, "PPA", 3) == 0) { // Track power
    // Power status
    dccPowerChange(packet[3]);
  }
  else if (strncmp (packet, "PTA", 3) == 0 && strlen(packet)>4) { // Change of turnout
    char *tPtr = &packet[4];
    uint8_t state = packet[3];
    uint8_t ptr = 0;
    bool found = false;

    // only update if in known state
    if (state=='2' || state=='4' || state=='1' || state=='8') {
      // first attempt to find by name
      for (ptr=0; ptr<turnoutCount && !found; ptr++) {
        if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (strcmp (turnoutList[ptr].sysName, tPtr) == 0) found = true;
          xSemaphoreGive(turnoutSem);
        }
        else semFailed ("turnoutSem", __FILE__, __LINE__);
      }
      if (!found) {
        for (ptr=0; ptr<turnoutCount && !found; ptr++) {
          if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            if (strcmp (turnoutList[ptr].userName, tPtr) == 0) found = true;
            xSemaphoreGive(turnoutSem);
          }
          else semFailed ("turnoutSem", __FILE__, __LINE__);
        }
      }
      if (found) {
        if (ptr > 0) ptr--; // the for loop should have incremented ptr past the found spot.
        if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          turnoutList[ptr].state = state - '0';
          xSemaphoreGive(turnoutSem);
        }
        else semFailed ("turnoutSem", __FILE__, __LINE__);
      }
    }
  }
  else if (strncmp (packet, "PRA", 3) == 0 && strlen(packet)>4) { // Change of route
    char *tPtr = &packet[4];
    uint8_t state = packet[3];
    uint8_t ptr = 0;
    bool found = false;
    for (ptr=0; ptr<routeCount && !found; ptr++) {
      if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (strcmp (routeList[ptr].sysName, tPtr) == 0) found = true;
        xSemaphoreGive(routeSem);
      }
      else semFailed ("routeSem", __FILE__, __LINE__);
    }
    if (found) {
      if (ptr > 0) ptr--; // the for loop should have incremented ptr past the found spot.
      if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        routeList[ptr].state = state;
        xSemaphoreGive(routeSem);
      }
      else semFailed ("routeSem", __FILE__, __LINE__);
    }
  }
  else if (packet[0] == '*') {
    // Keep alive
    char *tptr;
    keepAliveTime = strtol ((const char*)&packet[1], &tptr, 10);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Keep Alive interval set to %d seconds\r\n", getTimeStamp(), keepAliveTime);
      xSemaphoreGive(consoleSem);
    }
    #ifndef SERIALCTRL
    {
      // set to no tracking
      uint8_t maxMissedKeepAlive = nvs_get_int ("missedKeepAlive", 0);
      if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (maxMissedKeepAlive > 0) {
          if (maxMissedKeepAlive > 20) maxMissedKeepAlive = 20;
          maxKeepAliveTO = ((keepAliveTime * maxMissedKeepAlive) + 1 ) * uS_TO_S_FACTOR;
        }
        else maxKeepAliveTO = 0;
        xSemaphoreGive(shmSem);
      }
    }
    #endif
  }
  else if (strncmp (packet, "PFT", 3) == 0 && strlen(packet) > 7) { // Update fast clock
    for (uint8_t n=3; n<strlen(packet); n++) if (packet[n]=='<') packet[n]='\0';
    if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      fc_time = util_str2int(&packet[3]);
      xSemaphoreGive(fastClockSem);
    }
  }
  else if (packet[0] == 'H') { // Info and alert messages
    if (packet[1] == 'M' || packet[1] == 'm') {
      if (strlen (&packet[2]) > sizeof(lastMessage)-1) strncpy (lastMessage, &packet[2], sizeof(lastMessage)-1);
      strcpy (lastMessage, &packet[2]);
      lastMessage[sizeof(lastMessage)-1] = '\0';
    }
    else if (packet[1] == 'T' || packet[1] == 't') {
      if (packet[1] == 'T') {
        if (strlen (&packet[2]) > sizeof(remServerType)-1) strncpy (remServerType, &packet[2], sizeof(remServerType)-1);
        strcpy (remServerType, &packet[2]);
        remServerType[sizeof(remServerType)-1] = '\0';
      }
      else {
        if (strlen (&packet[2]) > sizeof(remServerDesc)-1) strncpy (remServerDesc, &packet[2], sizeof(remServerDesc)-1);
        strcpy (remServerDesc, &packet[2]);
        remServerType[sizeof(remServerDesc)-1] = '\0';        
      }
    }
  }
  // Now deal with sub fields and more complex data
  else {
    // tokenise the data
    int tokenPtr = 0;
    int subTokenPtr = 0;
    uint8_t subTokenCnt = 0;
    int tgtLength = strlen(packet);
    void *memBlock;
    char *subToken[MAXSUBTOKEN];
    char *workingToken;             // for easier use and lower dereferencing overhead
    char *tptr = NULL;
    char tType;
    
    for (tokenTally=0; tokenTally<WITHROTMAXFIELDS && tokenPtr<tgtLength; tokenPtr++) {
      if (packet[tokenPtr] == ']' && packet[tokenPtr+1] == '\\' && packet[tokenPtr+2] =='[') {
        // token delimiter reached
        packet[tokenPtr] = '\0'; // null terminate field
        tokenPtr+=2;
        majToken[tokenTally++] = &(packet[tokenPtr+1]);
      }
    }
    // if (tokenTally == 0) return;  // not all responses have major parts
    // parse the data type
    if (packet[0] == 'M' && (packet[2] == 'A' || packet[2] == '+' || packet[2] == '-' || packet[2] == 'S')) { // Multithrottle Action, Add, Remove and Steal
      // Status update
      char *tptr;
      int locoNumber = 0;
      int maxLocoArray = locomotiveCount + MAXCONSISTSIZE;
      uint8_t throtId = packet[1];
      bool found = false;

      if (packet[3] == '*') locoNumber = 0;
      else {
        tType = packet[3];
        locoNumber = strtol (&packet[4], &tptr, 10);
      }
      // we may have sub tokens, but just expect a single field
      for (int n=0; n<tgtLength; n++) {
        if (packet[n] == '<' && packet[n+1] == ';') packet[n] = '\0';
        else if (packet[n] == '>' && packet[n-1] == ';') tptr = &(packet[n+1]);
      }
      // now determine the action type
      if (packet[2] == '+') {  // Add locomotive
        for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
          if ((locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type) || (locoNumber == 0 && throtId == locoRoster[ptr].throttleNr)) {
            const char inChar = 'S';
            locoRoster[ptr].owned = true;
            locoRoster[ptr].steal = 'N';
            xQueueSend (locoUpdateQueue, &inChar, 0);
          }
        }        
      }
      else if (packet[2] == '-') {  // Remove locomotive
        for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
          if ((locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type) || (locoNumber == 0 && throtId == locoRoster[ptr].throttleNr)) {
            const char inChar = 'S';
            locoRoster[ptr].owned = false;
            locoRoster[ptr].reverseConsist = false;
            xQueueSend (locoUpdateQueue, &inChar, 0);
          }
        }        
      }
      else if (packet[2] == 'S') {  // Steal locomotive
        for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
          if ((locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type) || (locoNumber == 0 && throtId == locoRoster[ptr].throttleNr)) {
            locoRoster[ptr].steal = 'Y';
          }
        }        
      }
      else if (tptr != NULL) {
        if (tptr[0] == 'F') {  //Function Status
          int funcNumber = strtol (&tptr[2], &workingToken, 10);
          uint32_t mask = 0x01;
        
          mask <<= funcNumber;
          if (tptr[1] != '1') {
            mask = ~mask;
          }
          for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
            if ((locoNumber == 0 && throtId == locoRoster[ptr].throttleNr) || (locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type)) {
              if (locoNumber != 0) found = true;
              if (xSemaphoreTake(functionSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                const char inChar = 'F';
                if (tptr[1] == '1') locoRoster[ptr].function = locoRoster[ptr].function | mask;
                else locoRoster[ptr].function = locoRoster[ptr].function & mask;
                xSemaphoreGive(functionSem);
                xQueueSend (locoUpdateQueue, &inChar, 0);
              }
              else semFailed ("functionSem", __FILE__, __LINE__);
            }
          }
          funcChange = true;
        }
        //
        // Velocity change
        //
        else if (tptr[0] == 'V') {  // Velocity Update
          int velocity = strtol (&tptr[1], &workingToken, 10);
          if (velocity>127 || velocity < -1) velocity = 0;
          for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
            if ((locoNumber == 0 && throtId == locoRoster[ptr].throttleNr) || (locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type)) {
              if (locoNumber != 0) found = true;
              if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                const char inChar = 'S';
                locoRoster[ptr].speed = velocity;
                xSemaphoreGive(velociSem);
                xQueueSend (locoUpdateQueue, &inChar, 0);
              }
              else semFailed ("velociSem", __FILE__, __LINE__);
            }
          }
        }
        //
        // Direction (Reverse?true)
        //
        else if (tptr[0] == 'R') {  // Direction / Reverse
          for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
            if ((locoNumber == 0 && throtId == locoRoster[ptr].throttleNr) || (locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type)) {
              if (locoNumber != 0) found = true;
              if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                const char inChar = 'S';
                if (tptr[1] == '0') locoRoster[ptr].direction = REVERSE;
                else locoRoster[ptr].direction = FORWARD;
                xSemaphoreGive(velociSem);
                xQueueSend (locoUpdateQueue, &inChar, 0);
              }
              else semFailed ("velociSem", __FILE__, __LINE__);
            }
          }
        }
        //
        // Speed Step
        //
        else if (tptr[0] == 's') {  // Speed step
          // Speed step mode sn where 'n' is '1'=128step, '2'=28step, '4'=27step or '8'=14step
          uint8_t setSteps = 128;
          for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
            if ((locoNumber == 0 && throtId == locoRoster[ptr].throttleNr) || (locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type)) {
              if (locoNumber != 0) found = true;
              setSteps = tptr[1] - '0';
              switch (setSteps) {
                case 1:
                  setSteps = 128;
                  break;
                case 2:
                  setSteps = 28;
                  break;
                case 4:
                  setSteps = 27;
                  break;
                case 8:
                  setSteps = 14;
                  break;
                default:
                  setSteps = 128;
              }
              locoRoster[ptr].steps = setSteps;
            }
          }
        }
      }
    }
    //
    // Roster list
    //
    else if (strncmp (packet, "RL", 2) == 0 && (locomotiveCount == 0 || drivingLoco == false)) {
      memBlock = malloc (sizeof(struct locomotive_s) * (tokenTally + MAXCONSISTSIZE));
      struct locomotive_s *locoData = (struct locomotive_s*) memBlock;
      locomotiveCount = 0;
      for (tokenPtr=0; tokenPtr<tokenTally; tokenPtr++) {
        workingToken = majToken[tokenPtr];
        tgtLength = strlen(workingToken);
        for (subTokenPtr=0, subTokenCnt=0; subTokenPtr<tgtLength && subTokenCnt<MAXSUBTOKEN; subTokenPtr++) {
          if ((workingToken[subTokenPtr] == '}' && workingToken[subTokenPtr+1] == '|' && workingToken[subTokenPtr+2] == '{') ||
              (workingToken[subTokenPtr] == '<' && workingToken[subTokenPtr+1] == ';' && workingToken[subTokenPtr+2] == '>')) {
            workingToken[subTokenPtr] = '\0';
            subTokenPtr+=2;
            subToken[subTokenCnt++] = &(workingToken[subTokenPtr+1]);
          }
        }
        if (strlen(workingToken) > (NAMELENGTH-1)) workingToken[NAMELENGTH-1] = '\0'; // Allow up to 32 chars in a name
        strcpy (locoData[tokenPtr].name, workingToken);  // Name is in first part - pointed by by workingToken
        locoData[tokenPtr].type           = subToken[1][0];
        locoData[tokenPtr].direction      = STOP;
        locoData[tokenPtr].speed          = 0;
        locoData[tokenPtr].steps          = 128;
        locoData[tokenPtr].id             = (int) strtol ((const char*)subToken[0], &tptr, 10);
        locoData[tokenPtr].owned          = false;
        locoData[tokenPtr].function       = 0;
        locoData[tokenPtr].throttleNr     = 255;
        locoData[tokenPtr].relayIdx       = 255;
        locoData[tokenPtr].reverseConsist = false;
        locoData[tokenPtr].functionLatch  = 65535;  // should not use in JMRI, but set it to all on => use default.
        locoData[tokenPtr].functionString = NULL;
      }
      uint8_t maxLoco = tokenTally + MAXCONSISTSIZE;
      uint8_t n = 0;
      for (;tokenPtr < maxLoco; tokenPtr++) {
        sprintf (locoData[tokenPtr].name, "ad-hoc-%d", ++n);
        locoData[tokenPtr].owned          = false;
        locoData[tokenPtr].relayIdx       = 255;
        locoData[tokenPtr].reverseConsist = false;
        locoData[tokenPtr].functionLatch  = 65535;  // should not use in JMRI, but set it to all on => use default.
        locoData[tokenPtr].functionString = NULL;
      }
      if (locoRoster != NULL) free(locoRoster);   // free space if we had previously allocated it
      locoRoster      = locoData;
      locomotiveCount = tokenTally;
      if (tokenTally>1 && nvs_get_int("sortData", SORTDATA) == 1) sortLoco();
    }
    //
    // Turnout states
    // eg PTT]\[Turnouts}|{Turnout]\[Closed}|{2]\[Thrown}|{4]\[Unknown}|{1]\[Failed}|{8
    //
    else if (strcmp (packet, "PTT") == 0) {
      memBlock = malloc (sizeof(struct turnoutState_s) * (tokenTally - 1));
      struct turnoutState_s *turnoutStateData = (struct turnoutState_s*) memBlock;
      turnoutStateCount = 0;
      for (tokenPtr=1; tokenPtr<tokenTally; tokenPtr++) {
        workingToken = majToken[tokenPtr];
        tgtLength = strlen(workingToken);
        for (subTokenPtr=0, subTokenCnt=0; subTokenPtr<tgtLength && subTokenCnt<MAXSUBTOKEN; subTokenPtr++) {
          if (workingToken[subTokenPtr] == '}' && workingToken[subTokenPtr+1] == '|' && workingToken[subTokenPtr+2] == '{') {
            workingToken[subTokenPtr] = '\0';
            subTokenPtr+=2;
            subToken[subTokenCnt++] = &(workingToken[subTokenPtr+1]);
          }
        }
        if (strlen(workingToken) > (NAMELENGTH-1)) workingToken[NAMELENGTH-1] = '\0';   // Allow up to 32 chars in a name
        strcpy (turnoutStateData[tokenPtr-1].name, workingToken);
        turnoutStateData[tokenPtr-1].state = subToken[0][0] - '0'; // Assume single digit, and nor white space
      }
      if (turnoutState != NULL) free(turnoutState);
      turnoutState      = turnoutStateData;
      turnoutStateCount = tokenTally - 1;
    }
    //
    // turnout list
    //
    else if (strcmp (packet, "PTL") == 0) {
      memBlock = malloc (sizeof(struct turnout_s) * tokenTally);
      struct turnout_s *turnoutData = (struct turnout_s*) memBlock;
      turnoutCount = 0;
      for (tokenPtr=0; tokenPtr<tokenTally; tokenPtr++) {
        workingToken = majToken[tokenPtr];
        tgtLength = strlen(workingToken);
        for (subTokenPtr=0, subTokenCnt=0; subTokenPtr<tgtLength && subTokenCnt<MAXSUBTOKEN; subTokenPtr++) {
          if (workingToken[subTokenPtr] == '}' && workingToken[subTokenPtr+1] == '|' && workingToken[subTokenPtr+2] == '{') {
            workingToken[subTokenPtr] = '\0';
            subTokenPtr+=2;
            subToken[subTokenCnt++] = &(workingToken[subTokenPtr+1]);
          }
        }
        if (strlen(workingToken) > (NAMELENGTH-1)) workingToken[NAMELENGTH-1] = '\0';
        strcpy (turnoutData[tokenPtr].sysName, workingToken);
        if (strlen(subToken[0]) > (NAMELENGTH-1)) subToken[0][NAMELENGTH-1] = '\0'; // Allow up to 32 chars in a name
        strcpy (turnoutData[tokenPtr].userName, subToken[0]);
        turnoutData[tokenPtr].state = subToken[1][0] - '0'; // Assume single digit, and nor white space
      }
      if (turnoutList != NULL) free(turnoutList);
      turnoutList  = turnoutData;
      turnoutCount = tokenTally;
      if (tokenTally>1 && nvs_get_int("sortData", SORTDATA) == 1) sortTurnout();
    }
    //
    // Route states, very similar structure to turnout states
    //
    else if (strcmp (packet, "PRT") == 0) {
      memBlock = malloc (sizeof(struct routeState_s) * (tokenTally - 1));
      struct routeState_s *routeStateData = (struct routeState_s*) memBlock;
      routeStateCount = 0;
      for (tokenPtr=1; tokenPtr<tokenTally; tokenPtr++) {
        workingToken = majToken[tokenPtr];
        tgtLength = strlen(workingToken);
        for (subTokenPtr=0, subTokenCnt=0; subTokenPtr<tgtLength && subTokenCnt<MAXSUBTOKEN; subTokenPtr++) {
          if (workingToken[subTokenPtr] == '}' && workingToken[subTokenPtr+1] == '|' && workingToken[subTokenPtr+2] == '{') {
            workingToken[subTokenPtr] = '\0';
            subTokenPtr+=2;
            subToken[subTokenCnt++] = &(workingToken[subTokenPtr+1]);
          }
        }
        if (strlen(workingToken) > (NAMELENGTH-1)) workingToken[NAMELENGTH-1] = '\0';   // Allow up to 32 chars in a name
        strcpy (routeStateData[tokenPtr-1].name, workingToken);
        routeStateData[tokenPtr-1].state = subToken[0][0]; // Assume single digit, and nor white space
      }
      if (routeState != NULL) free(routeState);
      routeState      = routeStateData;
      routeStateCount = tokenTally - 1;
    }
    //
    // Route List
    //
    else if (strcmp (packet, "PRL") == 0) {
      memBlock = malloc (sizeof(struct route_s) * tokenTally);
      struct route_s *routeData = (struct route_s*) memBlock;
      routeCount = 0;
      for (tokenPtr=0; tokenPtr<tokenTally; tokenPtr++) {
        workingToken = majToken[tokenPtr];
        tgtLength = strlen(workingToken);
        for (subTokenPtr=0, subTokenCnt=0; subTokenPtr<tgtLength && subTokenCnt<MAXSUBTOKEN; subTokenPtr++) {
          if (workingToken[subTokenPtr] == '}' && workingToken[subTokenPtr+1] == '|' && workingToken[subTokenPtr+2] == '{') {
            workingToken[subTokenPtr] = '\0';
            subTokenPtr+=2;
            subToken[subTokenCnt++] = &(workingToken[subTokenPtr+1]);
          }
        }
        if (strlen(workingToken) > (NAMELENGTH-1)) workingToken[NAMELENGTH-1] = '\0';
        strcpy (routeData[tokenPtr].sysName, workingToken);
        if (strlen(subToken[0]) > (NAMELENGTH-1)) subToken[0][NAMELENGTH-1] = '\0'; // Allow up to 32 chars in a name
        strcpy (routeData[tokenPtr].userName, subToken[0]);
        routeData[tokenPtr].state = subToken[1][0]; // Assume single digit, and nor white space
      }
      if (routeList != NULL) free(routeList);
      routeList  = routeData;
      routeCount = tokenTally;
      if (tokenTally>1 && nvs_get_int("sortData", SORTDATA) == 1) sortRoute();
    }
  }
}
