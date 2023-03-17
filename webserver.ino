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



#ifdef WEBLIFETIME
/* --------------------------------------------------------------------------- *\
 *
 * This is the main web listener
 *
\* --------------------------------------------------------------------------- */
void webListener(void *pvParameters)
{
  static WiFiClient webServerClient[MAXWEBCONNECT];
  uint8_t i = 1;
  uint8_t j = 0;
  uint64_t lastActTime = esp_timer_get_time();
  uint64_t exitTime = (uS_TO_S_FACTOR * 60.0) * nvs_get_int ("webTimeOut", WEBLIFETIME);
  bool notAssigned = true;
 
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s webListener(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  // Want only one web server thread at a time
  // if starting a new one, wait for existing ones to finish
  while (i > 0 && j < 120) {
    if (xSemaphoreTake(webServerSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      i = webServerCount;
      if (i == 0) {
        webServerCount++;
        j = 0;
        }
      else j++;
      xSemaphoreGive(webServerSem);
      if (j > 0) delay(1000);
    }
    else semFailed ("webServerSem", __FILE__, __LINE__);
  }
  if (j == 120) {
    // we have waited 120 seconds for old web server to stop, it has not
    // therefore we will terminate this instance
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s New instance of Webserver stopped after waiting for previous one to complete\r\n", getTimeStamp());
      xSemaphoreGive(consoleSem);
    }
    vTaskDelete( NULL );
  }
  // Wait for WiFi connection
  while ((!APrunning) && WiFi.status() != WL_CONNECTED) delay (1000);
  delay (TIMEOUT);

  // Should we advertise as a service?
  #if WEBLIFETIME == 0
  mdns_txt_item_t txtrecord[] = {{(char*)"server", (char*)PRODUCTNAME}, {(char*)"version", (char*)VERSION}};
  mdns_service_add(NULL, "_http", "_tcp", webPort, txtrecord, 2);
  #endif

  // Listen for incoming connections
  webIsRunning = true;
  webServer->begin();
  // webServer->setNoDelay(true);
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    uint16_t tint =  nvs_get_int ("webTimeOut", WEBLIFETIME);
    Serial.printf ("%s Webserver started, port %d, ", getTimeStamp(), webPort);
    if (tint == 0) Serial.printf ("no idle expiry\r\n");
    else Serial.printf ("will stop after %d minutes inactivity\r\n", tint);
    xSemaphoreGive(consoleSem);
  }
  // generate the admin string as a once off task
  {
    size_t outputLength;
    char t_userName[64];
    char t_userPass[64];
    nvs_get_string ("webuser", t_userName, WEBUSER, sizeof(t_userName));
    nvs_get_string ("webpass", t_userPass, WEBPASS, sizeof(t_userPass));
    strcat (t_userName, ":");
    strcat (t_userName, t_userPass);
    unsigned char * encoded = base64_encode((const unsigned char *)t_userName, strlen(t_userName), &outputLength);
    if (outputLength < sizeof(webCredential)) {
      strncpy (webCredential, (const char*) encoded, outputLength);
      webCredential[outputLength] = '\0';
      for (uint8_t n=0; n < strlen(webCredential); n++) if (webCredential[n]<=' ') webCredential[n]='\0';
    }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Encoded web credential too long, configure shorter webuser or webpass.\r\n", getTimeStamp());
      xSemaphoreGive(consoleSem);
    }
    free (encoded);
  }
  // Wait for incoming connections for as long as there is a connection
  while (webIsRunning) {
    if (APrunning || WiFi.status() == WL_CONNECTED) {
      if (webServer->hasClient()) {
        notAssigned = true;
        for(i = 0; notAssigned && i < MAXWEBCONNECT; i++){
          if (!webServerClient[i] || !webServerClient[i].connected()){
            if(webServerClient[i]) webServerClient[i].stop();
            webServerClient[i] = webServer->available();
            xTaskCreate(webHandler, "webHandler", 7168, &webServerClient[i], 4, NULL);
            i = MAXWEBCONNECT + 1;
            lastActTime = esp_timer_get_time();
            notAssigned = false;
          }
        }
      }
      if (i == MAXWEBCONNECT) webServer->available().stop();  // no worker threads available
      delay (100); // pause to use fewer CPU cycles while waiting for connecton
    }
    else webIsRunning = false;
    if (exitTime > 0 && (esp_timer_get_time() - lastActTime) > exitTime) {
      webIsRunning = false;
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Webserver terminating due to inactivity\r\n", getTimeStamp());
        xSemaphoreGive(consoleSem);
      }
    }
  }
  // shutdown webserver
  webServer->close();
  webServer->stop();
  webServer->end();
  if (exitTime == 0 || (esp_timer_get_time() - lastActTime) < exitTime) {
    startWeb = true;
  }
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Webserver stopped\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (xSemaphoreTake(webServerSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    webServerCount--;
    xSemaphoreGive(webServerSem);
  }
  else semFailed ("webServerSem", __FILE__, __LINE__);
  vTaskDelete( NULL );
}


/* --------------------------------------------------------------------------- *\
 *
 * HTTP headers all follow a fairly close template, thus we will use this
 * Stick to HTTP 1.0, HTTP 1.1 and later require a lot more complexity than we'll offer.
 *
\* --------------------------------------------------------------------------- */
const char* mkWebHeader (WiFiClient *myClient, int code, uint8_t format, bool keepAlive)
{
  const char *shortMsg[]   = {"OK", "Server Error"         , "Not Found"     , "Unauthorized" };
  const char *errMessage[] = {"OK", "Internal server error", "Page not found", "Authorization required for access" };
  const char *msgType[]    = {"text/html", "text/plain", "text/css", "image/ico", "image/jpeg", "image/png" };
  char cacheStr[50];
  char keepAStr[30];
  char authStr[60];
  int  msgIndex;
  uint32_t cacheTimeout = (nvs_get_int ("cacheTimeout", WEBCACHE)) * 60;
  
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebHeader(%x, %d, %d, bool)\r\n", getTimeStamp(), myClient, code, format);
    xSemaphoreGive(consoleSem);
  }
  switch (code) {
    case 200: msgIndex = 0; break;
    case 401: msgIndex = 3; break;
    case 404: msgIndex = 2; break;
    default: code = 500; msgIndex = 1; break;
  }
  if (format>1) { sprintf (cacheStr, "Cache-Control: max-age=%d, public\r\n", cacheTimeout); }
  else { sprintf (cacheStr, "Pragma: no-cache\r\nCache-Control: no-cache\r\n"); }
  if (keepAlive) sprintf (keepAStr, "Connection: keep-alive\r\n");
  else sprintf (keepAStr, "Connection: close\r\n");
  if (code == 401) { sprintf (authStr, "WWW-Authenticate: Basic realm=\"%s\"\r\n", PRODUCTNAME); }
  else authStr[0] = '\0';
  // Careful: Each printf becomes a TCP packet fragment, combine packets where practical
  // excessive fragmentation is inefficient
  myClient->printf ("HTTP/1.0 %d %s\r\nContent-type: %s\r\nServer: %s\r\n%s%s%s\r\n", code, shortMsg[msgIndex], msgType[format], PRODUCTNAME, cacheStr, keepAStr, authStr);
  if (showWebHeaders && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("web  --> HTTP/1.0 %d %s\r\n", code, shortMsg[msgIndex]);
    Serial.printf ("web  --> Content-type: %s\r\n", msgType[format]);
    Serial.printf ("web  --> Server: %s\r\n", PRODUCTNAME);
    if (format>1) {
      Serial.printf ("web  --> Cache-Control: max-age=1800, public\r\n");
    }
    else {
      Serial.printf ("web  --> Pragma: no-cache\r\n");
      Serial.printf ("web  --> Cache-Control: no-cache\r\n");
    }
    if (keepAlive) Serial.printf ("web  --> Connection: keep-alive\r\n");
    else Serial.printf ("web  --> Connection: close\r\n");
    if (code == 401) {
      Serial.printf ("web  --> WWW-Authenticate: Basic realm=\"%s\"\r\n", PRODUCTNAME);
    }
    xSemaphoreGive(consoleSem);
  }
  return (errMessage[msgIndex]);
}

