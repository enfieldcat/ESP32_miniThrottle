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



#ifdef RELAYPORT
void relayListener(void *pvParameters)
{
  static WiFiClient relayClient[MAXRELAY];
  bool relayIsRunning = true;
  bool notAssigned = true;
  uint8_t i = 1;
  uint8_t j = 0;
  char relayName[16];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s relayListener(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }

  // Want only one web server thread at a time
  // if starting a new one, wait for existing ones to finish
  while (i > 0 && j < 120) {
    if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      i = relayCount;
      if (i == 0) {
        relayCount++;
        j = 0;
        }
      else j++;
      xSemaphoreGive(relaySvrSem);
      if (j > 0) delay(1000);
    }
    else semFailed ("relaySvrSem", __FILE__, __LINE__);
  }
  if (j == 120) {
    // we have waited 120 seconds for old relay server to stop, it has not
    // therefore we will terminate this instance
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s New instance of relay server stopped after waiting for previous one to complete\r\n", getTimeStamp());
      xSemaphoreGive(consoleSem);
    }
    vTaskDelete( NULL );
  }

  remoteSys = (relayConnection_s*) malloc (sizeof (relayConnection_s) * maxRelay);
  if (remoteSys != NULL) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Starting relay, port %d\r\n", getTimeStamp(), relayPort); 
      xSemaphoreGive(consoleSem);
    }
    for (uint8_t n=0; n<maxRelay; n++) {
      remoteSys[n].client = &(relayClient[n]);
      remoteSys[n].nodeName[0] = '\0';
      remoteSys[n].inPackets  = 0;
      remoteSys[n].outPackets = 0;
      remoteSys[n].id = n;
    }
    relayServer->begin();
    if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      relayCount++;
      xSemaphoreGive(relaySvrSem);
    }
    // relayServer->setNoDelay(true);
    mdns_txt_item_t txtrecord[] = {{(char*)"server" ,(char*)PRODUCTNAME},{(char*)"version", (char*)VERSION}};
    mdns_service_add(NULL, "_withrottle", "_tcp", relayPort, txtrecord, 2);
    while (relayIsRunning) {
      if (APrunning || WiFi.status() == WL_CONNECTED) {
        if (relayServer->hasClient()) {
          notAssigned = true;
          for(i = 0; notAssigned && i < maxRelay; i++){
            if (!relayClient[i].connected()){
              relayClient[i] = relayServer->available();
              if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                remoteSys[i].client = &relayClient[i];
                xSemaphoreGive(relaySvrSem);
              }
              else semFailed ("relaySvrSem", __FILE__, __LINE__);
              sprintf (relayName, "RelayHandler%d", i);
              // xTaskCreate(relayHandler, relayName, 6144, &remoteSys[i], 4, NULL);
              xTaskCreate(relayHandler, relayName, 7168, &remoteSys[i], 4, NULL);
              if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf ("%s Starting relay client as %s\r\n", getTimeStamp(), relayName); 
                xSemaphoreGive(consoleSem);
              }
              notAssigned = false;
            }
          }
        }
        if (i == maxRelay) {
          relayServer->available().stop();  // no worker threads available
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Rejecting new relay client, all available client slots connected.\r\n", getTimeStamp()); 
            xSemaphoreGive(consoleSem);
          }
        }
        delay (10); // pause to use fewer CPU cycles while waiting for connecton
      }
      else relayIsRunning = false;
    }

    if (remoteSys!=NULL) {
      free (remoteSys);
      remoteSys = NULL;
    }
  }
  else {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Unable to allocate %d bytes for relay\r\n", getTimeStamp(), (sizeof (relayConnection_s) * maxRelay)); 
      xSemaphoreGive(consoleSem);
    }
  }
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Stopping relay\r\n", getTimeStamp()); 
    xSemaphoreGive(consoleSem);
  }
  relayServer->close();
  relayServer->stop();
  relayServer->end();
  if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    while (relayCount>0) relayCount--;
    startRelay = true;
    xSemaphoreGive(relaySvrSem);
  }
  vTaskDelete( NULL );
}


// Rutine to find LocoId in a WiThrottle command string
uint16_t getWiThrotLocoID (char *inBuffer)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s getWiThrotLocoID(%s)\r\n", getTimeStamp(), inBuffer);
    xSemaphoreGive(consoleSem);
  }

  uint16_t retVal = 0;
  uint16_t buffLen = strlen (inBuffer);
  uint8_t  n = 0;
  for (n=0; inBuffer[n]!='<' && n<buffLen; n++);
  if (inBuffer[n] == '<') {
    inBuffer[n] = '\0';
    retVal = util_str2int(inBuffer);
    inBuffer[n] = '<';
  }
