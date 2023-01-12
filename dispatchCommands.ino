/*
 * Send initial data after protocol detection
 */
void setInitialData()
{
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setInitialData()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }

  if (!initialDataSent) {
    initialDataSent = true;  // Only send once per connection
    if (cmdProtocol == WITHROT) {
      uint64_t chipid = ESP.getEfuseMac();
      char myID[15];

      // Send identifying data
      sprintf (myID, "HU%02X%02X%02X%02X%02X%02X",(uint8_t)chipid, (uint8_t)(chipid>>8), (uint8_t)(chipid>>16), (uint8_t)(chipid>>24), (uint8_t)(chipid>>32), (uint8_t)(chipid>>40));
      txPacket (myID);
      txPacket ("N", tname);
    }
    else if (cmdProtocol == DCCEX) {
      uint8_t reqState;

      while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
      txPacket ("<s>");
      if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
        // wait for ack
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Warning: No response for initial info (DCC-Ex Status)\r\n", getTimeStamp());
          xSemaphoreGive(displaySem);
        }
      }
    }
  }
}

/*
 * Set track power state
 * 1 = On
 * 2 = Off
 * Other = no action
 */
void setTrackPower (uint8_t desiredState)
{
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setTrackPower(%d)\r\n", getTimeStamp(), desiredState);
    xSemaphoreGive(displaySem);
  }

  switch (desiredState) {
    case 1:
      if (cmdProtocol == WITHROT) txPacket ("PPA1");
      else if (cmdProtocol == DCCEX) {
        uint8_t reqState;
        
        while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
        // txPacket ("<1>");
        if (dccPowerFunc == BOTH) txPacket ("<1>");
        else if (dccPowerFunc == MAINONLY) txPacket ("<1 MAIN>");
        else if (dccPowerFunc == JOIN) txPacket ("<1 JOIN>");
        else txPacket ("<1 PROG>");
        if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
          // wait for ack
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Warning: No response for track power on\r\n", getTimeStamp());
            xSemaphoreGive(displaySem);
          }
        }
      }
      break;
    case 2:
      if (cmdProtocol == WITHROT) txPacket ("PPA0");
      else if (cmdProtocol == DCCEX) {
        uint8_t reqState;
        
        while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
        // txPacket ("<0>");
        if (dccPowerFunc == BOTH || dccPowerFunc == JOIN) txPacket ("<0>");
        else if (dccPowerFunc == MAINONLY) txPacket ("<0 MAIN>");
        else txPacket ("<0 PROG>");
        if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
          // wait for ack
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Warning: No response for track power off\r\n", getTimeStamp());
            xSemaphoreGive(displaySem);
          }
        }
      }
      break;
  }
}

bool setTurnout (uint8_t turnoutNr, const char desiredState)
{
  char outPacket[21];
  //char string[6];
  uint8_t reqState;
  bool retVal = true;
 
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setTurnout(%d, %d)\r\n", getTimeStamp(), turnoutNr, (int8_t) desiredState);
    xSemaphoreGive(displaySem);
  }

  if (turnoutNr>=turnoutCount) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Requested turnout %d is out of range, %d turnouts known\r\n", getTimeStamp(), turnoutNr, turnoutCount);
      xSemaphoreGive(displaySem);
    }
    return (false);
  }
  if (desiredState<turnoutStateCount) reqState = turnoutState[desiredState].state;
  if (cmdProtocol == WITHROT) {
    strcpy (outPacket, "PTA");
    if (reqState==2 || reqState=='C') outPacket[3] = 'C';
    else if (reqState==4 || reqState=='T') outPacket[3] = 'T';
    else outPacket[3] = '2';
    outPacket[4] = '\0';
    if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      strcat (outPacket, turnoutList[turnoutNr].sysName);
      xSemaphoreGive(turnoutSem);
      txPacket (outPacket);
    }
    else {
      retVal = false;
      semFailed ("turnoutSem", __FILE__, __LINE__);
    }
  }
  else if (cmdProtocol == DCCEX) {
    while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS) {} // clear ack Queue
    if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (desiredState == 'T' || desiredState == 'C') {
        sprintf (outPacket, "<T %s %c>", turnoutList[turnoutNr].sysName, desiredState);
      }
      else {
        sprintf (outPacket, "<T %s %c>", turnoutList[turnoutNr].sysName, turnoutState[desiredState].state);
      }
      xSemaphoreGive(turnoutSem);
      while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
      txPacket (outPacket);
      if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
        // wait for ack
        retVal = false;
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          turnoutList[turnoutNr].state = '8';
          Serial.printf ("%s Warning: No response for setting Turnout\r\n", getTimeStamp());
          xSemaphoreGive(displaySem);
          #ifdef RELAYPORT
          sprintf (outPacket, "PTA8%s\r\n", turnoutList[turnoutNr].sysName);
          relay2WiThrot (outPacket);
          #endif
        }
      }
    }
    else {
      retVal = false;
      semFailed ("turnoutSem", __FILE__, __LINE__);
    }
  }
  return (retVal);
}

