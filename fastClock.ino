#ifdef RELAYPORT
#ifdef USEWIFI
static TimerHandle_t fastClockTimer = NULL;
static QueueHandle_t fastClockQueue = NULL;

// Use a timer to set the semaphore to permit sending of keep alive packets
// Do not use interrupt type routines to generate I/O directly
static void fastClockTimerHandler (TimerHandle_t xTimer)
{
static uint8_t tint = 0; //nominal payload to place into queue

xQueueSend (fastClockQueue, &tint, 0);
}

void fastClock (void *pvParameters)
{
  uint8_t start_hour = nvs_get_int ("fc_hour", FC_HOUR);
  uint8_t start_min  = nvs_get_int ("fc_min",  FC_MIN);
  uint8_t queueData;
  uint8_t bumpcount = 0;
  uint32_t period = 1000;

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s fastClock(NULL) (Core %d)\r\n", getTimeStamp(), xPortGetCoreID());
    xSemaphoreGive(displaySem);
  }
  if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    fc_multiplier = nvs_get_int ("fc_rate", FC_RATE) / 100.00;
    fc_time = ((start_hour * 60) + start_min) * 60;
    xSemaphoreGive(fastClockSem);
  }

  delay (TIMEOUT*2);  // add a slight startup delay
  period = int (1000/fc_multiplier);
  if (fastClockQueue == NULL) {
    fastClockQueue = xQueueCreate (1, sizeof(uint8_t));
  }
  if (fastClockTimer == NULL) {
    fastClockTimer = xTimerCreate ("fastClockTimer", pdMS_TO_TICKS(period), pdTRUE, (void *) NULL, fastClockTimerHandler);
  }
  if (fastClockQueue == NULL || fastClockTimer == NULL) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("%s Failed to create timer for fast clock packets\r\n", getTimeStamp());
      xSemaphoreGive(displaySem);
    }
  }
  else {
    xTimerStart (fastClockTimer, pdMS_TO_TICKS(int (60000/fc_multiplier)));
    while (true) {
      if (fc_multiplier != 0.00) {
        if (bumpcount==0) fc_sendUpdate();
        if (xQueueReceive(fastClockQueue, &queueData, pdMS_TO_TICKS(period+1000)) != pdPASS) {
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("%s Missing fast-clock timer\r\n", getTimeStamp());
            xSemaphoreGive(displaySem);
          }
        }
        if (++bumpcount>59 && xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          fc_time += 60;      // Update time under semaphore protection
          xSemaphoreGive(fastClockSem);
          bumpcount = 0;
        }
      }
      else sleep(1);
      if (xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        if (fc_restart) {
          period = int (1000/fc_multiplier);
          xSemaphoreGive(fastClockSem);   // only hold semaphore long enough to recalc period
          fc_restart = false;
          bumpcount = 0;
          xTimerChangePeriod(fastClockTimer, pdMS_TO_TICKS(period), pdMS_TO_TICKS(70000));
        }
        else xSemaphoreGive(fastClockSem);
      }
      else semFailed ("fastClockSem", __FILE__, __LINE__);
    }
  }
  vTaskDelete( NULL );
}

void fc_sendUpdate()
{
  char outPacket[25];

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s fc_sendUpdate()\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }

  if (remoteSys!=NULL && xSemaphoreTake(fastClockSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    sprintf (outPacket, "PFT%d<;>%4.2f\r\n", fc_time, fc_multiplier);
    xSemaphoreGive(fastClockSem);
    for (uint8_t n=0; n<maxRelay ; n++) {
      if (remoteSys[n].client->connected()) {
        reply2relayNode (&remoteSys[n], outPacket);
      }
    }
  }
  else semFailed ("fastClockSem", __FILE__, __LINE__);

}

#endif
#endif