return (retVal);
}

void relayHandler(void *pvParameters)
{
  struct relayConnection_s *thisRelay = (struct relayConnection_s *) pvParameters;
  uint16_t inPtr = 0;
  char inBuffer[NETWBUFFSIZE];
  char inchar;
  char indicator = 'A';
  bool keepAlive = true;
  bool trackKeepAlive = false;
  
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s relayHandler(%x)\r\n", getTimeStamp(), thisRelay);
    xSemaphoreGive(consoleSem);
  }

  // Initialise the data structure for use
  if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    thisRelay->nodeName[0] = '\0';
    thisRelay->inPackets = 0;
    thisRelay->outPackets = 0;
    thisRelay->keepAlive = esp_timer_get_time();
    thisRelay->address = thisRelay->client->remoteIP();
    if (thisRelay->id < 10) indicator = '0' + thisRelay->id;
    else indicator = ('A' - 10) + thisRelay->id;
    relayClientCount++;
    if (relayClientCount > maxRelayCount) maxRelayCount = relayClientCount;
    xSemaphoreGive(relaySvrSem);
  }
  else semFailed ("relaySvrSem", __FILE__, __LINE__);
  // Start up message
  if (debuglevel>0 && xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Relay client at %d.%d.%d.%d connecting\r\n", getTimeStamp(), thisRelay->address[0], thisRelay->address[1], thisRelay->address[2], thisRelay->address[3]);
      xSemaphoreGive(consoleSem);
    }
    xSemaphoreGive(relaySvrSem);
  }
  else semFailed ("relaySvrSem", __FILE__, __LINE__);
  // For WiThrottle connections, send inventory information as par of the initial headers
  if (relayMode == WITHROTRELAY) {
    sendWiThrotHeader(thisRelay, &inBuffer[0]);
  }
  while (keepAlive && relayConnState(thisRelay, 1)) {
    inchar = '\0';
    while (keepAlive && relayConnState(thisRelay, 2)) {
      if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        inchar = thisRelay->client->read();
        xSemaphoreGive(tcpipSem);
      }
      else semFailed ("tcpipSem", __FILE__, __LINE__);
      if (inchar == '\r' || inchar == '\n') {
        if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (thisRelay->client->available()>0 && thisRelay->client->peek() == '\n') thisRelay->client->read();
          xSemaphoreGive(tcpipSem);
        }
        else semFailed ("tcpipSem", __FILE__, __LINE__);
        if (inPtr > 0) {
          inBuffer[inPtr] = '\0';  // terminate input string
          // Packet count
          if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            thisRelay->inPackets++;
            xSemaphoreGive(relaySvrSem);
          }
          else semFailed ("relaySvrSem", __FILE__, __LINE__);
          // Display packets if required
          if (showPackets && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("<%cR %s\r\n", indicator, inBuffer);
            xSemaphoreGive(consoleSem);
          }
          // Use the appropriate relay function
          // - but treat JMRI keep alives as a lower level feature dealt with here
          if (relayMode == WITHROTRELAY) {
            // keep alive handling
            if (inBuffer[0] == '*' && strlen(inBuffer) < 3) {
              if (inBuffer[1] == '+') { trackKeepAlive= true; Serial.println ("keepalive on"); }
              else if (inBuffer[1] == '-') { trackKeepAlive= false; Serial.println ("keepalive off"); }
              reply2relayNode (thisRelay, "HTJMRI\r\n");  // Ack by sending Node type
            }
            else wiThrotRelayPkt (thisRelay, inBuffer, indicator);
          }
          else dcc_relay (inBuffer, indicator);
        }
        inPtr = 0;
        if (trackKeepAlive) thisRelay->keepAlive = esp_timer_get_time();
      }
      else if (inchar>31 && inchar<127) inBuffer[inPtr++] = inchar;
      if (!relayConnState(thisRelay, 2)) delay(20);
    }
    if (trackKeepAlive && (esp_timer_get_time() - thisRelay->keepAlive) > maxRelayTimeOut) {
      keepAlive = false;
      if (debuglevel>0 && xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Relay client at %d.%d.%d.%d keep-alive time-out\r\n", getTimeStamp(), thisRelay->address[0], thisRelay->address[1], thisRelay->address[2], thisRelay->address[3]);
          xSemaphoreGive(consoleSem);
        }
        xSemaphoreGive(relaySvrSem);
      }
      else semFailed ("relaySvrSem", __FILE__, __LINE__);
    }
    delay(20);
  }
  // Tear down message
  if ( debuglevel>0 && xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Relay client at %d.%d.%d.%d stopping\r\n", getTimeStamp(), thisRelay->address[0], thisRelay->address[1], thisRelay->address[2], thisRelay->address[3]);
      xSemaphoreGive(consoleSem);
    }
    xSemaphoreGive(relaySvrSem);
  }
  else semFailed ("relaySvrSem", __FILE__, __LINE__);
  // Drop relay count and terminate thread
  if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    relayClientCount--;
    xSemaphoreGive(relaySvrSem);
  }
  else semFailed ("relaySvrSem", __FILE__, __LINE__);
  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    thisRelay->client->stop();
    xSemaphoreGive(tcpipSem);
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  // Stop and drop any owned locos
  for (uint16_t n=0; n<locomotiveCount+MAXCONSISTSIZE; n++) {
    inchar = 0;
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (locoRoster[n].throttleNr == thisRelay->id) {
        locoRoster[n].throttleNr = 255;
        inchar = 1;
        sprintf (inBuffer, "<- %d>\r\n", locoRoster[n].id);
      }
      xSemaphoreGive(velociSem);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
    if (inchar > 0) {
      setLocoSpeed (n, -1, STOP);                 //Emergency stop
      forward2serial (inBuffer);
    }
  }
  vTaskDelete( NULL );
}