void setRoute (uint8_t routeNr)
{
  char outPacket[BUFFSIZE];
  char message[40];
  #ifdef RELAYPORT
  char     relayMsg[40];
  #endif
  
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s sendRoute(%d)\r\n", getTimeStamp(), routeNr);
    xSemaphoreGive(displaySem);
  }

  if (routeNr>=routeCount) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Requested route %d is out of range, %d routes known\r\n", getTimeStamp(), routeNr, routeCount);
      xSemaphoreGive(displaySem);
    }
    return;
  }
  if (cmdProtocol == WITHROT) {
    if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (outPacket, "PRA2%s", routeList[routeNr].sysName);
      xSemaphoreGive(routeSem);
      txPacket (outPacket);
    }
  }
  else if (cmdProtocol == DCCEX) {
    routeInitiate (routeNr);
    routeSetup (routeNr, true);
    routeConfirm (routeNr);
  }
}


bool invalidLocoIndex (const uint8_t locoIndex, const char *description)
{
  if (locoIndex>=locomotiveCount+MAXCONSISTSIZE) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s %s: Requested locomotive %d is out of range, %d locomotives known\r\n", getTimeStamp(), description, locoIndex, locomotiveCount);
      xSemaphoreGive(displaySem);
    }
    return (true);
  }
  return (false);
}

void setLocoFunction (uint8_t locoIndex, uint8_t funcIndex, bool set)
{
  char commandPacket[40];
  
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (set) Serial.printf ("%s setLocoFunction(%d, %d, TRUE)\r\n",  getTimeStamp(), locoIndex, funcIndex);
    else     Serial.printf ("%s setLocoFunction(%d, %d, FALSE)\r\n", getTimeStamp(), locoIndex, funcIndex);
    xSemaphoreGive(displaySem);
  }

  if (invalidLocoIndex(locoIndex, "Set loco function")) return;
  if (cmdProtocol == WITHROT) {
    char setVal = '0';
    if (set) setVal = '1';
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (commandPacket, "M%cA%c%d<;>F%c%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, setVal, funcIndex);
      xSemaphoreGive(velociSem);
      txPacket (commandPacket);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
  else if (cmdProtocol == DCCEX) {
    uint32_t mask;
    uint32_t latchVal;
    uint32_t function;
    uint16_t loco;
    uint8_t reqState;
    char setVal = 0;
    char varName[16];

    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      loco = locoRoster[locoIndex].id;     // get the loco function specific values
      function = locoRoster[locoIndex].function;
      xSemaphoreGive(velociSem);
      mask = 1 << funcIndex;               // determine if this is a latching function or not
      sprintf (varName, "FLatch%d", loco);
      latchVal = nvs_get_int (varName, defaultLatchVal) & mask;
      if (set) setVal = 1;                 // set or unset
      if (latchVal == 0 || setVal ==1) {   // Tx if non-latching or function-on toggles state
        if (latchVal>0) {
          if ((function & mask) > 0) setVal = 0; //is latching and is already on
        }
        sprintf (commandPacket, "<F %d %d %d>\r\n<t %d>", loco, funcIndex, setVal, loco);
        while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
        txPacket (commandPacket);
        if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            if (setVal==0) locoRoster[locoIndex].function = locoRoster[locoIndex].function & (~mask);
            else locoRoster[locoIndex].function = locoRoster[locoIndex].function | mask;
            xSemaphoreGive(velociSem);
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
          // wait for ack
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Warning: No response for setting function\r\n", getTimeStamp());
            xSemaphoreGive(displaySem);
          }
        }
      }
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
}

void setLocoOwnership(uint8_t locoIndex, bool owned)
{
  char commandPacket[40];
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (owned) Serial.printf ("%s setLocoOwnership(%d, TRUE)\r\n",  getTimeStamp(), locoIndex);
    else       Serial.printf ("%s setLocoOwnership(%d, FALSE)\r\n", getTimeStamp(), locoIndex);
    xSemaphoreGive(displaySem);
  }

  if (invalidLocoIndex(locoIndex, "Set loco ownership")) return;
  if (cmdProtocol == WITHROT) {
    char setVal = '-';

    if (owned) setVal = '+';
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (commandPacket, "M%c%c%c%d<;>%s", locoRoster[locoIndex].throttleNr, setVal, locoRoster[locoIndex].type, locoRoster[locoIndex].id, locoRoster[locoIndex].name);
      xSemaphoreGive(velociSem);
      txPacket (commandPacket);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }  
}

