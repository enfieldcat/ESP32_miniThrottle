/*
 * Send initial data after protocol detection
 */
void setInitialData()
{
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
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          Serial.println ("Warning: No response for initial info");
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
  
  switch (desiredState) {
    case 1:
      if (cmdProtocol == JMRI) txPacket ("PPA1");
      else if (cmdProtocol == DCCPLUS) {
        uint8_t reqState;
        
        while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
        txPacket ("<1>");
        if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
          // wait for ack
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            Serial.println ("Warning: No response for track power on");
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
        txPacket ("<0>");
        if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
          // wait for ack
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            Serial.println ("Warning: No response for track power off");
            xSemaphoreGive(displaySem);
          }
        }
      }
      break;
  }
}

void setTurnout (uint8_t turnoutNr, char desiredState)
{
  char outPacket[21];
  //char string[6];
  uint8_t reqState;
 
  reqState = turnoutState[desiredState].state;
  if (cmdProtocol == JMRI) {
    strcpy (outPacket, "PTA");
    if (reqState == 2) outPacket[3] = 'C';
    else if (reqState == 4) outPacket[3] = 'T';
    else outPacket[3] = '2';
    outPacket[4] = '\0';
    strcat (outPacket, turnoutList[turnoutNr].sysName);
    txPacket (outPacket);
  }
  else if (cmdProtocol == DCCPLUS) {
    while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS) {} // clear ack Queue
    if (desiredState == 'T' || desiredState == 'C') {
      sprintf (outPacket, "<T %s %c>", turnoutList[turnoutNr].sysName, desiredState);
    }
    else {
      sprintf (outPacket, "<T %s %c>", turnoutList[turnoutNr].sysName, turnoutState[desiredState].state);
    }
    while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
    txPacket (outPacket);
    if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
      // wait for ack
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("Warning: No response for setting Turnout");
        xSemaphoreGive(displaySem);
      }
    }
  }
}

void setRoute (uint8_t routeNr)
{
  char outPacket[BUFFSIZE];
  char message[40];
  
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
          displayTempMessage (NULL, message, false);
          setTurnout (turnoutNr, route[n]);
          if (pause>0) {
            delay (pause);
          }
        }
        else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          Serial.printf ("route: turnout %s not found\r\n", start);
          xSemaphoreGive(displaySem);
        }
        n++;
      }
    }
  }
}

void setLocoFunction (uint8_t locoIndex, uint8_t funcIndex, bool set)
{
  char commandPacket[40];
  
  if (cmdProtocol == JMRI) {
    char setVal = '0';
    if (set) setVal = '1';
    sprintf (commandPacket, "M%cA%c%d<;>F%c%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, setVal, funcIndex);
    txPacket (commandPacket);
  }
  else if (cmdProtocol == DCCPLUS) {
    uint8_t reqState;
    char setVal = 0;
    
    if (set) setVal = 1;
    sprintf (commandPacket, "<F %d %d %d>", locoRoster[locoIndex].id, funcIndex, setVal);
    while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
    txPacket (commandPacket);
    if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
      // wait for ack
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("Warning: No response for setting function");
        xSemaphoreGive(displaySem);
      }
    }
  }
}

void setLocoOwnership(uint8_t locoIndex, bool owned)
{
  char commandPacket[40];
  if (cmdProtocol == JMRI) {
    char setVal = '-';

    if (owned) setVal = '+';
    sprintf (commandPacket, "M%c%c%c%d<;>%s", locoRoster[locoIndex].throttleNr, setVal, locoRoster[locoIndex].type, locoRoster[locoIndex].id, locoRoster[locoIndex].name);
    txPacket (commandPacket);
  }  
}

void setStealLoco(uint8_t locoIndex)
{
  char commandPacket[40];
  if (cmdProtocol == JMRI) {
    sprintf (commandPacket, "M%cS%c%d<;>%s", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, locoRoster[locoIndex].name);
    txPacket (commandPacket);
  }  
}


void setLocoSpeed (uint8_t locoIndex, int16_t speed, int8_t direction)
{
  char commandPacket[40];
  uint8_t reqState;
  
  if (cmdProtocol == JMRI) {
    // if (speed<0) speed = 0;
    sprintf (commandPacket, "M%cA%c%d<;>V%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, speed);
    txPacket (commandPacket);
  }
  else if (cmdProtocol == DCCPLUS) {
    uint8_t tdir = 0;
    if (direction == FORWARD) tdir = 1;
    sprintf (commandPacket, "<t 1 %d %d %d>", locoRoster[locoIndex].id, speed, tdir);
    while (xQueueReceive(dccAckQueue, &reqState, 0) == pdPASS);
    txPacket (commandPacket);
    if (xQueueReceive(dccAckQueue, &reqState, pdMS_TO_TICKS(DCCACKTIMEOUT)) != pdPASS) {
      // wait for ack
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("Warning: No response for setting loco speed");
        xSemaphoreGive(displaySem);
      }
    }
  }
}


void setLocoDirection (uint8_t locoIndex, uint8_t direction)
{
  char commandPacket[40];
  if (cmdProtocol == JMRI) {
    if (direction != STOP) {
      int dirFlag = 1;

      if (direction == REVERSE) dirFlag = 0;
      sprintf (commandPacket, "M%cA%c%d<;>R%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, dirFlag);
      txPacket (commandPacket);
    }
  }
}


void setDisconnected()
{
  if (cmdProtocol == JMRI) {
    txPacket ("Q"); // quit
  }
}


void getAddress()
{
  if (cmdProtocol == DCCPLUS) {
    txPacket ("<R>");
  }
}

void getCV(int16_t cv)
{
  if (cmdProtocol == DCCPLUS) {
    char packette[32];
    sprintf (packette, "<R %d %d %d>", cv, CALLBACKNUM, CALLBACKSUB);
    txPacket (packette);
  }  
}