bool relayConnState (struct relayConnection_s *thisRelay, uint8_t chkmode)
{
  bool retval = false;

  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    switch (chkmode) {
    case 1: retval = (APrunning || WiFi.status() == WL_CONNECTED) && thisRelay->client->connected();
            break;
    case 2: retval = (APrunning || WiFi.status() == WL_CONNECTED) && thisRelay->client->connected() && thisRelay->client->available()>0;
            break;
    }
    xSemaphoreGive(tcpipSem);
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  return (retval);
}


// Forward to serial port and add up tally
void forward2serial (char *packet)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s forward2serial(%s)\r\n", getTimeStamp(), packet);
    xSemaphoreGive(consoleSem);
  }
  if (xSemaphoreTake(serialSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    serial_dev.write (packet, strlen(packet));  // Forward to serial
    xSemaphoreGive(serialSem);
    if (showPackets && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("F-> %s", packet);
      xSemaphoreGive(consoleSem);
    }
    if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      localoutPkts++;
      xSemaphoreGive(relaySvrSem);
    }
    else semFailed ("relaySvrSem", __FILE__, __LINE__);
  }
  else semFailed ("serialSem", __FILE__, __LINE__);
}


// Respond back to relayed node and add up tally
void reply2relayNode (struct relayConnection_s *thisRelay, const char *outPacket)
{ 
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (outPacket==NULL) Serial.printf ("%s reply2relayNode(%x, NULL)\r\n", getTimeStamp(), thisRelay);
    else Serial.printf ("%s reply2relayNode(%x, %s", getTimeStamp(), thisRelay, outPacket);
    xSemaphoreGive(consoleSem);
  }
  if (thisRelay != NULL && thisRelay->client!=NULL && thisRelay->client->connected()) {
    if (outPacket != NULL && strlen(outPacket)>0 && xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      thisRelay->client->write (outPacket, strlen(outPacket));
      xSemaphoreGive(tcpipSem);
      if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        thisRelay->outPackets++;
        xSemaphoreGive(relaySvrSem);
      }
      else semFailed ("relaySvrSem", __FILE__, __LINE__);
      if (showPackets && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("R-> %s", outPacket);
        xSemaphoreGive(consoleSem);
      }
    }
    else semFailed ("tcpipSem", __FILE__, __LINE__);
  }
}


