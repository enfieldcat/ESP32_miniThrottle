/*
miniThrottle, A WiThrottle/DCC-Ex Throttle for model train control

MIT License

Copyright (c) [2021-2024] [Enfield Cat]

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



#ifndef NODISPLAY
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

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s keypadHandler(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }

  keypad.setDebounceTime(debounceTime);

  while (true) {
    char inChar = keypad.getKey();
    if (inChar != NO_KEY) {
      if (showKeypad) Serial.println (inChar);
      if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        diagEnqueue ('k', (char *) &inChar, false);
        xSemaphoreGive(diagPortSem);
      }
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
          if (bidirectionalMode) bidirectionalMode = false;
          else bidirectionalMode = true;
          #ifdef TRAINSETLED
          if (bidirectionalMode) digitalWrite(TRAINSETLED, HIGH);
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
#endif   // keynone
#endif   // NODISPLAY
