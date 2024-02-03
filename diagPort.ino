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

#ifdef USEWIFI

void diagPortMonitor (void *pvParameters)
// This is the diagnostic port monitor task.
{
  (void) pvParameters;
  static WiFiClient diagServerClient[MAXDIAGCONNECT];
  const char paramm = 'm';
  const char paramw = 'w';
  const char paramu = 'u';
  int readState = 0;
  uint16_t diagPort = nvs_get_int ("diagPort", 23);
  uint16_t outPtr = 0;
  uint8_t i = 1;
  char inChar;
  char outBuffer[512];
  bool quit = false;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s diagPortMonitor (NULL)\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    quit = diagReceiveOK;
    diagReceiveOK = true;
    xSemaphoreGive(shmSem);
    if (quit) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Diag port monitor closed (already running)\r\n", getTimeStamp());
        xSemaphoreGive(consoleSem);
      }
      vTaskDelete( NULL );
      return;
    }
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  while ((!APrunning) && WiFi.status() != WL_CONNECTED) delay (1000);
  delay (TIMEOUT);
 
  // Listen for incoming connections
  if (diagServer == NULL) {
    diagServer = new WiFiServer(diagPort);
  }
  diagServer->begin();
  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    diagIsRunning = true;
    xSemaphoreGive(shmSem);
  }
  // Advertise as a service
  mdns_txt_item_t txtrecord[] = {{(char*)"server", (char*)PRODUCTNAME}, {(char*)"version", (char*)VERSION}};
  mdns_service_add(NULL, "_telnet", "_tcp", diagPort, txtrecord, 2);
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Diagnostic port monitor open on port %d for max of %d clients.\r\n", getTimeStamp(), diagPort, MAXDIAGCONNECT);
    xSemaphoreGive(consoleSem);
  }
  // while (xQueueReceive(diagQueue, &inChar, pdMS_TO_TICKS(debounceTime)) == pdPASS) {} // clear the diag queue
  while (xQueueReceive(diagQueue, &inChar, 0) == pdPASS) {} // clear the diag queue
  while (diagIsRunning) {
    if (APrunning || WiFi.status() == WL_CONNECTED) {
      if (diagServer->hasClient()) {
        for(i = 0; i < MAXDIAGCONNECT; i++){
          if (!diagServerClient[i] || !diagServerClient[i].connected()){
            if(diagServerClient[i]) diagServerClient[i].stop();
            diagServerClient[i] = diagServer->available();
            diagServerClient[i].setNoDelay (true);
            i = MAXDIAGCONNECT + 1;
          }
        }
        if (i == MAXDIAGCONNECT) {
          diagServer->available().stop();  // no worker threads available
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Rejecting new diagnostic connection, all available slots connected.\r\n", getTimeStamp());
            xSemaphoreGive(consoleSem);
          }
        }
        else {
          diagWrite (&(diagServerClient[i]), (char*) "New diag client connects:\r\n");
          diagHelp(&(diagServerClient[i]));
        }
      }
      if (xQueueReceive(diagQueue, &inChar, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
        outBuffer[outPtr++] = inChar;
        if (inChar=='\n' || outPtr==(sizeof(outBuffer)-1)) {
          outBuffer[outPtr++] = '\0';
          diagWrite (&(diagServerClient[i]), outBuffer);
          outPtr = 0;
        }
      }
      for(i = 0; i < MAXDIAGCONNECT; i++){
        if (diagServerClient[i] && diagServerClient[i].connected()){
          if(diagServerClient[i].available()){
            //get data from the telnet client
            //inChar = diagServerClient[i].read();
            readState = diagServerClient[i].read((uint8_t*) &inChar, 1);
            // read options & ignore
            if (readState == 1) {
              if (inChar == 0xFF) {
                delay(10);
                if(diagServerClient[i].available()) diagServerClient[i].read((uint8_t*) &inChar, 1);
                if(diagServerClient[i].available()) diagServerClient[i].read((uint8_t*) &inChar, 1);
              }
              else switch (inChar) {
                case 'k':
                case 'K':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    if (diagMonitorMode == 'k') {
                      diagMonitorMode = ' ';
                      xSemaphoreGive(shmSem);
                      diagWrite (&(diagServerClient[i]), (char*)  "----  Pause Keypad Data  ----\r\n");
                    }
                    else {
                      diagMonitorMode = 'k';
                      xSemaphoreGive(shmSem);
                      diagWrite (&(diagServerClient[i]), (char*)  "----  Show Keypad Data  ----\r\n");
                    }
                  }
                  break;
                case 'h':
                case 'H':
                case '?':
                  diagHelp(&(diagServerClient[i]));
                  break;
                case 'c':
                case 'C':
                  diagServerClient[i].write((char*)  "----  Close Connection  ----\r\n");
                  diagServerClient[i].stop();
                  break;
                case 'q':
                case 'Q':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    diagIsRunning = false;
                    diagMonitorMode = ' ';
                    xSemaphoreGive(shmSem);
                    diagWrite (&(diagServerClient[i]), (char*)  "----  Quit Diagnostic Mode  ----\r\n");
                  }
                  break;
                case 'p':
                case 'P':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    if (diagMonitorMode == 'p') {
                      diagMonitorMode = ' ';
                      xSemaphoreGive(shmSem);
                      diagWrite (&(diagServerClient[i]), (char*)  "----  Pause Packets  ----\r\n");
                    }
                    else {
                      diagMonitorMode = 'p';
                      xSemaphoreGive(shmSem);
                      diagWrite (&(diagServerClient[i]), (char*)  "----  Show Packets  ----\r\n");
                      diagWrite (&(diagServerClient[i]), (char*)  "    --> Sent by this unit\r\n");
                      #ifndef RELAYPORT
                      diagWrite (&(diagServerClient[i]), (char*)  "    KA> Keep Alives sent\r\n");
                      #endif
                      diagWrite (&(diagServerClient[i]), (char*)  "    <-- Received by this Unit\r\n");
                    }
                  }
                  break;
                #ifdef RELAYPORT
                case 'r':
                case 'R':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    if (diagMonitorMode == 'r') {
                      diagMonitorMode = ' ';
                      xSemaphoreGive(shmSem);
                      diagWrite (&(diagServerClient[i]), (char*)  "----  Pause Relay Packets  ----\r\n");
                    }
                    else {
                      diagMonitorMode = 'r';
                      xSemaphoreGive(shmSem);
                      diagWrite (&(diagServerClient[i]), (char*)  "----  Show Relay Packets  ----\r\n");
                      diagWrite (&(diagServerClient[i]), (char*)  "    n-> Sent by this unit\r\n");
                      diagWrite (&(diagServerClient[i]), (char*)  "    n<- Received by this Unit\r\n");
                      diagWrite (&(diagServerClient[i]), (char*)  "    Where n is the relay client number, or * for all clients\r\n");
                    }
                  }
                  break;
                #endif
                case 'm':
                case 'M':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    diagMonitorMode = 'm';
                    xSemaphoreGive(shmSem);
                    diagWrite (&(diagServerClient[i]), (char*)  "----  Switch to memory dump mode  ----\r\n");
                    xTaskCreate(diagSlaveRunner, "runner_m", 4096, (void*) &paramm, 4, NULL);
                  }
                  break;
                case 'w':
                case 'W':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    diagMonitorMode = 'w';
                    xSemaphoreGive(shmSem);
                    diagWrite (&(diagServerClient[i]), (char*)  "----  Switch to WiFi scan  ----\r\n");
                    xTaskCreate(diagSlaveRunner, "runner_w", 4096, (void*) &paramw, 4, NULL);
                  }
                  break;
                #ifdef OTAUPDATE
                #ifndef NOHTTPCLIENT
                case 'u':
                case 'U':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    diagMonitorMode = 'u';
                    xSemaphoreGive(shmSem);
                    diagWrite (&(diagServerClient[i]), (char*)  "----  Switch to Over The Air Update Mode  ----\r\n");
                    xTaskCreate(diagSlaveRunner, "runner_u", 10240, (void*) &paramu, 4, NULL);
                  }
                  break;
                #endif
                #endif
              }
            }
          }
        }
        else {
          if (diagServerClient[i]) {
            diagServerClient[i].stop();
          }
        }
      }
    }
    else if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      diagIsRunning = false;
      xSemaphoreGive(shmSem);
    }
    else semFailed ("shmSem", __FILE__, __LINE__);
  }

  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    diagIsRunning = false;
    diagMonitorMode = ' ';
    xSemaphoreGive(shmSem);
  }
  else semFailed ("shmSem", __FILE__, __LINE__);
  // shutdown diagserver
  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    {
      bool changed = false;
      for(i = 0; i < MAXDIAGCONNECT; i++) {
        if (diagServerClient[i] && diagServerClient[i].connected()){
          diagServerClient[i].write("----  Close Connection  ----\r\n");
          diagServerClient[i].stop();
          changed = true;
        }
      }
      if (changed) delay(1000);   // small close-wait before terminating service.
    }
    diagServer->close();
    diagServer->stop();
    diagServer->end();
    xSemaphoreGive(tcpipSem);
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Diagnostic port monitor closed (quit)\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    diagReceiveOK = false;
    xSemaphoreGive(shmSem);
  }
  else semFailed ("shmSem", __FILE__, __LINE__);
  vTaskDelete( NULL );
}