void wiThrotRelayPkt (struct relayConnection_s *thisRelay, char *inPacket, char indicator)
{
  uint16_t buffLen = strlen(inPacket);
  char outBuffer[420];   // need to cater for long function descriptor packets

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s wiThrotRelayPkt(%x, %s, %c)\r\n", getTimeStamp(), thisRelay, inPacket, indicator);
    xSemaphoreGive(consoleSem);
  }
  outBuffer[0] = '\0';
  // remote system name & ID
  if (inPacket[0] == 'N' || (inPacket[0]=='H' && inPacket[1]=='U')) {
    uint8_t tint = 1;
    if (inPacket[0]=='H') tint = 2;
    if (strlen(thisRelay->nodeName) == 0 || tint == 1) {
      if (strlen(inPacket) > sizeof(thisRelay->nodeName)) inPacket[sizeof(thisRelay->nodeName)] = '\0';
      strcpy (thisRelay->nodeName, &inPacket[tint]);
    }
    reply2relayNode (thisRelay, "HTJMRI\r\n");
  }
  // Power On/Off - Use "native" throttle power on/off for this
  else if (buffLen == 4 && strncmp (inPacket, "PPA", 3) == 0) {
    if (inPacket[3] == '1') setTrackPower (1);
    else setTrackPower (2);
  }
  // Turnouts - strip packet and forward
  else if (buffLen>4 && strncmp(inPacket, "PTA", 3) == 0 && (trackPower || nvs_get_int("noPwrTurnouts", 0) == 1)) {
    char *turnName = &inPacket[4];
    uint8_t func = 0;                  // Close
    if (inPacket[3] == 'T') func = 1;  // Throw
    else if (inPacket[3] == '2') {     // Toggle - Find the turnout and its current setting
      func = 199;
      if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        for (uint8_t n=0; n<turnoutCount && func==199; n++) if (strcmp (turnName, turnoutList[n].sysName) == 0) func = turnoutList[n].state;
        xSemaphoreGive(turnoutSem);
        if (func=='C' || func =='2' || func==2) func = 1;
        else func = 0;
      }
      else semFailed ("turnoutSem", __FILE__, __LINE__);
    }
    if (func<199) {
      sprintf (outBuffer, "<T %s %d>\r\n", turnName, func);
      forward2serial (outBuffer);
    }
  }
  // Routes - Find route and set as a background task
  else if (buffLen>4 && strncmp(inPacket, "PRA2", 4) == 0 && (trackPower || nvs_get_int("noPwrTurnouts", 0) == 1)) {
    char *param = &inPacket[4];
    uint8_t id = 255;
    for (uint8_t n=0; n<routeCount && id==255;n++) if (strcmp (routeList[n].sysName, param)==0) id = n;
    if (id < routeCount) {
      xTaskCreate(setRouteBg, "setRouteBgRelay", 4096, &id, 4, NULL);
      delay (250);
    }
    else {
      sprintf (outBuffer, "PRA0%s\r\n", param);
      relay2WiThrot (outBuffer);
    }
  }
  // Operate loco in multi controller
  else if (inPacket[0] == 'M') {
    char *param = NULL;
    uint32_t t_function;
    uint32_t mask;
    int16_t locoID = -1;  // Will assume LocoID == -1 => ALL locos
    int16_t t_id;
    uint8_t action;
    uint8_t command = 0, n = 0;
    uint8_t t_relayIdx;
    uint8_t t_type;
    uint8_t t_speed;
    uint8_t t_direction;
    uint8_t t_throttleNr;

    if (inPacket[3] != '*') locoID = getWiThrotLocoID (&inPacket[4]);
    for (n=4; n<buffLen && inPacket[n]!='>'; n++);
    if (inPacket[n] == '>') {
      command = inPacket[n+1]; 
      if (command != '\0') param = &inPacket[n+2];
    }
    action = inPacket[2];
    // Query
    if (action == 'A' && command == 'q') {
      uint8_t j=0, limit=locomotiveCount+MAXCONSISTSIZE;
      if (inPacket[3] != '*') locoID = getWiThrotLocoID (&inPacket[4]);
      if (inPacket[3] == '*' || locoID != 0) {
        for (j=0; j<limit; j++) {
          // Gather data that may be temporal by gaining semphore access
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            t_relayIdx   = locoRoster[j].relayIdx;
            t_id         = locoRoster[j].id;
            t_type       = locoRoster[j].type;
            t_speed      = locoRoster[j].speed;
            t_direction  = locoRoster[j].direction;
            t_throttleNr = locoRoster[j].throttleNr;
            xSemaphoreGive(velociSem);
            if (t_relayIdx == thisRelay->id && t_throttleNr == inPacket[1] && (inPacket[3] == '*' || (t_id == locoID && t_type == inPacket[3]))) {
              outBuffer[0] = '\0';
              // Actual speed and direction data is expected to change in flight - lock our access to it
              if (inPacket[buffLen-1] == 'R') {
                uint8_t x = 1;
                if (t_direction == REVERSE) x = 0;
                sprintf (outBuffer, "M%cA%c%d<;>R%d\r\n", inPacket[1], t_type, t_id, x);
                }
              if (inPacket[buffLen-1] == 'V') {
                sprintf (outBuffer, "M%cA%c%d<;>V%d\r\n", inPacket[1], t_type, t_id, t_speed);
              }
              // If we have a query response lets send it
              if (outBuffer[0] != '\0') reply2relayNode (thisRelay, outBuffer);
            }
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
        }
      }
    }
    // Set speed
    else if (action == 'A' && command == 'V') {
      uint8_t j=0, limit=locomotiveCount+MAXCONSISTSIZE;
      int8_t  speed = 1;
      for (j=strlen(inPacket)-1; inPacket[j]!='V' && j>0; j--);
      if (inPacket[j]=='V') {
        if (trackPower) speed = util_str2int(&inPacket[j+1]);
        else speed = 0;
      }
      if (inPacket[3] == '*' || locoID >= 0) {
        for (j=0; j<limit; j++) {
          // Gather data that may be temporal by gaining semphore access
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            t_relayIdx   = locoRoster[j].relayIdx;
            t_id         = locoRoster[j].id;
            t_type       = locoRoster[j].type;
            t_direction  = locoRoster[j].direction;
            t_throttleNr = locoRoster[j].throttleNr;
            xSemaphoreGive(velociSem);
            if (t_relayIdx == thisRelay->id && t_throttleNr == inPacket[1] && (inPacket[3] == '*' || (t_id == locoID && t_type == inPacket[3]))) {
              setLocoSpeed (j, speed, t_direction); // set speed
            }
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
        }
      }
    }
    // Set direction
    else if (action == 'A' && command == 'R') {
      uint8_t j=0, limit=locomotiveCount+MAXCONSISTSIZE;
      int8_t  direction = REVERSE;
      if (inPacket[strlen(inPacket)-1] == '1') {
        direction = FORWARD;
      }
      if (inPacket[3] == '*' || locoID >= 0) {
        for (j=0; j<limit; j++) {
          // Gather data that may be temporal by gaining semphore access
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            t_relayIdx   = locoRoster[j].relayIdx;
            t_id         = locoRoster[j].id;
            t_type       = locoRoster[j].type;
            t_speed      = locoRoster[j].speed;
            t_throttleNr = locoRoster[j].throttleNr;
            xSemaphoreGive(velociSem);
            if (t_relayIdx == thisRelay->id && t_throttleNr == inPacket[1] && (inPacket[3] == '*' || (t_id == locoID && t_type == inPacket[3]))) {
              setLocoSpeed (j, t_speed, direction); // set direction
            }
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
        }
      }
    }
    // Forward functions
    else if (action == 'A' && command == 'F' && trackPower) {
      if (param!=NULL && strlen(param)>1) {
        uint8_t j=0, limit=locomotiveCount+MAXCONSISTSIZE;
        if (inPacket[3] == '*' || locoID >= 0) {
          action = param[0];  // No further checks outside this else of action, so can repurpose of on/off
          n = util_str2int(&param[1]);
          for (j=0; j<limit; j++) {
            // Gather data that may be temporal by gaining semphore access
            if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              t_relayIdx   = locoRoster[j].relayIdx;
              t_id         = locoRoster[j].id;
              t_type       = locoRoster[j].type;
              t_throttleNr = locoRoster[j].throttleNr;
              t_function   = locoRoster[j].function;
              xSemaphoreGive(velociSem);
              if (t_relayIdx == thisRelay->id && t_throttleNr == inPacket[1] && (inPacket[3] == '*' || (t_id == locoID && t_type == inPacket[3]))) {
                if (action == '1') setLocoFunction (j, n, true);
                else setLocoFunction (j, n, false);
              }
            }
            else semFailed ("velociSem", __FILE__, __LINE__);
          }
        }
      }
    }
    // Remove loco
    else if (action == '-') {
      uint8_t j=0, limit=locomotiveCount+MAXCONSISTSIZE;

      // ack the remove request
      strcat (inPacket, "\r\n");  // Echo back a confirmation of removal
      reply2relayNode (thisRelay, inPacket);           // Ack to relayed node
      // Now send emg-stop and remove from DCC-Ex inventory
      if (inPacket[3] == '*' || locoID >= 0) {
        for (j=0; j<limit; j++) {
          // Gather data that may be temporal by gaining semphore access
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            t_relayIdx   = locoRoster[j].relayIdx;
            t_id         = locoRoster[j].id;
            t_type       = locoRoster[j].type;
            t_speed      = locoRoster[j].speed;
            t_throttleNr = locoRoster[j].throttleNr;
            xSemaphoreGive(velociSem);
            // Tell DCC-Ex the associated locos should stop, and be removed
            if (t_relayIdx == thisRelay->id && t_throttleNr == inPacket[1] && (inPacket[3] == '*' || (t_id == locoID && t_type == inPacket[3]))) {
              if (t_speed > 0) setLocoSpeed (j, -1, STOP);                 //Emergency stop
              sprintf (outBuffer, "<- %d>\r\n", t_id);
              forward2serial (outBuffer);  // Forward to serial
              if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                locoRoster[j].relayIdx = 255;
                locoRoster[j].speed = 0;
                xSemaphoreGive(velociSem);
              }
              else semFailed ("velociSem", __FILE__, __LINE__);
            }
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
        }
      }
    }
    // Add or steal loco
    else if (action == '+' || action == 'S') {
      uint8_t j=0, limit=locomotiveCount+MAXCONSISTSIZE;
      if (locoID>=0) {
        for (j=0; j<limit; j++) {
          // Gather data that may be temporal by gaining semphore access
          if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            t_relayIdx   = locoRoster[j].relayIdx;
            t_id         = locoRoster[j].id;
            t_type       = locoRoster[j].type;
            t_speed      = locoRoster[j].speed;
            t_direction  = locoRoster[j].direction;
            t_function   = locoRoster[j].function;
            t_throttleNr = locoRoster[j].throttleNr;
            // Check if ad-hoc number and set defaults for unknown unit.
            if (j>=locomotiveCount && t_relayIdx == 255) {
              n = inPacket[3];
              if (n!='S' && n!='L') {
                if (t_id<128) n='S';
                else n='L';
              }
              locoRoster[j].id        = t_id        = locoID;
              locoRoster[j].type      = t_type      = n;
              locoRoster[j].speed     = t_speed     = 0;
              locoRoster[j].function  = t_function  = 0;
              locoRoster[j].direction = t_direction = FORWARD;
            }
            xSemaphoreGive(velociSem);
            if ((j<locomotiveCount && t_id == locoID && t_type == inPacket[3]) || (j>=locomotiveCount && (t_relayIdx == 255 || (t_relayIdx != 255 && t_id == locoID && t_type == inPacket[3])))) {
              if (t_relayIdx == 255 || inPacket[2] == 'S') {
                if (inPacket[2] == 'S') {                                 //  inform our "donor" they've lost the loco.
                  if (locoRoster[j].owned) {
                    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                      locoRoster[j].owned = false;
                      xSemaphoreGive(velociSem);
                    }
                  }
                  else {
                    sprintf (outBuffer, "M%c-%c%d<;>r\r\n", t_throttleNr, t_type, t_id);
                    reply2relayNode (&(remoteSys[t_relayIdx]), outBuffer);  //  Send theft notification
                  }
                }
                // update remote throttle identifiers for this loco
                if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                  if (locoRoster[j].direction != REVERSE) locoRoster[j].direction = FORWARD;
                  locoRoster[j].relayIdx = thisRelay->id;
                  locoRoster[j].throttleNr = inPacket[1];
                  xSemaphoreGive(velociSem);  // Release the semaphore for loco speed and velocity once we have set our vital data
                  t_relayIdx = thisRelay->id;
                  t_throttleNr = inPacket[1];
                }
                else semFailed ("velociSem", __FILE__, __LINE__);
                // Ack control
                sprintf (outBuffer, "M%c+%c%d<;>\r\n", t_throttleNr, t_type, t_id);
                reply2relayNode (thisRelay, outBuffer); // Ack packet
                // send function key statuses, but only if they are from the standard roster
                if (j<locomotiveCount) {
                  char functLabel[420];
                  char labelName[16];
                  char *token;
                  char *rest = NULL;
                  char *funcStringDCC = NULL;
                  uint8_t idx = 0;
                  
                  // if imported from DCC look for labels in working storage section, else in NV RAM
                  if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    funcStringDCC = locoRoster[j].functionString;
                    xSemaphoreGive(velociSem);  // Release the semaphore for loco speed and velocity once we have set our vital data
                  }
                  if (funcStringDCC == NULL) {
                    functLabel[0]='\0';
                    sprintf (labelName, "FLabel%d", t_id);    // try to get custom labels, then default labels.
                    nvs_get_string (labelName, functLabel, (char*)"", sizeof(functLabel));
                  }
                  else {
                    if (strlen(funcStringDCC) > sizeof(functLabel)) funcStringDCC[sizeof(functLabel)] = '\0';
                    strcpy (functLabel, funcStringDCC);
                  }
                  if (functLabel[0] == '\0') nvs_get_string ("FLabelDefault", functLabel, FUNCTNAMES, sizeof(functLabel));
                  // split function labels to send to client
                  if (functLabel[0] != '\0') {
                    sprintf (outBuffer, "M%cL%c%d<;>", t_throttleNr, t_type, t_id); 
                    for (idx=0, token=strtok_r(functLabel, "~", &rest); token!=NULL && idx<29; token=strtok_r(NULL, "~", &rest), idx++) {
                      strcat (outBuffer, "]\\[");
                      strcat (outBuffer, token);
                    }
                    for (;idx<29; idx++) {
                      sprintf (labelName, "%d", idx);
                      strcat (outBuffer, "]\\[");
                      strcat (outBuffer, labelName);
                    }
                    strcat (outBuffer, "\r\n");
                    reply2relayNode (thisRelay, outBuffer);//  Send function label list
                  }
                  for (n=0, mask=1; n<29; n++) {
                    mask<<=1;
                    if ((t_function & mask) == 0) j = 0;
                    else j = 1;
                    sprintf (outBuffer, "M%cA%c%d<;>F%d%d\n\r", t_throttleNr, t_type, t_id, j, n);
                    reply2relayNode (thisRelay, outBuffer);//  Send function status
                  }
                }
                j = limit + 1;
                // send speed and direction info
                sprintf (outBuffer, "M%cA%c%d<;>V%d\r\n", t_throttleNr, t_type, t_id, t_speed);
                reply2relayNode (thisRelay, outBuffer); //  Update velocity
                if (t_direction == REVERSE) n = 0;
                else n = 1;
                sprintf (outBuffer, "M%cA%c%d<;>R%d\r\n", t_throttleNr, t_type, t_id, n);
                reply2relayNode (thisRelay, outBuffer); //  Update direction
              }
              else {
                sprintf (outBuffer, "%s\r\n", inPacket);//  Change the request into a "steal" request
                outBuffer[2] = 'S';
                reply2relayNode (thisRelay, outBuffer); //  Request Steal
              }
              j = limit + 1; // exceed but do not equal limit so we can stop once found but still detect a not found situation
            }
          }
          else semFailed ("velociSem", __FILE__, __LINE__);
        }
        if (j==limit) { // reached end without any allocation made - and the semaphore will not have been released.
          xSemaphoreGive(velociSem);
          if (showPackets && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Unable to allocate roster table space to new request for loco %d\r\n", getTimeStamp(), locoID);
            xSemaphoreGive(consoleSem);
          }
        }
      }
    }
  }
  // Quit remote throttle
  else if (inPacket[0] == 'Q' && inPacket[1] == '\0') {
    uint8_t j=0, limit=locomotiveCount+MAXCONSISTSIZE;
    for (uint8_t n=0; n<limit; n++) if (locoRoster[n].relayIdx == thisRelay->id) {
      if (locoRoster[n].speed > 0) setLocoSpeed (n, -1, STOP); //Emergency stop
      locoRoster[n].relayIdx = 255;
    }
  }
}