/* --------------------------------------------------------------------------- *\
 *
 * Handler for each new http connection
 * Each handler forms its own thread
 *
\* --------------------------------------------------------------------------- */
void webHandler(void *pvParameters)
{
  WiFiClient *myClient = (WiFiClient*) pvParameters;
  char *content = NULL;
  char myUri[BUFFSIZE];
  char inBuffer[BUFFSIZE];
  char hostName[64];
  char inChar, lastChar;
  bool collectHeader = true;
  bool cont = true;
  bool keepAlive = true;
  bool authenticated = false;
  uint8_t myID;
  uint16_t contentLength = 0;
  uint16_t tPtr = 0;
  uint16_t lineCount = 0;
  uint16_t dataSize  = 0;
  fs::FS fs = (fs::FS) SPIFFS;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s webHandler(%x)\r\n", getTimeStamp(), myClient);
    xSemaphoreGive(consoleSem);
  }
  myUri[0] = '\0';
  if (xSemaphoreTake(webServerSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (++webClientCount > maxWebClientCount) maxWebClientCount = webClientCount;
    myID = webClientCount;
    xSemaphoreGive(webServerSem);
  }
  else semFailed ("webServerSem", __FILE__, __LINE__);
  if (showWebHeaders && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("web%d --- OPEN %d\r\n", myID, myID);
    xSemaphoreGive(consoleSem);
  }
  keepAlive = true;
  while (keepAlive && webConnState(myClient, 1)) {
    authenticated = false;
    hostName[0] = '\0';
    dataSize = 0;
    if (xSemaphoreTake(webServerSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      if (webClientCount >= (MAXWEBCONNECT-1)) keepAlive = false; // avoid hogging of web connections.
      xSemaphoreGive(webServerSem);
    }
    while (collectHeader && webConnState(myClient, 1)) {
      for (uint8_t n=0; n<10 && webConnState(myClient, 3); n++) {
        delay(20); // delay if there is no data immediately available
      }
      if (collectHeader && webConnState(myClient, 2)) {
        if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          inChar = myClient->read();
          xSemaphoreGive(tcpipSem);
        }
        else semFailed ("tcpipSem", __FILE__, __LINE__);
        if (inChar=='\n' || inChar == '\r') {
          if (lineCount == 0 && tPtr == 0) ; // White-space-fluff at start of request - ignore
          else if (inChar == '\n' && lastChar == '\r'); // ignore \n if it follows \r
          else {
            if (inChar == lastChar) collectHeader = false;
            else {
              if (tPtr==sizeof(inBuffer)) inBuffer[tPtr-1] = '\0';
              else inBuffer[tPtr] = '\0';
              tPtr = 0;
              if (showWebHeaders) Serial.printf ("web%d <-- %s\r\n", myID, inBuffer);
              lastChar = inChar;
              lineCount++;
              if (lineCount == 1) {
                if (strncmp(inBuffer, "GET ", 4) == 0 || strncmp(inBuffer, "POST ", 5) == 0) {
                  uint16_t n=4;
                  while (inBuffer[n] == ' ') n++; // waste additional white space
                  for (uint16_t j=n; j<BUFFSIZE && cont; j++) {
                    if (inBuffer[j] == ' ' || inBuffer[j] > 126 || inBuffer[j] < 32) {
                      cont = false;
                      inBuffer[j] = '\0';
                    }
                  }
                  strcpy (myUri, &inBuffer[n]);
                }
                else {
                  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    Serial.printf  ("%s Unrecognised web request: ", getTimeStamp());
                    Serial.println (inBuffer);
                    myUri[0] = '\0';
                    xSemaphoreGive(consoleSem);
                  }
                }
              }
              else {
                if (strncmp (inBuffer, "Authorization: Basic ", 21) == 0) {
                  if (strcmp (&inBuffer[21], webCredential) == 0) {
                    authenticated = true;
                  }
                }
                uint8_t n = strlen(inBuffer);
                for (uint8_t j=0; j<n; j++) {
                  inBuffer[j] = tolower(inBuffer[j]);
                }
                if (strncmp (inBuffer, "connection: ", 12) == 0) {
                  if (strncmp (&inBuffer[12], "keep-alive", 10) == 0) keepAlive = true;
                  else keepAlive = false;
                }
                else if (strncmp (inBuffer, "content-length: ", 16) == 0) {
                  contentLength = util_str2int (&inBuffer[16]);
                }
                else if (strncmp (inBuffer, "host: ", 6) == 0) {
                  if (strlen(inBuffer) > (sizeof(hostName) + 6)) inBuffer[sizeof(hostName)+5] = '\0';
                  strcpy (hostName, &inBuffer[6]);
                }
              }
            }
          }
        }
        else {
          lastChar = inChar;
          if (tPtr<sizeof(inBuffer)) { inBuffer[tPtr++] = inChar; }
        }
      }
    }
    keepAlive = false;
    // for post data we expect non-zero length content - allocate space for it and read in
    while (myClient->available()>0 && (myClient->peek()=='\n' || myClient->peek()=='\r')) myClient->read(); // Waste trailing cr, lf sequences
    if (contentLength > 0 && myClient->connected()) {
      char inCode[2];
      uint16_t n = 0;
      uint16_t i = 0;
  
      content = (char*) malloc(contentLength+1); // Add 1 for a terminating null
      for (n=0, i=0; n<contentLength && myClient->available()>0; n++) {
        inChar = myClient->read();
        if (inChar == '%' && myClient->available()>0) {
          inCode[0] = myClient->read();
          inCode[1] = myClient->read();
          n += 2;
          inChar = 0;
          for (uint8_t q=0; q<sizeof(inCode); q++){
            inChar = inChar * 16;
            if (inCode[q]>='0' && inCode[q]<='9') inChar += inCode[q]-'0';
            else if (inCode[q]>='a') inChar += inCode[q] - ('a' - 10);
            else inChar += inCode[q] - ('A' - 10);
          }
        }
        else if (inChar == '+') inChar = ' ';
        else if (inChar == '&') inChar = '\0';
        content[i++] = inChar;
      }
      content[i++] = '\0';
      dataSize = i;
      if (showWebHeaders) mt_dump (content, dataSize);
    }
    if (myClient->connected() && myUri[0]!='\0') {
      // now call a hander appropriate to the thing being requested
      if (strcmp (myUri, "/") == 0) mkWebSysStat(myClient, keepAlive, false, NULL, 0);  // Unauthenticated stats page
      else if (strncmp (myUri, "/status", 7) == 0) {
        if (authenticated) mkWebSysStat(myClient, keepAlive, true, content, dataSize);  // Authenticated stats page
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      else if (strncmp (myUri, "/edit/", 6) == 0) {                            // Edit a file
        if (authenticated) mkWebEditor (myClient, myUri, keepAlive, hostName);
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      else if (strcmp  (myUri, "/save") == 0) {                                // Save general config settings
        if (authenticated) {
          keepAlive = false;
          mkWebSave (myClient, content, dataSize, keepAlive);
          }
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      else if (strcmp  (myUri, "/files") == 0) {                               // List internal files
        if (authenticated) {
          mkWebFileIndex (myClient);
          }
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      else if (strcmp  (myUri, "/config") == 0) {                              // Edit general config settings
        if (authenticated) {
          mkWebConfig (myClient, keepAlive);
          }
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      else if (strcmp  (myUri, "/wifi") == 0) {                                // Update WiFi networks and passwords
        if (authenticated) {
          mkWebWiFi (myClient, content, dataSize, keepAlive);
          }
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      else if (strcmp  (myUri, "/password") == 0) {                            // Update local admin password
        if (authenticated) {
          mkWebAdmin (myClient, content, dataSize, keepAlive);
          }
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      else if (strcmp  (myUri, "/functions") == 0) {                           // Update function maps
        if (authenticated) {
          mkWebFunctionMap (myClient, content, dataSize, keepAlive);
          }
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      #ifdef RELAYPORT
      else if (strcmp  (myUri, "/fastclock") == 0) {                           // configure fast-clock
        if (authenticated) {
          mkFastClock (myClient, content, dataSize, keepAlive);
          }
        else mkWebError (myClient, 401, myUri, keepAlive);
        }
      #endif
      else if (fs.exists(myUri)) {                                             // Display a file if it exists
        File file = fs.open(myUri);
        if(file){
          int format = 1;
          if (strlen(myUri)>6 && strcmp (&myUri[strlen(myUri)-4], ".htm") == 0 || strcmp (&myUri[strlen(myUri)-5], ".html") == 0) format = 0;
          else if (strlen(myUri)>6 && strcmp (&myUri[strlen(myUri)-4], ".css") == 0) format = 2;
          else if (strlen(myUri)>6 && strcmp (&myUri[strlen(myUri)-4], ".ico") == 0) format = 3;
          else if (strlen(myUri)>6 && strcmp (&myUri[strlen(myUri)-4], ".jpg") == 0 || strcmp (&myUri[strlen(myUri)-5], ".jpeg") == 0) format = 4;
          else if (strlen(myUri)>6 && strcmp (&myUri[strlen(myUri)-4], ".png") == 0) format = 5;
          mkWebHeader (myClient, 200, format, keepAlive);
          webDumpFile (myClient, &file, false);
          file.close();
        }
        else mkWebError(myClient, 404, myUri, keepAlive);                       // last resort: 404 error, file not found.
      }
      else {
        mkWebError (myClient, 404, myUri, keepAlive);
      }
      myClient->flush();
      myUri[0] = '\0';
    }
    if (content != NULL) {
      free(content);
      content = NULL;
    }
    contentLength = 0;
  }

  // Close this handler
  if (xSemaphoreTake(webServerSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    webClientCount--;
    xSemaphoreGive(webServerSem);
  }
  else semFailed ("webServerSem", __FILE__, __LINE__);
  myClient->stop();
  if (showWebHeaders && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("web%d --- CLOSED %d\r\n", myID, myID);
    xSemaphoreGive(consoleSem);
  }
  vTaskDelete( NULL );
}


bool webConnState (WiFiClient *myClient, uint8_t chkmode)
{
  bool retval = false;
  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    switch (chkmode) {
    case 1: retval = (APrunning || WiFi.status() == WL_CONNECTED) && myClient->connected();
            break;
    case 2: retval = (APrunning || WiFi.status() == WL_CONNECTED) && myClient->connected() && myClient->available()>0;
            break;
    case 3: retval = (APrunning || WiFi.status() == WL_CONNECTED) && myClient->connected() && myClient->available()<1;
            break;
    }
    xSemaphoreGive(tcpipSem);
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  return (retval);
}



/* --------------------------------------------------------------------------- *\
 *
 * Scan the data returned for a specific variable, and return to offset
 *    contents = Data returned by POST
 *    varName  = Variable to search for
 *    dataSize = Max size of data to scan
 *
\* --------------------------------------------------------------------------- */
char* webScanData (char* contents, const char* varName, const uint16_t dataSize)
{
  char *retVal = NULL;
  char myVar[32];
  uint16_t offset = 0;
  uint8_t varLen = 0;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s webScanData(%x, %s, %d)\r\n", getTimeStamp(), contents, varName, dataSize);
    xSemaphoreGive(consoleSem);
  }
  strcpy (myVar, varName);
  strcat (myVar, "=");
  varLen = strlen(myVar);
  while (offset<dataSize && retVal==NULL) {
    while (offset < dataSize && contents[offset] == '\0' || contents[offset] == '\n') offset++;  // skip over space
    if (offset<dataSize) {
      if (strncmp (myVar, &contents[offset], varLen) == 0) retVal = contents + offset + varLen;
      else while (offset < dataSize && contents[offset] != '\0') offset++;
    }
  }
  return (retVal);
}




/* --------------------------------------------------------------------------- *\
 *
 * Dump the contents of a file to MyClient
 * optionally add html encoding
 *
\* --------------------------------------------------------------------------- */
void webDumpFile (WiFiClient *myClient, File *file, bool encode)
{
  char inChar;
  char dumpBuffer[1450];
  uint16_t dumpPtr = 0;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s webDumpFile(%x, %x, encode)\r\n", getTimeStamp(), myClient, file);
    xSemaphoreGive(consoleSem);
  }
  while (file->available()) {
    while (file->available() && dumpPtr<sizeof(dumpBuffer)) {
      inChar = file->read();
      if (encode) {
        if (dumpPtr > 0 && (inChar == '&' || inChar == '"' || inChar == '<' || inChar == '>')) {
          myClient->write(dumpBuffer, dumpPtr);
          dumpPtr = 0;
        }
        switch (inChar) {
        case '&': myClient->printf ("&amp;");     break;
        case '"': myClient->printf ("&quot;");    break;
        case '<': myClient->printf ("&lt;");      break;
        case '>': myClient->printf ("&gt;");      break;
        default:  dumpBuffer[dumpPtr++] = inChar; break;
        }
      }
      else dumpBuffer[dumpPtr++] = inChar;
    }
    myClient->write(dumpBuffer, dumpPtr);
    dumpPtr = 0;
  }
}



/* --------------------------------------------------------------------------- *\
 *
 * Create an editor screen for editing config / command files
 *
\* --------------------------------------------------------------------------- */
void mkWebEditor (WiFiClient *myClient, char *myUri, bool keepAlive, char *hostName)
{
  fs::FS fs = (fs::FS) SPIFFS;
  File file = fs.open(&myUri[5]);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebEditor(%x, %s, keepAlive, %s)\r\n", getTimeStamp(), myClient, myUri, hostName);
    xSemaphoreGive(consoleSem);
  }
  mkWebHeader (myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, &myUri[5], 0);
  myClient->printf ((const char*)"<form action=\"/save\" method=\"post\"><label for=\"fileName\">Save file as: </label><input id=\"fileName\" name=\"fileName\" value=\"%s\"><br><br>", &myUri[5]);
  myClient->printf ((const char*)"<textarea id=\"editdata\" name=\"editdata\" rows=\"24\" cols=\"80\">");
  if(file){
    webDumpFile (myClient, &file, true);
    file.close();
  }
  else {
    myClient->printf ((const char*)"File not found");
  }
  myClient->printf ((const char*)"</textarea><br><br><input type=\"checkbox\" name=\"applyconfig\" id=\"applyconfig\"><label for=\"applyconfig\">Apply file as configuration after saving</label><br><input type=\"submit\" value=\"Save\"></form><br><p>To access file outside editor: <a href=\"http://");
  if (hostName == NULL) {
    myClient->printf ((const char*)"%s.local%s\">http://%s.local%s</a></p>", tname, &myUri[5], tname, &myUri[5]);
  }
  else {
    myClient->printf ((const char*)"%s%s\">http://%s%s</a></p>", hostName, &myUri[5], hostName, &myUri[5]);
  }
  myClient->printf ((const char*)"</td></tr></table><hr></body></html>\r\n");
}



/* --------------------------------------------------------------------------- *\
 *
 * Update password data over clear text transmission
 *
\* --------------------------------------------------------------------------- */
void mkWebClearTextWarning (WiFiClient *myClient, const char *parameter, const char* action)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebEditor(%x, %s, %s)\r\n", getTimeStamp(), myClient, parameter, action);
    xSemaphoreGive(consoleSem);
  }
  myClient->printf ((const char*)"<h2>Warning</h2><p>By continuing you will transfer password secrets in plain text. This means other parties may be able to scan %s on this %s if they are snooping on this network. ", parameter, PRODUCTNAME);
  myClient->printf ((const char*)"Using the serial interface to check or update passwords is more secure.</p><p>Updating of passwords using this web interface is strongly discouraged.</p><p>Do you wish to continue?</p><form action=\"/%s\" method=\"post\">", action);
  myClient->printf ((const char*)"<input type=\"radio\" id=\"yesnocrypt\" name=\"acceptnocrypt\" value=\"yes\"><label for=\"yesnocrypt\">Yes</label>");
  myClient->printf ((const char*)"<input type=\"radio\" id=\"nonocrypt\" name=\"acceptnocrypt\" value=\"no\" checked=\"true\"><label for=\"nonocrypt\">No</label>");
  myClient->printf ((const char*)"<br><br><input type=\"submit\" value=\"Continue\"></form>");
}


void mkWebWiFi(WiFiClient *myClient, char *data, uint16_t dataSize, bool keepAlive)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebWiFi(%x, %x, %d, keepAlive)\r\n", getTimeStamp(), myClient, data, dataSize);
    xSemaphoreGive(consoleSem);
  }
  char *webcontinue = webScanData (data, "acceptnocrypt", dataSize);
  if (webcontinue != NULL && strcmp(webcontinue, "no")==0) {
     mkWebSysStat(myClient, keepAlive, false, NULL, 0);
     return;
  }
  mkWebHeader (myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, "Update Wifi Access", 0);
  if (webcontinue == NULL) {
    mkWebClearTextWarning (myClient, "all the WiFi password(s)", "wifi");
  }
  else if (strcmp(webcontinue, "yes")==0) {
    char ssidName[16];
    char passName[16];
    char myssid[33];
    char mypass[33];
    uint8_t WiFiMode   = nvs_get_int ("WiFiMode", defaultWifiMode);
    uint8_t apChannel  = nvs_get_int ("apChannel", APCHANNEL);
    uint8_t staConnect = nvs_get_int ("staConnect", 0);
    
    myClient->printf ((const char*)"<form action=\"/save\" method=\"post\">");
    myClient->printf ((const char*)"<h2>Access Point Mode</h2><table><tr><th>Setting</th><th>Value</th></tr>");
    myClient->printf ((const char*)"<tr><td>Access Point Mode<br>AKA &quot;Hot Spot&quot;</td><td><input type=\"radio\" id=\"APEnableYes\" name=\"APEnable\" value=\"yes\"");
    if ((WiFiMode & 2) > 0) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"APEnableYes\">Enable</label><br><input type=\"radio\" id=\"APEnableNo\" name=\"APEnable\" value=\"no\"");
    if ((WiFiMode & 2) == 0) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"APEnableNo\">Disable</label></td></tr>");
    nvs_get_string ("APname", myssid, tname,  sizeof(myssid));
    nvs_get_string ("APpass", mypass, "none", sizeof(mypass));
    myClient->printf ((const char*)"<tr><td>AP SSID Name</td><td><input type=\"text\" name=\"APname\" value=\"%s\" minlength=\"4\"></td></tr>", myssid);
    myClient->printf ((const char*)"<tr><td>AP Password</td><td><input type=\"password\" name=\"APpass\" value=\"%s\" minlength=\"4\"></td></tr>", mypass);
    myClient->printf ((const char*)"<tr><td>AP Channel (1-13)</td><td><input type=\"number\" name=\"apChannel\" value=\"%d\" min=\"1\" max=\"13\" size=\"5\"></td></tr>", apChannel);
    myClient->printf ((const char*)"<tr><td>Max Clients (1-%d)</td><td><input type=\"number\" name=\"apClients\" value=\"%d\" min=\"1\" max=\"13\" size=\"5\"></td></tr>", MAXAPCLIENTS, nvs_get_int("apClients", DEFAPCLIENTS));
    myClient->printf ((const char*)"</table><p><strong>Note:</strong></p><ul><li>A password of less than 8 characters will create an open access point</li><li>Access point mode is typically only used for relays, and not for stand-alone throttles</li></ul><h2>Station Mode</h2><table>");
    myClient->printf ((const char*)"<tr><th>Setting</th><th>Value</th></tr><tr><td>Station Mode</td><td><input type=\"radio\" id=\"STAEnableYes\" name=\"STAEnable\" value=\"yes\"");
    if ((WiFiMode & 1) > 0) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"STAEnableYes\">Enable</label><br><input type=\"radio\" id=\"STAEnableNo\" name=\"STAEnable\" value=\"no\"");
    if ((WiFiMode & 1) == 0) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"STAEnableNo\">Disable</label></td></tr><tr><td>Connect to</td><td><input type=\"radio\" id=\"STAConnect0\" name=\"staConnect\" value=\"0\"");
    if (staConnect == 0) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"STAConnect0\">First found in sequence below</label><br><input type=\"radio\" id=\"STAConnect1\" name=\"staConnect\" value=\"1\"");
    if (staConnect == 1) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"STAConnect1\">Strongest signal in table below</label><br><input type=\"radio\" id=\"STAConnect2\" name=\"staConnect\" value=\"2\"");
    if (staConnect == 2) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"STAConnect2\">Strongest signal including any unknown open access points</label><br><input type=\"radio\" id=\"STAConnect3\" name=\"staConnect\" value=\"3\"");
    if (staConnect == 3) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"STAConnect3\">Sequential probe of list below (use for hidden SSIDs)</label></td></tr></table><p><strong>Note</strong></p><ul><li>First 3 options work for SSIDs which are not hidden</li></ul>");
    myClient->printf ((const char*)"<table><tr><th>Index</th><th>SSID</th><th>Password</th></tr>");
    for (uint8_t n=0; n<WIFINETS; n++) {
      sprintf (ssidName, "wifissid_%d", n);
      sprintf (passName, "wifipass_%d", n);
      nvs_get_string (ssidName, myssid, "none", sizeof(myssid));
      nvs_get_string (passName, mypass, "none", sizeof(mypass));
      myClient->printf ((const char*)"<tr><td align=\"right\">%d</td><td><input type=\"text\" name=\"%s\" value=\"%s\" minlength=\"4\"></td><td><input type=\"password\" name=\"%s\" value=\"%s\" minlength=\"4\"></input></tr>", n, ssidName, myssid, passName, mypass);
    }
    myClient->printf ((const char*)"</table><p><strong>Note:</strong></p><ul><li>Hidden SSIDs will be ignored</li><li>Use a password of &quot;none&quot; if SSID has no password (open accesspoint)</li></ul>");
      if (numberOfNetworks > 0) {
      myClient->printf ((const char*)"<h2>Discovered Networks</h2><table><tr><th>SSID</th><th>RSSI</th><th>Address</th><th>Channel</th><th>Encryption</th></tr>");
      for (uint8_t i=0; i<numberOfNetworks; i++) {
        myClient->printf ((const char*)"<tr><td>%s</td><td align=\"right\">%d</td><td>%s</td><td align=\"right\">%d</td><td>%s</td></tr>", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.BSSIDstr(i).c_str(), WiFi.channel(i), wifi_EncryptionType(WiFi.encryptionType(i)));
      }
      myClient->printf ((const char*)"</table>");
    }
  myClient->printf ((const char*)"<br><br><input type=\"submit\" value=\"Save\"></form></td></tr></table><hr></body></html>\r\n");
  }
}


