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



void receiveNetData(void *pvParameters)
// This is the network receiver task.
{
  (void) pvParameters;
  uint64_t statusTime = 0;
  uint16_t bumpCount=0;
  char inBuffer[NETWBUFFSIZE];
  char inChar;
  uint8_t bufferPtr = 0;
  bool quit = false;

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s receiveNetData(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }
  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    quit = netReceiveOK;
    netReceiveOK = true;
    xSemaphoreGive(tcpipSem);
    if (quit) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("%s Network connection closed (duplicate thread)\r\n", getTimeStamp());
        xSemaphoreGive(displaySem);
      }
      vTaskDelete( NULL );
      return;
    }
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  setInitialData();
  #ifdef SERIALCTRL
  while (true) {
    while (serial_dev.available()) {
      if (xSemaphoreTake(serialSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        inChar = serial_dev.read();
        xSemaphoreGive(serialSem);
      }
      else semFailed ("serialSem", __FILE__, __LINE__);
  #else
  while (netConnState (1)) {
    while (netConnState(2)) {
      if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        inChar = client.read();
        xSemaphoreGive(tcpipSem);
      }
      else semFailed ("tcpipSem", __FILE__, __LINE__);
  #endif
      if (inChar == '\r' || inChar == '\n' || (cmdProtocol==DCCEX && inChar=='>') || bufferPtr == (NETWBUFFSIZE-1)) {
        if (bufferPtr > 0) {
          if (cmdProtocol==DCCEX && inChar=='>') inBuffer[bufferPtr++] = '>';
          inBuffer[bufferPtr] = '\0';
          processPacket (inBuffer);
          bufferPtr = 0;
        }
      }
      else inBuffer[bufferPtr++] = inChar;
    }
    delay (debounceTime);
    if (cmdProtocol == DCCEX) { // send out periodic <s> status request - a keep alive of sorts on an approx 10 minute basis
      if (bumpCount++ > 1000) {
        bumpCount = 0;
        if (statusTime+(600*uS_TO_S_FACTOR) < esp_timer_get_time()) {
          statusTime = esp_timer_get_time();
          txPacket ("<s>");
        }
      }
    }
  }
  #ifndef SERIALCTRL
  #ifdef TRACKPWR
  digitalWrite(TRACKPWR, LOW);
  #endif
  #ifdef TRACKPWRINV
  digitalWrite(TRACKPWRINV, HIGH);
  #endif
  cmdProtocol = UNDEFINED;
  initialDataSent = false;
  #endif
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Network connection closed (disconnect)\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }
  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    netReceiveOK = false;
    xSemaphoreGive(tcpipSem);
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  vTaskDelete( NULL );
}


#ifndef SERIALCTRL
bool netConnState (uint8_t chkmode)
{
  bool retval = false;
  if (xSemaphoreTake(tcpipSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    switch (chkmode) {
    case 1: retval = (APrunning || WiFi.status() == WL_CONNECTED) && client.connected();
            break;
    case 2: retval = (APrunning || WiFi.status() == WL_CONNECTED) && client.connected() && client.available()>0;
            break;
    }
    xSemaphoreGive(tcpipSem);
  }
  else semFailed ("tcpipSem", __FILE__, __LINE__);
  return (retval);
}
#endif


void processPacket (char *packet)
{
  if (packet == NULL || packet[0]=='\0') return;
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s processPacket(%s)\r\n", getTimeStamp(), packet);
    xSemaphoreGive(displaySem);
  }
  if (showPackets) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("<-- ");
      Serial.println (packet);
      xSemaphoreGive(displaySem);
    }
  }
  #ifdef RELAYPORT
  if (xSemaphoreTake(relaySvrSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    localinPkts++;
    xSemaphoreGive(relaySvrSem);
  }
  if (packet != NULL && packet[0] == '<') { // Stuff that is worth forwarding
    if (relayMode == DCCRELAY) { // if relaying DCC-Ex, just forward to any connected connections
      for (uint8_t n=0; n<maxRelay; n++) if (remoteSys != NULL && remoteSys[n].client != NULL && remoteSys[n].client->connected()) {
        reply2relayNode (&remoteSys[n], packet);
      }
    }
  }
  #endif
  // First handle some simple things
  if (strncmp (packet, "VN", 2) == 0 || strncmp (packet, "PFC", 3) == 0) {  // protocol version
    // Version number
    if (cmdProtocol != WITHROT) { // Looks like WITHROT, but we have defaulted to the wrong thing
      cmdProtocol = WITHROT;
      setInitialData();
    }
    if (strncmp (packet, "VN", 2) == 0 && strcmp (packet, "VN2.0") != 0) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf  ("%s Warning: Unexpected WiThrottle protocol version not supported.\r\n", getTimeStamp());
        Serial.print   ("         Expected VN2.0, found ");
        Serial.println (packet);
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    switch (cmdProtocol) {
      case WITHROT:
        processWiThrotPacket (packet);
        break;
      case DCCEX:
        processDccPacket (packet);
        break;
      default:
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("%s Warning: Received packet for unknown protocol.\r\n", getTimeStamp());
          xSemaphoreGive(displaySem);
        }
        break;
    }
  }
}