/*
*  For straight-through (F)orwarding relay - AKA DCC-Ex mode
*/
void dcc_relay (char *packet, char indicator)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s dcc_relay(%s, %c)\r\n", getTimeStamp(), packet, indicator);
    xSemaphoreGive(consoleSem);
  }
  if (packet[0]=='<') {
    if (xSemaphoreTake(serialSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      serial_dev.write (packet, strlen(packet));
      serial_dev.write ("\r\n", 2);
      xSemaphoreGive(serialSem);
      // count forwarded packets
      if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        localoutPkts ++;
        xSemaphoreGive(relaySvrSem);
      }
      else semFailed ("relaySvrSem", __FILE__, __LINE__);
      if (showPackets) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("F%c-> %s\r\n", indicator, packet);
          xSemaphoreGive(consoleSem);
        }
      }
    }
    else semFailed ("serialSem", __FILE__, __LINE__);
  }
  else {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Reject packet not in DCC-Ex format:\r\n    %s\r\n", getTimeStamp(), packet);
      xSemaphoreGive(consoleSem);
    }
  }
}


// Send dcc info back to all relay nodes
void relay2WiThrot (char *outBuffer)
{
  if (outBuffer == NULL) return;
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s relay2WiThrot(%s", getTimeStamp(), outBuffer);
    xSemaphoreGive(consoleSem);
  }
  if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    uint8_t chk = relayCount;
    if (chk>1 && relayClientCount==0) chk = 0; // Also skip if no clients connected
    xSemaphoreGive(relaySvrSem);
    if (chk < 2) return;                       // relay not initialised or running
  }
  if (outBuffer[0]!='\0') {
    for (uint8_t n=0; n<maxRelay; n++) if (remoteSys != NULL && remoteSys[n].client!=NULL && remoteSys[n].client->connected()) {
      reply2relayNode (&remoteSys[n], outBuffer);
    }
  }
}



