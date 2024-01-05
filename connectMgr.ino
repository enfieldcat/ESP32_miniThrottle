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
// static WiFiMulti wifiMulti;

void connectionManager(void *pvParameters)
{
  char paramName[SSIDLENGTH];
  char apssid[SSIDLENGTH];
  char appass[SSIDLENGTH];
  char stassid[SSIDLENGTH];
  char stapass[SSIDLENGTH];
  uint8_t index     = 0;
  uint8_t scanIndex = 0;
  uint8_t staPref   = 0;
  bool networkFound = false;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s connectionManager(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }

  // Scan networks at startup just for the record
  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT*10)) == pdTRUE) {
    wifi_scanNetworks(true);
    xSemaphoreGive(tcpipSem);
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  // Start AP if required
  if ((nvs_get_int("WiFiMode", defaultWifiMode) & 2) > 0) {
    nvs_get_string ("APname", apssid, tname,  sizeof(apssid));
    nvs_get_string ("APpass", appass, "none", sizeof(appass));
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s WiFi Access Point starting: %s\r\n", getTimeStamp(), apssid);
      xSemaphoreGive (consoleSem);
    }
    if (strlen(appass)<8) 
      WiFi.softAP(apssid, NULL, nvs_get_int ("apChannel", APCHANNEL), 0, nvs_get_int("apClients", DEFAPCLIENTS));
    else
      WiFi.softAP(apssid, appass, nvs_get_int ("apChannel", APCHANNEL), 0, nvs_get_int("apClients", DEFAPCLIENTS));
    APrunning  = true;
    MDNS.begin(tname);
  }
  while (true) {
    // Check if WiFi station mode needs to be connected and is connected
    if ((nvs_get_int("WiFiMode", defaultWifiMode) & 1) > 0) {
      if (WiFi.status() != WL_CONNECTED) {
        if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT*10)) == pdTRUE) {
          wifi_scanNetworks();
          xSemaphoreGive(tcpipSem);
        }
        else semFailed ("tcpipSem", __FILE__, __LINE__);
        staPref = nvs_get_int ("staConnect", 0);
        // Preference is to only access known access points in a strict order of preference
        if (staPref == 0) {
          networkFound = false;
          for (index = 0; index < WIFINETS && !networkFound; index++) {
            sprintf (paramName, "wifissid_%d", index);
            if (index == 0) nvs_get_string (paramName, stassid, MYSSID, sizeof(stassid));
            else nvs_get_string (paramName, stassid, "none", sizeof(stassid));
            if (strcmp (stassid, "none") != 0) {
              for (scanIndex = 0; scanIndex < numberOfNetworks && !networkFound; scanIndex++) {
                if (strcmp (stassid, WiFi.SSID(scanIndex).c_str()) == 0) {
                  sprintf (paramName, "wifipass_%d", index);
                  nvs_get_string (paramName, stapass, "none", sizeof(stapass));
                  networkFound = true;
                }
              }
            }
          }
        }
        // preference is to probe APs in sequence. only option if SSID is hidden
        else if (staPref == 3) {
          static uint8_t nextWifi = 0;

          // previously found but dropped connection
          if (nextWifi >= WIFINETS) {
            nextWifi = 0;
            // nothing found
            if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("%s ConnectionManager: No networks defined\r\n", getTimeStamp());
              xSemaphoreGive(consoleSem);
            }
          }
          // find next candidate
          networkFound = false;
          for (index = nextWifi++; index < WIFINETS && !networkFound; index++) {
            sprintf (paramName, "wifissid_%d", index);
            if (index == 0) nvs_get_string (paramName, stassid, MYSSID, sizeof(stassid));
            else nvs_get_string (paramName, stassid, "none", sizeof(stassid));
            if (strcmp (stassid, "none") != 0) {
              sprintf (paramName, "wifipass_%d", index);
              nvs_get_string (paramName, stapass, "none", sizeof(stapass));
              networkFound = true;
            }
          }
        }
        // Preference is to access access-points by order of signal strength preference
        else {
          int entryNum[numberOfNetworks];
          int strength[numberOfNetworks];
          int limit = numberOfNetworks - 1;
          int exchanger;
          bool swapped = true;

          // Prime arrays with SSID details
          for (int n=0;n < numberOfNetworks; n++) {
            entryNum[n] = n;
            strength[n] = WiFi.RSSI(n);
          }
          // simple bubble sort
          while (swapped && limit>0) {
            swapped = false;
            for (int n=0; n<limit; n++) {
              if (strength[n] < strength[n+1]) {
                swapped       = true;
                exchanger     = entryNum[n];
                entryNum[n]   = entryNum[n+1];
                entryNum[n+1] = exchanger;
                exchanger     = strength[n];
                strength[n]   = strength[n+1];
                strength[n+1] = exchanger;
              }
            }
          }
          // Now check if they meet our selection criteria
          for (scanIndex = 0; scanIndex < numberOfNetworks && !networkFound; scanIndex++) {
            exchanger = entryNum[scanIndex];
            // permit any open network?!! - Yikes low security bar.
            if (staPref==2 && WiFi.encryptionType(exchanger) == WIFI_AUTH_OPEN) {
              strcpy (stassid, WiFi.SSID(exchanger).c_str());
              strcpy (stapass, "none");
              networkFound = true;
            }
            // only access known access points
            else {
              for (index = 0; index < WIFINETS && !networkFound; index++) {
                sprintf (paramName, "wifissid_%d", index);
                if (index == 0) nvs_get_string (paramName, stassid, MYSSID, sizeof(stassid));
                else nvs_get_string (paramName, stassid, "none", sizeof(stassid));
                if (strcmp (stassid, "none") != 0) {
                  if (strcmp (stassid, WiFi.SSID(exchanger).c_str()) == 0) {
                    sprintf (paramName, "wifipass_%d", index);
                    nvs_get_string (paramName, stapass, "none", sizeof(stapass));
                    networkFound = true;
                  }
                }
              }
            }
          }
        }
        // Connect to the network
        if (networkFound) {
          if (debuglevel>0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s WiFi Station mode connecting to: %s\r\n", getTimeStamp(), stassid);
            xSemaphoreGive (consoleSem);
          }
          if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT*10)) == pdTRUE) {
            strcpy (ssid, stassid);
            if (strcmp (stapass, "none") == 0) WiFi.begin(stassid, NULL);
            else WiFi.begin(stassid, stapass);
            xSemaphoreGive(tcpipSem);
          }
          delay (TIMEOUT);
          if ((!APrunning) && xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT*10)) == pdTRUE) {
            MDNS.begin(tname);
            xSemaphoreGive(tcpipSem);
          }
        }
        // Warn if no candidates found
        else {
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s WiFi Station mode: no access point found.\r\n", getTimeStamp());
            xSemaphoreGive (consoleSem);
          }
          if ((nvs_get_int("WiFiMode", defaultWifiMode) & 2) == 0) {
            #ifdef WEBLIFETIME
            startWeb = true;
            #endif
            #ifdef RELAYPORT
            startRelay = true;
            #endif
          }
          ssid[0] = '\0';
        }
      }
    }
    // Does the web server need (re)starting?
    #ifdef WEBLIFETIME
    if (startWeb && (APrunning || WiFi.status() == WL_CONNECTED)) {
      static int8_t num = 0;
      static char label[16];
      startWeb = false;
      sprintf (label, "webServer%d", num++);
      xTaskCreate(webListener, label, 6144, NULL, 4, NULL);
    }
    #endif
    // Does the relay need to be restarted?
    #ifdef RELAYPORT
    if (startRelay && (APrunning || WiFi.status() == WL_CONNECTED)) {
      if (relayMode != NORELAY) {
        static int8_t num = 0;
        static char label[16];
        sprintf (label, "relayServer%d", num++);
        xTaskCreate(relayListener, label, 6144, NULL, 4, NULL);
      }
      else {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Skipping relay startup, mode is No-Relay\r\n", getTimeStamp());
          xSemaphoreGive (consoleSem);
        }
      }
      startRelay = false;
    }
    #endif
    // Does the connection to the Control System need to be estabilished?
    #ifndef SERIALCTRL
    // if Wifi connected but server not connected
    if ((APrunning || WiFi.status() == WL_CONNECTED) && !client.connected()) {
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
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s MDNS look up service found: %s:%d\r\n", getTimeStamp(), server, port);
            xSemaphoreGive (consoleSem);
          }
          connect2server (server, port);
          delay (TIMEOUT);
        }
        else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s MDNS look up: no services found.\r\n", getTimeStamp());
          xSemaphoreGive (consoleSem);
        }
      }
      // now work through server list until we connect or just give up
      for (index = 0; index <WIFINETS && !client.connected(); index++) {
        sprintf (paramName, "server_%d", index);
        nvs_get_string (paramName, server, "none", sizeof(server));
        if (strcmp (server, "none") != 0) {
          sprintf (paramName, "port_%d", index);
          port = nvs_get_int (paramName, PORT);
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Attempting connection to: %s:%d\r\n", getTimeStamp(), server, port);
            xSemaphoreGive (consoleSem);
          }
          connect2server (server, port);
          delay(TIMEOUT);
        }
      }
    }
    #endif // NOT SERIALCTRL
    delay (10000);
  }
  vTaskDelete( NULL );
}