void setStealLoco(uint8_t locoIndex)
{
  char commandPacket[40];
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setStealLoco(%d)\r\n", getTimeStamp(), locoIndex);
    xSemaphoreGive(displaySem);
  }

  if (invalidLocoIndex(locoIndex, "Steal loco")) return;
  if (cmdProtocol == WITHROT) {
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (commandPacket, "M%cS%c%d<;>%s", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, locoRoster[locoIndex].name);
      xSemaphoreGive(velociSem);
      txPacket (commandPacket);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }  
}


void setLocoSpeed (uint8_t locoIndex, int16_t speed, int8_t direction)
{
  char commandPacket[40];
  uint8_t reqState;
  
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setLocoSpeed(%d, %d, %d)\r\n", getTimeStamp(), locoIndex, speed, direction);
    xSemaphoreGive(displaySem);
  }

  if (invalidLocoIndex(locoIndex, "Set loco speed")) return;
  if (cmdProtocol == WITHROT) {
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (commandPacket, "M%cA%c%d<;>V%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, speed);
      xSemaphoreGive(velociSem);
      txPacket (commandPacket);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
  else if (cmdProtocol == DCCEX) {
    uint8_t tdir = 0;
    if (direction == FORWARD) tdir = 1;
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (commandPacket, "<t 1 %d %d %d>", locoRoster[locoIndex].id, speed, tdir);
      xSemaphoreGive(velociSem);
      while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
      xQueueSend (dccLocoRefQueue, &locoIndex, 0);
      txPacket (commandPacket);
      if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
        // wait for ack
        // if not Ack'ed then remove loco from queue
        xQueueReceive(dccLocoRefQueue, &reqState, 0);
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Warning: No response for setting loco speed\r\n", getTimeStamp());
          xSemaphoreGive(displaySem);
        }
      }
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
}


void setLocoDirection (uint8_t locoIndex, uint8_t direction)
{
  char commandPacket[40];
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setLocoDirection(%d, %d)\r\n", getTimeStamp(), locoIndex, direction);
    xSemaphoreGive(displaySem);
  }

  if (invalidLocoIndex(locoIndex, "Set loco direction")) return;
  if (cmdProtocol == WITHROT) {
    if (direction != STOP) {
      int dirFlag = 1;

      if (direction == REVERSE) dirFlag = 0;
      if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        sprintf (commandPacket, "M%cA%c%d<;>R%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, dirFlag);
        xSemaphoreGive(velociSem);
        txPacket (commandPacket);
      }
      else semFailed ("velociSem", __FILE__, __LINE__);
    }
  }
}


void setDisconnected()
{
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setDisconnected()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }

  if (cmdProtocol == WITHROT) {
    txPacket ("Q"); // quit
  }
}


void getAddress()
{
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s getAddress()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }

  if (cmdProtocol == DCCEX) {
    txPacket ("<R>");
  }
}

void getCV(int16_t cv)
{
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s getCV(%d)\r\n", getTimeStamp(), cv);
    xSemaphoreGive(displaySem);
  }

  if (cmdProtocol == DCCEX) {
    char packette[32];
    sprintf (packette, "<R %d %d %d>", cv, CALLBACKNUM, CALLBACKSUB);
    txPacket (packette);
  }  
}


/*
 * Set up up route as a background task
 */
void setRouteBg (void *pvParameters)
{
  uint8_t  routeNr = ((uint8_t*) pvParameters)[0];

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setRouteBg(%d)\r\n", getTimeStamp(), routeNr);
    xSemaphoreGive(displaySem);
  }

  if (routeNr < routeCount) {    
    if (cmdProtocol == DCCEX) {
      routeInitiate (routeNr);
      routeSetup (routeNr, false);
      routeConfirm (routeNr);
    }
    else {
      if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        char route[25];
        routeList[routeNr].state = '8';
        printf (route, "PRA2%s\r\n", routeList[routeNr].sysName);
        xSemaphoreGive(routeSem);
        txPacket (route);
      }
      else semFailed ("routeSem", __FILE__, __LINE__);
    }
  }
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setRouteBg(%d) called with out of range route, max = %d\r\n", getTimeStamp(), routeNr, routeCount);
    xSemaphoreGive(displaySem);
  }
  vTaskDelete( NULL );
}


/*
 * set through the route definition and set turnouts to desired state
 */