void mkWebAdmin(WiFiClient *myClient, char *data, uint16_t dataSize, bool keepAlive)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebWiFi(%x, %x, %d, keepAlive)\r\n", getTimeStamp(), myClient, data, dataSize);
    xSemaphoreGive(consoleSem);
  }
  char *webcontinue = webScanData (data, "acceptnocrypt", dataSize);
  if (webcontinue != NULL && strcmp(webcontinue, "no")==0) {
     mkWebSysStat(myClient, keepAlive, false, NULL, 0);
     return;
  }
  mkWebHeader (myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, "Update Web-Admin Access", 0);
  if (webcontinue == NULL) {
    mkWebClearTextWarning (myClient, "the web administrative credential", "password");
  }
  else if (strcmp(webcontinue, "yes")==0) {
    char webuser[64];
    char webpass[64];

    nvs_get_string ("webuser", webuser, WEBUSER, sizeof(webuser));
    nvs_get_string ("webpass", webpass, WEBPASS, sizeof(webpass));

    myClient->printf ((const char*)"<form action=\"/save\" method=\"post\"><h2>Administrative User</h2><table><tr><td align=\"right\">");
    myClient->printf ((const char*)"Admin User Name </td><td><input type=\"text\" id=\"webuser\" name=\"webuser\" value=\"%s\" minlength=\"4\"></td></tr><tr><td align=\"right\">", webuser);
    myClient->printf ((const char*)"Password </td><td><input type=\"password\" id=\"webpass\" name=\"webpass\" value=\"%s\" minlength=\"4\"></td></tr></table>", webpass);
    myClient->printf ((const char*)"<br><br><input type=\"submit\" value=\"Save\"></form>");
  }
  myClient->printf ((const char*)"</td></tr></table><hr></body></html>\r\n");
}


