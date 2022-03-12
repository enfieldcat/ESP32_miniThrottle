
// Keep a large statically allocated token table for major tokens
static char *majToken[JMRIMAXFIELDS];
static uint8_t tokenTally;


void processJmriPacket (char *packet)
{
  if (strncmp (packet, "PPA", 3) == 0) { // Track power
    // Power status
    dccPowerChange(packet[3]);
  }
  else if (strncmp (packet, "PTA", 3) == 0) { // Change of turnout
    uint8_t state = packet[3];
    uint8_t ptr = 0;
    bool found = false;

     // only update if in known state
    if (state == '2' || state == '4') {
      // first attempt to find by name
      for (ptr=0; ptr<switchCount && !found; ptr++) {
        if (strcmp (turnoutList[ptr].sysName, &packet[4]) == 0) found = true;
      }
      if (!found) {
        //char *tptr;
        //int devNumber = strtol (&packet[4], &tptr, 10);
        for (ptr=0; ptr<switchCount && !found; ptr++) {
          if (strcmp (turnoutList[ptr].userName, &packet[4])) found = true;
        }
      }
      if (found) {
        if (ptr > 0) ptr--; // the for loop should have incremented ptr past the found spot.
        turnoutList[ptr].state = state - '0';
      }
    }
  }
  else if (packet[0] == '*') {
    // Keep alive
    char *tptr;
    keepAliveTime = strtol ((const char*)&packet[1], &tptr, 10);
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.print   ("Keep Alive interval set to ");
      Serial.print   (keepAliveTime);
      Serial.println (" seconds");
      xSemaphoreGive(displaySem);
    }
  }
  else if (packet[0] == 'H') { // Info and alert messages
    if (packet[1] == 'M' || packet[1] == 'm') {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        if (packet[1] == 'M') Serial.print ("Error: ");
        else Serial.print ("Info: ");
        Serial.println ((char*) &packet[2]);
        xSemaphoreGive(displaySem);
      }
      if (strlen (&packet[2]) > sizeof(lastMessage)-1) strncpy (lastMessage, &packet[2], sizeof(lastMessage)-1);
      strcpy (lastMessage, &packet[2]);
      lastMessage[sizeof(lastMessage)-1] = '\0';
    }
    else if (packet[1] == 'T' || packet[1] == 't') {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        if (packet[1] == 'T') Serial.print ("Identity: ");
        else Serial.print ("Description: ");
        Serial.println ((char*) &packet[2]);
        xSemaphoreGive(displaySem);
      }
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
    
    for (tokenTally=0; tokenTally<JMRIMAXFIELDS && tokenPtr<tgtLength; tokenPtr++) {
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
            locoRoster[ptr].owned = true;
            locoRoster[ptr].steal = 'N';
          }
        }        
      }
      else if (packet[2] == '-') {  // Remove locomotive
        for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
          if ((locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type) || (locoNumber == 0 && throtId == locoRoster[ptr].throttleNr)) {
            locoRoster[ptr].owned = false;
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
              if (tptr[1] == '1') locoRoster[ptr].function = locoRoster[ptr].function | mask;
              else locoRoster[ptr].function = locoRoster[ptr].function & mask;
            }
          }
          funcChange = true;
        }
        //
        // Velocity change
        //
        else if (tptr[0] == 'V') {  // Velocity Update
          int velocity = strtol (&tptr[1], &workingToken, 10);
          for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
            if ((locoNumber == 0 && throtId == locoRoster[ptr].throttleNr) || (locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type)) {
              if (locoNumber != 0) found = true;
              locoRoster[ptr].speed = velocity;
            }
           }
        }
        //
        // Direction (Reverse?true) / Rigting
        //
        else if (tptr[0] == 'R') {  // Direction / Reverse
          for (uint8_t ptr=0; ptr<maxLocoArray && !found; ptr++) {
            if ((locoNumber == 0 && throtId == locoRoster[ptr].throttleNr) || (locoNumber == locoRoster[ptr].id && tType == locoRoster[ptr].type)) {
              if (locoNumber != 0) found = true;
              if (tptr[1] == '0') locoRoster[ptr].direction = REVERSE;
              else locoRoster[ptr].direction = FORWARD;
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
      memBlock = malloc ((sizeof(struct locomotive_s) * tokenTally) + (sizeof(struct locomotive_s) * MAXCONSISTSIZE));
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
        if (strlen(workingToken) > (NAMELENGTH-1)) workingToken[NAMELENGTH-1] = '\0'; // Allow up to 16 chars in a name
        strcpy (locoData[tokenPtr].name, workingToken);  // Name is in first part - pointed by by workingToken
        locoData[tokenPtr].type      = subToken[1][0];
        locoData[tokenPtr].direction = STOP;
        locoData[tokenPtr].speed     = 0;
        locoData[tokenPtr].steps     = 128;
        locoData[tokenPtr].id        = (int) strtol ((const char*)subToken[0], &tptr, 10);
        locoData[tokenPtr].owned     = false;
        locoData[tokenPtr].function  = 0;
        locoData[tokenPtr].throttleNr= 255;
      }
      uint8_t maxLoco = tokenTally + MAXCONSISTSIZE;
      uint8_t n = 0;
      for (;tokenPtr < maxLoco; tokenPtr++) {
        sprintf (locoData[tokenPtr].name, "ad-hoc-%d", ++n);
        locoData[tokenPtr].owned = false;
      }
      if (locoRoster != NULL) free(locoRoster);   // free space if we had previously allocated it
      locoRoster      = locoData;
      locomotiveCount = tokenTally;
    }
    //
    // Turnout states
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
        if (strlen(workingToken) > (NAMELENGTH-1)) workingToken[NAMELENGTH-1] = '\0';   // Allow up to 16 chars in a name
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
      struct turnout_s *switchData = (struct turnout_s*) memBlock;
      switchCount = 0;
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
        strcpy (switchData[tokenPtr].sysName, workingToken);
        // switchData[tokenPtr].id = (int) strtol ((const char*)workingToken, &tptr, 10);
        if (strlen(subToken[0]) > (NAMELENGTH-1)) subToken[0][NAMELENGTH-1] = '\0'; // Allow up to 16 chars in a name
        strcpy (switchData[tokenPtr].userName, subToken[0]);
        switchData[tokenPtr].state = subToken[1][0] - '0'; // Assume single digit, and nor white space
      }
      if (turnoutList != NULL) free(turnoutList);
      turnoutList  = switchData;
      switchCount = tokenTally;
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
        if (strlen(workingToken) > (NAMELENGTH-1)) workingToken[NAMELENGTH-1] = '\0';   // Allow up to 16 chars in a name
        strcpy (routeStateData[tokenPtr-1].name, workingToken);
        routeStateData[tokenPtr-1].state = subToken[0][0] - '0'; // Assume single digit, and nor white space
      }
      if (routeState != NULL) free(routeState);
      routeState      = routeStateData;
      routeStateCount = tokenTally - 1;
    }
    //
    // Route List
    //
  }
}