bool routeSetup (int8_t routeNr, bool displayable)
{
  uint16_t pause = routeDelay[nvs_get_int("routeDelay", 2)];
  #ifdef RELAYPORT
  char relayMsg[25];
  #endif
  #ifndef NODISPLAY
  char message[40];
  #endif
  bool retVal = true;

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    char *displayState = {"false"};
    if (displayable) displayState = "true";
    Serial.printf ("%s routeSetup(%d, %s)\r\n", getTimeStamp(), routeNr, displayState);
    xSemaphoreGive(displaySem);
  }
  for (uint8_t n=0; n<MAXRTSTEPS && routeList[routeNr].turnoutNr[n] < 255; n++) {
    if (routeList[routeNr].turnoutNr[n] < 200 && routeList[routeNr].desiredSt[n] < 200) {
      #ifndef NODISPLAY
      if (displayable) {
        if (routeList[routeNr].desiredSt[n] == 'C') strcpy (message, "Close ");
        else strcpy (message, "Throw ");
        strcat (message, turnoutList[routeList[routeNr].turnoutNr[n]].userName);
        displayTempMessage (NULL, message, false);
      }
      #endif
      if (!setTurnout (routeList[routeNr].turnoutNr[n], routeList[routeNr].desiredSt[n])) {
        retVal = false;
        if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (routeList[routeNr].state != '4') {
            routeList[routeNr].state = '4';
            #ifdef RELAYPORT
            sprintf (relayMsg, "PRA4%s\r\n", routeList[routeNr].sysName);
            #endif
          }
          #ifdef RELAYPORT
          else relayMsg[0] = '\0';
          #endif
          xSemaphoreGive(routeSem);
          #ifdef RELAYPORT
          if (relayMsg[0] != '\0') relay2WiThrot (relayMsg);
          #endif
        }
        else semFailed ("routeSem", __FILE__, __LINE__);
      }
    }
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Error in route %s step %d.\r\n", getTimeStamp(), routeList[routeNr].userName, (n+1));
        xSemaphoreGive(displaySem);
      }
    }
    if (pause>0) {
      delay (pause);
    }
  }
  return (retVal);
}


/*
 * Mark a route as inconsistent prior to setup, ie not fully set - yet.
 */
void routeInitiate (int8_t routeNr)
{
  #ifdef RELAYPORT
  char     relayMsg[25];
  #endif

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s routeInitiate(%d)\r\n", getTimeStamp(), routeNr);
    xSemaphoreGive(displaySem);
  }
  if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    routeList[routeNr].state = '8';
    xSemaphoreGive(routeSem);
    #ifdef RELAYPORT
    sprintf (relayMsg, "PRA8%s\r\n", routeList[routeNr].sysName);
    relay2WiThrot (relayMsg);
    #endif
  }
  else semFailed ("routeSem", __FILE__, __LINE__);
}


/*
 * if a route remains inconsistent after setup it has not been invalidated, and in now active
 */
void routeConfirm (int8_t routeNr)
{
  #ifdef RELAYPORT
  char relayMsg[25];
  #endif

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s routeConfirm(%d)\r\n", getTimeStamp(), routeNr);
    xSemaphoreGive(displaySem);
  }
  if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (routeList[routeNr].state == '8') routeList[routeNr].state = '2';
    xSemaphoreGive(routeSem);
    #ifdef RELAYPORT
    if (routeList[routeNr].state == '2') {
      sprintf (relayMsg, "PRA2%s\r\n", routeList[routeNr].sysName);
      relay2WiThrot (relayMsg);
    }
    #endif
  }
  else semFailed ("routeSem", __FILE__, __LINE__);
}


/*
 * check if a turnout change invalidates a route
 */
void invalidateRoutes (int8_t turnoutNr, char state)
{
  bool active;
  #ifdef RELAYPORT
  char relayMsg[25];
  #endif

  for (uint8_t n=0; n<turnoutCount; n++) {
    if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (routeList[n].state == '8' || routeList[n].state == '2') active = true;
      else active = false;
      xSemaphoreGive(routeSem);
      if (active) {
        for (uint8_t i=0; i<MAXRTSTEPS && routeList[n].turnoutNr[i] != 255; i++) {
          if (routeList[n].turnoutNr[i] == turnoutNr && routeList[n].desiredSt[i] != state) {
            if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              routeList[n].state = '4';
              #ifdef RELAYPORT
              sprintf (relayMsg, "PRA4%s\r\n", routeList[n].sysName);
              #endif
              xSemaphoreGive(routeSem);
              #ifdef RELAYPORT
              relay2WiThrot (relayMsg);
              #endif
            }
            i = MAXRTSTEPS;
          }
        }
      }
    }
    else semFailed ("routeSem", __FILE__, __LINE__);
  }
}
