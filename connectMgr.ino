/*
 * Connection manager
 * 
 * Manage network connections
 * * Network connectivity
 * * estabilish controller connection
 * 
 * WiFi networks should not use hidden SSIDs, will connect to strongest signal
 * Cycle through controller list, connect to the first possible controller
 * 
 */
static WiFiMulti wifiMulti;

void connectionManager(void *pvParameters)
{
  uint8_t index = 0;
  uint8_t preferRef = 0;
  char paramName[16];
  char password[33];
  
  // load array of possible networks
  {
    for (index = 0; index < WIFINETS; index++) {
      sprintf (paramName, "wifissid_%d", index);
      // use a different default for ssid for wifi 0 to other wifi
      if (index == 0) nvs_get_string (paramName, ssid, MYSSID, sizeof(ssid));
      else nvs_get_string (paramName, ssid, "none", sizeof(ssid));
      if (strcmp (ssid, "none") != 0) {
        sprintf (paramName, "wifipass_%d", index);
        nvs_get_string (paramName, password, "none", sizeof(password));
        if (strcmp (password, "none") == 0) wifiMulti.addAP((const char*)ssid, NULL);
        else wifiMulti.addAP((const char*) ssid, (const char*) password);
      }
    }
    WiFi.setHostname(tname);
  }
  // Check that we are connected
  while (true) {
    // if WiFi not connected
    while (WiFi.status() != WL_CONNECTED) {
      ssid[0] = '\0';
      trackPower = false;
      #ifdef TRACKPWR
      digitalWrite(TRACKPWR, LOW);
      #endif
      if (wifiMulti.run() == WL_CONNECTED) {
        // Place name of actually selected SSID in ssid variable
        WiFi.SSID().toCharArray(ssid, sizeof(ssid));
        MDNS.begin(tname);
        Serial.println ("");
        Serial.print   ("WiFi Connected: ");
        Serial.println (ssid);
        // Find the index number of the actually used SSID, use that as prefered ref to server index
        for (preferRef=255, index = 0; index < WIFINETS && preferRef == 255; index++) {
          sprintf (paramName, "wifissid_%d", index); // NB do not replace live SSID while doing the comparison, use local password variable instead
          if (index == 0) nvs_get_string (paramName, password, MYSSID, sizeof(password));  // ssid has a different default to all others
          else nvs_get_string (paramName, password, "none", sizeof(ssid));
          if (strcmp (ssid, password) == 0) preferRef = index;
        }
      }
      else delay (1000);
    }
    // if Wifi connected but server not connected
    if (WiFi.status() == WL_CONNECTED && !client.connected()) {
      char server[65];
      int  port;

      trackPower = false;
      #ifdef TRACKPWR
      digitalWrite(TRACKPWR, LOW);
      #endif
      // first check our preferred connection index, ie the one with the same index ID as the connected network
      if (preferRef < WIFINETS) {  // should not be "not found", but just in case of a bug...
        sprintf (paramName, "server_%d", preferRef);
        nvs_get_string (paramName, server, HOST, sizeof(server));
        sprintf (paramName, "port_%d", preferRef);
        port = nvs_get_int (paramName, PORT);
        connect2server (server, port);
      }
      // now work through server list until we connect or just give up
      for (index = 0; index <WIFINETS && !client.connected(); index++) if (index != preferRef) {
        sprintf (paramName, "server_%d", index);
        nvs_get_string (paramName, server, "none", sizeof(server));
        sprintf (paramName, "port_%d", index);
        port = nvs_get_int (paramName, PORT);
        connect2server (server, port);
      }
    }
    delay (1000);
  }
  vTaskDelete( NULL );
}


void connect2server (char *server, int port) {
  if (strcmp (server, "none") == 0) return;
  if (client.connect(server, port)) {
    if (strlen(server) < sizeof(remServerNode)-1) strcpy (remServerNode, server);
    else {
      strncpy (remServerNode, server, sizeof(remServerNode)-1);
      remServerNode[sizeof(remServerNode)-1] = '\0';
    }
    // Once connected, we can listen for returning packets
    xTaskCreate(receiveNetData, "JRMI_In", 4096, NULL, 4, NULL);
    // Print diagnostic
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("");
      Serial.print   ("Connected to server: ");
      Serial.print   (server);
      Serial.print   (":");
      Serial.println (port);
      xSemaphoreGive(displaySem);
    }
    for (uint8_t n=0; n<INITWAIT && cmdProtocol==UNDEFINED; n++) {
      delay (1000);   // wait to get some data back, then check we not on unknown protocol
    }
    if (cmdProtocol == UNDEFINED) {  // probe with JMRI if nothing received
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("Timeout on protocol identification: defaulting to JMRI protocol");
        xSemaphoreGive(displaySem);
      }
      cmdProtocol = JMRI;
      setInitialData();
    }
  }
}

void txPacket (char *header, char *pktData)
{
  if (!client.connected()) return;
  if (pktData == NULL) return;
  if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    if (header != NULL) client.print (header);
    client.println (pktData);
    client.flush();
    xSemaphoreGive(transmitSem);
    if (showPackets) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print ("--> ");
        if (header != NULL) Serial.print (header);
        Serial.print (pktData);
        Serial.println ("");
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("ERROR: Could not get semaphore to transmit data");
      xSemaphoreGive(displaySem);
    }
  }
}

void txPacket (char *pktData)
{
  if (!client.connected()) return;
  if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    if (pktData!= NULL) {
      client.println (pktData);
      client.flush();
    }
    xSemaphoreGive(transmitSem);
    if (showPackets) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print ("--> ");
        if (pktData!= NULL) Serial.print (pktData);
        Serial.println ("");
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("ERROR: Could not get semaphore to transmit data");
        xSemaphoreGive(displaySem);
      }
    }
  }
}
