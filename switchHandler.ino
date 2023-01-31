/*
 * Handle switches, eg direction or rotary encoder
 */
#ifndef NODISPLAY
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
  #ifndef BACKLIGHTREF
  // No work for this thread if none of the above are defined
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s No switches or encoder defined\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }
  vTaskDelete( NULL );
  return;
  #endif
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
  int32_t potReading = 0;
  uint8_t lastPotReading = 0;
  const uint8_t potLoopLimit = 10;
  uint8_t potLoopCount = 0;
  #endif

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s switchMonitor(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }

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
    #ifdef BACKLIGHTPIN
    #ifdef BACKLIGHTREF
    backlightValue = (analogRead(BACKLIGHTREF))>>2;
    ledcWrite(0, backlightValue);
    #endif
    #endif
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
        if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          static int voo = potReading / potLoopCount;
          static int vim = voo >> 2;
          Serial.printf (" - Pot Reading: (%d / %d) = %d Adjust: %d\r\n", potReading, potLoopCount, voo, vim);
          xSemaphoreGive(displaySem);
        }
        potReading = (potReading / potLoopCount) >> 2; // oversample and over scale adjustment to DCC sized values
        if (abs (lastPotReading - potReading) > 1) {  // ignore changes teetering on the cusp of change
          if      (potReading < 0)   potReading = 0;
          else if (potReading > 255) potReading = 255;
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
  
  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s sendDirChange(%d, %d)\r\n", getTimeStamp(), fwdPin, revPin);
    xSemaphoreGive(displaySem);
  }

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
  int16_t actSpeed;
  int16_t actSteps;
  #ifdef BRAKEPRESPIN
  int16_t lastSpeed;
  #endif
  int8_t actDirec;

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s sendPotThrot(%d, %d)\r\n", getTimeStamp(), dir, speed);
    xSemaphoreGive(displaySem);
  }

  for (int8_t n=0; n<limit; n++) if (locoRoster[n].owned) {
    if (xSemaphoreTake(velociSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      actSpeed = locoRoster[n].speed;
      actSteps = locoRoster[n].steps;
      actDirec = locoRoster[n].direction;
      xSemaphoreGive(velociSem);
    }
    if (actSteps!=128) tSpeed = ((speed<<7)/actSteps) -1;
    else tSpeed = speed -1;
    if (tSpeed >= actSteps - 1) tSpeed = actSteps- 2;
    if (actSpeed != tSpeed) {
      #ifdef BRAKEPRESPIN
      lastSpeed = actSpeed;
      #endif
      if      (tSpeed <= 0)    setLocoSpeed (n, tSpeed, STOP);
      else if (dir!=UNCHANGED) setLocoSpeed (n, tSpeed, dir);
      else                     setLocoSpeed (n, tSpeed, actDirec);
      #ifdef BRAKEPRESPIN
      if (lastSpeed > tSpeed && (dir==UNCHANGED || dir==actDirec)) {
        brakedown(lastSpeed - tSpeed);
      }
      #endif
    }
    if (dir!=UNCHANGED && dir!=actDirec) {
      setLocoDirection (n, dir);
      #ifdef BRAKEPRESPIN
      brakedown(lastSpeed + tSpeed);
      #endif
    }
  }
}
#endif     // POTTHROTPIN
#endif     // NODISPLAY
