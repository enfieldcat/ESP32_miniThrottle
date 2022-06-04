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
#ifdef USEWIFI
static WiFiMulti wifiMulti;

void connectionManager(void *pvParameters)
{
  uint8_t index = 0;
  uint8_t preferRef = 0;
  char paramName[SSIDLENGTH];
  char password[SSIDLENGTH];

  uint8_t use_multiwifi = nvs_get_int ("use_multiwifi", 0);
  // load array of possible networks
  if (use_multiwifi == 1) {
    for (index = 0; index < WIFINETS; index++) {
      sprintf (paramName, "wifissid_%d", index);
      // use a different default for ssid for wifi 0 to other wifi
      if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(20000)) == pdTRUE) {
        if (index == 0) nvs_get_string (paramName, ssid, MYSSID, sizeof(ssid));
        else nvs_get_string (paramName, ssid, "none", sizeof(ssid));
        xSemaphoreGive(transmitSem);
      }
      if (strcmp (ssid, "none") != 0) {
        sprintf (paramName, "wifipass_%d", index);
        nvs_get_string (paramName, password, "none", sizeof(password));
        if (strcmp (password, "none") == 0) wifiMulti.addAP((const char*)ssid, NULL);
        else wifiMulti.addAP((const char*) ssid, (const char*) password);
      }
    }
    WiFi.setHostname(tname);
  }
  if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(20000)) == pdTRUE) {
    ssid[0] = '\0';
    xSemaphoreGive(transmitSem);
  }
  // Check that we are connected
  while (true) {
    // if WiFi not connected
    while (WiFi.status() != WL_CONNECTED) {
      ssid[0] = '\0';
      trackPower = false;
      #ifdef TRACKPWR
      digitalWrite(TRACKPWR, HIGH);
      #endif
      #ifdef TRACKPWRINV
      digitalWrite(TRACKPWRINV, LOW);
      #endif
      delay (500);
      #ifdef TRACKPWR
      digitalWrite(TRACKPWR, LOW);
      #endif
      #ifdef TRACKPWRINV
      digitalWrite(TRACKPWRINV, HIGH);
      #endif
      if (use_multiwifi == 1) {
        Serial.println ("Will search for strongest WiFi signal");
        wifiMulti.run();
      }
      else {
        Serial.println ("Polling for first available WiFi connection");
        net_single_connect();
      }
      if (WiFi.status() != WL_CONNECTED) {
        delay (10000);
      }
      else {
        // Place name of actually selected SSID in ssid variable
        if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(20000)) == pdTRUE) {
          WiFi.SSID().toCharArray(ssid, sizeof(ssid));
          xSemaphoreGive(transmitSem);
        }
        MDNS.begin(tname);
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
    }
    // if Wifi connected but server not connected
    if (WiFi.status() == WL_CONNECTED && !client.connected()) {
      char server[65];
      int  port;

      trackPower = false;
      #ifdef TRACKPWR
      digitalWrite(TRACKPWR, LOW);
      #endif
      #ifdef TRACKPWRINV
      digitalWrite(TRACKPWRINV, HIGH);
      #endif
      // Use mdns to look up service
      if ((!client.connected()) && nvs_get_int ("mdns", 1) == 1) {
        server[0] = '\0';
        port = mdnsLookup ("_withrottle", server);
        if (port != 0 && strlen(server) > 0) {
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            Serial.printf ("MDNS look up service: %s:%d\r\n", server, port);
            xSemaphoreGive (displaySem);
          }
          connect2server (server, port);
        }
      }
      // first check our preferred connection index, ie the one with the same index ID as the connected network
      if ((!client.connected()) && preferRef < WIFINETS) {  // should not be "not found", but just in case of a bug...
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


bool net_single_connect()
{
  bool net_connected = false;
  char msgBuffer[40];
  char wifi_passwd[WIFINETS][SSIDLENGTH];
  char wifi_ssid[WIFINETS][SSIDLENGTH];
  
  if (WiFi.status() == WL_CONNECTED) return (true);
  for (uint8_t n=0; n<WIFINETS; n++) {
    sprintf (msgBuffer, "wifissid_%d", n);
    // use a different default for ssid for wifi 0 to other wifi
    if (n==0) nvs_get_string (msgBuffer, wifi_ssid[n], MYSSID, sizeof(wifi_ssid[0]));
    else nvs_get_string (msgBuffer, wifi_ssid[n], "none", sizeof(wifi_ssid[0]));
    if (strcmp (wifi_ssid[n], "none") != 0) {
      sprintf (msgBuffer, "wifipass_%d", n);
      nvs_get_string (msgBuffer, wifi_passwd[n], "none", sizeof(wifi_passwd[0]));
    }
  }
  WiFi.setHostname(tname);
  for (int loops = 1; loops>0 && !net_connected; loops--) {
    for (uint8_t n=0; n<WIFINETS && !net_connected; n++) {
      if (strcmp (wifi_ssid[n], "none") != 0) {
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          Serial.print   ("Polling network: ");
          Serial.println (wifi_ssid[n]);
          xSemaphoreGive (displaySem);
        }
        if (strcmp (wifi_passwd[n], "none") == 0) WiFi.begin(wifi_ssid[n], NULL);
        else WiFi.begin(wifi_ssid[n], wifi_passwd[n]);
        delay (2000);
        for (uint8_t z=0; z<14 && WiFi.status() != WL_CONNECTED; z++) delay (1000);
        if (WiFi.status() == WL_CONNECTED) {
          net_connected = true;
          if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(20000)) == pdTRUE) {
            strcpy (ssid, wifi_ssid[n]);
            xSemaphoreGive(transmitSem);
          }
          MDNS.begin(tname);
        }
        else {
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            Serial.print   ("Not connecting to WiFi network: ");
            Serial.println (wifi_ssid[n]);
            xSemaphoreGive (displaySem);
          }
        }
      }
    }
  }
  if (WiFi.status() == WL_CONNECTED) net_connected = true;
  else net_connected = false;
  return (net_connected);
}


