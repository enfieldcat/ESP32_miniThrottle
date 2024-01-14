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



void diagPortMonitor (void *pvParameters)
// This is the diagnostic port monitor task.
{
  (void) pvParameters;
  static WiFiClient diagServerClient[MAXDIAGCONNECT];
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
          diagWrite (&(diagServerClient[0]), (char*) "New diag client connects:\r\n");
          diagHelp(&(diagServerClient[0]));
        }
      }
      if (xQueueReceive(diagQueue, &inChar, pdMS_TO_TICKS(debounceTime)) == pdPASS) {
        outBuffer[outPtr++] = inChar;
        if (inChar=='\n' || outPtr==(sizeof(outBuffer)-1)) {
          outBuffer[outPtr++] = '\0';
          diagWrite (&(diagServerClient[0]), outBuffer);
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
                case 'h':
                case 'H':
                case '?':
                  diagHelp(&(diagServerClient[0]));
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
                    diagWrite (&(diagServerClient[0]), (char*)  "----  Quit Diagnostic Mode  ----\r\n");
                  }
                  break;
                case 'p':
                case 'P':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    if (diagMonitorMode == 'p') {
                      diagMonitorMode = ' ';
                      diagWrite (&(diagServerClient[0]), (char*)  "----  Pause Packets  ----\r\n");
                    }
                    else {
                      diagMonitorMode = 'p';
                      diagWrite (&(diagServerClient[0]), (char*)  "----  Show Packets  ----\r\n");
                      diagWrite (&(diagServerClient[0]), (char*)  "    --> Sent by this unit\r\n");
                      #ifndef RELAYPORT
                      diagWrite (&(diagServerClient[0]), (char*)  "    KA> Keep Alives sent\r\n");
                      #endif
                      diagWrite (&(diagServerClient[0]), (char*)  "    <-- Received by this Unit\r\n");
                    }
                    xSemaphoreGive(shmSem);
                  }
                  break;
                #ifdef RELAYPORT
                case 'r':
                case 'R':
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    if (diagMonitorMode == 'r') {
                      diagMonitorMode = ' ';
                      diagWrite (&(diagServerClient[0]), (char*)  "----  Pause Relay Packets  ----\r\n");
                    }
                    else {
                      diagMonitorMode = 'r';
                      diagWrite (&(diagServerClient[0]), (char*)  "----  Show Relay Packets  ----\r\n");
                      diagWrite (&(diagServerClient[0]), (char*)  "    n-> Sent by this unit\r\n");
                      diagWrite (&(diagServerClient[0]), (char*)  "    n<- Received by this Unit\r\n");
                      diagWrite (&(diagServerClient[0]), (char*)  "    Where n is the relay client number, or * for all clients\r\n");
                    }
                    xSemaphoreGive(shmSem);
                  }
                  break;
                #endif
                case 'm':
                case 'M':
                  const char *param = {"m"};
                  if (xSemaphoreTake(shmSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                    diagMonitorMode = 'm';
                    xSemaphoreGive(shmSem);
                    diagWrite (&(diagServerClient[0]), (char*)  "----  Pause Packets  ----\r\n");
                    xTaskCreate(diagSlaveRunner, "runner_m", 4096, (void*) param, 4, NULL);
                  }
                  break;
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
  diagWrite (myClient, (char*) " * p - show/pause packets with control system\r\n");
  #ifdef RELAYPORT
  diagWrite (myClient, (char*) " * r - show/pause relayed packets\r\n");
  #endif
  diagWrite (myClient, (char*) " * m - memory dump\r\n");
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

  if (debuglevel>2 && source!='m' && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
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
  }
  vTaskDelete( NULL ); 
}