// Send JMRI Header data
void sendWiThrotHeader(struct relayConnection_s *thisRelay, char *inBuffer)
{
  uint32_t tint = 0;
  char tBuffer[80];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s sendWiThrotHeader(%x, %x)\r\n", getTimeStamp(), thisRelay, inBuffer);
    xSemaphoreGive(consoleSem);
  }
  if (trackPower) tint = 1;
  else tint = 0;
  sprintf (inBuffer, "VN2.0\r\n");
  reply2relayNode (thisRelay, inBuffer);
  sprintf (inBuffer, "PPA%d\r\n", tint);
  reply2relayNode (thisRelay, inBuffer);
  if (locomotiveCount>0 && locoRoster!=NULL) {
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (inBuffer, "RL%d", locomotiveCount);
      for (uint8_t n=0; n<locomotiveCount; n++) {
        sprintf (tBuffer, "]\\[%s}|{%d}|{%c", locoRoster[n].name, locoRoster[n].id, locoRoster[n].type);
        strcat (inBuffer, tBuffer);
      }
      xSemaphoreGive(velociSem);
      strcat (inBuffer, "\r\n");
      reply2relayNode (thisRelay, inBuffer);
    }
  }
  if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    strcpy (inBuffer, "PTT]\\[Turnouts}|{Turnout");
    for (uint8_t n=0; n<turnoutStateCount; n++) {
      sprintf (tBuffer, "]\\[%s}|{%c", turnoutState[n].name, turnoutState[n].state);
      strcat (inBuffer, tBuffer);
    }
    xSemaphoreGive(turnoutSem);
    strcat (inBuffer, "\r\n");
    reply2relayNode (thisRelay, inBuffer);
  }
  if (turnoutCount>0 && turnoutList!=NULL) {
    if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (inBuffer, "PTL");
      for (uint8_t n=0; n<turnoutCount; n++) {
        sprintf (tBuffer, "]\\[%s}|{%s}|{%c", turnoutList[n].sysName, turnoutList[n].userName, turnoutList[n].state);
        strcat (inBuffer, tBuffer);
      }
      xSemaphoreGive(turnoutSem);
      strcat (inBuffer, "\r\n");
      reply2relayNode (thisRelay, inBuffer);
    }
  }
  if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    strcpy (inBuffer, "PRT]\\[Routes}|{Route");
    for (uint8_t n=0; n<routeStateCount; n++) {
      sprintf (tBuffer, "]\\[%s}|{%c", routeState[n].name, routeState[n].state);
      strcat (inBuffer, tBuffer);
    }
    xSemaphoreGive(routeSem);
    strcat (inBuffer, "\r\n");
    reply2relayNode (thisRelay, inBuffer);
  }
  if (routeCount>0 && turnoutList!=NULL) {
    if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sprintf (inBuffer, "PRL");
      for (uint8_t n=0; n<routeCount; n++) {
        sprintf (tBuffer, "]\\[%s}|{%s}|{%c", routeList[n].sysName, routeList[n].userName, routeList[n].state);
        strcat (inBuffer, tBuffer);
      }
      xSemaphoreGive(routeSem);
      strcat (inBuffer, "\r\n");
      reply2relayNode (thisRelay, inBuffer);
    }
  }
  #ifdef WEBLIFETIME
  sprintf (inBuffer, "PW%d\r\n", webPort);
  reply2relayNode (thisRelay, inBuffer);
  #endif
  sprintf (inBuffer, "*%d\r\n", nvs_get_int ("relayKeepAlive", KEEPALIVETIMEOUT));
  reply2relayNode (thisRelay, inBuffer);
  sprintf (inBuffer, "HtJMRI %s v%s (%s)\r\n", PRODUCTNAME, VERSION, tname);
  reply2relayNode (thisRelay, inBuffer);
  tint = 1;
  if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (fc_multiplier>0) {
      sprintf (inBuffer, "PFT%d<;>%4.2f\r\n", fc_time, fc_multiplier);
      }
    else tint = 0;
    xSemaphoreGive(fastClockSem);
    if (tint>0) reply2relayNode (thisRelay, inBuffer);
  }
}
#endif
