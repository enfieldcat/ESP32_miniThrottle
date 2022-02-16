void receiveNetData(void *pvParameters)
// This is the network receiver task.
{
  (void) pvParameters;
  char inBuffer[NETWBUFFSIZE];
  char inChar;
  uint8_t bufferPtr = 0;

  while (WiFi.status() == WL_CONNECTED) {
    while (client.available()) {
      inChar = client.read();
      if (inChar == '\r' || inChar == '\n' || bufferPtr == (NETWBUFFSIZE-1)) {
        if (bufferPtr > 0) {
          inBuffer[bufferPtr] = '\0';
          processPacket (inBuffer);
          bufferPtr = 0;
        }
      }
      else inBuffer[bufferPtr++] = inChar;
    }
    delay (debounceTime);
  }
  #ifdef TRACKPWR
  digitalWrite(TRACKPWR, LOW);
  #endif
  #ifdef TRACKPWRINV
  digitalWrite(TRACKPWRINV, HIGH);
  #endif
  cmdProtocol = UNDEFINED;
  initialDataSent = false;
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println ("Network connection closed");
    xSemaphoreGive(displaySem);
  }
  vTaskDelete( NULL );
}


void processPacket (char *packet)
{
  if (packet == NULL) return;
  if (showPackets) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.print ("<-- ");
      Serial.println (packet);
      xSemaphoreGive(displaySem);
    }
  }
  // First handle some simple things
  if (strncmp (packet, "VN", 2) == 0) {  // protocol version
    // Version number
    cmdProtocol = JMRI;
    if (strcmp (packet, "VN2.0") != 0) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("Warning: Unexpected JMRI JThrottle protocol version not supported.");
        Serial.print   ("         Expected VN2.0, found ");
        Serial.println (packet);
        xSemaphoreGive(displaySem);
      }
    }
    setInitialData();
  }
  else {
    if (cmdProtocol == JMRI) processJmriPacket (packet);
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("Warning: Received packet for unknown protocol.");
        xSemaphoreGive(displaySem);
      }
    }
  }
}
