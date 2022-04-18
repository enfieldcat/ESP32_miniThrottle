#ifndef keynone
void keypadMonitor(void *pvParameters)
// This is the keypad monitor task.
// Read a keypad char, and place it in a keyboard queue
{
  (void)  pvParameters;
  byte rowPins[ROWCOUNT] = {MEMBR_ROWS}; //connect to the row pinouts of the keypad
  byte colPins[COLCOUNT] = {MEMBR_COLS}; //connect to the column pinouts of the keypad
  Keypad keypad = Keypad( makeKeymap(keymap), rowPins, colPins, ROWCOUNT, COLCOUNT );
  char inChar = 'A';
  char lastKey = 'A';
  bool keyRelPending = false;

  keypad.setDebounceTime(debounceTime);

  while (true) {
    char inChar = keypad.getKey();
    if (inChar != NO_KEY) {
      if (showKeypad) Serial.println (inChar);
      if (menuMode) {   // when driving menus, use numbers as arrows
        switch (inChar) {
          case 'U': inChar = 'D' ; break;  // transpose meaning of up and down
          case 'D': inChar = 'U' ; break;
          case '2': inChar = 'D' ; break;  // 2 acts as down
          case '8': inChar = 'U' ; break;  // 8 acts as up
          case '4': inChar = 'L' ; break;  // 4 as left
          case '5': inChar = 'S' ; break;  // 5 as select
          case '6': inChar = 'R' ; break;  // 6 as right
          case '0': inChar = 'E' ; break;  // 0 as escape
          case '#': inChar = 'S' ; break;  // # as select
        }
      }
      xQueueSend (keyboardQueue, &inChar, 0);
      lastKey = inChar;
      if (lastKey >= '0' && lastKey <='9') { // for function keys in driving mode we want to know if they are released
        keyRelPending = true;
      }
      switch (inChar) { // set function indicator - Universally applied ones, may have others in CAB mode
        case '*':
          if (trainSetMode) trainSetMode = false;
          else trainSetMode = true;
          #ifdef TRAINSETLED
          if (trainSetMode) digitalWrite(TRAINSETLED, HIGH);
          else  digitalWrite(TRAINSETLED, LOW);
          #endif
          break;
        #ifdef POTTHROTPIN
        case 'P':
          if (enablePot) enablePot = false;
          else enablePot = true;
          speedChange = true;
          break;
        #endif
      }
    }
    // for function keys in driving mode we want to know if they are released
    if (keyRelPending && keypad.getState() == RELEASED) {
      if (showKeypad) {
        Serial.print   ("Release ");
        Serial.println (lastKey);
      }
      keyRelPending = false;
      xQueueSend (keyReleaseQueue, &lastKey, 0);
    }
    delay (debounceTime);
  }
  vTaskDelete( NULL );
}
#endif