/* --------------------------------------------------------------------------- *\
 *
 * Make web configuration
 *
\* --------------------------------------------------------------------------- */
void mkWebConfig (WiFiClient *myClient, bool keepAlive)
{
  uint8_t speedOpts[] = { 80, 160, 240, 0 };
  const char *dccPwrLabel[] = { "Main & Prog tracks", "Main Only", "Prog Only", "Join Main and Prog tracks" };
  const char *statusLabel[] = { "Top of page", "Bottom of page", "Not shown" };
  const char *powerLabel[]  = { "Not Displayed", "Displayed" };
  const char *clockLabel[]  = { "24hr (15:20)", "24hr (15h20)", "12hr (3:20)" };
  char labelName[16];
  char labelDesc[64];
  int compInt;
  int cpuSpeed;
  #ifdef SCREENROTATE
  #if SCREENROTATE == 2
  uint8_t rotateCount = 2;
  const char *rotateOpts[] = {"0 deg (Normal)", "180 deg (Invert)"};
  #else
  uint8_t rotateCount = 4;
  const char *rotateOpts[] = {"0 deg (Normal)", "90 deg Right", "180 deg (Invert)", "90 deg Left"};
  #endif   // SCREENROTATE == 2
  #endif   // SCREENROTATE
  #ifdef OTAUPDATE
  char ota_url[120];
  ota_control theOtaControl;
  #endif

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebConfig(%x, keepAlive)\r\n", getTimeStamp(), myClient);
    xSemaphoreGive(consoleSem);
  }
  mkWebHeader (myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, "Update Configuration", 0);
  myClient->printf ((const char*)"<form action=\"/save\" method=\"post\"><h2>Device &amp; Display</h2><table>");
  myClient->printf ((const char*)"<tr><td>Name </td><td><input type=\"text\" id=\"tname\" name=\"tname\" value=\"%s\" minlength=\"4\" maxlength=\"%d\"></td></tr>", tname, sizeof(tname)-1);
  myClient->printf ((const char*)"<tr><td>CPU Speed </td><td>");
  cpuSpeed = nvs_get_int ("cpuspeed", 0);
  for (uint8_t n=0 ; n<sizeof(speedOpts); n++) {
    if (n>0) myClient->printf ("<br>");
    sprintf (labelName, "speed%d", speedOpts[n]);
    if (speedOpts[n] == 0) strcpy (labelDesc, "Default");
    else sprintf (labelDesc, "%d MHz", speedOpts[n]);
    myClient->printf ((const char*)"<input type=\"radio\" id=\"%s\" name=\"cpuspeed\" value=\"%d\"", labelName, speedOpts[n]);
    if (speedOpts[n] == cpuSpeed) myClient->printf (" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"%s\">%s</label>", labelName, labelDesc);
  }
  #ifdef SCREENROTATE
  myClient->printf ((const char*)"</td></tr><tr><td>Screen Orientation </td><td>");
  compInt = nvs_get_int ("screenRotate", 0);
  if (compInt >= rotateCount) compInt = 0;
  for (uint8_t n=0 ; n<rotateCount; n++) {
    sprintf (labelName, "screenRot%d", n);
    if (n>0) myClient->printf ("<br>");
    myClient->printf ((const char*)"<input type=\"radio\" id=\"%s\" name=\"screenRotate\" value=\"%d\"", labelName, n);
    if (n == compInt) myClient->printf (" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"%s\">%s</label>", labelName, rotateOpts[n]);
  }
  #endif  //SCREEN ROTATE
  #ifndef NODISPLAY
  myClient->printf ((const char*)"</td></tr><tr><td>Font </td><td>");
  compInt = nvs_get_int ("fontIndex", 1);
  if (compInt >= sizeof(fontWidth)) compInt = 0;
  for (uint8_t n=0 ; n<sizeof(fontWidth); n++) {
    sprintf (labelName, "fontIdx%d", n);
    sprintf (labelDesc, "%s (%dx%d chars)", fontLabel[n], (screenWidth/fontWidth[n]), (screenHeight/fontHeight[n]));
    if (n>0) myClient->printf ("<br>");
    myClient->printf ((const char*)"<input type=\"radio\" id=\"%s\" name=\"fontIndex\" value=\"%d\"", labelName, n);
    if (n == compInt) myClient->printf (" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"%s\">%s</label>", labelName, labelDesc);
  }
  nvs_get_string ("funcOverlay", labelDesc, FUNCOVERLAY, 32);
  myClient->printf ((const char*)"</td></tr><tr><td>Function Overlay</td><td><input type=\"text\" id=\"funcOverlay\" name=\"funcOverlay\" value=\"%s\" minlength=\"16\" maxlength=\"29\" size=\"32\"></td></tr>", labelDesc);
  myClient->printf ((const char*)"<tr><td>Fast Clock Format</td><td>");
  compInt = nvs_get_int ("clockFormat", 0);
  for (uint8_t n=0 ; n<3; n++) {
    sprintf (labelName, "clockFmt%d", n);
    if (n>0) myClient->printf ("<br>");
    myClient->printf ((const char*)"<input type=\"radio\" id=\"%s\" name=\"clockFormat\" value=\"%d\"", labelName, n);
    if (n == compInt) myClient->printf (" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"%s\">%s</label>", labelName, clockLabel[n]);
  }
  myClient->printf ((const char*)"</td></tr>");
  compInt = nvs_get_int ("allMenuItems", 0);
  myClient->printf ((const char*)"<tr><td>Menu Options</td><td><input type=\"radio\" id=\"allMenuRestict\" name=\"allMenuItems\" value=\"0\"");
  if (compInt == 0) myClient->printf((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"allMenuRestrict\">Only show valid menu options</label><br>");
  myClient->printf ((const char*)"<input type=\"radio\" id=\"allMenuEvery\" name=\"allMenuItems\" value=\"1\"");
  if (compInt == 1) myClient->printf((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"allMenuEvery\">Show all menu options</label></td></tr>");
  compInt = nvs_get_int ("menuWrap", 0);
  myClient->printf ((const char*)"<tr><td>Menu Wrap around</td><td><input type=\"radio\" id=\"menuWrapNo\" name=\"menuWrap\" value=\"0\"");
  if (compInt == 0) myClient->printf((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"menuWrapNo\">Hard stop at menu top and bottom</label><br>");
  myClient->printf ((const char*)"<input type=\"radio\" id=\"menuWrapYes\" name=\"menuWrap\" value=\"1\"");
  if (compInt == 1) myClient->printf((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"menuWrapYes\">Menus wrap around</label></td></tr>");
  #endif   // NODISPLAY
  #ifdef DELAYONSTART
  compInt = nvs_get_int ("delayOnStart", DELAYONSTART);
  myClient->printf ((const char*)"</td></tr><tr><td>Startup Delay</td><td><input type=\"number\" id=\"delayOnStart\" name=\"delayOnStart\" value=\"%d\" min=\"0\" max=\"120\" size=\"5\"> Seconds (0-120)</td></tr>", compInt);
  #endif    // DELAYONSTART
  #ifndef NODISPLAY     // backlight settings only make sense if there is a display
  #ifdef BACKLIGHTPIN
  #ifndef BACKLIGHTREF
  compInt = nvs_get_int ("backlightValue", 200);
  myClient->printf ((const char*)"<tr><td>Screen brightness</td><td><input type=\"number\" id=\"backlightValue\" name=\"backlightValue\" value=\"%d\" min=\"50\" max=\"255\" size=\"5\"> (50-255)</td></tr>", compInt);
  #endif   // BACKLIGHTREF
  compInt = nvs_get_int ("screenSaver", 0);
  myClient->printf ((const char*)"<tr><td>Screen blank after</td><td><input type=\"number\" id=\"screenSaver\" name=\"screenSaver\" value=\"%d\" min=\"0\" max=\"600\" size=\"5\"> Minutes</td></tr>", compInt);
  #endif   // BACKLIGHTPIN
  #endif   // NODISPLAY
  myClient->printf ((const char*)"</table>");
  #ifdef RELAYPORT
  if (cpuSpeed!=240) myClient->printf ((const char*)"<p><strong>NB:</strong> When relaying a CPU speed of 240 MHz is recommended. Current value is %d MHz.</p>", cpuSpeed);
  #endif   // RELAYPORT
  #ifndef NODISPLAY  // debounce settings only make sense if there is a user interface
  compInt = nvs_get_int ("debounceTime", DEBOUNCEMS);
  myClient->printf ((const char*)"<h2>Control Sensitivity</h2><table><tr><td>Debounce time</td><td><input type=\"number\" name=\"debounceTime\" value=\"%d\" min=\"10\" max=\"100\" size=\"5\"> mS</td><td> - Time between keypad clicks", compInt);
  compInt = nvs_get_int ("detentCount", 2);
  myClient->printf ((const char*)"</td></tr><tr><td>Detent count</td><td><input type=\"number\" name=\"detentCount\" value=\"%d\" min=\"1\" max=\"19\" size=\"5\"> clicks</td><td> - Encoder click count per speed change", compInt);
  compInt = nvs_get_int ("speedStep", 4);
  myClient->printf ((const char*)"</td></tr><tr><td>Step increase</td><td><input type=\"number\" name=\"speedStep\" value=\"%d\" min=\"1\" max=\"20\" size=\"5\"> steps</td><td> - Step change in speed per click", compInt);
  #ifdef WARNCOLOR
  compInt = nvs_get_int ("warnSpeed", 90);
  myClient->printf ((const char*)"</td></tr><tr><td align=\"right\">Warning Speed</td><td><input type=\"number\" name=\"warnSpeed\" value=\"%d\" min=\"50\" max=\"101\" size=\"5\"> percent</td><td> - speed-bar changes color at %% (50-101)", compInt);
  #endif    // WARNCOLOR
  myClient->printf ((const char*)"</td></tr></table>");
  #endif    // NODISPLAY
  if (cmdProtocol == DCCEX) {
    compInt = nvs_get_int ( "routeDelay", 2);
    myClient->printf ((const char*)"<h2>DCC-Ex Settings</h2><table><tr><td>Delay between setting route turnouts</td><td>");
    for (uint8_t n=0 ; n<sizeof(routeDelay)/sizeof(uint16_t); n++) {
      sprintf (labelName, "rtDelay%d", n);
      sprintf (labelDesc, "%d mS", routeDelay[n]);
      if (n>0) myClient->printf ("<br>");
      myClient->printf ((const char*)"<input type=\"radio\" id=\"%s\" name=\"routeDelay\" value=\"%d\"", labelName, n);
      if (n == compInt) myClient->printf ((const char*)" checked=\"true\"");
      myClient->printf ((const char*)"><label for=\"%s\">%s</label>", labelName, labelDesc);
    }
    myClient->printf ((const char*)"</td></tr><tr><td>Power on affects</td><td>");
    compInt = nvs_get_int ("dccPower", DCCPOWER);
    for (uint8_t n=0 ; n<=JOIN; n++) {
      sprintf (labelName, "dccPwr%d", n);
      if (n>0) myClient->printf ("<br>");
      myClient->printf ((const char*)"<input type=\"radio\" id=\"%s\" name=\"dccPower\" value=\"%d\"", labelName, n);
      if (n == compInt) myClient->printf ((const char*)" checked=\"true\"");
      myClient->printf ((const char*)"><label for=\"%s\">%s</label>", labelName, dccPwrLabel[n]);
    }
    myClient->printf ((const char*)"</td></tr><tr><td>On error in setting routes</td><td>");
    compInt = nvs_get_int ("dccRtError", 0);
    myClient->printf ((const char*)"<input type=\"radio\" id=\"dccRtError0\" name=\"dccRtError\" value=\"0\"");
    if (compInt==0) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"dccRtError0\">Continue setting up remainder of route</label><br><input type=\"radio\" id=\"dccRtError1\" name=\"dccRtError\" value=\"1\"");
    if (compInt==1) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"dccRtError1\">Abort setting up route</label>");

    compInt = nvs_get_int ("toOffset", 100);
    myClient->printf ((const char*)"</td></tr><tr><td>Turnout numbering starts at</td><td><input type=\"number\" name=\"toOffset\" value=\"%d\" min=\"1\" max=\"1000\" size=\"6\"></td></tr></table>", compInt);
  }
  #ifdef BRAKEPRESPIN
  compInt = nvs_get_int ( "brakeup", 1);
  myClient->printf ((const char*)"<h2>Brake Sensitivity</h2><table><tr><td>Increase</td><td><input type=\"number\" id=\"brakeup\" name=\"brakeup\" value=\"%d\" min=\"1\" max=\"10\" size=\"5\"></td><td>Higher number means faster recovery", compInt);
  compInt = nvs_get_int ( "brakedown", 20);
  myClient->printf ((const char*)"</td></tr><tr><td>Decrease</td><td><input type=\"number\" id=\"brakedown\" name=\"brakedown\" value=\"%d\" min=\"1\" max=\"100\" size=\"5\"></td><td>Higher number means faster drop per speed decrease", compInt);
  myClient->printf ((const char*)"</td></tr></table>");
  #endif   // BRAKEPRESPIN
  #ifdef RELAYPORT
  compInt = nvs_get_int ("relayMode", WITHROTRELAY);
  myClient->printf ((const char*)"<h2>Relay Service</h2><table><tr><td>Relay mode</td><td><input type=\"radio\" id=\"relayoff\" name=\"relayMode\" value=\"%d\"", NORELAY);
  if (compInt == NORELAY)  myClient->printf ((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"relayoff\">Disabled</label><br><input type=\"radio\" id=\"relaywithrot\" name=\"relayMode\" value=\"%d\"", WITHROTRELAY);
  if (compInt == WITHROTRELAY)  myClient->printf ((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"relaywithrot\">WiThrottle</label><br><input type=\"radio\" id=\"relaydcc\" name=\"relayMode\" value=\"%d\"", DCCRELAY);
  if (compInt == DCCRELAY)  myClient->printf ((const char*)" checked=\"true\"");
  compInt = nvs_get_int ("relayPort", RELAYPORT);
  myClient->printf ((const char*)"><label for=\"relaydcc\">DCC-Ex</label></td></tr><tr><td>Relay port (1-65534)</td><td><input type=\"number\" name=\"relayPort\" value=\"%d\" min=\"1\" max=\"65534\" size=\"7\"> Default is %d</td></tr>", compInt, RELAYPORT);
  compInt = nvs_get_int ("maxRelay", MAXRELAY);
  myClient->printf ((const char*)"<tr><td>Max clients (3-%d)</td><td><input type=\"number\" name=\"maxRelay\" value=\"%d\" min=\"3\" max=\"%d\" size=\"5\"></td></tr></table>", MAXRELAY, compInt, MAXRELAY);
  #endif   // RELAYPORT
  #ifdef WEBLIFETIME    // should not be building a web page without this, but anyhow...
  compInt = nvs_get_int ("webTimeOut", WEBLIFETIME);
  myClient->printf ((const char*)"<h2>Web Service</h2><table><tr><td>Webserver idle lifetime</td><td><input type=\"number\" name=\"webTimeOut\" value=\"%d\" min=\"0\" max=\"600\" size=\"5\"> Minutes (0 => always on)</td></tr>", compInt);
  compInt = nvs_get_int ("webPort", WEBPORT);
  myClient->printf ((const char*)"<tr><td>Web port (1-65534)</td><td><input type=\"number\" name=\"webPort\" value=\"%d\" min=\"1\" max=\"65534\" size=\"7\"> Std http port is 80</td></tr>", compInt);
  compInt = nvs_get_int ("cacheTimeout", WEBCACHE);
  myClient->printf ((const char*)"<tr><td>Cache time out for static content</td><td><input type=\"number\" name=\"cacheTimeout\" value=\"%d\" min=\"5\" max=\"1440\" size=\"6\"> Minutes</td></tr>", compInt);
  compInt = nvs_get_int ("webRefresh", WEBREFRESH);
  myClient->printf ((const char*)"<tr><td>Status page refresh time</td><td><input type=\"number\" name=\"cacheTimeout\" value=\"%d\" min=\"0\" max=\"3600\" size=\"6\"> Seconds (0 => No auto refresh)</td></tr>", compInt);
  compInt = nvs_get_int ("webStatus", 0);
  myClient->printf ((const char*)"<tr><td>Show device description with status</td><td>");
  for (uint8_t n=0; n<3; n++) {
    if (n>0) myClient->printf ("<br>");
    sprintf (labelName, "webStatus_%d", n);
    myClient->printf ((const char*)"<input type=\"radio\" id=\"%s\" name=\"webStatus\" value=\"%d\"", labelName, n);
    if (n == compInt) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"%s\">%s</label>", labelName, statusLabel[n]);
  }
  compInt = nvs_get_int ("webPwrSwitch", 1);
  myClient->printf ((const char*)"<tr><td>Show power switch</td><td>");
  for (uint8_t n=0; n<2; n++) {
    if (n>0) myClient->printf ("<br>");
    sprintf (labelName, "webPower_%d", n);
    myClient->printf ((const char*)"<input type=\"radio\" id=\"%s\" name=\"webPwrSwitch\" value=\"%d\"", labelName, n);
    if (n == compInt) myClient->printf ((const char*)" checked=\"true\"");
    myClient->printf ((const char*)"><label for=\"%s\">%s</label>", labelName, powerLabel[n]);
  }
  myClient->printf ((const char*)"</td></tr></table>");
  #endif   // WEBLIFETIME
  #ifndef SERIALCTRL
  myClient->printf ((const char*)"<h2>Server Discovery</h2><table><tr><td>mDNS Discovery</td><td><input type=\"radio\" id=\"mdnson\" name=\"mdns\" value=\"1\"");
  compInt = nvs_get_int ("mdns", 1);
  if (compInt == 1) myClient->printf ((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"mdnson\">Use mDNS search then lookup table</label><br><input type=\"radio\" id=\"mdnsoff\" name=\"mdns\" value=\"0\"");
  if (compInt == 0) myClient->printf ((const char*)" checked=\"true\"");
  compInt = nvs_get_int ("defaultProto", WITHROT);
  myClient->printf ((const char*)"><label for=\"mdnsoff\">Use lookup table only</label></td></tr><tr><td>Default Protocol</td><td><input type=\"radio\" name=\"defaultProto\" id=\"defProtoWITHROT\" value=\"%d\"", WITHROT);
  if (compInt == WITHROT) myClient->printf (" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"defProtoWITHROT\">WiThrottle</label><br><input type=\"radio\" name=\"defaultProto\" id=\"defProtoDCC\" value=\"%d\"", DCCEX);
  if (compInt == DCCEX) myClient->printf (" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"defProtoDCC\">DCC-Ex</label></td></tr></table><br><br><table><tr><th>Server</th><th>Port</th></tr>");
  for (uint8_t n=0; n<WIFINETS; n++) {
    sprintf (labelName, "server_%d", n);
    if (n==0) nvs_get_string (labelName, labelDesc, HOST, sizeof(labelDesc));
    else nvs_get_string (labelName, labelDesc, "none", sizeof(labelDesc));
    myClient->printf ((const char*)"<tr><td><input type=\"text\" name=\"%s\" value=\"%s\"></td>", labelName, labelDesc);
    sprintf (labelName, "port_%d", n);
    compInt = nvs_get_int (labelName, PORT);
    myClient->printf ((const char*)"<td><input type=\"number\" name=\"%s\" value=\"%d\" min=\"1\" max=\"65534\" size=\"7\"></td></tr>", labelName, compInt);
  }
  myClient->printf ((const char*)"</table>");
  #endif   // WEBLIFETIME
  #ifdef OTAUPDATE
  nvs_get_string ("ota_url", ota_url, OTAUPDATE, sizeof(ota_url));
  myClient->printf ((const char*)"<h2>Over The Air Updates</h2><table><tr><td>Source</td><td><input type=\"text\" name=\"ota_url\" value=\"%s\" minlength=\"16\" maxlength=\"%d\" size=\"64\"></td></tr>", ota_url, sizeof(ota_url));
  sprintf (labelName, "ota_%s", theOtaControl.get_boot_partition_label());
  nvs_get_string (labelName, ota_url, "Initial Installation", sizeof(ota_url));
  myClient->printf ((const char*)"<tr><td>Boot: %s</td><td>%s</td></tr>", theOtaControl.get_boot_partition_label(), ota_url);
  if (nvs_get_int ("ota_initial", 0) == 1) {
    sprintf (labelName, "ota_%s", theOtaControl.get_next_partition_label());
    nvs_get_string (labelName, ota_url, "Initial Installation", sizeof(ota_url));
    myClient->printf ((const char*)"<tr><td>Next: %s</td><td>%s</td></tr>", theOtaControl.get_next_partition_label(), ota_url);
  }
  myClient->printf ((const char*)"<tr><td>Update Options</td><td><input type=\"radio\" id=\"otaNone\" name=\"ota_action\" value=\"0\" checked=\"true\"><label for=\"otaNone\">No OTA check</label>");
  myClient->printf ((const char*)"<br><input type=\"radio\" id=\"otaCheck\" name=\"ota_action\" value=\"1\"><label for=\"otaCheck\">Check for update</label>");
  if (nvs_get_int ("ota_initial", 0) == 1) {
    myClient->printf ((const char*)"<br><input type=\"radio\" id=\"otaBackout\" name=\"ota_action\" value=\"2\"><label for=\"otaBackout\">Switch boot partitions</label>");
  }
  myClient->printf ((const char*)"</td></tr></table>");
  #endif
  myClient->printf ((const char*)"<h2>Misc Settings</h2><table><tr><td>Roster</td><td><input type=\"radio\" name=\"sortData\" id=\"sortTrue\" value=\"1\"");
  compInt = nvs_get_int ((const char*)"sortData", 1);
  if (compInt == 1) myClient->printf (" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"sortTrue\">Sorted</label><br><input type=\"radio\" name=\"sortData\" id=\"sortFalse\" value=\"0\"");
  if (compInt == 0) myClient->printf (" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"sortFalse\">Unsorted</label></td></tr>");
  #ifndef NODISPLAY
  myClient->printf ((const char*)"<tr><td>OperatingMode</td><td><input type=\"radio\" name=\"bidirectional\" id=\"bidiFalse\" value=\"0\"");
  compInt = bidirectionalMode;
  if (compInt == 0) myClient->printf (" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"bidiFalse\">Standard</label><br><input type=\"radio\" name=\"bidirectional\" id=\"bidiTrue\" value=\"1\"");
  if (compInt == 1) myClient->printf (" checked=\"true\"");
  compInt = nvs_get_int ("buttonStop", 1);
  myClient->printf ((const char*)"><label for=\"bidiTrue\">Bidirectional</label></td></tr><tr><td>Encoder button</td><td><input type=\"radio\" name=\"buttonStop\" id=\"buttonStopNo\" value=\"0\"");
  if (compInt == 0) myClient->printf (" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"buttonStopNo\">Loco menu only in driving mode, must be stopped</label><br><input type=\"radio\" name=\"buttonStop\" id=\"buttonStopYes\" value=\"1\"");
  if (compInt == 1)  myClient->printf (" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"buttonStopYes\">Push stops train or loco menu if stopped in driving mode<label></td></tr>");
  #endif  // NODISPLAY
  compInt = nvs_get_int ("noPwrTurnouts", 0);
  myClient->printf ((const char*)"<tr><td>Turnout/Route Options</td><td><input type=\"radio\" id=\"noPwrTurnout\" name=\"noPwrTurnouts\" value=\"0\"");
  if (compInt == 0) myClient->printf((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"noPwrTurnout\">Track power required to operate turnouts</label><br>");
  myClient->printf ((const char*)"<input type=\"radio\" id=\"yesPwrTurnout\" name=\"noPwrTurnouts\" value=\"1\"");
  if (compInt == 1) myClient->printf((const char*)" checked=\"true\"");
  myClient->printf ((const char*)"><label for=\"yesPwrTurnout\">Turnouts work without track power</label></td></tr>");
  myClient->printf ((const char*)"<tr><td>Restart</td><td>");
  myClient->printf ((const char*)"<input type=\"radio\" name=\"rebootOnUpdate\" id=\"rebootOnUpdateNo\" value=\"N\"><label for=\"rebootOnUpdateNo\">No restart</label><br>");
  myClient->printf ((const char*)"<input type=\"radio\" name=\"rebootOnUpdate\" id=\"rebootOnUpdateCond\" value=\"C\" checked=\"true\"><label for=\"rebootOnUpdateCond\">Restart if updated</label><br>");
  myClient->printf ((const char*)"<input type=\"radio\" name=\"rebootOnUpdate\" id=\"rebootOnUpdateYes\" value=\"Y\"><label for=\"rebootOnUpdateYes\">Restart unconditionally</label></td></tr></table><br><input type=\"submit\" value=\"Save\"></form>");
  myClient->printf ((const char*)"</td></tr></table><hr></body></html>\r\n");
}



/* --------------------------------------------------------------------------- *\
 *
 * Save returned data
 *
\* --------------------------------------------------------------------------- */
void mkWebSave(WiFiClient *myClient, char *data, uint16_t dataSize, bool keepAlive)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebSave(%x, %x, %d, keepAlive)\r\n", getTimeStamp(), myClient, data, dataSize);
    xSemaphoreGive(consoleSem);
  }
  char *resultPtr;
  char *altPtr;
  char checkString[64];
  char varName[16];
  bool hasChanged     = false;
  bool rebootRequired = false;
  int checkNum;
  int inputNum;

  mkWebHeader (myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, "Saved file/data", 0);
  //
  // Post new file contents
  //
  resultPtr = webScanData (data, "fileName", dataSize);
  if (resultPtr != NULL && resultPtr[0]!='\0') {
    fs::FS fs = (fs::FS) SPIFFS;
    char filename[strlen(resultPtr)+1];
    char inChar;
    bool unchanged = true;
    bool applyconfig = false;

    myClient->printf ((const char*)"<h2>Update File</h2><ul>");
    strcpy (filename, resultPtr);
    if (webScanData (data, "applyconfig", dataSize)) applyconfig = true;;
    resultPtr = webScanData (data, "editdata", dataSize);
    File file = fs.open(filename);
    inputNum = 0;
    checkNum = strlen(resultPtr);
    while (file.available() && !hasChanged && inputNum<checkNum) {
      inChar = file.read();
      if (inChar != resultPtr[inputNum]) hasChanged = true; // mismatch
      inputNum++;
    }
    if (inputNum<checkNum) hasChanged=true;  // there is more data in update than in file
    if (file.available())  hasChanged=true;  // there is more data on file than we want in the update
    file.close();
    if (hasChanged) {
      if (fs.exists(filename)) fs.remove(filename);
      writeFile = fs.open(filename, FILE_WRITE);
      if(!writeFile){
        myClient->printf ((const char*)"<li>failed to open file %s for writing</li>", filename);
      }
      else {
        writeFile.write ((uint8_t*) resultPtr, strlen(resultPtr));
        myClient->printf ((const char*)"<li>updated file %s</li>", filename);
      }
      writeFile.close();
    }
    else myClient->printf ((const char*)"<li>file %s is unchanged</li>", filename);
    if (applyconfig) myClient->printf ((const char*)"<li>apply %s as configuration data</li>", filename);
    myClient->printf ((const char*)"<li><a href=\"/\">Back to main page</a></li></ul><hr></body></html>\r\n");
    if (applyconfig) {
      myClient->flush();
      util_readFile (SPIFFS, filename, true);
    }
    return;
  }
  //
  // Download a URL
  //
  resultPtr = webScanData (data, "downloadURL", dataSize);
  if (resultPtr != NULL && resultPtr[0]!='\0') {
    altPtr = NULL;
    myClient->printf ((const char*)"<h2>Download File</h2><ul>");
    for (int16_t n=strlen(resultPtr)-1; n>0 && altPtr==NULL; n--) if (resultPtr[n]=='/') altPtr = resultPtr + n;
    if (resultPtr[strlen(resultPtr)-1] == '/' || strncmp (resultPtr, "http", 4) != 0) {
      myClient->printf ((const char*)"<li>malformed URL: %s</li>", resultPtr);
      myClient->printf ((const char*)"<li>URL should start with http, and not end with \"/\"</li>");
    }
    else {
      myClient->printf ((const char*)"<li>downloading URL: %s as %s</li>", resultPtr, altPtr);
      getHttp2File (SPIFFS, resultPtr, altPtr);
    }
    myClient->printf ((const char*)"<li><a href=\"/\">Back to main page</a></li></ul><hr></body></html>\r\n");
    return;
  }
  //
  // Update any other parameters
  //
  myClient->printf ((const char*)"<h2>Saved Data</h2><ul>");
  inputNum = 0;
  resultPtr = webScanData (data, "APEnable",  dataSize);
  if (resultPtr != NULL && strcmp (resultPtr, "yes") == 0) inputNum += 2;
  resultPtr = webScanData (data, "STAEnable", dataSize);
  if (resultPtr != NULL && strcmp (resultPtr, "yes") == 0) inputNum += 1;
  checkNum = nvs_get_int ("WiFiMode", defaultWifiMode);
  if (inputNum != 0 && inputNum != checkNum) {
    nvs_put_int ("WiFiMode", inputNum);
    myClient->printf ("<li>WiFi Mode = %d</li>", inputNum);
    hasChanged = true;
  }
  for (uint8_t n=0; n<(sizeof(nvsVars)/sizeof(struct nvsVar_s)); n++) {
    resultPtr = webScanData (data, (char*) nvsVars[n].varName, dataSize);
    if (resultPtr != NULL && resultPtr[0]!='\0') {
      if (nvsVars[n].varType == INTEGER) {
        checkNum = nvs_get_int ((char*) nvsVars[n].varName, nvsVars[n].numDefault);
        inputNum = util_str2int (resultPtr);
        if (checkNum != inputNum) {
          if (inputNum<nvsVars[n].varMin || inputNum>nvsVars[n].varMax) {
            myClient->printf ((const char*)"<li>%s (%s) Acceptable Range Between %d And %d</li>", nvsVars[n].varName, nvsVars[n].varDesc, nvsVars[n].varMin, nvsVars[n].varMax);
          }
          else {
            nvs_put_int (nvsVars[n].varName, inputNum);
            myClient->printf ("<li>%s = %d</li>", nvsVars[n].varDesc, inputNum);
            hasChanged = true;
            #ifdef SCREENSAVER
            if (strcmp (nvsVars[n].varName, "backlightValue") == 0) { backlightValue = inputNum; ledcWrite(0, backlightValue); }
            if (strcmp (nvsVars[n].varName, "screenSaver") == 0) {
              if (xSemaphoreTake(screenSvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                blankingTime  = inputNum * 60 * uS_TO_S_FACTOR;
                xSemaphoreGive(screenSvrSem);
              }
              else semFailed ("screenSvrSem", __FILE__, __LINE__);
            }
            #endif
          }
        }
      }
      else {
        nvs_get_string (nvsVars[n].varName, checkString, nvsVars[n].strDefault, nvsVars[n].varMax);
        if (strlen(resultPtr) > nvsVars[n].varMax) {
          resultPtr[nvsVars[n].varMax] = '\0';
          myClient->printf ((const char*)"<li>%s truncated to %d characters</li>", nvsVars[n].varDesc, nvsVars[n].varMax);
        }
        if (strlen(resultPtr) < nvsVars[n].varMin) {
          myClient->printf ((const char*)"<li>%s ignored, should be %d or more characters long</li>", nvsVars[n].varDesc, nvsVars[n].varMin);
        }
        else if (strcmp (resultPtr, checkString) != 0) {
          nvs_put_string (nvsVars[n].varName, resultPtr);
          hasChanged = true;
          if (strcmp (nvsVars[n].varName, "tname") == 0) strcpy (tname, resultPtr);
          myClient->printf ("<li>%s = %s</li>", nvsVars[n].varDesc, resultPtr);
        }
      }
    }
  }
  for (uint8_t n=0; n<WIFINETS; n++) {
    sprintf (varName, "wifissid_%d", n);
    resultPtr = webScanData (data, varName, dataSize);
    nvs_get_string (varName, checkString, "none", sizeof(checkString));
    if (resultPtr != NULL && resultPtr[0]!='\0' && strcmp (resultPtr, checkString)!=0) {
      if (strlen(resultPtr) >= sizeof(checkString)) resultPtr[sizeof(checkString)-1] = '\0';
      nvs_put_string (varName, resultPtr);
      myClient->printf ((const char*)"<li>WiFi SSID #%d = %s</li>", n, resultPtr);
      hasChanged = true;
    }
    sprintf (varName, "wifipass_%d", n);
    resultPtr = webScanData (data, varName, dataSize);
    nvs_get_string (varName, checkString, "none", sizeof(checkString));
    if (resultPtr != NULL && resultPtr[0]!='\0' && strcmp (resultPtr, checkString)!=0) {
      if (strlen(resultPtr) >= sizeof(checkString)) resultPtr[sizeof(checkString)-1] = '\0';
      nvs_put_string (varName, resultPtr);
      myClient->printf ((const char*)"<li>WiFi password #%d changed</li>", n);
      hasChanged = true;
    }
    sprintf (varName, "server_%d", n);
    resultPtr = webScanData (data, varName, dataSize);
    nvs_get_string (varName, checkString, "none", sizeof(checkString));
    if (resultPtr != NULL && resultPtr[0]!='\0' && strcmp (resultPtr, checkString)!=0) {
      if (strlen(resultPtr) >= sizeof(checkString)) resultPtr[sizeof(checkString)-1] = '\0';
      nvs_put_string (varName, resultPtr);
      myClient->printf ((const char*)"<li>Control station (Server) #%d = %s</li>", n, resultPtr);
      hasChanged = true;
    }
    sprintf (varName, "port_%d", n);
    resultPtr = webScanData (data, varName, dataSize);
    nvs_get_string (varName, checkString, "none", sizeof(checkString));
    if (resultPtr != NULL && resultPtr[0]!='\0') {
      checkNum = nvs_get_int (varName, PORT);
      inputNum = util_str2int (resultPtr);
      if (checkNum != inputNum) {
        if (inputNum<1 || inputNum>65534) {
          myClient->printf ((const char*)"<li>Port Acceptable Range Between 1 And 65534</li>");
        }
        else {
          nvs_put_int (varName, inputNum);
          myClient->printf ((const char*)"<li>Port #%d = %d</li>", n, inputNum);
          hasChanged = true;
        }
      }
    }
  }
  if (!hasChanged) {
     myClient->printf ((const char*)"<li>No changes saved.</li>");
  }
  #ifdef OTAUPDATE
  resultPtr = webScanData (data, "ota_action", dataSize);
  if (resultPtr != NULL) {
    if (strcmp (resultPtr, "1") == 0) {
      char outBuffer[120];
      outBuffer[0] = '\0';
      myClient->printf ((const char*)"<li>Checking if update is available, this may take several minutes... ...please wait.</li>");
      if (OTAcheck4update(outBuffer)) hasChanged = true;;
      if (outBuffer[0] != '\0') myClient->printf ((const char*)"<li><pre>%s</pre></li>", outBuffer);
    }
    else if (strcmp (resultPtr, "2") == 0) {
      char outBuffer[120];
      myClient->printf ((const char*)"<li>Switching boot partitions, reboot required to take effect</li></ul></body></html>");
      if (OTAcheck4rollback(outBuffer)) hasChanged = true;;
      if (outBuffer[0] != '\0') myClient->printf ((const char*)"<li><pre>%s</pre></li>", outBuffer);
    }
  }
  #endif
  resultPtr = webScanData (data, "rebootOnUpdate", dataSize);
  if (resultPtr!=NULL && (resultPtr[0]=='Y' || (hasChanged && resultPtr[0]=='C'))) {
    rebootRequired = true;
    myClient->printf ((const char*)"<li>Rebooting %s.</li>", tname);
  } 
  myClient->printf ((const char*)"</ul><p><a href=\"/\">Back to main page</a></p></td></tr></table><hr></body></html>\r\n");
  if (rebootRequired) {
    myClient->stop();
    delay (1000);
    mt_sys_restart ((const char*)"web requested reboot");
  }
}



/* --------------------------------------------------------------------------- *\
 *
 * List files on the internal filesystem
 *
\* --------------------------------------------------------------------------- */
void mkWebFileIndex(WiFiClient *myClient)
{
  char *dirname = {(char*)"/"};
  char *suffix;
  char msgBuffer[80];
  bool editable;
  fs::FS fs = (fs::FS) SPIFFS;
  File root = fs.open(dirname);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebFileIndex(%x)\r\n", getTimeStamp(), myClient);
    xSemaphoreGive(consoleSem);
  }
  mkWebHeader(myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, "Local Files", 0);
  myClient->printf((const char*)"<h2>File List</h2>");
  if(!root){
    myClient->printf ((const char*)"<p><strong>Error:</strong> Failed to open directory: %s<p>", dirname);
    return;
  }
  if(!root.isDirectory()){
    myClient->printf((const char*)"<p><strong>Error:</strong> %s is not a directory</p>", dirname);
    return;
  }

  File file = root.openNextFile();
  myClient->printf ((const char*)"<table><tr><th>Type</th><th></th><th>Name</th><th></th><th>Size</th></tr>");
  while(file){
    if(file.isDirectory()){
      myClient->printf  ((const char*)"<tr><td>DIR</td><td rowspan=\"2\">%s</td></tr>",(char*)file.name());
      mkWebFileIndex(myClient);
    }
    else {
      editable = true;
      suffix = (char*) file.name() + (strlen((char*) file.name()) - 4);
      if (strcmp(suffix, ".ico") == 0 || strcmp(suffix, ".png") == 0 || strcmp(suffix, ".jpg") == 0 || strcmp(suffix, "jpeg") == 0) editable = false;
      myClient->printf ((const char*)"<tr><td>FILE</td><td>&nbsp;&nbsp;</td><td><a href=\"");
      if (editable) myClient->printf ((const char*)"/edit");
      myClient->printf ((const char*)"%s\">%s</a></td><td>&nbsp;&nbsp;</td><td align=\"right\">%d</td></tr>", (char*)file.name(), (char*)file.name(), (uint) file.size());
    }
    file = root.openNextFile();
  }
  myClient->printf ((const char*)"</table><p>%d bytes used of %d available (%d%% used)</p>", SPIFFS.usedBytes(), SPIFFS.totalBytes(), (SPIFFS.usedBytes()*100)/SPIFFS.totalBytes());
  myClient->printf ((const char*)"<p><strong>Note:</strong> File names should always start with a forward slash (/)</p>");
  myClient->printf ((const char*)"<form action=\"/save\" method=\"post\"><h2>Download</h2><p><input name=\"downloadURL\" type=\"url\" value=\"http://\">&nbsp;<input type=\"submit\" value=\"Download\"><hr></form></td></tr></table></body></html>\r\n");
}



/* --------------------------------------------------------------------------- *\
 *
 * The header at the top of each page forms a standard pattern
 *
\* --------------------------------------------------------------------------- */
void mkWebHtmlHeader (WiFiClient *myClient, const char *header, uint8_t refreshTime)
{
  fs::FS fs = (fs::FS) SPIFFS;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebHtmlHeader(%x, %x, %d)\r\n", getTimeStamp(), myClient, header, refreshTime);
    xSemaphoreGive(consoleSem);
  }
  myClient->printf ((const char*)"<!DOCTYPE html>\r\n<html><head><title>%s: %s</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">", tname, header);
  if (fs.exists(CSSFILE)) {
    myClient->printf ((const char*)"<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">", CSSFILE);
  }
  if (refreshTime > 0) {
    myClient->printf ((const char*)"<meta http-equiv=\"refresh\" content=\"%d\">", refreshTime);
  }
  // The table of links is created below
  myClient->printf ((const char*)"</head><body><center><h1>%s: %s</h1></center><hr><table><tr><td valign=\"top\"><table><tr><td><a href=\"/\">Status&nbsp;Page</a></td></tr>", tname, header);  
  myClient->printf ((const char*)"<tr><td><a href=\"/config\">Main&nbsp;config</a></td></tr><tr><td><a href=\"/functions\">Function&nbsp;Map</a></td></tr><tr><td><a href=\"/files\">Local&nbsp;Files</a></td></tr>");
  #ifdef RELAYPORT
  myClient->printf ((const char*)"<tr><td><a href=\"/fastclock\">Fast&nbsp;Clock</a></td></tr>");
  #endif
  myClient->printf ((const char*)"<tr><td><a href=\"/wifi\">WiFi&nbsp;Networks</a></td></tr><tr><td><a href=\"/password\">Web&nbsp;Password</a></td></tr></table></td><td>&nbsp;&nbsp;</td><td valign=\"top\">");
  // Actual data goes in to the 3rd column, page must close outer table
}


#ifdef RELAYPORT
/* --------------------------------------------------------------------------- *\
 *
 * Fast clock functions
 *
\* --------------------------------------------------------------------------- */
void mkFastClock(WiFiClient *myClient, char *data, uint16_t dataSize, bool keepAlive)
{
  float multiplier = 0.0;
  char *time = NULL, *strMultiplier = NULL;
  uint32_t mins = 0;
  uint32_t hours = 0;
  uint8_t n = 0;
  uint8_t send2dcc = 0;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkFastClock(%x, %x, %d, keepAlive)\r\n", getTimeStamp(), myClient, data, dataSize);
    xSemaphoreGive(consoleSem);
  }
  // Check if we have any updates to time
  time = webScanData (data, "fctime", dataSize);
  if (time != NULL) {
    strMultiplier =  webScanData (data, "fastclock2dcc", dataSize);
    if (strMultiplier[0]=='1') send2dcc = 1;
    nvs_put_int ("fastclock2dcc", send2dcc);
    strMultiplier =  webScanData (data, "fcmultiplier", dataSize);
    if (strMultiplier != NULL) {
      multiplier = util_str2float (strMultiplier);
      for (n=0; n<strlen(time) && time[n]!=':'; n++);
      if (n<strlen(time)) {
        mins = util_str2int(&time[n+1]);
        time[n] = '\0';
        hours = util_str2int(time);
        if (webScanData (data, "retain", dataSize) != NULL) {
          nvs_put_int ("fc_hour", hours);
          nvs_put_int ("fc_min",  mins);
          nvs_put_int ("fc_rate", (int) (multiplier*100.00));
        }
        mins = ((hours*60) + mins) * 60;
        if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          if (fc_time != mins || fc_multiplier != multiplier) {
            fc_time = mins;
            fc_multiplier = multiplier;
            fc_restart = true;
          }
          xSemaphoreGive(fastClockSem);
        }
        else semFailed ("fastClockSem", __FILE__, __LINE__);
      }
    }
  }
  // get the current time for web display
  if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    mins = fc_time;
    multiplier = fc_multiplier;
    xSemaphoreGive(fastClockSem);
    mins  = mins / 60;
    hours = (mins / 60) % 24;
    mins  = mins % 60;
  }
  else semFailed ("fastClockSem", __FILE__, __LINE__);
  // presnt web update form
  send2dcc = nvs_get_int ("fastclock2dcc", 0);
  mkWebHeader(myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, "Fast Clock", nvs_get_int("webRefresh", WEBREFRESH));
  myClient->printf ((const char*) "<form action=\"/fastclock\" method=\"post\"><h3>Clock Settings</h3><table>");
  myClient->printf ((const char*) "<tr><td align=\"right\">Time (HH:MM)</td><td><input type=\"time\" name=\"fctime\" value=\"%02d:%02d\"></td></tr>", hours, mins);
  myClient->printf ((const char*) "<tr><td align=\"right\">Multiplier</td><td><input type=\"number\" name=\"fcmultiplier\" value=\"%3.2f\" min=\"0\" max=\"12\" size=\"5\" step=\"0.05\"></td></tr>", multiplier);
  myClient->printf ((const char*) "<tr><td>&nbsp;</td><td>Multiplier=0.00, clock stopped</td></tr><tr><td>&nbsp;</td><td>Multiplier=1.00, normal pace of time</td></tr><tr><td>&nbsp;</td><td>Multiplier=3.00, 1 min wall clock time is 3 mins scale time</td></tr>");
  myClient->printf ((const char*) "<tr><td align=\"right\">Retain</td><td><input type=\"checkbox\" name=\"retain\" id=\"retain\" value=\"low\"><label for=\"retain\">Use above settings as startup default</label></td></tr>");
  myClient->printf ((const char*) "</table><br><h3>Ex-RAIL Integration</h3><table>");
  myClient->printf ((const char*) "<tr><td>Send Time to<br> Ex-CommandStation</td><td><input type=\"radio\" id=\"no2dcc\" name=\"fastclock2dcc\" value=\"0\"");
  if (send2dcc==0) myClient->printf ((const char*) " checked=\"true\"");
  myClient->printf ((const char*) "><label for=\"no2dcc\">Normal, do not sent time to DCC-Ex</label><br><input type=\"radio\" id=\"yes2dcc\" name=\"fastclock2dcc\" value=\"1\"");
  if (send2dcc!=0) myClient->printf ((const char*) " checked=\"true\"");
  myClient->printf ((const char*) "><label for=\"yes2dcc\">Send time to DCC-EX for Ex-RAIL integration</label></td></tr>");
  myClient->printf ((const char*) "<tr><td align=\"right\">Notes</td><td>Do not use fractional multipliers if using Ex-RAIL integration<br>If relaying DCC-Ex fast clock is not sent to DCC-Ex clients</td></tr>");
  myClient->printf ((const char*) "</table><input type=\"submit\" value=\"Set Time\"></form></td></tr></table><hr></body></html>\r\n");
}
#endif


