/*
 * Handle switches, eg direction or rotary encoder
 */
uint8_t toggleSwitch[] = {
  #ifdef ENCODE_SW
  (uint8_t)ENCODE_SW,
  #endif
  #ifdef DIRFWDPIN
  (uint8_t)DIRFWDPIN,
  #endif
  #ifdef DIRREVPIN
  (uint8_t)DIRREVPIN,
  #endif
  99   // end of array "filler" in the event DIRREVPIN is undefined
  };
uint8_t directionSetting = STOP;
#ifdef ENCODE_UP
#ifdef ENCODE_DN
ESP32Encoder encoder;
#endif
#endif

void switchMonitor(void *pvParameters)
// This is the control switch monitor task.
// Read a control switch, and place it in a keyboard queue
{
  (void)  pvParameters;
  #ifndef POTTHROTPIN
  #ifndef ENCODE_UP
  #ifndef ENCODE_DN
  #ifndef ENCODE_SW
  #ifndef DIRFWDPIN
  #ifndef DIRREVPIN
  // No work for this thread if none of the above are defined
  Serial.println ("No switches or encoder defined");
  vTaskDelete( NULL );
  return;
  #endif
  #endif
  #endif
  #endif
  #endif
  #endif  
  int encodeValue;
  int detentCount = 0;
  static char escapeKey = 'E';
  static char submitKey = 'S';
  static char downKey = 'D';
  static char upKey = 'U';
  uint8_t lastRead[sizeof(toggleSwitch)-1];
  uint8_t currentSetting[sizeof(toggleSwitch)-1];
  bool changed = false;
  uint8_t readChar;
  #ifdef POTTHROTPIN
  uint16_t potReading = 0;
  uint16_t lastPotReading = 0;
  const uint8_t potLoopLimit = 10;
  uint8_t potLoopCount = 0;
  #endif

  // deal with the encoder first
  #ifdef ENCODE_UP
  #ifdef ENCODE_DN
  ESP32Encoder::useInternalWeakPullResistors=UP;
  encoder.attachFullQuad(ENCODE_UP, ENCODE_DN);
  encoder.setFilter(1023);
  encoder.setCount (100);
  #endif
  #endif
  #ifdef POTTHROTPIN
  // We only need 8 bit resolution, higher resolution is wasted compute power
  analogReadResolution(10);
  adcAttachPin(POTTHROTPIN);
  analogSetPinAttenuation(POTTHROTPIN, ADC_11db);  // param 2 = attenuation, range 0-3 sets FSD: 0:ADC_0db=800mV, 1:ADC_2_5db=1.1V, 2:ADC_6db=1.35V, 3:ADC_11db=2.6V
  #endif
  // now initialise other switches
  for (uint8_t n=0; n<sizeof(toggleSwitch)-1; n++) {
    pinMode(toggleSwitch[n], INPUT_PULLUP);
    lastRead[n] = 1;
    currentSetting[n] = 1;
  }
  while (true) {
    changed = false;
    for (uint8_t n=0; n<sizeof(toggleSwitch)-1; n++) {
      readChar = digitalRead(toggleSwitch[n]);
      if (lastRead[n] != readChar) {
        changed = true;
        lastRead[n] = readChar;
      }
    }
    if (changed) {
      changed = false;
      delay (debounceTime);
      for (uint8_t n=0; n<sizeof(toggleSwitch)-1; n++) {
        readChar = digitalRead(toggleSwitch[n]);
        if (lastRead[n] == readChar && currentSetting[n] != readChar) {
          currentSetting[n] = readChar;
          switch (toggleSwitch[n]) {
            #ifdef DIRFWDPIN
            case DIRFWDPIN:
              sendDirChange (currentSetting[n], currentSetting[n+1]);
              break;
            #endif
            #ifdef DIRREVPIN
            case DIRREVPIN:
              sendDirChange (currentSetting[n-1], currentSetting[n]);
              break;
            #endif
            #ifdef ENCODE_SW
            case ENCODE_SW:
              if (readChar==0) {
                if (showKeypad) Serial.println (submitKey);
                if (drivingLoco) xQueueSend (keyboardQueue, &escapeKey, 0);
                xQueueSend (keyboardQueue, &submitKey, 0);
              }
              break;
            #endif
            default:
              break;
          }
        }
      }
    }
    #ifdef ENCODE_UP
    #ifdef ENCODE_DN
    encodeValue = encoder.getCount();
    if (encodeValue != 100) {
      if (++detentCount >= nvs_get_int ("detentCount", 2)) {
        if (encodeValue > 100) {
          if (showKeypad) Serial.println (downKey);
          xQueueSend (keyboardQueue, &downKey, 0);
        }
        else {
          if (showKeypad) Serial.println (upKey);
          xQueueSend (keyboardQueue, &upKey, 0);
        }
        encoder.setCount (100);
        detentCount = 0;
      }
    }
    #endif
    #endif
    #ifdef POTTHROTPIN
    if (drivingLoco && enablePot) { // Ignore potentiometer if not actually driving loco
      // Oversample to get a more stable average
      potReading += analogRead(POTTHROTPIN);
      if (++potLoopCount >= potLoopLimit) {
        potReading = (potReading / potLoopCount) >> 2;
        if (abs (lastPotReading - potReading) > 1) {  // ignore changes teetering on the cusp of change
          lastPotReading = potReading;
          if (bidirectionalMode) {
            if (potReading <= 127) {
              sendPotThrot (REVERSE, 127-potReading);
            }
            else {
              sendPotThrot (FORWARD, potReading-128);
            }
          }
          else {
            sendPotThrot (UNCHANGED, potReading>>1);
          }
          // speedChange = true;
        }
        potReading = 0;
        potLoopCount = 0;
      }
    }
    #endif
    delay(debounceTime);
  }
  vTaskDelete( NULL );
}