void diagHelp(WiFiClient* myClient)
{
  diagWrite (myClient, (char*) " * k - keypad trace\r\n");
  diagWrite (myClient, (char*) " * m - memory dump\r\n");
  #ifdef RELAYPORT
  diagWrite (myClient, (char*) " * p - show/pause packets with DCC-Ex\r\n");
  diagWrite (myClient, (char*) " * r - show/pause relayed packets with clients\r\n");
  #else
  diagWrite (myClient, (char*) " * p - show/pause packets with control system\r\n");
  #endif
  diagWrite (myClient, (char*) " * w - wifi scan\r\n");
  #ifdef OTAUPDATE
  #ifndef NOHTTPCLIENT
  diagWrite (myClient, (char*) " * u - over the air update\r\n");
  #endif
  #endif
  diagWrite (myClient, (char*) " * h - help\r\n");
  diagWrite (myClient, (char*) " * c - close connection, but leave diag port running\r\n");
  diagWrite (myClient, (char*) " * q - quit diagnostic mode, and stop diag port service\r\n\n");
}

void diagWrite (WiFiClient *client, char *message)
{
  uint16_t i;
  for(i = 0; i < MAXDIAGCONNECT; i++){
    if (client[i] && client[i].connected()){
      client[i].write(message);
    }
  }
}