char* wifi_EncryptionType(wifi_auth_mode_t encryptionType)
{
  switch (encryptionType) {
    case (WIFI_AUTH_OPEN):
      return (char*) "Open";
      break;
    case (WIFI_AUTH_WEP):
      return (char*) "WEP";
      break;
    case (WIFI_AUTH_WPA_PSK):
      return (char*) "WPA_PSK";
      break;
    case (WIFI_AUTH_WPA2_PSK):
      return (char*) "WPA2_PSK";
      break;
    case (WIFI_AUTH_WPA_WPA2_PSK):
      return (char*) "WPA_WPA2_PSK";
      break;
    case (WIFI_AUTH_WPA2_ENTERPRISE):
      return (char*) "WPA2_ENTERPRISE";
      break;
    }
}

void wifi_scanNetworks()
{
if (debuglevel>1) wifi_scanNetworks (true);
else wifi_scanNetworks (false);
}


void wifi_scanNetworks(bool echo)
{
  numberOfNetworks = WiFi.scanNetworks();
  if (echo && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf("Number of networks found: %d\r\n", numberOfNetworks);
    Serial.printf("%-16s %8s %-17s Channel Type\r\n", "Name", "Strength", "Address");
    for (int i = 0; i < numberOfNetworks; i++) {
      Serial.printf ("%-16s %8d %-17s %7d %s\r\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.BSSIDstr(i).c_str(), WiFi.channel(i), wifi_EncryptionType(WiFi.encryptionType(i)));
    }
    xSemaphoreGive(consoleSem);
  }
}


#ifndef SERIALCTRL
void connect2server (char *server, int port)
{
  uint32_t clientTimeout;
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s connect2server(%s, %d)\r\n", getTimeStamp(), server, port);
    xSemaphoreGive(consoleSem);
  }

  clientTimeout = nvs_get_int ("clientTimeout", 5000);
  if (strcmp (server, "none") == 0) return;
  if (client.connect(server, port, clientTimeout)) {
    client.setNoDelay (true);
    initialDataSent = false;
    cmdProtocol = nvs_get_int ("defaultProto", WITHROT);  // Go straight to default protocol
    if (strlen(server) < sizeof(remServerNode)-1) strcpy (remServerNode, server);
    else {
      strncpy (remServerNode, server, sizeof(remServerNode)-1);
      remServerNode[sizeof(remServerNode)-1] = '\0';
    }
    // Once connected, we can listen for returning packets
    xTaskCreate(receiveNetData, "Network_In", 4096, NULL, 4, NULL);
    // Print diagnostic
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf  ("\r\n%s Connected to server: %s:%d\r\n", getTimeStamp(), server, port);
      xSemaphoreGive(consoleSem);
    }
    if (cmdProtocol==DCCEX) {
      strcpy (remServerType, "DCC-Ex");
      initDccEx();
    }
  }
}
#endif // NOT SERIALCTRL

// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mdns.html
// Use this pattern to return service list
void mdnsLookup (const char *service)
{
  static const char * if_str[] = {"STA", "AP", "ETH", "MAX"};
  static const char * ip_protocol_str[] = {"V4", "V6", "MAX"};
  mdns_result_t *results = NULL;
  mdns_result_t *r;
  mdns_ip_addr_t * a = NULL;
  int i = 1, t;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mdnsLookup(%s)\r\n", getTimeStamp(), service);
    xSemaphoreGive(consoleSem);
  }

  // esp_err_t mdns_query_ptr(const char *service_type, const char *proto, uint32_t timeout, size_t max_results, mdns_result_t **results)
  esp_err_t err = mdns_query_ptr(service, "_tcp", 3000, 20,  &results);

  if ((!err) && results != NULL && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
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
    xSemaphoreGive(consoleSem);
  }

  if (results!=NULL) {
    mdns_query_results_free (results);
  }
}

// mdns lookup of service, returns port number and writes the IP address to the buffer pointed to by *addr
int mdnsLookup (const char *service, char *addr)
{
  int retVal = 0;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mdnsLookup(%s, %x)\r\n", getTimeStamp(), service, addr);
    xSemaphoreGive(consoleSem);
  }

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

#ifndef SERIALCTRL
// Transmit a packet
void txPacket (const char *header, const char *pktData)
{
  if (!client.connected()) return;
  if (pktData == NULL) return;
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (header==NULL) Serial.printf ("%s txPacket(NULL, %s)\r\n", getTimeStamp(), pktData);
    else Serial.printf ("%s txPacket(%s, %s)\r\n", getTimeStamp(), header, pktData);
    xSemaphoreGive(consoleSem);
  }

  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (header != NULL) {
      client.print (header);
    }
    client.println (pktData);
    client.flush();
    xSemaphoreGive(tcpipSem);
    if (diagIsRunning) {
      diagEnqueue ('p', (char *) "--> ", false);
      if (header != NULL) {
        diagEnqueue ('p', (char*) header, false);
      }
      diagEnqueue ('p', (char*) pktData, true);
    }
    if (showPackets) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print ("--> ");
        if (header != NULL) Serial.print (header);
        Serial.println (pktData);
        xSemaphoreGive(consoleSem);
      }
    }
  }
  else {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s ERROR: Could not get semaphore to transmit data\r\n", getTimeStamp());
      xSemaphoreGive(consoleSem);
    }
  }
}
#endif // NOT SERIALCTRL
#endif // USEWIFI


