/*
 * Handle switches, eg direction or rotary encoder
 */
uint8_t toggleSwitch[] = {(uint8_t)ENCODE_SW, (uint8_t)DIRFWDPIN, (uint8_t)DIRREVPIN};
uint8_t directionSetting = STOP;
ESP32Encoder encoder;

void switchMonitor(void *pvParameters)
// This is the control switch monitor task.
// Read a control switch, and place it in a keyboard queue
{
  (void)  pvParameters;
  int encodeValue;
  int detentCount = 0;
  static char escapeKey = 'E';
  static char submitKey = 'S';
  static char downKey = 'D';
  static char upKey = 'U';
  uint8_t lastRead[sizeof(toggleSwitch)];
  uint8_t currentSetting[sizeof(toggleSwitch)];
  bool changed = false;
  uint8_t readChar;
  #ifdef POTTHROTPIN
  uint16_t potReading;
  uint16_t lastPotReading = 0;
  #endif

  // deal with the encoder first
  ESP32Encoder::useInternalWeakPullResistors=UP;
  encoder.attachFullQuad(ENCODE_UP, ENCODE_DN);
  encoder.setFilter(1023);
  encoder.setCount (100);
  #ifdef POTTHROTPIN
  analogReadResolution(12);
  adcAttachPin(POTTHROTPIN);
  analogSetPinAttenuation(POTTHROTPIN, 3);  // param 2 = attenuation, range 0-3 sets FSD: 0=800mV, 1=1.1V, 2=1.35V, 3=2.6V
  #endif
  // now initialise other switches
  for (uint8_t n=0; n<sizeof(toggleSwitch); n++) {
    pinMode(toggleSwitch[n], INPUT_PULLUP);
    lastRead[n] = 1;
    currentSetting[n] = 1;
  }
  while (true) {
    changed = false;
    for (uint8_t n=0; n<sizeof(toggleSwitch); n++) {
      readChar = digitalRead(toggleSwitch[n]);
      if (lastRead[n] != readChar) {
        changed = true;
        lastRead[n] = readChar;
      }
    }
    if (changed) {
      changed = false;
      delay (debounceTime);
      for (uint8_t n=0; n<sizeof(toggleSwitch); n++) {
        readChar = digitalRead(toggleSwitch[n]);
        if (lastRead[n] == readChar && currentSetting[n] != readChar) {
          currentSetting[n] = readChar;
          switch (toggleSwitch[n]) {
            case DIRFWDPIN:
              sendDirChange (currentSetting[n], currentSetting[n+1]);
              break;
            case DIRREVPIN:
              sendDirChange (currentSetting[n-1], currentSetting[n]);
              break;
            case ENCODE_SW:
              if (readChar==0) {
                if (showKeypad) Serial.println (submitKey);
                if (drivingLoco) xQueueSend (keyboardQueue, &escapeKey, 0);
                xQueueSend (keyboardQueue, &submitKey, 0);
              }
              break;
          }
        }
      }
    }
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
    #ifdef POTTHROTPIN
    potReading = analogRead(POTTHROTPIN) / (4096/256);
    if (lastPotReading != potReading) {
      lastPotReading = potReading;
      if (trainSetMode) {
        if (potReading < 128) {
          sendPotThrot (REVERSE, abs(potReading-128);
        }
        else {
          sendPotThrot (FORWARD, potReading-128);
        }
      }
      else {
        sendPotThrot (BRAKE, potReading>>1);
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

  for (int8_t n=0; n<limit; n++) if (locoRoster[n].owned) {
    tSpeed = (speed<<7)/locoRoster[n].steps;
    if (tSpeed >= locoRoster[n].steps) tSpeed = locoRoster[n].steps - 1;
    if (locoRoster[n].speed != tSpeed) setLocoSpeed (n, tSpeed);
    if (dir != BRAKE) {
      if (locoRoster[n].direction != dir) setLocoDirection (n, dir);
    }
  }
}
#endif