/* --------------------------------------------------------------------------- *\
 *
 * Edit function key configuration
 *
\* --------------------------------------------------------------------------- */
void mkWebFunctionMap (WiFiClient *myClient, char *postData, uint16_t dataSize, bool keepAlive)
{
  char *functionLabelPtr[29];
  char *token;
  char *rest = NULL;
  uint32_t mask = 1;
  uint32_t latchVal = 0;
  uint32_t leadVal = 0;
  uint16_t loco = 65535;
  uint8_t locoIdx = 255;
  uint8_t idx = 0;
  uint8_t n;
  char pgHeader[sizeof(locoRoster[0].name)];
  char functionLabelTxt[512];
  char labelName[16];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebFunctionMap(%x, %x, %d, keepAlive)\r\n", getTimeStamp(), myClient, postData, dataSize);
    xSemaphoreGive(consoleSem);
  }
  pgHeader[0] = '\0';
  functionLabelTxt[0] = '\0';
  if (postData!=NULL) {
    char *resultPtr;
    resultPtr = webScanData (postData, "locoIdx", dataSize);
    if (resultPtr != NULL && util_str_isa_int(resultPtr)) {
      locoIdx = util_str2int(resultPtr);
      if (locoIdx > locomotiveCount) locoIdx = 255;
    }
  }
  if (locoIdx<255) {
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      loco = locoRoster[locoIdx].id;
      strcpy (pgHeader, locoRoster[locoIdx].name);
      xSemaphoreGive(velociSem);
    }
    else semFailed ("velociSem", __FILE__, __LINE__);
  }
  if (pgHeader[0]=='\0') {
    if (cmdProtocol==DCCEX) strcpy (pgHeader, "Default");
    else strcpy (pgHeader, "Functions Affect");
  }
  if (webScanData (postData, "fLabel_0", dataSize) != NULL) { // we must contain some function key update
    for (n=0, mask=1; n<29; n++, mask<<=1) {
      sprintf (labelName, "fLabel_%d", n);
      token = webScanData (postData, labelName, dataSize);
      if (n>0) strcat (functionLabelTxt, "~");
      if (token != NULL && token[0] != '\0') {
        for (uint8_t q=0; q<strlen(token); q++) if (token[q]=='~') token[q]='^'; // stop any use of "our" field separator in a label
        strcat (functionLabelTxt, token);
      }
      sprintf (labelName, "latch_%d", n);
      if (webScanData (postData, labelName, dataSize) != NULL) latchVal += mask;
      sprintf (labelName, "lead_%d", n);
      if (webScanData (postData, labelName, dataSize) != NULL) leadVal += mask;
    }
    if (loco==65535) {
      nvs_put_string ("FLabelDefault", functionLabelTxt);
      nvs_put_int    ("FLatchDefault", latchVal);
      nvs_put_int    ("FLeadDefault",  leadVal);
      defaultLatchVal = latchVal;
      defaultLeadVal  = leadVal;
    }
    else {
      sprintf (labelName, "FLabel%d", loco);
      nvs_put_string (labelName, functionLabelTxt);
      sprintf (labelName, "FLatch%d", loco);
      nvs_put_int (labelName, latchVal);
      if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        locoRoster[n].functionLatch = latchVal;
        xSemaphoreGive(velociSem);
      }
    else semFailed ("velociSem", __FILE__, __LINE__);
    }
  }
  // we have the data for the form submission, so now get data to build the page
  mkWebHeader(myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, "Functions", 0);
  myClient->printf ((const char*) "<h2>%s</h2>", pgHeader);
  // Now get function defs, loco specific first -> then default from NVS -> then hard coded default
  if (loco<65535) {
    myClient->printf ((const char*) "<ul><li>Loco Id is: %d</li>", loco);
    sprintf (labelName, "FLabel%d", loco);
    nvs_get_string (labelName, functionLabelTxt, "", sizeof(functionLabelTxt));
    if (functionLabelTxt[0] == '\0') {
      myClient->printf ((const char*) "<li>Using default functions</li>");
    }
    else {
      sprintf (labelName, "FLatch%d", loco);
      latchVal = nvs_get_int (labelName, defaultLatchVal);
    }
    myClient->printf ((const char*) "</ul><br>");
  }
  if (functionLabelTxt[0] == '\0') {
    nvs_get_string ("FLabelDefault", functionLabelTxt, FUNCTNAMES, sizeof(functionLabelTxt));
    latchVal = defaultLatchVal;
    leadVal  = defaultLeadVal;
  }
  for (idx=0, token=strtok_r(functionLabelTxt, "~", &rest); token!=NULL; token=strtok_r(NULL, "~", &rest), idx++) {
    functionLabelPtr[idx] = token;
  }
  functionLabelTxt[sizeof(functionLabelTxt)-1] = '\0';
  for (; idx<29; idx++) functionLabelPtr[idx] = &functionLabelTxt[sizeof(functionLabelTxt)-1];
  // create form
  myClient->printf ((const char*) "<form action=\"/functions\" method=\"post\"><input type=\"hidden\" name=\"locoIdx\" value=\"%d\">", locoIdx);
  myClient->printf ((const char*) "<table><tr><th>Function<br>Number</th>");
  if (cmdProtocol==DCCEX) myClient->printf ((const char*) "<th>Latching</th>");
  if (loco==65535) myClient->printf ((const char*) "<th>MultiThrottle Consist<br>Lead Loco Only</th>");
  myClient->printf ((const char*) "<th>Description</th></tr>");
  for (n=0, mask=1; n<29; n++, mask<<=1) {
    myClient->printf ((const char*) "<tr><td style=\"text-align:center;\">%d</td", n);
    if (cmdProtocol==DCCEX) {
      myClient->printf ((const char*) "><td style=\"text-align:center;\"><input type=\"checkbox\" id=\"latch_%d\" name=\"latch_%d\" value=\"%d\"", n, n, n);
      if ((latchVal&mask) > 0) myClient->printf ((const char*) " checked=\"true\"");
    }
    if (loco==65535) {
      myClient->printf ((const char*) "></td><td style=\"text-align:center;\"><input type=\"checkbox\" id=\"lead_%d\" name=\"lead_%d\" value=\"%d\"", n, n, n);
      if ((leadVal&mask) > 0) myClient->printf ((const char*) " checked=\"true\"");
    }
    myClient->printf ((const char*) "></td><td><input type=\"text\" name=\"fLabel_%d\" value=\"%s\" maxlength=\"16\">", n, functionLabelPtr[n]);
    myClient->printf ((const char*) "</td></tr>");
  }
  myClient->printf ((const char*) "</table><br><hr><input type=\"submit\" value=\"Update\"></form>");
  myClient->printf ((const char*) "</td></tr></table><hr></body></html>\r\n");
}