#ifdef SERIALCTRL
//void connectionManager()
// Rn as a separate thread in case there are start up delays in running population or connection routines
void serialConnectionManager(void *pvParameters)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s serialConnectionManager(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }

  // Connect to DCC-Ex over serial connection
  if (xSemaphoreTake(serialSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    serial_dev.begin (DCCSPEED, SERIAL_8N1, DCCRX, DCCTX);
    cmdProtocol = DCCEX;
    xSemaphoreGive(serialSem);
    delay (1500);
    // Once connected, we can listen for returning packets
    xTaskCreate(receiveNetData, "Network_In", 5120, NULL, 4, NULL);
  }
  else semFailed ("serialSem", __FILE__, __LINE__);
  delay (1500);
  initDccEx();
  vTaskDelete( NULL );
}

// Transmit a packet
void txPacket (const char *header, const char *pktData)
{
  if (pktData == NULL) return;
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (header==NULL) Serial.printf ("%s txPacket(NULL, %s)\r\n", getTimeStamp(), pktData);
    else Serial.printf ("%s txPacket(%s, %s)", getTimeStamp(), header, pktData);
    xSemaphoreGive(consoleSem);
  }
  if (xSemaphoreTake(serialSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (header!=NULL) {
      serial_dev.write ((const uint8_t*)header, strlen(header));
    }
    serial_dev.write ((const uint8_t*)pktData, strlen(pktData));
    serial_dev.write ((const uint8_t*)"\r\n", 2);
    xSemaphoreGive(serialSem);
    if (diagIsRunning) {
      diagEnqueue ('p', (char *) "--> ", false);
      if (header!=NULL) {
        diagEnqueue ('p', (char*) header, false);
      }
      diagEnqueue ('p', (char*) pktData, true);
    }
    if (showPackets) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print ("--> ");
        if (header != NULL) Serial.print (header);
        Serial.println (pktData);
        xSemaphoreGive(consoleSem);
      }
    }
    #ifdef RELAYPORT
    if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      localoutPkts ++;
      xSemaphoreGive(relaySvrSem);
    }
    else semFailed ("relaySvrSem", __FILE__, __LINE__);
    #endif
  }
  else semFailed ("serialSem", __FILE__, __LINE__);
}
#endif // SERIALCTRL


void initDccEx()
{
  // Send initial data
  setInitialData();
  // Load stored data from Non-Volatile Storage (NVS)
  dccPopulateLoco();
  if (nvs_get_int("sortData", SORTDATA) == 1) sortLoco();
  dccPopulateTurnout();
  if (nvs_get_int("sortData", SORTDATA) == 1) sortTurnout();
  dccPopulateRoutes();
  if (nvs_get_int("sortData", SORTDATA) == 1) sortRoute();
}


void txPacket (const char *pktData)
{
  txPacket (NULL, pktData);
}
