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
    if (cmdProtocol == JMRI) {
      uint64_t chipid = ESP.getEfuseMac();
      char myID[15];

      // Send identifying data
      sprintf (myID, "HU%02X%02X%02X%02X%02X%02X",(uint8_t)chipid, (uint8_t)(chipid>>8), (uint8_t)(chipid>>16), (uint8_t)(chipid>>24), (uint8_t)(chipid>>32), (uint8_t)(chipid>>40));
      txPacket (myID);
      txPacket ("N", tname);
    }
    else if (cmdProtocol == DCCPLUS) {
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
      if (cmdProtocol == JMRI) txPacket ("PPA1");
      else if (cmdProtocol == DCCPLUS) {
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
      if (cmdProtocol == JMRI) txPacket ("PPA0");
      else if (cmdProtocol == DCCPLUS) {
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

void setTurnout (uint8_t turnoutNr, const char desiredState)
{
  char outPacket[21];
  //char string[6];
  uint8_t reqState;
 
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s setTurnout(%d, %d)\r\n", getTimeStamp(), turnoutNr, (int8_t) desiredState);
    xSemaphoreGive(displaySem);
  }

  if (turnoutNr>=turnoutCount) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Requested turnout %d is out of range, %d turnouts known\r\n", getTimeStamp(), turnoutNr, turnoutCount);
      xSemaphoreGive(displaySem);
    }
    return;
  }
  if (desiredState<turnoutStateCount) reqState = turnoutState[desiredState].state;
  if (cmdProtocol == JMRI) {
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
    else semFailed ("turnoutSem", __FILE__, __LINE__);
  }
  else if (cmdProtocol == DCCPLUS) {
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
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          turnoutList[turnoutNr].state = '8';
          Serial.printf ("%s Warning: No response for setting Turnout\r\n", getTimeStamp());
          xSemaphoreGive(displaySem);
        }
      }
    }
    else semFailed ("turnoutSem", __FILE__, __LINE__);
  }
}

void setRoute (uint8_t routeNr)
{
  char outPacket[BUFFSIZE];
  char message[40];
  
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
  if (cmdProtocol == JMRI) {
    sprintf (outPacket, "PRA2%s", routeList[routeNr].sysName);
    txPacket (outPacket);
  }
  else if (cmdProtocol == DCCPLUS) {
    char route[BUFFSIZE];
    uint16_t pause = routeDelay[nvs_get_int("routeDelay", 2)];
    
    nvs_get_string ("route", routeList[routeNr].userName, route, "not found", BUFFSIZE);
    if (strcmp (route, "not found") != 0) {
      int limit = strlen (route);
      int turnoutNr;
      char *start;
      for (int n=0; n<limit; n++) {
        start = route + n;
        while (route[n] != ' ' && n<limit) n++;
        route[n++] = '\0';
        turnoutNr = 1024;
        for (uint8_t j=0; j<turnoutCount && turnoutNr == 1024; j++) {
          if (strcmp (turnoutList[j].userName, start) == 0) turnoutNr = j;
        }
        if (turnoutNr < 1024) {
          if (route[n] == 'C') strcpy (message, "Close ");
          else strcpy (message, "Throw ");
          strcat (message, start);
          #ifndef NODISPLAY
          displayTempMessage (NULL, message, false);
          #endif
          setTurnout (turnoutNr, route[n]);
          if (pause>0) {
            delay (pause);
          }
        }
        else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s route: turnout %s not found\r\n", getTimeStamp(), start);
          xSemaphoreGive(displaySem);
        }
        n++;
      }
    }
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
  if (cmdProtocol == JMRI) {
    char setVal = '0';
    if (set) setVal = '1';
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (commandPacket, "M%cA%c%d<;>F%c%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, setVal, funcIndex);
      xSemaphoreGive(velociSem);
      txPacket (commandPacket);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
  else if (cmdProtocol == DCCPLUS) {
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
        sprintf (commandPacket, "<F %d %d %d>", loco, funcIndex, setVal);
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
  if (cmdProtocol == JMRI) {
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
  if (cmdProtocol == JMRI) {
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
  if (cmdProtocol == JMRI) {
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (commandPacket, "M%cA%c%d<;>V%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, speed);
      xSemaphoreGive(velociSem);
      txPacket (commandPacket);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
  else if (cmdProtocol == DCCPLUS) {
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
  if (cmdProtocol == JMRI) {
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

  if (cmdProtocol == JMRI) {
    txPacket ("Q"); // quit
  }
}


void getAddress()
{
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s getAddress()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }

  if (cmdProtocol == DCCPLUS) {
    txPacket ("<R>");
  }
}

void getCV(int16_t cv)
{
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s getCV(%d)\r\n", getTimeStamp(), cv);
    xSemaphoreGive(displaySem);
  }

  if (cmdProtocol == DCCPLUS) {
    char packette[32];
    sprintf (packette, "<R %d %d %d>", cv, CALLBACKNUM, CALLBACKSUB);
    txPacket (packette);
  }  
}