/* --------------------------------------------------------------------------- *\
 *
 * Present System Stats to web page
 *
\* --------------------------------------------------------------------------- */
void mkWebDeviceDescript (WiFiClient *myClient)
{
  esp_chip_info_t chip_info;
  uint32_t throt_time = 36;
  uint16_t mins  = esp_timer_get_time() / (uS_TO_S_FACTOR * 60.0);
  uint16_t hours = mins / 60;
  uint16_t days  = hours / 24;
  uint8_t pwrState = 0;
  uint8_t lcdLines = 0;
  const char *pwrValue[] = {"Off", "On"};

  mins = mins - (hours * 60);
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebDeviceDescript(%x)\r\n", getTimeStamp(), myClient);
    xSemaphoreGive(consoleSem);
  }
  
  hours = hours - (days * 24);
  esp_chip_info(&chip_info);

  if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (fc_time != throt_time) {
      throt_time = fc_time;
    }
    xSemaphoreGive(fastClockSem);
  }
  else semFailed ("fastClockSem", __FILE__, __LINE__);

  myClient->printf ((const char*)"<h2>Device Description</h2><table>");
  myClient->printf ((const char*)"<tr><td align=\"right\">Device Name:</td><td>%s", tname);
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    myClient->printf (" (%d.%d.%d.%d)", ip[0], ip[1], ip[2], ip[3]);
  }
  if (APrunning) {
    myClient->printf (" (192.168.4.1)");
  }
  myClient->printf ((const char*)"</td></tr><tr><td align=\"right\">Hardware:</td><td>%d core ESP32 revision %d, flash = %d MB %s</td></tr>", \
     chip_info.cores, \
     ESP.getChipRevision(), \
     spi_flash_get_chip_size() / (1024 * 1024), \
     (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
  myClient->printf ((const char*)"<tr><td align=\"right\">Software:</td><td>%s %s</td></tr>", PRODUCTNAME, VERSION);
  myClient->printf ((const char*)"<tr><td align=\"right\">Compile time:</td><td>%s %s</td></tr>", __DATE__, __TIME__);
  myClient->printf ((const char*)"<tr><td align=\"right\">Uptime:</td><td>");
  if (days>0) myClient->printf ((const char*)"%d Days ", days);
  myClient->printf ((const char*)"%02d:%02d</td></tr>", hours, mins);
  myClient->printf ((const char*)"<tr><td align=\"right\">Free Memory:</td><td>%s bytes (", util_ftos (ESP.getFreeHeap(), 0));
  myClient->printf ((const char*)"%s%%)</td></tr>", util_ftos ((ESP.getFreeHeap()*100.0)/ESP.getHeapSize(), 1));
  myClient->printf ((const char*)"<tr><td align=\"right\">Min Free Memory:</td><td>%s bytes (", util_ftos (ESP.getMinFreeHeap(), 0));
  myClient->printf ((const char*)"%s%%)</td></tr>", util_ftos ((ESP.getMinFreeHeap()*100.0)/ESP.getHeapSize(), 1));
  myClient->printf ((const char*)"<tr><td align=\"right\">Non Volatile Storage:</td><td>%d x 32 byte blocks</td></tr>", nvs_get_freeEntries());
  myClient->printf ((const char*)"<tr><td align=\"right\">CPU Frequency:</td><td>%s MHz</td></tr>", util_ftos (ESP.getCpuFreqMHz(), 0));
  if (strlen(ssid) > 0) {
    myClient->printf ((const char*)"<tr><td align=\"right\">SSID:</td><td>%s</td></tr>", ssid);
  }
  if (strlen(remServerNode) > 0) {
    myClient->printf ((const char*)"<tr><td align=\"right\">Server Address:</td><td>%s</td></tr>", remServerNode);
  }
  if (strlen(remServerDesc) > 0) {
    myClient->printf ((const char*)"<tr><td align=\"right\">Server Description:</td><td>%s</td></tr>", remServerDesc);
  }
  if (strlen(lastMessage) > 0) {
    myClient->printf ((const char*)"<tr><td align=\"right\">Server Message:</td><td>%s</td></tr>", lastMessage);
  }
  myClient->printf ((const char*)"<tr><td align=\"right\">Track Power:</td><td>");
  if (trackPower) pwrState = 1;
  myClient->printf ((const char*)"<table><tr><td>%s&nbsp;</td><td width=\"50px\" class=\"%s\">&nbsp;</td>", pwrValue[pwrState], pwrValue[pwrState]);
  if (nvs_get_int ("webPwrSwitch", 1) == 1) {
     myClient->printf ((const char*)"<td><form action=\"/status\" method=\"post\">&nbsp;<input type=\"hidden\" name=\"op\" value=\"pwr\"><input type=\"submit\" value=\"switch\"></form></td>");
  }
  myClient->printf ((const char*)"</tr></table></td></tr>");
  myClient->printf ((const char*)"<tr><td align=\"right\">Max Web Clients:</td><td>%d used of %d provisioned</td></tr>", maxWebClientCount, MAXWEBCONNECT);
  #ifdef RELAYPORT
  myClient->printf ((const char*)"<tr><td align=\"right\">Max Relay Clients:</td><td>");
  if (relayMode == NORELAY) myClient->printf ((const char*)"Disabled</td></tr>");
  else myClient->printf ((const char*)"%d used of %d provisioned</td></tr>", maxRelayCount, maxRelay);
  #endif
  if (throt_time != 36) {
    char tString[10];
    timeFormat (tString, throt_time);
    myClient->printf ((const char*)"<tr><td align=\"right\">Fast Clock:</td><td>%s", tString);
    #ifdef RELAYPORT
    if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      myClient->printf ((const char*)" Speed-up %sx", util_ftos (fc_multiplier ,1));
      xSemaphoreGive(fastClockSem);
    }
    else semFailed ("fastClockSem", __FILE__, __LINE__);
    #endif
    myClient->printf ((const char*)"</td></tr>");
  }
  for (uint8_t n=0; n<4; n++) if (dccLCD[n][0] != '\0') lcdLines++;
  if (lcdLines>0) {
    lcdLines = 0;
    myClient->printf ((const char*)"<tr><td align=\"right\">DCC-Ex LCD</td><td>");
    for (uint8_t n=0; n<4; n++) if (dccLCD[n][0] != '\0') {
      if (lcdLines>0) myClient->printf ((const char*)"<br>");
      myClient->printf ((const char*) "%s", dccLCD[n]);
      lcdLines++;
    }
    myClient->printf ((const char*)"</td></tr>");
  }
  myClient->printf ((const char*)"</table>");
}

