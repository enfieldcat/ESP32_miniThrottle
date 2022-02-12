static TimerHandle_t keepAliveTimer = NULL;
static QueueHandle_t keepAliveQueue = NULL;

// Use a timer to set the semaphore to permit sending of keep alive packets
// Do not use interrupt type routines to generate I/O directly
static void keepAliveTimerHandler (TimerHandle_t xTimer)
{
static uint8_t tint = 0; //nominal payload to pplace into queue

xQueueSend (keepAliveQueue, &tint, 0);
}

void keepAlive(void *pvParameters)
// This is the keepalive task.
{
  (void)  pvParameters;
  int     lastTime;
  uint8_t queueData;
  bool turnOff = true;

  if (keepAliveQueue == NULL) {
    keepAliveQueue = xQueueCreate (1, sizeof(uint8_t));
  }
  if (keepAliveTimer == NULL) {
    keepAliveTimer = xTimerCreate ("keepAliveTimer", pdMS_TO_TICKS(keepAliveTime * 1000), pdTRUE, (void *) NULL, keepAliveTimerHandler);
  }
  if (keepAliveQueue == NULL || keepAliveTimer == NULL) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("Failed to create timer for keep alive packets");
      xSemaphoreGive(displaySem);
    }
  }
  else {
    lastTime = keepAliveTime;
    if (lastTime > 0) xTimerStart (keepAliveTimer, pdMS_TO_TICKS(lastTime * 1000));
    while (lastTime > 0) {
      while (lastTime > 0 && WiFi.status() == WL_CONNECTED) {
        while (lastTime > 0 && client.connected()) {
          if (xQueueReceive(keepAliveQueue, &queueData, pdMS_TO_TICKS((lastTime*1000)+1000)) != pdPASS) {
            if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
              Serial.println ("Missing keep-alive packet");
              xSemaphoreGive(displaySem);
            }
          }
          if (lastTime > 0 && !turnOff) sendKeepAlive ("*");    // send the heartbeat, if we have started the sequence
          if (lastTime != keepAliveTime) { // has the period changed?
            lastTime = keepAliveTime;
            if (lastTime > 0) xTimerChangePeriod(keepAliveTimer, pdMS_TO_TICKS(lastTime * 1000), pdMS_TO_TICKS(1100));
          }
          if (turnOff) { // Enable heartbeats if they have stopped.
            turnOff = false;
            sendKeepAlive ("*+");
          }
        }
        turnOff = true;
        delay (1000); // wait for client connection
      }
      delay (1000);   // wait for WiFi reconnect
    }
  }
  if (client.connected()) {
    sendKeepAlive ("*-");
  }
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println ("Keep alive manager exits");
    xSemaphoreGive(displaySem);
  }
  if (keepAliveTimer != NULL) xTimerDelete (keepAliveTimer, pdMS_TO_TICKS(2000));
  if (keepAliveQueue != NULL) vQueueDelete (keepAliveQueue);
  vTaskDelete( NULL );
}

/*
 * Keep keep alive transmit data separate from normal keep alive transmits so that
 * console show flags are separate from normal packet transmit reporting
 */
void sendKeepAlive(char *pktData)
{
  if (xSemaphoreTake(transmitSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    if (pktData!= NULL) {
      client.println (pktData);
      client.flush();
    }
    xSemaphoreGive(transmitSem);
    if (showKeepAlive) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print ("--> ");
        if (pktData!= NULL) Serial.print (pktData);
        Serial.println ("");
        xSemaphoreGive(displaySem);
      }
    }
  }
}