void connect2server (char *server, int port)
{
  if (strcmp (server, "none") == 0) return;
  if (client.connect(server, port)) {
    if (strlen(server) < sizeof(remServerNode)-1) strcpy (remServerNode, server);
    else {
      strncpy (remServerNode, server, sizeof(remServerNode)-1);
      remServerNode[sizeof(remServerNode)-1] = '\0';
    }
    // Once connected, we can listen for returning packets
    xTaskCreate(receiveNetData, "Network_In", 4096, NULL, 4, NULL);
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
      cmdProtocol = nvs_get_int ("defaultProto", JMRI);
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print   ("Timeout on protocol identification: defaulting to ");
        Serial.print   (protoList[cmdProtocol]);
        Serial.println (" protocol");
        xSemaphoreGive(displaySem);
      }
      if (cmdProtocol==DCCPLUS) {
        strcpy (remServerType, "DCC++");
        dccPopulateLoco();
        dccPopulateTurnout();
        dccPopulateRoutes();
        if (nvs_get_int("sortData", SORTDATA) == 1) {
          sortLoco();
          sortTurnout();
          sortRoute();
        }
      }
      setInitialData();
    }
  }
}

// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mdns.html
// Use this pattern to return service list
void mdnsLookup (char *service)
{
  static const char * if_str[] = {"STA", "AP", "ETH", "MAX"};
  static const char * ip_protocol_str[] = {"V4", "V6", "MAX"};
  mdns_result_t *results = NULL;
  mdns_result_t *r;
  mdns_ip_addr_t * a = NULL;
  int i = 1, t;

  // esp_err_t mdns_query_ptr(const char *service_type, const char *proto, uint32_t timeout, size_t max_results, mdns_result_t **results)
  esp_err_t err = mdns_query_ptr(service, "_tcp", 3000, 20,  &results);

  if ((!err) && results != NULL && xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    r = results;
    while (r){
      Serial.printf("%d: Interface: %s, Type: %s\r\n", i++, if_str[r->tcpip_if], ip_protocol_str[r->ip_protocol]);
      if (r->instance_name){
        Serial.printf("  PTR : %s\r\n", r->instance_name);
      }
      if (r->hostname){
         Serial.printf("  SRV : %s.local:%u\r\n", r->hostname, r->port);
      }
      if (r->txt_count){
        Serial.printf("  TXT : [%u] ", r->txt_count);
        for(t=0; t<r->txt_count; t++){
          Serial.printf("%s=%s; ", r->txt[t].key, r->txt[t].value);
        }
        Serial.printf("\r\n");
      }
      a = r->addr;
      while (a){
        if(a->addr.type == IPADDR_TYPE_V6){
          Serial.printf("  AAAA: " IPV6STR "\r\n", IPV62STR(a->addr.u_addr.ip6));
        } else {
          Serial.printf("  A   : " IPSTR "\r\n", IP2STR(&(a->addr.u_addr.ip4)));
        }
        a = a->next;
      }
      r = r->next;
    }
    xSemaphoreGive(displaySem);
  }

  if (results!=NULL) {
    mdns_query_results_free (results);
  }
}

// mdns lookup of service, returns port number and writes the IP address to the buffer pointed to by *addr
int mdnsLookup (char *service, char *addr)
{
  int retVal = 0;

  mdns_result_t *results = NULL;
  mdns_result_t *r;
  mdns_ip_addr_t * a = NULL;

  addr[0] = '\0';
  esp_err_t err = mdns_query_ptr(service, "_tcp", 3000, 20,  &results);
  if ((!err) && results != NULL) {
    r = results;
    while (r && addr[0]=='\0'){
      retVal = r->port;
      a = r->addr;
      while (a){
        if(a->addr.type == IPADDR_TYPE_V6){
          sprintf(addr, IPV6STR, IPV62STR(a->addr.u_addr.ip6));
        } else {
          sprintf(addr, IPSTR,   IP2STR(&(a->addr.u_addr.ip4)));
        }
        a = a->next;
      }
      r = r->next;
    }
  }

  if (results!=NULL) {
    mdns_query_results_free (results);
  }
  return (retVal);
}

// Transmit a packet
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
#else
void connectionManager()
{
  if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    serial_dev.begin (DCCSPEED, SERIAL_8N1, DCCRX, DCCTX);
    cmdProtocol = DCCPLUS;
    delay (1000);
    // Once connected, we can listen for returning packets
    xTaskCreate(receiveNetData, "Network_In", 4096, NULL, 4, NULL);
  }
}

// Transmit a packet
void txPacket (char *header, char *pktData)
{
  if (header!=NULL) serial_dev.write (header, strlen(header));
  serial_dev.write (pktData, strlen(pktData));
  serial_dev.write ("\r\n", 2);
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
#endif

void txPacket (char *pktData)
{
  txPacket (NULL, pktData);
}