void mkWebSysStat(WiFiClient *myClient, bool keepAlive, bool authenticated, char *postData, uint16_t dataSize)
{
  uint16_t tCount = 0;
  const char *dirName[] = { "Forward", "Stop", "Reverse", "Unchanged", "Em. Stop" };
  char *tPtr = NULL;
  uint8_t showDescript;
  uint8_t noPwrTurnouts = nvs_get_int ("noPwrTurnouts", 0);

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebSysStat(%x, keepAlive, authenicated, %x, %d)\r\n", getTimeStamp(), myClient, postData, dataSize);
    xSemaphoreGive(consoleSem);
  }
  
  // display headers
  mkWebHeader(myClient, 200, 0, keepAlive);
  mkWebHtmlHeader (myClient, "Status", nvs_get_int("webRefresh", WEBREFRESH));
  // If there is post data, then we probably need to operate a turn out or some other equipment
  if (authenticated && postData!=NULL) {
    char *resultPtr;
    uint8_t targetUnit = 255;
    uint8_t targetOperation;
    char outCommand[40];
    resultPtr = webScanData (postData, "op", dataSize);
    if (resultPtr != NULL) {
      if (strcmp (resultPtr, "pwr") == 0) {         // Power switch
        uint8_t pwrState = 1;
        if (trackPower) pwrState = 2;
        setTrackPower (pwrState);
      }
      else if (strcmp (resultPtr, "turn") == 0) {   // turnout operation
        resultPtr = webScanData (postData, (char*) "unit", dataSize);
        outCommand[0] = '\0';
        if (resultPtr != NULL) {
          if (util_str_isa_int (resultPtr)) targetUnit = util_str2int(resultPtr); // Assume parameter is index number
          else {                                                                  // Assume parameter in sysName
            if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {   // avoid contention when accessing data
              for (uint8_t n=0; n<turnoutCount && targetUnit==255; n++) if (strcmp (turnoutList[n].sysName, resultPtr) == 0) targetUnit = n;
              xSemaphoreGive(turnoutSem);
            }
            else semFailed ("turnoutSem", __FILE__, __LINE__);
          }
          if (targetUnit < turnoutCount) {
            if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) { // avoid contention when accessing data
              targetOperation = turnoutList[targetUnit].state;
              xSemaphoreGive(turnoutSem);
              if (targetOperation==2 || targetOperation=='2' || targetOperation=='C') targetOperation = 'T';
              else targetOperation = 'C';
              if (targetUnit < 255) {
                setTurnout (targetUnit, targetOperation);
              }
            }
            else semFailed ("turnoutSem", __FILE__, __LINE__);
          }
          else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("Turnout ID %s not found\r\n", resultPtr);
            xSemaphoreGive(consoleSem);
          }
        }
      }
      else if (strcmp (resultPtr, "route") == 0) {   // turnout operation
        resultPtr = webScanData (postData, (char*) "unit", dataSize);
        outCommand[0] = '\0';
        if (resultPtr != NULL) {
          if (util_str_isa_int (resultPtr)) targetUnit = util_str2int(resultPtr); // Assume parameter is index number
          else {                                                                  // Assume parameter in sysName
            if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {     // avoid contention when accessing data
              for (uint8_t n=0; n<routeCount && targetUnit==255; n++) if (strcmp (routeList[n].sysName, resultPtr) == 0) targetUnit = n;
              xSemaphoreGive(routeSem);
            }
            else semFailed ("routeSem", __FILE__, __LINE__);
          }
          if (targetUnit < routeCount) {
            if (cmdProtocol == WITHROT) setRoute(targetUnit);
            else xTaskCreate(setRouteBg, "setRouteBgWeb", 4096, &targetUnit, 4, NULL);
          }
        }
      }
    delay (250); // nominal delay for action to initiate
    }
  }
  showDescript = nvs_get_int ("webStatus", 0);
  // Show description at top of page?
  if (showDescript == 0) mkWebDeviceDescript(myClient);
  // Print loco roster
  // first check if there are any locos "beyond the roster", so we can still show detail even with an empty roster
  tCount = 0;
  if (locomotiveCount==0) for (uint8_t n=locomotiveCount; n<locomotiveCount+MAXCONSISTSIZE && tCount==0; n++) {
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) { // avoid contention when accessing data
      if (locoRoster[n].relayIdx != 255) tCount++;
      xSemaphoreGive(velociSem);
    }
  }
  if (locomotiveCount>0 || tCount>0) {
    int16_t speedPercent;
    uint8_t locoDir;
    uint8_t locoSteps;
    uint8_t limit = locomotiveCount+MAXCONSISTSIZE;
    #ifdef RELAYPORT
    uint8_t relayIdx;
    char    throttleNr;
    #endif
    tCount = 0;
    myClient->printf ((const char*)"<h2>Locomotive Roster</h2><table><tr><th>ID</th><th>Description</th>");
    #ifdef RELAYPORT
    myClient->printf ((const char*)"<th>Operator</th>");
    #endif
    myClient->printf ((const char*)"<th align=\"right\">Speed %%</th><th>Speed Graph</th>");
    if (cmdProtocol==DCCEX) {
      myClient->printf ((const char*)"<th>Configure</td>");
    }
    myClient->printf ((const char*)"</tr>");
    for (uint8_t n=0; n<limit; n++) {
      // If beyond the limit of the defined roster, check if the entry is for an ad-hoc address
      if (n>=locomotiveCount) {
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) { // avoid contention when accessing data
          if (locoRoster[n].relayIdx != 255) tCount=1;
          else tCount=0;
          xSemaphoreGive(velociSem);
        }
      }
      // If this is for a known loco print the detail
      if ((n<locomotiveCount || tCount>0) && locoRoster!=NULL) {
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {  // do inside loop, to yield to any network I/O demands
          myClient->printf ((const char*)"<tr><td align=\"right\">%d</td><td align=\"left\">%s</td>", locoRoster[n].id, locoRoster[n].name);
          locoDir = locoRoster[n].direction;
          locoSteps = locoRoster[n].steps-2;
          speedPercent = locoRoster[n].speed;
          #ifdef RELAYPORT
          relayIdx = locoRoster[n].relayIdx;
          throttleNr = locoRoster[n].throttleNr;
          #endif
          xSemaphoreGive(velociSem);
          if (speedPercent < 0) {
            speedPercent = 0;
            locoDir = 4;
          }
          speedPercent = (speedPercent*100)/locoSteps;
          #ifdef RELAYPORT
          if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            if (relayIdx == 255) myClient->printf ((const char*)"<td></td>");
            else {
              char suffix[5];
              if (relayMode == WITHROTRELAY) sprintf (suffix, "+%c", throttleNr);
              else suffix[0] = '\0';
              if (locoRoster[n].relayIdx == 240) myClient->printf ((const char*)"<td>Serial line%s</td>", suffix);
              else if (remoteSys[relayIdx].nodeName[0] != '\0') myClient->printf ((const char*)"<td>%s%s</td>", remoteSys[relayIdx].nodeName, suffix);
              else {
                IPAddress remClient = remoteSys[relayIdx].address;
                myClient->printf ((const char*)"<td>%d.%d.%d.%d%s</td>", remClient[0], remClient[1], remClient[2], remClient[3], suffix);
              }
            }
            xSemaphoreGive(relaySvrSem);
          }
          else semFailed ("relaySvrSem", __FILE__, __LINE__);
          #endif
          myClient->printf ((const char*)"<td align=\"right\">%s %d</td><td><table><tr><td>", dirName[locoDir], speedPercent);
          switch (locoDir) {
            FORWARD: myClient->printf ((const char*)">");
                     break;
            REVERSE: myClient->printf ((const char*)"<");
                     break;
            default: myClient->printf ((const char*)"&nbsp;");
                     break;
          }
          myClient->printf ((const char*)"</td><td width=\"%dpx\" class=\"speed\"></td><td width=\"%dpx\" class=\"space\"></td></tr></table></td>", speedPercent, 100-speedPercent);
          if (cmdProtocol==DCCEX) {
            if (n<locomotiveCount)
              myClient->printf ((const char*)"<td><form action=\"/functions\" method=\"post\"><input type=\"hidden\" name=\"locoIdx\" value=\"%d\"><input type=\"submit\" value=\"Functions\"></form></td>", n);
            else myClient->printf ((const char*)"<td>&nbsp;</td>");
          }
          myClient->printf ((const char*)"</tr>");
        }
        else semFailed ("velociSem", __FILE__, __LINE__);
      }
    }
    myClient->printf ((const char*)"</table>");
  }
  if (turnoutCount>0 && turnoutCount<255 && turnoutList!=NULL && turnoutState!=NULL) {
    char prefix[] = {'\0','\0'};
    myClient->printf ((const char*)"<h2>Turnouts</h2><table><tr><th>ID</th><th>Description</th><th>State</th><th>Operate</th></tr>");
    for (uint8_t n=0; n<turnoutCount; n++) {
      tPtr = NULL;
      for (uint8_t z=0; z<turnoutStateCount && tPtr==NULL; z++) if (turnoutList[n].state == turnoutState[z].state) tPtr = turnoutState[z].name;
      if (tPtr == NULL) tPtr = (char*)"unknown";
      if (xSemaphoreTake(turnoutSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        myClient->printf ((const char*)"<tr><td align=\"left\">");
        if (cmdProtocol == DCCEX) {
          myClient->printf ((const char*)"T");
        }
        myClient->printf ((const char*)"%s</td><td align=\"left\">%s</td>", turnoutList[n].sysName, turnoutList[n].userName);
        xSemaphoreGive(turnoutSem);
        myClient->printf ((const char*)"<td class=\"%s\">%s</td><td>", tPtr, tPtr);
        if (noPwrTurnouts==1 || trackPower) {
          myClient->printf ((const char*)"<form action=\"/status\" method=\"post\"><input type=\"hidden\" name=\"op\" value=\"turn\"><input type=\"hidden\" name=\"unit\" value=\"%d\"><input type=\"submit\" value=\"Change\"></form>", n);
        }
        myClient->printf ((const char*)"</td></tr>");
      }
      else semFailed ("turnoutSem", __FILE__, __LINE__);
    }
    myClient->printf ((const char*)"</table>");
  }
  if (routeCount>0 && routeCount<255 && routeList!=NULL && routeState!=NULL) {
    myClient->printf ((const char*)"<h2>Routes</h2><p>Max steps per route = %d</p><table><tr><th>ID</th><th>Description</th><th>State</th><th>Operate</th></tr>", MAXRTSTEPS);
    for (uint8_t n=0; n<routeCount; n++) {
      tPtr = NULL;
      for (uint8_t z=0; z<routeStateCount && tPtr==NULL; z++) if (routeList[n].state == routeState[z].state) tPtr = routeState[z].name;
      if (tPtr == NULL) tPtr = (char*)"unknown";
      if (xSemaphoreTake(routeSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        myClient->printf ((const char*)"<tr><td align=\"left\">%s</td><td align=\"left\">%s</td><td class=\"%s\">%s</td><td>", routeList[n].sysName, routeList[n].userName, tPtr, tPtr);
        if (routeList[n].state == '4' && (noPwrTurnouts==1 || trackPower)) {
          myClient->printf ((const char*)"<form action=\"/status\" method=\"post\"><input type=\"hidden\" name=\"op\" value=\"route\"><input type=\"hidden\" name=\"unit\" value=\"%d\"><input type=\"submit\" value=\"Activate\"></form>", n);
        }
        myClient->printf ((const char*)"</td></tr>");
        xSemaphoreGive(routeSem);
      }
      else semFailed ("routeSem", __FILE__, __LINE__);
    }
    myClient->printf ((const char*)"</table>");
  }
  if (locomotiveCount>0 || turnoutCount>0) {
    myClient->printf ((const char*)"<br><p><strong>NB:</strong>Throttle speed and turnout state data may vary from actual controller data, as this throttle might not have received all status updates or historical data.</p>");
  }
  #ifdef RELAYPORT
  if (remoteSys != NULL) {
    bool isActiveRelay = false;
    char localIPaddr[16]; // expect IPv4
    char remoteName[NAMELENGTH];
    uint32_t inPackets;
    uint32_t outPackets;
    myClient->printf ((const char*)"<h2>Relay Statistics</h2><p>Relay port is: %d, relay protocol is %s.</p><table><tr><th>Remote IP</th><th>Name</th><th>Received Commands</th><th>Sent Commands</th></tr>", relayPort, relayTypeList[relayMode]);
    if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      myClient->printf ((const char*)"<tr><td>Serial line</td><td>%s</td><td align=\"right\">%d</td><td align=\"right\">%d</td></tr>", tname, localinPkts, localoutPkts);
      xSemaphoreGive(relaySvrSem);
    }
    else semFailed ("relaySvrSem", __FILE__, __LINE__);
    for (uint8_t n=0; n<maxRelay; n++) {
      if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (remoteSys[n].client->connected()) {
          isActiveRelay = true;
          sprintf (localIPaddr, "%d.%d.%d.%d", remoteSys[n].address[0], remoteSys[n].address[1], remoteSys[n].address[2], remoteSys[n].address[3]);
          strcpy (remoteName,  remoteSys[n].nodeName);
          inPackets  = remoteSys[n].inPackets;
          outPackets = remoteSys[n].outPackets;
        }
        else isActiveRelay = false;
        xSemaphoreGive(relaySvrSem);
        if (isActiveRelay) {
          myClient->printf ((const char*)"<tr><td>%s</td><td>%s</td><td align=\"right\">%d</td><td align=\"right\">%d</td></tr>", localIPaddr, remoteName, inPackets, outPackets);
        }
      }
      else semFailed ("relaySvrSem", __FILE__, __LINE__);
    }
    myClient->printf ((const char*)"</table>");
  }
  #endif
  // Show description at top of page?
  if (showDescript == 1) mkWebDeviceDescript(myClient);
  // Show footer
  myClient->printf ((const char*) "</td></tr></table><hr></body></html>\r\n");
}



/* --------------------------------------------------------------------------- *\
 *
 *
\* --------------------------------------------------------------------------- */
void mkWebError(WiFiClient *myClient, uint16_t code, char* myUri, bool keepAlive)
{
  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s mkWebError(%x, %d, %s, keepAlive)\r\n", getTimeStamp(), myClient, code, myUri);
    xSemaphoreGive(consoleSem);
  }
  char *msgTxt = (char*) mkWebHeader(myClient, code, 0, keepAlive);
  myClient->printf ((const char*)"<!DOCTYPE html>\r\n");
  myClient->printf ((const char*)"<html><head><title>%s</title></head><body bgcolor=#888888><center><h1>%s</h1></center><hr><p>Error %d: %s. ", msgTxt, msgTxt, code ,msgTxt);
  myClient->printf ((const char*)"(%s, %s %s)</p>",tname, PRODUCTNAME, VERSION);
  if (myUri != NULL) {
    if (strlen(myUri) == 0)  myClient->printf ((const char*)"<p>URI: None provided.</p>");
    else myClient->printf ((const char*)"<p>URI: %s</p>", myUri);
  }
  myClient->printf ((const char*)"<hr></body></html>\r\n");
}
#endif