//
// Send direction code to keypad queue on brake lever change
//
void sendDirChange (uint8_t fwdPin, uint8_t revPin)
{
  static char directionCode;
  
  if (fwdPin == 0) {       // Switch to forward
    directionCode = 'R';
    directionSetting = FORWARD;
  }
  else if (revPin == 0) {  // Switch to Reverse
    directionCode = 'L';
    directionSetting = REVERSE;
  }
  else {                   // Switch to brake
    directionCode = 'B';
    directionSetting = STOP;
  }
  if (showKeypad) Serial.println (directionCode);
  xQueueSend (keyboardQueue, &directionCode, 0);
}


#ifdef POTTHROTPIN
// Send speed and direction to all relevant DCC locos on "pot-throt" change
void sendPotThrot (int8_t dir, int8_t speed)
{
  int16_t limit = locomotiveCount + MAXCONSISTSIZE;
  int16_t tSpeed;
  #ifdef BRAKEPRESPIN
  int16_t lastSpeed;
  #endif

  for (int8_t n=0; n<limit; n++) if (locoRoster[n].owned) {
    if (locoRoster[n].steps!=128) tSpeed = ((speed<<7)/locoRoster[n].steps) -1;
    else tSpeed = speed -1;
    if (tSpeed >= locoRoster[n].steps - 1) tSpeed = locoRoster[n].steps - 2;
    if (locoRoster[n].speed != tSpeed) {
      #ifdef BRAKEPRESPIN
      lastSpeed = locoRoster[n].speed;
      #endif
      if (tSpeed <= 0) {
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          setLocoSpeed (n, tSpeed, STOP);
          xSemaphoreGive(velociSem);
        }
      }
      else if (dir!=UNCHANGED) {
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          setLocoSpeed (n, tSpeed, dir);
          xSemaphoreGive(velociSem);
        }
      }
      else {
        if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          setLocoSpeed (n, tSpeed, locoRoster[n].direction);
          xSemaphoreGive(velociSem);
        }
      }
      #ifdef BRAKEPRESPIN
      if (lastSpeed > tSpeed && (dir==UNCHANGED || dir==locoRoster[n].direction)) {
        brakedown(lastSpeed - tSpeed);
      }
      #endif
    }
    if (dir!=UNCHANGED && dir!=locoRoster[n].direction) {
      if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        setLocoDirection (n, dir);
        xSemaphoreGive(velociSem);
      }
      #ifdef BRAKEPRESPIN
      brakedown(lastSpeed + tSpeed);
      #endif
    }
  }
}
#endif