/*
 * Place new data in the monitor queue
 */
void diagEnqueue (char source, char *data, bool addTermination)
{
  bool isRunning = false;

  if (debuglevel>3 && source!='m' && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s diagEnqueue (%c, %s)\r\n", getTimeStamp(), source, data);
    xSemaphoreGive(consoleSem);
  }
  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    isRunning = diagIsRunning;
    if (source != diagMonitorMode) isRunning = false; // print if mode matches or it is an "emergency" message.
    if (source == 'e') isRunning = true;
    xSemaphoreGive(shmSem);
  }
  else semFailed ("shmSem", __FILE__, __LINE__);

  if (!isRunning) return;
  else {
    uint16_t limit = strlen(data);
    for (uint16_t n=0; n<limit; n++) {
      xQueueSend (diagQueue, &(data[n]), pdMS_TO_TICKS(TIMEOUT/4));
    }
    if (addTermination) {
      char tdata[] = {'\r', '\n'};
      xQueueSend (diagQueue, &(tdata[0]), pdMS_TO_TICKS(TIMEOUT/4));
      xQueueSend (diagQueue, &(tdata[1]), pdMS_TO_TICKS(TIMEOUT/4));
    }
  }
}


/*
 * run child processes in separate thread so main thread can process the queue populated by child processes
 */
void diagSlaveRunner(void *pvParameters)
{
  char *option = (char*) pvParameters;
  const char *param[] = {"dump"};

  switch (option[0]){
  case 'm':
    mt_dump_data (1, param);
    break;
  case 'w':
    char outBuffer[80];
    diagEnqueue ('w', "Please wait. Scanning WiFi networks", true);
    numberOfNetworks = WiFi.scanNetworks();
    sprintf(outBuffer, "Number of networks found: %d", numberOfNetworks);
    diagEnqueue ('w', outBuffer, true);
    sprintf(outBuffer, "%-16s %8s %-17s Channel Type", "Name", "Strength", "Address");
    diagEnqueue ('w', outBuffer, true);
    for (int i = 0; i < numberOfNetworks; i++) {
      sprintf (outBuffer, "%-16s %8d %-17s %7d %s", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.BSSIDstr(i).c_str(), WiFi.channel(i), wifi_EncryptionType(WiFi.encryptionType(i)));
      diagEnqueue ('w', outBuffer, true);
    }
    break;
  #ifdef OTAUPDATE
  #ifndef NOHTTPCLIENT
  case 'u':
    if (OTAcheck4update(NULL)) {
      delay (1000);
      mt_sys_restart ("Restarting to boot updated code.");
      }
    break;
  #endif
  #endif
  }
  vTaskDelete( NULL ); 
}

#endif
