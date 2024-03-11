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


void serialConsole(void *pvParameters)
// This is the console task.
{
  (void) pvParameters;
  uint8_t inChar;
  uint8_t bufferPtr = 0;
  uint8_t inBuffer[BUFFSIZE];

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s serialConsole(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }

  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Console ready\r\n", getTimeStamp());
    Serial.printf ("Type \"help\" or \"help summary\" or \"help <command>\" for more information.\r\n\r\n");
    xSemaphoreGive(consoleSem);
  }
  // Serial.print ("> ");
  while ( true ) {
    if (Serial.available()) {
      inChar = Serial.read();
      if (inChar == 8 || inChar == 127) {
        if (bufferPtr > 0) {
          bufferPtr--;
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.print ((char)0x08);
            Serial.print ((char) ' ');
            Serial.print ((char)0x08);
            xSemaphoreGive(consoleSem);
          }
        }
      }
      else {
        if (inChar == '\n' || inChar == '\r') {
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.println ("");
            xSemaphoreGive(consoleSem);
          }
          if (bufferPtr>0) {
            inBuffer[bufferPtr] = '\0';
            processSerialCmd (inBuffer);
            bufferPtr = 0;
          }
          // Serial.print ("> ");
        }
        else if (bufferPtr < BUFFSIZE-1) {
          inBuffer[bufferPtr++] = inChar;
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.print ((char)inChar);
            xSemaphoreGive(consoleSem);
          }
        }
      }
    }
    delay(10);
  }
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s All console services stopping\r\n", getTimeStamp());
    xSemaphoreGive(consoleSem);
  }
  vTaskDelete( NULL );
}

void processSerialCmd (uint8_t *inBuffer)
{
  char   *param[MAXPARAMS];
  uint8_t nparam;
  uint8_t n;
  bool inSpace = false;

  if (debuglevel>2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s processSerialCmd(%s)\r\n", getTimeStamp(), inBuffer);
    xSemaphoreGive(consoleSem);
  }

  #ifdef FILESUPPORT
  if (writingFile) {
    if (strcmp ((char *)inBuffer, ".") == 0) util_closeWriteFile();
    else util_appendWriteFile((char *)inBuffer);
    return;
  }
  #endif
  param[0] = (char*) &inBuffer[0];
  for (n=0, nparam=1; n<BUFFSIZE && inBuffer[n]!= '\0' && nparam<MAXPARAMS; n++) {
    if (inBuffer[n]<=' ') {
      inBuffer[n] = '\0';
      inSpace = true;
    }
    else if (inSpace) {
      inSpace = false;
      param[nparam++] = (char*) &inBuffer[n];
    }
  }
  for (n=nparam; n<MAXPARAMS; n++) param[n] = NULL;
  if (strlen(param[0]) == 0) return;
  else if (nparam>=4 && strcmp (param[0], "add") == 0)           mt_add_gadget       (nparam, param);
  #ifdef BACKLIGHTPIN
  #ifndef BACKLIGHTREF
  else if (nparam<=2 && strcmp (param[0], "backlight") == 0)     mt_set_backlight    (nparam, param);
  #endif
  #endif
  #ifdef BRAKEPRESPIN
  else if ((nparam==1 || nparam==3) && strcmp (param[0], "brake") == 0) mt_brake     (nparam, param);
  #endif
  else if (nparam==1 && strcmp (param[0], "config")  == 0)       mt_sys_config       ();
  else if (nparam<=2 && strcmp (param[0], "cpuspeed") == 0)      mt_set_cpuspeed     (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "debouncetime") == 0)  mt_set_debounceTime (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "debug") == 0)         mt_set_debug        (nparam, param);
  else if (nparam==3 && (strcmp (param[0], "del") == 0 || strcmp (param[0], "delete") == 0)) mt_del_gadget (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "detentcount") == 0)   mt_set_detentCount  (nparam, param);
  else if (nparam==1 && strcmp (param[0], "diag") == 0)          startDiagPort();
  else if (nparam<=2 && strcmp (param[0], "dump") == 0)          mt_dump_data        (nparam, (const char**) param);
  else if (nparam==1 && strcmp (param[0], "export") == 0)        mt_export           ();
  #ifdef FILESUPPORT
  else if (nparam==3 && strcmp (param[1], "del")   == 0 && strcmp (param[0], "file") == 0) util_deleteFile (SPIFFS, param[2]);
  else if (nparam==2 && strcmp (param[1], "dir")   == 0 && strcmp (param[0], "file") == 0) util_listDir    (SPIFFS, "/", 0);
  else if (nparam==3 && strcmp (param[1], "list")  == 0 && strcmp (param[0], "file") == 0) util_readFile   (SPIFFS, param[2], false);
  else if (nparam==3 && strcmp (param[1], "replay")== 0 && strcmp (param[0], "file") == 0) util_readFile   (SPIFFS, param[2], true);
  else if (nparam==3 && strcmp (param[1], "write") == 0 && strcmp (param[0], "file") == 0) util_writeFile  (SPIFFS, param[2]);
  #endif
  #ifdef USEWIFI
  #ifndef NOHTTPCLIENT
  else if (nparam==4 && strcmp (param[1], "download") == 0 && strcmp (param[0], "file") == 0) getHttp2File (SPIFFS, param[2], param[3]);
  #endif
  #endif
  #ifndef NODISPLAY
  else if (nparam<=2 && strcmp (param[0], "font") == 0)          mt_set_font         (nparam, param);
  #endif
  else if (nparam<=2 && strcmp (param[0], "help") == 0)          help                (nparam, param);
  else if (nparam==2 && strcmp (param[0], "kill") == 0)          runAutomation::killProc(param[1]);
  #ifdef USEWIFI
  else if (nparam<=2 && strcmp (param[0], "mdns") == 0)          set_mdns            (nparam, param);
  #endif
  else if (nparam==1 && strcmp (param[0], "memory") == 0)        showMemory          ();
  else if (nparam<=2 && strcmp (param[0], "name") == 0)          mt_set_name         (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "nvs") == 0)           mt_dump_nvs         (nparam, param);
  #ifdef OTAUPDATE
  else if (nparam<=2 && strcmp (param[0], "ota") == 0)           mt_ota              (nparam, param);
  #endif
  else if (nparam==1 && strcmp (param[0], "pins")  == 0)         showPinConfig       ();
  else if (nparam==1 && strcmp (param[0], "procs")  == 0)        runAutomation::listProcs ();
  #ifndef SERIALCTRL
  else if (nparam<=2 && strcmp (param[0], "protocol") == 0)      mt_set_protocol     (nparam, param);
  #endif
  else if (nparam==1 && strcmp (param[0], "restart") == 0)       mt_sys_restart      ("command line request");
  else if (nparam<=2 && strcmp (param[0], "routedelay") == 0)    mt_set_routedelay   (nparam, param);
  #ifdef SCREENROTATE
  else if (nparam<=2 && strcmp (param[0], "rotatedisplay") == 0) mt_set_rotateDisp   (nparam, param);
  #endif
  else if (nparam>=2 && strcmp (param[0], "rpn") == 0)           (new runAutomation)->rpn  (nparam, param);
  else if (nparam==2 && strcmp (param[0], "run") == 0)           runAutomation::runbg (param[1]);
  #ifndef SERIALCTRL
  else if (nparam<=4 && strcmp (param[0], "server") == 0)        mt_set_server       (nparam, param);
  #endif
  else if ((nparam==1 || nparam>=3) && strcmp (param[0], "set") == 0) mt_set         (nparam, param);
  else if (nparam>=2 && strcmp (param[0], "sendcmd") == 0)       mt_sendcmd          (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "speedstep") == 0)     mt_set_speedStep    (nparam, param);
  #ifdef WARNCOLOR
  else if (nparam<=2 && strcmp (param[0], "warnspeed") == 0)     mt_set_warnspeed    (nparam, param);
  #endif
  #ifdef WEBLIFETIME
  else if (nparam<=2 && strcmp (param[0], "webuser") == 0)       mt_set_webuser      (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "webpass") == 0)       mt_set_webpass      (nparam, param);
  #endif
  #ifdef USEWIFI
  else if (nparam<=4 && strcmp (param[0], "wifi") == 0)          mt_set_wifi         (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "wifimode") == 0)      mt_set_wifimode     (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "wifiscan") == 0)      mt_set_wifiscan     (nparam, param);
  #endif
  else if (nparam==1 && strcmp (param[0], "locos") == 0)         displayLocos        ();
  else if (nparam==2 && strcmp (param[0], "trace") == 0)         runAutomation::traceProc(param[1]);
  else if (nparam==1 && strcmp (param[0], "turnouts") == 0)      displayTurnouts     ();
  else if (nparam==1 && strcmp (param[0], "partitions") == 0)    displayPartitions   ();
  else if (nparam==1 && strcmp (param[0], "showpackets")     == 0) showPackets    = true;
  else if (nparam==1 && strcmp (param[0], "noshowpackets")   == 0) showPackets    = false;
  #ifndef SERIALCTRL
  else if (nparam==1 && strcmp (param[0], "showkeepalive")   == 0) showKeepAlive  = true;
  else if (nparam==1 && strcmp (param[0], "noshowkeepalive") == 0) showKeepAlive  = false;
  #endif
  #ifdef WEBLIFETIME
  else if (nparam==1 && strcmp (param[0], "showweb")         == 0) showWebHeaders = true;
  else if (nparam==1 && strcmp (param[0], "noshowweb")       == 0) showWebHeaders = false;
  #endif
  else if (nparam==1 && strcmp (param[0], "showkeypad")      == 0) showKeypad     = true;
  else if (nparam==1 && strcmp (param[0], "noshowkeypad")    == 0) showKeypad     = false;
  else if (nparam==1 && strcmp (param[0], "bidirectional")   == 0) mt_setbidirectional (true);
  else if (nparam==1 && strcmp (param[0], "nobidirectional") == 0) mt_setbidirectional (false);
  else if (nparam==1 && strcmp (param[0], "sortdata")        == 0) nvs_put_int    ((char*) "sortData", 1);
  else if (nparam==1 && strcmp (param[0], "nosortdata")      == 0) nvs_put_int    ((char*) "sortData", 0);
  else if (nparam==1 && strcmp (param[0], "buttonstop")      == 0) nvs_put_int    ((char*) "buttonStop", 1);
  else if (nparam==1 && strcmp (param[0], "nobuttonstop")    == 0) nvs_put_int    ((char*) "buttonStop", 0);
  else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT))   == pdTRUE) {
    Serial.println ("Command not recognised.");
    xSemaphoreGive(consoleSem);
  }
}

// Use for seting any known NVS (Non Volatile Storage) variable to a value
// Expect to use nvs as the get equivalent to read NVS
void mt_set (int nparam, char **param)
{
  uint8_t limit = sizeof(nvsVars) / sizeof(nvsVar_s);
  bool notFound = true;

  if (nparam == 1) {
    char msgBuffer[BUFFSIZE];
    int val;
    for (uint8_t n=0; n<limit; n++) {
      if (nvsVars[n].varType==INTEGER) {
        val = nvs_get_int (nvsVars[n].varName, nvsVars[n].numDefault);
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("set %-15s %d\r\n", nvsVars[n].varName, val);
          xSemaphoreGive(consoleSem);
        }
      }
      else if (nvsVars[n].varType==STRING) {
        nvs_get_string (nvsVars[n].varName, msgBuffer, nvsVars[n].strDefault, sizeof(msgBuffer));
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.printf ("set %-15s %s\r\n", nvsVars[n].varName, msgBuffer);
          xSemaphoreGive(consoleSem);
        }
      }
    }
    return;
  }
  // we are setting something
  // first check if it is a shared register
  if (strncmp (param[1], "shreg", 3) == 0 && param[1][5]>='0' && param[1][5]<='9' && param[1][6] == '\0') {
    float temp = (new runAutomation())->rpn (nparam-2, &param[2]);
    int8_t i = param[1][5] - '0';
    notFound = false;
    if (xSemaphoreTake(procSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      sharedRegister[i] = temp;
      xSemaphoreGive(procSem);
    }
  }
  // if not found it will be an eeprom setting
  for (uint8_t n=0; n<limit && notFound; n++) {
    if (strcmp (param[1], nvsVars[n].varName) == 0) {
      notFound = false;
      if (nvsVars[n].varType == INTEGER) {
        if (nparam==3 && util_str_isa_int (param[2])) {
          int val = util_str2int(param[2]);
          if (val>=nvsVars[n].varMin && val<=nvsVars[n].varMax) nvs_put_int (param[1], val);
          else {
            if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("Integer variable \"%s\" should be between %d and %d\r\n", param[1], nvsVars[n].varMin, nvsVars[n].varMax);
              xSemaphoreGive(consoleSem);
            }
          }
        }
        else {
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("Integer variable \"%s\" should have one numeric value\r\n", param[1]);
            xSemaphoreGive(consoleSem);
          }
        }
      }
      else if (nvsVars[n].varType == STRING) {
        char msgBuffer[BUFFSIZE];
        strcpy (msgBuffer, param[2]);
        for (uint8_t n=3; n<nparam; n++) {
          strcat (msgBuffer, " ");
          strcat (msgBuffer, param[n]);
        }
        if (strlen(msgBuffer) >= nvsVars[n].varMin && strlen(msgBuffer) <= nvsVars[n].varMax) nvs_put_string (param[1], msgBuffer);
        else {
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.printf ("String variable \"%s\" should be between %d and %d chars long\r\n", param[1], nvsVars[n].varMin, nvsVars[n].varMax);
            xSemaphoreGive(consoleSem);
          }
        }
      }
    }
  }
  if (notFound) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Variable \"%s\" not valid using set command\r\n", param[1]);
      xSemaphoreGive(consoleSem);
    }
  }
}

#ifdef BRAKEPRESPIN
void mt_brake (int nparam, char **param)
{
  if (nparam==1) {
    int brakeup = nvs_get_int ( "brakeup", 1);
    int brakedown = nvs_get_int ("(char*) brakedown", 20);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("brake-up = ");
      Serial.print (brakeup);
      Serial.print (", brake-down = ");
      Serial.println (brakedown);
      xSemaphoreGive(consoleSem);
    }
  }
  else if (nparam == 3) {
    if (util_str_isa_int(param[1]) && util_str_isa_int(param[2])) {
      int val = util_str2int(param[1]);
      if (val > 0 && val<10) nvs_put_int ( "brakeup", val);
      val = util_str2int(param[2]);
      if (val > 0 && val<100) nvs_put_int ( "brakedown", val);
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("brake-up and brake-down values should both be integer");
        xSemaphoreGive(consoleSem);
      }
    }
  }
}
#endif

// Only support backlight if there is a backlight pin without a backlight ADC reference
#ifdef BACKLIGHTPIN
#ifndef BACKLIGHTREF
void mt_set_backlight (int nparam, char **param)
{
  uint16_t val = 200;
  if (nparam==1) {
    val = nvs_get_int ( "backlightValue", 200);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("backlight = %d\r\n", val);
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if (util_str_isa_int(param[1])) {
      val = util_str2int(param[1]);
      if (val>=0 && val<=255) {
        backlightValue = val;
        nvs_put_int ( "backlightValue", backlightValue);
        ledcWrite(0, backlightValue);
      }
      else {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.println ("backlight value should be less than 256");
          xSemaphoreGive(consoleSem);
        }
      }
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("backlight value should be less than 256");
        xSemaphoreGive(consoleSem);
      }
    }
  }
}
#endif
#endif

/*
 * Export config data
 */
void mt_export()
{
  const char *namespaces[] = {"loco", "turnout", "route"};
  int count;
  int limit = sizeof(nvsVars) / sizeof(nvsVar_s);
  char *nvsData, *nvsPtr;
  char varName[16];
  char msgBuffer[BUFFSIZE];
  bool differs = true;

  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println ("#");
    Serial.println ("# MiniThrottle export");
    Serial.println ("#");
    Serial.println ("###########################");
    Serial.println ("#");
    Serial.println ("# Delete any security information like WiFi passwords before posting to web");
    Serial.print   ("# Software Vers: ");
    Serial.print   (PRODUCTNAME);
    Serial.print   (" ");
    Serial.println (VERSION);
    Serial.print   ("# Compile time:  ");
    Serial.print   (__DATE__);
    Serial.print   (" ");
    Serial.println (__TIME__);
    Serial.print   ("# Display Type:  ");
    #ifdef NODISPLAY
    Serial.println ("No Display");
    #else
    Serial.println (DISPLAYNAME);
    #endif
    #ifdef SERIALCTRL
    Serial.println ("# Direct serial connection to DCC-Ex");
    #endif
    Serial.println ("#");
    Serial.println ("###########################");
    Serial.println ("#");
    nvs_get_string ("tname", msgBuffer, "none", sizeof(msgBuffer));
    if (strcmp (msgBuffer, "none") != 0) Serial.printf ("name %s\r\n", msgBuffer);
    count = nvs_get_int ( "cpuspeed", -1);
    if (count>0) Serial.printf ("cpuspeed %d\r\n", count);
    count = nvs_get_int ( "detentCount", -1);
    if (count >= 0) Serial.printf ("detentcount %d\r\n", count);
    count = nvs_get_int ( "debounceTime", -1);
    if (count >= 0) Serial.printf ("debouncetime %d\r\n", count);
    count = nvs_get_int ( "routeDelay", -1);
    if (count >= 0) Serial.printf ("routedelay %d\r\n", count);
    count = nvs_get_int ( "speedStep", -1);
    if (count >= 0) Serial.printf ("speedstep %d\r\n", count);
    count = nvs_get_int ( "brakeup", -1);
    if (count >= 0 || nvs_get_int ("brakedown", -1) >= 0){
      Serial.printf ("brake %d %d\r\n", nvs_get_int ("brakeup", 1), nvs_get_int ("brakedown", 20));
    }
    count = nvs_get_int ( "sortData", -1);
    if (count == 0) Serial.println ("nosortdata");
    else if (count == 1) Serial.println ("sortdata");
    count = nvs_get_int ( "bidirectional", -1);
    if (count == 0) Serial.println ("nobidirectional");
    else if (count == 1) Serial.println ("bidirectional");
    #ifndef NODISPLAY
    count = nvs_get_int ( "fontIndex", -1);
    if (count > -1) Serial.printf ("font %d\r\n", count);
    #endif
    #ifdef SCREENROTATE
    count = nvs_get_int ( "screenRotate", -1);
    if (count > -1) Serial.printf ("rotatedisplay %d\r\n", count);
    #endif
    count = nvs_get_int ( "warnspeed", -1);
    if (count > -1) Serial.printf ("warnspeed %d\r\n", count);
    count = nvs_get_int ( "buttonStop", -1);
    if (count > -1) {
      if (count == 0) Serial.printf ("no");
      Serial.printf ("buttonstop\r\n");
    }
    #ifdef USEWIFI
    count = nvs_get_int ( "mdns", -1);
    if (count > -1) {
      Serial.printf ("mdns o");
      if (count == 0) Serial.printf ("ff\r\n");
      else Serial.printf ("n\r\n");
    }
    count = nvs_get_int ( "defaultProto", -1);
    if (count >= 0) {
      Serial.print ("protocol ");
      if (count == WITHROT) Serial.println ("withrottle");
      else Serial.println ("dccex");
    }
    for (uint8_t n=0; n<WIFINETS; n++) {
      sprintf (varName, "wifissid_%d", n);
      nvs_get_string (varName, msgBuffer, "none", sizeof(msgBuffer));
      if (strcmp (msgBuffer, "none") != 0) {
        Serial.printf ("wifi %d %s", n, msgBuffer);
        sprintf (varName, "wifipass_%d", n);
        nvs_get_string (varName, msgBuffer, "none", sizeof(msgBuffer));
        if (strcmp (msgBuffer, "none") != 0) Serial.printf (" %s\r\n", msgBuffer);
        else Serial.printf ("\r\n");
      }
    }
    for (uint8_t n=0; n<WIFINETS; n++) {
      sprintf (varName, "server_%d", n);
      nvs_get_string (varName, msgBuffer, "none", sizeof(msgBuffer));
      if (strcmp (msgBuffer, "none") != 0) {
        sprintf (varName, "port_%d", n);
        count = nvs_get_int (varName, 1080);
        Serial.printf ("server %d %s %d\r\n", n, msgBuffer, count);
      }
    }
    #endif
    xSemaphoreGive(consoleSem);
  }
  for (uint8_t n=0; n<3; n++) {
    count = nvs_count ((char*)namespaces[n], "all");
    if (count > 0) {
      nvsData = (char*) nvs_extractStr ((char*)namespaces[n], count, BUFFSIZE);
      nvsPtr = nvsData;
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        for (int j=0; j<count; j++) {
          Serial.print ("add ");
          Serial.print (namespaces[n]);
          Serial.print (" ");
          Serial.print (nvsPtr);
          Serial.print (" ");
          nvsPtr += 16;
          Serial.println (nvsPtr);
          nvsPtr += BUFFSIZE;
        }
        xSemaphoreGive(consoleSem);
      }
      free (nvsData);
    }
  }
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.print ("\r\n\r\n#\r\n# The following set commands complement CLI commands above to complete configuration\r\n#\r\n");
    xSemaphoreGive(consoleSem);
  }
  for (uint8_t n=0; n<limit; n++) {
    if (nvsVars[n].varType == INTEGER) {
      count = nvs_get_int (nvsVars[n].varName, nvsVars[n].numDefault);
      if (count != nvsVars[n].numDefault && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("# %s (min=%d, max=%d, default=%d)\r\n", nvsVars[n].varDesc, nvsVars[n].varMin, nvsVars[n].varMax, nvsVars[n].numDefault);
        Serial.printf ("set %s %d\r\n", nvsVars[n].varName, count);
        xSemaphoreGive(consoleSem);
      }
    }
    else if (nvsVars[n].varType == STRING) {
      nvs_get_string (nvsVars[n].varName, msgBuffer, nvsVars[n].strDefault, sizeof(msgBuffer));
      if (strcmp (msgBuffer, nvsVars[n].strDefault) == 0 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("# %s (default: \"%s\")\r\n", nvsVars[n].varDesc, nvsVars[n].strDefault);
        Serial.printf ("set %s %s\r\n", nvsVars[n].varName, msgBuffer);
        xSemaphoreGive(consoleSem);
      }
    }
  } 
}

/*
 * Use to send arbitrary commands to the CS
 * For diagnostic use only
 */
void mt_sendcmd (int nparam, char **param)
{
  char cmdBuffer[BUFFSIZE];

  strcpy (cmdBuffer, param[1]);
  for (uint8_t n=2; n<nparam; n++) {
    strcat (cmdBuffer, " ");
    strcat (cmdBuffer, param[n]);
  }
  txPacket (cmdBuffer);
}


/*
 * When running DCC-Ex use this to add Loco,turnout or route
 */
void mt_add_gadget (int nparam, char **param)
{
  char dataBuffer[BUFFSIZE];
  
  if (strcmp (param[1], "loco") == 0) {
    if (util_str_isa_int(param[2]) && util_str2int(param[2])<=10239 && util_str2int(param[2])>0) {
      strcpy (dataBuffer, param[3]);
      for (uint8_t n=4; n<nparam && strlen(dataBuffer)<(NAMELENGTH-1); n++) {
        strcat (dataBuffer, " ");
        strcat (dataBuffer, param[n]);
      }
      dataBuffer[NAMELENGTH-1] = '\0';
      nvs_put_string (param[1], param[2], dataBuffer);
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print   ("Error: ");
        Serial.print   (param[1]);
        Serial.println (" number must be between 1 and 10239");
        xSemaphoreGive(consoleSem);
      }
    }
  }
  else if (strcmp (param[1], "turnout") == 0) {
    if (strlen(param[2]) > 15) param[2][15] = '\0';
    if (strcmp(param[3], "DCC") == 0 || strcmp(param[3], "SERVO") == 0 || strcmp (param[3], "VPIN") == 0) {
      strcpy (dataBuffer, param[3]);
      for (uint8_t n=4; n<nparam; n++) {
        strcat (dataBuffer, " ");
        strcat (dataBuffer, param[n]);
      }
      nvs_put_string (param[1], param[2], dataBuffer);
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print   ("Error: ");
        Serial.print   (param[1]);
        Serial.println (" control method must be one of DCC, SERVO, VPIN");
        xSemaphoreGive(consoleSem);
      }
    }
  }
  else if (strcmp (param[1], "route") == 0) {
    bool isOK = true;
    if ((nparam%2) == 1) {
      if (strlen(param[2]) > 15) param[2][15] = '\0';
      for (uint8_t n=3; n<nparam; n=n+2) {
        if (param[n+1][0] == 'c' || param[n+1][0] == '0') param[n+1][0] = 'C';
        if (param[n+1][0] == 't' || param[n+1][0] == '1') param[n+1][0] = 'T';
        if (strcmp (param[n+1], "C") == 0 || strcmp (param[n+1], "T") == 0) {
          nvs_get_string ("turnout", param[n], dataBuffer, "not found", sizeof(dataBuffer));
          if (strcmp (dataBuffer, "not found") == 0) {
            isOK = false;
            if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.print   ("Error: ");
              Serial.print   (param[n]);
              Serial.println (" does not match any defined turnout name, names are case sensitive");
              xSemaphoreGive(consoleSem);
            }
          }
        }
        else {
          isOK = false;
          if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.print   ("Error: Turnout state should be either C or T, found ");
            Serial.print   (param[n+1]);
            Serial.println ("instead");
            xSemaphoreGive(consoleSem);
          }
        }
      }
      if (isOK) {
        strcpy (dataBuffer, param[3]);
        for (uint8_t n=4; n<nparam; n++) {
          strcat (dataBuffer, " ");
          strcat (dataBuffer, param[n]);
        }
        nvs_put_string (param[1], param[2], dataBuffer);
      }
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Route definition should be followed by turnout-name and state pairs");
        Serial.print   ("Maximum pair count is: ");
        Serial.println ((MAXPARAMS - 3) / 2);
        xSemaphoreGive(consoleSem);
      }
    }
  }
  else {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print   ("Error: ");
      Serial.print   (param[1]);
      Serial.println (" not a recognised parameter, should be: loco, turnout, route");
      xSemaphoreGive(consoleSem);
    }
  }
}


void mt_del_gadget (int nparam, char **param)
{
  if (strcmp (param[1], "loco") == 0 || strcmp (param[1], "turnout") == 0 || strcmp (param[1], "route") == 0) {
    if (strcmp (param[1], "loco") == 0) {
      if (util_str_isa_int(param[2]) && util_str2int(param[2])<=10239 && util_str2int(param[2])>0) {
        nvs_del_key (param[1], param[2]);
      }
      else {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.print   ("Error: ");
          Serial.print   (param[1]);
          Serial.println (" number must be between 1 and 10239");
          xSemaphoreGive(consoleSem);
        }
      }
    }
    else nvs_del_key (param[1], param[2]);
  }
  else {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print   ("Error: ");
      Serial.print   (param[1]);
      Serial.println (" not a recognised parameter, should be: loco, turnout, route");
      xSemaphoreGive(consoleSem);
    }
  }
}


void mt_dump_nvs (int nparam, char **param)
{
  if (nparam==1) nvs_dumper ("Throttle");
  else {
    if (strcmp(param[1],"*")==0) nvs_dumper (NULL);
    else nvs_dumper (param[1]);
  }
}

void mt_sys_config()   // display all known configuration data
{
  mt_ruler ("Throttle Hardware");
  mt_set_name         (1, NULL);
  showMemory          ();
  displayPartitions   ();
  showPinConfig       ();
  mt_ruler ("Server Data");
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (strlen(remServerType)>0) {
      Serial.print   ("   Server Type: ");
      Serial.println (remServerType);      
    }
    if (strlen(remServerDesc)>0) {
      Serial.print   ("Server Descrip: ");
      Serial.println (remServerDesc);      
    }
    Serial.print  ("Track Power is: O");
    if (trackPower) Serial.println ("n");
    else Serial.println ("ff");
    if (strlen(lastMessage )>0) {
      Serial.print   ("Server Message: ");
      Serial.println (lastMessage);
    }
    for (uint8_t n=0; n<4; n++) {
      if (dccLCD[n][0] != '\0') Serial.printf ("          LCD%d: %s\r\n", n, dccLCD[n]);
    }
    xSemaphoreGive(consoleSem);
  }
  #ifndef SERIALCTRL
  mt_set_server       (2, NULL);
  #endif
  mt_ruler ("Local Data");
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.print  (" Bidirectional: O");
    if (bidirectionalMode) Serial.println ("n");
    else Serial.println ("ff");
    Serial.print  ("Sort Menu Data: ");
    if (nvs_get_int ("sortData", SORTDATA) == 1) Serial.println ("Yes");
    else Serial.println ("No");
    Serial.print ("  Enc Btn-Stop: ");
    if (nvs_get_int ("buttonStop", 1) == 1) Serial.println ("Yes");
    else Serial.println ("No");    
    #ifndef NODISPLAY
    if (display.width() > 0) {
      uint8_t fontIndex = nvs_get_int ("fontIndex", 1);
      Serial.printf ("       Display: %s (Pixels: %d wide x %d high)\r\n", DISPLAYNAME, display.width(), display.height());
      Serial.printf ("  Current Font: %s (%d x %d chars)\r\n", fontLabel[fontIndex], (screenWidth/fontWidth[fontIndex]), (screenHeight/fontHeight[fontIndex]));
    }
    #endif
    xSemaphoreGive(consoleSem);
  }
  #ifndef SERIALCTRL
  mt_set_wifi         (2, NULL);
  #endif
  mt_set_debounceTime (1, NULL);
  mt_set_detentCount  (1, NULL);
  #ifndef SERIALCTRL
  mt_set_protocol (1, NULL);
  #endif
  mt_set_speedStep (1, NULL);
  mt_ruler (NULL);
}

// draw a line 75 chars wide
void mt_ruler (char* title)
{
  uint8_t cntr = 75;

  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("\r\n");
    if (title != NULL) {
      cntr = 70 - strlen(title);
      Serial.printf ("--- %s ", title);
      }
    for (uint8_t n=0; n<cntr; n++) Serial.printf ("-");
    Serial.printf ("\r\n");
    xSemaphoreGive(consoleSem);
  }
}

void displayPartitions()
{
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    printf("Mem Partitions:\r\n");
    Serial.printf("+------+-----+----------+----------+------------------+\r\n");
    Serial.printf("| Type | Sub | Offset   | Size     | Label            |\r\n");
    Serial.printf("+------+-----+----------+----------+------------------+\r\n");
    for (uint8_t part_type=0 ; part_type<2; part_type++) {
      const char descr[][4] = {"APP", "DAT"};
      const esp_partition_type_t ptype[] = { ESP_PARTITION_TYPE_APP, ESP_PARTITION_TYPE_DATA };
      // esp_partition_iterator_t pi = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
      esp_partition_iterator_t pi = esp_partition_find (ptype[part_type], ESP_PARTITION_SUBTYPE_ANY, NULL);
      // esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
      if (pi != NULL) {
        do {
          const esp_partition_t* p = esp_partition_get(pi);
          Serial.printf("| %-4s | %02x  | 0x%06X | 0x%06X | %-16s |\r\n",
            descr[part_type], p->subtype, p->address, p->size, p->label);
        } while (pi = (esp_partition_next(pi)));
      }
    }
  Serial.printf("+------+-----+----------+----------+------------------+\r\n");
    xSemaphoreGive(consoleSem);
  }
}


void mt_sys_restart (const char *reason) // restart the throttle
{
  int maxLocoArray = locomotiveCount + MAXCONSISTSIZE;

  setDisconnected();
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("Restarting system: %s\r\n", reason);
    xSemaphoreGive(consoleSem);
  }
  for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
    setLocoSpeed (n, 0, STOP);
    setLocoOwnership (n, false);
    if (cmdProtocol==DCCEX) {
      locoRoster[n].owned = false;
    }
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("-> Stopping loco: %s\r\n", locoRoster[n].name);
      xSemaphoreGive(consoleSem);
    }
  }
  if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    diagEnqueue ('e', (char*) "### Restart: ", false);
    if (reason!=NULL) diagEnqueue ('e', (char*) reason, true);
    else diagEnqueue ('e', (char*) "Unknown cause.", true);
    xSemaphoreGive(diagPortSem);
    delay(1000); // allow time for message to be processed
  }
  esp_restart();  
}

void mt_set_detentCount (int nparam, char **param) //set count of clicks required on rotary encoder
{
  if (nparam == 1) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Detent Count  = ");
      Serial.println (nvs_get_int ("detentCount", 2));
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    char *tptr;
    int count;
    count = strtol (param[1], &tptr, 10);
    if (count<1 || count>10) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Detent count should be between 1 and 10");
        xSemaphoreGive(consoleSem);
      }
    }
    else {
      nvs_put_int ("detentCount", count);
    }
  }
}

void mt_set_debounceTime (int nparam, char **param) //set count of clicks required on rotary encoder
{
  if (nparam == 1) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Debounce time = %dmS\r\n", nvs_get_int ("debounceTime", DEBOUNCEMS));
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    char *tptr;
    int count;
    count = strtol (param[1], &tptr, 10);
    if (count<1 || count>200) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Debounce time should be between 1 and 200");
        xSemaphoreGive(consoleSem);
      }
    }
    else {
      nvs_put_int ("debounceTime", count);
      if (configHasChanged) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.println ("Change becomes effective on restart");
          xSemaphoreGive(consoleSem);
        }
      }
    }
  }
}

void mt_set_debug (int nparam, char **param) //set debug level
{
  if (nparam == 1) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Debug Level = %d\r\n", nvs_get_int ("debuglevel", DEBOUNCEMS));
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    char *tptr;
    int count;
    count = strtol (param[1], &tptr, 10);
    if (count<0 || count>3) {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Debug level is between 0 (terse) and 3 (verbose)");
        xSemaphoreGive(consoleSem);
      }
    }
    else {
      nvs_put_int ("debuglevel", count);
      debuglevel = count;
      if (configHasChanged) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.println ("Debug level becomes effective immediately and remains effective on restart");
          xSemaphoreGive(consoleSem);
        }
      }
    }
  }
}

#ifndef NODISPLAY
void mt_set_font (int nparam, char **param)  // set font
{
  uint8_t fontIndex;
  
  if (nparam == 1) {
    fontIndex = nvs_get_int ("fontIndex", 1);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Current font %d: %s\r\n", fontIndex, fontLabel[fontIndex]);
      Serial.println ("Font options are:");
      for (uint8_t n=0; n<sizeof(fontWidth); n++) {
        Serial.printf ("%d: %s (%dx%d chars)\r\n", n, fontLabel[n], (screenWidth/fontWidth[n]), (screenHeight/fontHeight[n]));
      }
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if (util_str_isa_int (param[1])) {
      fontIndex = util_str2int(param[1]);
      if (fontIndex >= 0 && fontIndex < sizeof(fontWidth)) {
        nvs_put_int ("fontIndex", fontIndex);
      }
      else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("Font index should be between 0 and %d\r\n", sizeof(fontWidth) - 1);
        xSemaphoreGive(consoleSem);
      }
    }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Font index should be between 0 and %d\r\n", sizeof(fontWidth) - 1);
      xSemaphoreGive(consoleSem);
    }
  }
}
#endif

#ifdef SCREENROTATE
void mt_set_rotateDisp (int nparam, char **param)  // Rotation of screen
{
  uint8_t rotateIndex;
  #if SCREENROTATE == 2
  uint8_t opts = 2;
  const char *rotateOpts[] = {"0 deg (Normal)", "180 deg (Invert)"};
  #else
  uint8_t opts = 4;
  const char *rotateOpts[] = {"0 deg (Normal)", "90 deg Right", "180 deg (Invert)", "90 deg Left"};
  #endif
  if (nparam == 1) {
    rotateIndex = nvs_get_int ("screenRotate", 0);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Screen orientation is: %d, %s\r\n", rotateIndex, rotateOpts[rotateIndex]);
      Serial.printf ("Rotation Options are:\r\n");
      for (uint8_t n=0; n<opts; n++) {
        Serial.printf ("%d: %s\r\n", n, rotateOpts[n]);
      }
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if (util_str_isa_int (param[1])) {
      rotateIndex = util_str2int(param[1]);
      if (rotateIndex >= 0 && rotateIndex < opts) {
        nvs_put_int ("screenRotate", rotateIndex);
      }
      else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("Rotate index should be between 0 and %d\r\n", opts - 1);
        xSemaphoreGive(consoleSem);
      }
    }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Rotate index should be between 0 and %d\r\n", opts - 1);
      xSemaphoreGive(consoleSem);
    }
  }

}
#endif

void mt_set_name (int nparam, char **param)   // Set name of throttle
{
  if (nparam == 1) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf (" Throttle Name: %s\r\n", tname);
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if (strlen (param[1]) > (sizeof(tname)-1)) param[1][sizeof(tname)-1] = '\0';
    strcpy (tname, param[1]);
    nvs_put_string ("tname", tname);
  }
}


#ifdef WEBLIFETIME
void mt_set_webuser (int nparam, char **param)   // Set name of web user
{
  char webuser[64];

  if (nparam == 1) {
    nvs_get_string ("webuser", webuser, WEBUSER, sizeof(webuser));
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Web user name: ");
      Serial.println (webuser);
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if (strlen (param[1]) > (sizeof(webuser)-1)) param[1][sizeof(webuser)-1] = '\0';
    nvs_put_string ("webuser", param[1]);
  }
}

void mt_set_webpass (int nparam, char **param)   // Set password of web user
{
  char webpass[64];

  if (nparam == 1) {
    nvs_get_string ("webpass", webpass, WEBPASS, sizeof(webpass));
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Web user password: ");
      Serial.println (webpass);
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if (strlen (param[1]) > (sizeof(webpass)-1)) param[1][sizeof(webpass)-1] = '\0';
    nvs_put_string ("webpass", param[1]);
  }
}
#endif


void mt_set_speedStep(int nparam, char **param)
{
  uint8_t speedStep = 4;

  if (nparam==1) {
    speedStep = nvs_get_int ("speedStep", 4);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Speed adjust per click: ");
      Serial.println (speedStep);
      xSemaphoreGive(consoleSem);
    }
  }
  else if (util_str_isa_int(param[1])) {
    speedStep = util_str2int(param[1]);
    if (speedStep > 0 && speedStep < 10) {
      nvs_put_int ("speedStep", speedStep);
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print ("Speed adjust per click must be  between 1 and 9");
        xSemaphoreGive(consoleSem);
      }
    }
  }
  else {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Speed adjust per click must be numeric");
      xSemaphoreGive(consoleSem);
    }    
  }
}

#ifdef USEWIFI
void mt_set_wifi (int nparam, char **param)  // set WiFi parameters
{
  char *tptr;
  char myssid[33];
  char passwd[33];
  char paramName[15];
  char outBuffer[80];
  
  if (nparam<=2) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("WiFi Network table\r\n");
      Serial.printf ("+------+----------------------------------+----------------------------------+\r\n");
      Serial.printf ("|%5s | %-32s | %-32s |\r\n", "Idx", "SSID", "Password");
      Serial.printf ("+------+----------------------------------+----------------------------------+\r\n");
      for (uint8_t index=0; index<WIFINETS; index++) {
        sprintf (paramName, "wifissid_%d", index);
        if (index==0) nvs_get_string (paramName, myssid, MYSSID, sizeof(myssid));
        else nvs_get_string (paramName, myssid, "none", sizeof(myssid));
        if (strcmp (myssid, "none") != 0) {
          sprintf (paramName, "wifipass_%d", index);
          nvs_get_string (paramName, passwd, "", sizeof(passwd));
        }
        else strcpy (passwd, "");
        if (nparam<2 || strcmp (myssid, "none") != 0) {
          if (strcmp (passwd, "none")==0) passwd[0] = '\0';
          Serial.printf ("|%5d | %-32s | %-32s |\r\n", index, myssid, passwd);
        }
      }
      Serial.printf ("+------+----------------------------------+----------------------------------+\r\n");
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print ("Connected to SSID: ");
        Serial.print (ssid);
        Serial.print (", IP: ");
        Serial.println (WiFi.localIP());
      }
      else Serial.println ("WiFi not connected in station mode");
      if (APrunning) {
        Serial.println ("WiFi running in access point mode: IP address 192.168.4.1");
      }
      xSemaphoreGive(consoleSem);
      if (nparam<2 && xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Checking for SSIDs");
        xSemaphoreGive(consoleSem);
        wifi_scanNetworks(true);
      }
    }
    return;
    }
  /*
   * Update settings
   */
  int tlong = strtol ((const char*)param[1], &tptr, 10);
  // index = (uint8_t) tlong;
  if (tlong<0 || tlong >= (int) WIFINETS) {
    sprintf (outBuffer, "WiFi index must be between 0 and %d", (WIFINETS - 1));
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (outBuffer);
      xSemaphoreGive(consoleSem);
    }
    return;
  }
  if (strlen(param[2])>sizeof(myssid)-1) param[2][sizeof(myssid)-1] = '\0';
  sprintf (paramName, "wifissid_%d", tlong);
  nvs_put_string (paramName, param[2]);
  sprintf (paramName, "wifipass_%d", tlong);
  if (nparam < 4) {
    nvs_put_string (paramName, "");
  }
  else {
    if (strlen(param[3])>sizeof(passwd)-1) param[3][sizeof(passwd)-1] = '\0';
    nvs_put_string (paramName, param[3]);
  }
  if (configHasChanged) Serial.println ("Change becomes effective on restart");
}

void mt_set_wifiscan(int nparam, char **param)
{
  int staConnect = 99;
  
  if (nparam==1) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("wifi station scan mode: ");
      staConnect = nvs_get_int ("staConnect", 0);
      if (staConnect == 0) Serial.println ("list");
      else if (staConnect == 1) Serial.println ("strength");
      else Serial.println ("any");
      xSemaphoreGive(consoleSem);
      wifi_scanNetworks (true);
    }
  }
  else {
    if (strcmp (param[1], "list") == 0) staConnect = 0;
    else if (strcmp (param[1], "strength") == 0) staConnect = 1;
    else if (strcmp (param[1], "any") == 0) staConnect = 2;
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("wifiscan should be list, strength or any");
        xSemaphoreGive(consoleSem);
      }
      return;
    }
    if (staConnect>=0 || staConnect<=2) nvs_put_int ("staConnect", staConnect);
  }
}

void set_mdns(int nparam, char **param)
{
  if (nparam==1) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("mDNS lookup: O");
      if (nvs_get_int ("mdns", 1) == 1) Serial.println ("n");
      else Serial.println ("ff");
      xSemaphoreGive(consoleSem);
    }
  }
  else if (strcmp (param[1], "on") == 0) {
    nvs_put_int ("mdns", 1);
  }
  else if (strcmp (param[1], "off") == 0) {
    nvs_put_int ("mdns", 0);
  }
  else {
    if (param[1][0] != '_') {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("if searching for service prefix service name with \"_\".");
        xSemaphoreGive(consoleSem);
      }
    }
    mdnsLookup (param[1]);
  }
}

#ifdef OTAUPDATE
/*
 * Over the air update controls
 */
void mt_ota (int nparam, char **param)
{

  if (nparam==1 || strcmp(param[1], "status") == 0) {
    char *tPtr =  (char*) OTAstatus();
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (tPtr);
      xSemaphoreGive(consoleSem);
    }
  }
  else if (nparam==2) {
    if (strcmp(param[1], "update") == 0) if (OTAcheck4update(NULL))  mt_sys_restart ("Restarting to boot updated code.");
    else if (strcmp(param[1], "revert") == 0) if (OTAcheck4rollback(NULL)) mt_sys_restart ("Restarting to boot reverted code.");
    else if (strncmp(param[1], "http://", 7) == 0 || strncmp(param[1], "https://", 8) == 0) {
      nvs_put_string ("ota_url", param[1]);
    }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("ota parameter not understood: ");
      Serial.println (param[1]);
    }
  }
}
#endif

void mt_set_server (int nparam, char **param)  // set details about remote servers
{
  char *tptr;
  int  port;
  char host[65];
  char paramName[15];
  char outBuffer[80];
  
  if (nparam<=2) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("  Server Table:\r\n");
      Serial.printf ("+------+----------------------------------+-------+\r\n");
      Serial.printf ("|  Idx | Server                           |  Port |\r\n");
      Serial.printf ("+------+----------------------------------+-------+\r\n");
      for (uint8_t index=0; index<WIFINETS; index++) {
        sprintf (paramName, "server_%d", index);
        if (index==0) nvs_get_string (paramName, host, HOST, sizeof(host));
        else nvs_get_string (paramName, host, "none", sizeof(host));
        if (strcmp (host, "none") != 0) {
          sprintf (paramName, "port_%d", index);
          port = nvs_get_int (paramName, PORT);
          Serial.printf ("|%5d | %-32s | %5d |\r\n", index, host, port);
        }
        else if (nparam<2) Serial.printf ("|%5d | %-32s |       |\r\n", index, "none");
      }
      Serial.printf ("+------+----------------------------------+-------+\r\n");
      if (!wiCliConnected) {
        Serial.printf ("--- Server not connected ---\r\n");
      }
      xSemaphoreGive(consoleSem);
    }
  return;
  }
  /*
   * Update settings
   */
  int tlong = strtol ((const char*)param[1], &tptr, 10);
  // index = (uint8_t) tlong;
  if (tlong<0 || tlong >= (int) WIFINETS) {
    sprintf (outBuffer, "Server index must be between 0 and %d", (WIFINETS - 1));
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (outBuffer);
      xSemaphoreGive(consoleSem);
    }
    return;
  }
  if (strlen(param[2])>sizeof(host)-1) param[2][sizeof(host)-1] = '\0';
  sprintf (paramName, "server_%d", tlong);
  nvs_put_string (paramName, param[2]);
  sprintf (paramName, "port_%d", tlong);
  if (nparam < 4) {
    nvs_put_int (paramName, PORT);
  }
  else {
    tlong = strtol ((const char*)param[3], &tptr, 10);
    if (tlong < 1024 || tlong > 65530) tlong = PORT;
    nvs_put_int (paramName, tlong);
  }
  if (configHasChanged) Serial.println ("Change becomes effective on restart");
}

void mt_set_protocol(int nparam, char **param)
{
  if (nparam==1) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Default protocol is ");
      if (nvs_get_int ("defaultProto", WITHROT) == WITHROT) Serial.println ("WiThrottle");
      else Serial.println ("DCC-Ex");
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if (strcmp (param[1], "withrot") == 0 || strcmp (param[1], "withrottle") == 0 || strcmp (param[1], "dccex") == 0) {
      if (strcmp (param[1], "withrot") == 0 || strcmp (param[1], "withrottle") == 0) nvs_put_int ("defaultProto", WITHROT);
      else nvs_put_int ("defaultProto", DCCEX);
    }
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("protocol should be one of: withrot, dccex");
        xSemaphoreGive(consoleSem);
      }
    }
  }
}

void mt_set_wifimode(int nparam, char **param)
{
  uint8_t mode;

  if (nparam==1) {
    mode = nvs_get_int ("WiFiMode", defaultWifiMode);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Default WiFi mode is ");
      if (mode == WIFIAP) Serial.println ("AP");
      else if (mode == WIFISTA) Serial.println ("STA");
      else Serial.println ("BOTH");
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if      (strcmp (param[1], "ap")   == 0) nvs_put_int ("WiFiMode", WIFIAP);
    else if (strcmp (param[1], "sta")  == 0) nvs_put_int ("WiFiMode", WIFISTA);
    else if (strcmp (param[1], "both") == 0) nvs_put_int ("WiFiMode", WIFIBOTH);
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("wifimode should be one of: ap, sta or both");
        xSemaphoreGive(consoleSem);
      }
    }
  }
}

#endif

void startDiagPort()
{
   xTaskCreate(diagPortMonitor, "diagPortMonitor", 4096, NULL, 4, NULL);
}

void displayLocos()  // display locomotive data
{
  const static char dirIndic[3][5] = { "Fwd", "Stop", "Rev" };
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf   ("Locomotive Count = %d\r\n", locomotiveCount);
    if (locomotiveCount > 0 && locomotiveCount<255 && locoRoster!=NULL) {
      Serial.printf ("+--------+----------------------------------+------+-------+\r\n");
      Serial.printf ("| %6s | %-32s | %-4s | %s |\r\n", "ID", "Name", "Dir", "Speed");
      Serial.printf ("+--------+----------------------------------+------+-------+\r\n");
      for (uint8_t n=0; n<locomotiveCount; n++) {
        Serial.printf ("| %5d%c | %-32s | %-4s | %5d |\r\n", locoRoster[n].id, locoRoster[n].type, locoRoster[n].name, dirIndic[locoRoster[n].direction], locoRoster[n].speed);
      }
      Serial.printf ("+--------+----------------------------------+------+-------+\r\n");
    }
    else Serial.printf ("No locomotives defined.\r\n");
    xSemaphoreGive(consoleSem);
  }
}

void displayTurnouts()  // display known data about switches / points
{
  char stateBuffer[10];
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf  ("Turnout (switch/point) statuses = %d\r\n", turnoutStateCount);
    if (turnoutStateCount>0 && turnoutStateCount<255) {
      Serial.printf ("+------------+------------------+\r\n");
      Serial.printf ("|  ID        | State            |\r\n");
      Serial.printf ("+------------+------------------+\r\n");
      for (uint8_t n=0; n<turnoutStateCount; n++) {
        if (turnoutState[n].state <= 32 || turnoutState[n].state >= 127) sprintf (stateBuffer, "(0x%02x)", turnoutState[n].state);
        else sprintf (stateBuffer, "(%c)", turnoutState[n].state);
        Serial.printf ("| %3d %-6s | %-16s |\r\n", turnoutState[n].state, stateBuffer, turnoutState[n].name);
      }
      Serial.printf ("+------------+------------------+\r\n\n");
    }
    else Serial.printf ("No turnout statuses defined\r\n\n");
    Serial.printf  ("Turnout (switch/point) count = %d\r\n", turnoutCount);
    if (turnoutCount > 0 && turnoutCount<255 && turnoutList!=NULL) {
      Serial.printf ("+------------------+------------------+------------+\r\n");
      Serial.printf ("| %-16s | %-16s | %-10s |\r\n", "System-Name", "User-Name", "State");
      Serial.printf ("+------------------+------------------+------------+\r\n");
      for (uint8_t n=0; n<turnoutCount; n++) {
        if (turnoutList[n].state <= 32 || turnoutList[n].state >= 127) sprintf (stateBuffer, "(0x%02x)", turnoutList[n].state);
        else sprintf (stateBuffer, "(%c)", turnoutList[n].state);
        Serial.printf ("| %-16s | %-16s | %3d %-6s |\r\n", turnoutList[n].sysName, turnoutList[n].userName, turnoutList[n].state, stateBuffer);
      }
      Serial.printf ("+------------------+------------------+------------+\r\n");
    }
    else Serial.printf ("No turnouts defined\r\n");
    xSemaphoreGive(consoleSem);
  }
}

void displayRoutes()  // display known data about switches / points
{
  char outBuffer[80];
  char stateBuffer[10];
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.print   ("Route statuses = ");
    Serial.println (routeStateCount);
    if (routeStateCount>0 && routeStateCount<255 && routeState != NULL) {
      Serial.println ("   ID        | State");
      for (uint8_t n=0; n<routeStateCount; n++) {
        if (routeState[n].state <= 32 || routeState[n].state >= 127) sprintf (stateBuffer, "(0x%02x)", routeState[n].state);
        else sprintf (stateBuffer, "(%c)", routeState[n].state);
        sprintf (outBuffer, "  %3d %-6s | %s", routeState[n].state, stateBuffer, routeState[n].name);
        Serial.println (outBuffer);
      }
    }
    Serial.print   ("Routes count = ");
    Serial.println (routeCount);
    if (routeCount > 0 && routeCount<255 && routeList != NULL) {
      sprintf (outBuffer, "%-16s | %-16s | %s", "System-Name", "User-Name", "State");
      Serial.println (outBuffer);
      for (uint8_t n=0; n<routeCount; n++) {
        if (routeList[n].state <= 32 || routeList[n].state >= 127) sprintf (stateBuffer, "(0x%02x)", routeList[n].state);
        else sprintf (stateBuffer, "(%c)", routeList[n].state);
        sprintf (outBuffer, "%-16s | %-16s | %d %s", routeList[n].sysName, routeList[n].userName, routeList[n].state, stateBuffer);
        Serial.println (outBuffer);
      }
    }
    xSemaphoreGive(consoleSem);
  }
}

void mt_set_routedelay (int nparam, char **param)
{
  if (nparam == 1) {
    int dval = nvs_get_int("routeDelay", 2);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Delay between switch changes when setting routes is option ");
      Serial.print (dval);
      Serial.print (" => ");
      Serial.print (routeDelay[dval]);
      Serial.println (" mS");
      Serial.println ("Options are");
      for (uint8_t n=0; n<(sizeof(routeDelay)/sizeof(uint16_t)); n++) {
        Serial.print ("   ");
        Serial.print (n);
        Serial.print (" => ");
        Serial.print (routeDelay[n]);
        Serial.println (" mS");
      }
      xSemaphoreGive(consoleSem);
    }
  }
  else {
    if (util_str_isa_int (param[1])) {
      int dval = util_str2int (param[1]);
      if (dval>=0 && dval<(sizeof(routeDelay)/sizeof(uint16_t))) {
        nvs_put_int ("routeDelay", dval);
      }
      else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print ("routedelay value should be less than ");
        Serial.println (sizeof(routeDelay)/sizeof(uint16_t));
        xSemaphoreGive(consoleSem);
      }
    }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println ("routedelay value should be an integer");
      xSemaphoreGive(consoleSem);
    }
  }
}

void mt_setbidirectional (bool setting)
{
  bidirectionalMode = setting;
  if (setting) nvs_put_int ("bidirectional", 1);
  else nvs_put_int ("bidirectional", 0);
}



void mt_dump_data (int nparam, const char **param)  // set details about remote servers
{
  char message[80];

  if (nparam == 1) {
    #ifdef RELAYPORT
    const char *newParams[] = {"void", "loco", "turnout", "turnoutstate", "route", "routestate", "relay"};
    for (uint8_t n=0; n<6; n++) {
    #else
    const char *newParams[] = {"void", "loco", "turnout", "turnoutstate", "route", "routestate"};
    for (uint8_t n=0; n<5; n++) {
    #endif
      mt_dump_data (2, &newParams[n]);
    }
  }
  else {
    uint16_t blk_size;
    uint16_t blk_count;
    char *blk_start;
    if (strcmp (param[1], "loco") == 0) {
      blk_size = sizeof(struct locomotive_s);
      blk_count = locomotiveCount + MAXCONSISTSIZE;
      blk_start = (char*) locoRoster;
      sprintf (message, "%d additional slots allocated for ad-hoc loco IDs", MAXCONSISTSIZE);
      if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        diagEnqueue ('m', (char*) message, true);
        xSemaphoreGive(diagPortSem);
      }
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println (message);
        xSemaphoreGive(consoleSem);
      }
    }
    else if (strcmp (param[1], "turnout") == 0) {
      blk_size = sizeof(struct turnout_s);
      blk_count = turnoutCount;
      blk_start = (char*) turnoutList;
    }
    else if (strcmp (param[1], "turnoutstate") == 0) {
      blk_size = sizeof(struct turnoutState_s);
      blk_count = turnoutStateCount;
      blk_start = (char*) turnoutState;
    }
    else if (strcmp (param[1], "route") == 0) {
      blk_size = sizeof(struct route_s);
      blk_count = routeCount;
      blk_start = (char*) routeList;
    }
    else if (strcmp (param[1], "routestate") == 0) {
      blk_size = sizeof(struct routeState_s);
      blk_count = routeStateCount;
      blk_start = (char*) routeState;
    }
    #ifdef RELAYPORT
    else if (strcmp (param[1], "relay") == 0) {
      if (remoteSys != NULL) {
        blk_size = sizeof(struct relayConnection_s);
        blk_count = maxRelay;
        blk_start = (char*) remoteSys;
      }
    }
    #endif
    else {
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("dump parameter not recognised");
        xSemaphoreGive(consoleSem);
      }
      return;
    }
    if (blk_start == NULL || blk_count == 0) {
      sprintf (message, "%s: No data available to dump", param[1]);
      if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        diagEnqueue ('m', (char*) message, true);
        xSemaphoreGive(diagPortSem);
      }
      if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println (message);
        xSemaphoreGive(consoleSem);
      }
      return;
    }
    sprintf (message, "--- %s (n=%d) ---", param[1], blk_count);
    if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      diagEnqueue ('m', (char*) message, true);
      xSemaphoreGive(diagPortSem);
    }
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (message);
      for (uint16_t n=0; n<blk_count; n++) {
        mt_dump (blk_start, blk_size);
        blk_start += blk_size;
      }
      for (uint8_t n = 0; n<15; n++) Serial.print ("-----");
      Serial.println ("---");
      xSemaphoreGive(consoleSem);
    }
    if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      strcpy (message, "---");
      for (uint8_t n = 0; n<15; n++) strcat (message, "-----");
      diagEnqueue ('m', (char*) message, true);
      xSemaphoreGive(diagPortSem);
    }
  }
}


/*
 * Dump a block of memory to console
 */
void mt_dump (char* memblk, int memsize)
{
  char message[78];
  char postfix[17];
  char tstring[4];
  int memoffset;
  int rowoffset;

    sprintf (message, "--- Address 0x%08x, Size %d bytes ", (uint32_t) memblk, memsize);
    for (rowoffset=strlen(message); rowoffset<77; rowoffset++) strcat (message, "-");
    Serial.println (message);
    if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      diagEnqueue ('m', (char*) message, true);
      xSemaphoreGive(diagPortSem);
    }
    for (memoffset=0; memoffset<memsize; ) {
      sprintf (message, "0x%04x ", memoffset);
      Serial.print (message);
      if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        diagEnqueue ('m', (char*) message, false);
        xSemaphoreGive(diagPortSem);
      }
      message[0] = '\0';
      for (rowoffset=0; rowoffset<16 && memoffset<memsize; rowoffset++) {
        if (memblk[memoffset]>=' ' && memblk[memoffset]<='z') postfix[rowoffset] = memblk[memoffset];
        else postfix[rowoffset] = '.';
        if (rowoffset==8) strcat (message, " -");
        sprintf (tstring, " %02x", memblk[memoffset]);
        strcat  (message, tstring);
        memoffset++;
      }
      postfix[rowoffset] = '\0';
      if (rowoffset<=8) strcat (message, "  ");
      for (; rowoffset<16; rowoffset++) strcat (message, "   ");
      Serial.print   (message);
      Serial.print   ("   ");
      Serial.println (postfix);
      if (diagIsRunning && xSemaphoreTake(diagPortSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        diagEnqueue ('m', (char*) message, false);
        diagEnqueue ('m', (char*) "   ",   false);
        diagEnqueue ('m', (char*) postfix, true);
        xSemaphoreGive(diagPortSem);
      }
    }
}


void mt_set_cpuspeed(int nparam, char **param)
{
  int storedSpeed;
  int xtal;

  if (nparam==1) {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      storedSpeed = nvs_get_int ("cpuspeed", 0);
      Serial.printf (" * CPU speed: %dMHz, Xtal freq: %dMHz\r\n", getCpuFrequencyMhz(), getXtalFrequencyMhz());
      if (storedSpeed==0) Serial.println (" * Using default CPU speed");
      else if (storedSpeed!=getCpuFrequencyMhz()) {
        Serial.printf (" * WARNING: configured CPU %dMHz speed mismatch with actual speed\r\n", storedSpeed);
      }
      xSemaphoreGive(consoleSem);
    }
  }
  else if (util_str_isa_int (param[1])) {
    storedSpeed = util_str2int(param[1]);
    xtal = getXtalFrequencyMhz();
    if (storedSpeed==240 || storedSpeed==160 || storedSpeed==80 || storedSpeed==0
       #ifndef USEWIFI
       // Do not use WiFi if using speeds less than 80MHz
       || (xtal==40 &&(storedSpeed==40 || storedSpeed==20 || storedSpeed==10)) ||
       (xtal==26 &&(storedSpeed==26 || storedSpeed==13)) ||
       (xtal==24 &&(storedSpeed==24 || storedSpeed==12))
       #endif
       ) {
      nvs_put_int ("cpuspeed", storedSpeed);
      // wait for reboot to reset speed, or we may have unwanted WiFi errors
    }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println ("Invalid CPU speed specified");
      xSemaphoreGive(consoleSem);
    }
  }
  else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println ("Use integer value to set CPU speed.");
    xSemaphoreGive(consoleSem);
  }
}

#ifdef WARNCOLOR
void mt_set_warnspeed(int nparam, char **param)
{
  int warnspeed;
  
  if (nparam==1) {
    warnspeed = nvs_get_int((char*) "warnSpeed", 90);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ((char*) " * Warn speed: %d%%\r\n", warnspeed);
      xSemaphoreGive(consoleSem);
    }
  }
  else if (util_str_isa_int (param[1])) {
    warnspeed = util_str2int(param[1]);
    if (warnspeed>=50 && warnspeed<=101) {
      nvs_put_int ( "warnSpeed", warnspeed);
    }
    else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ((char*) "Warning speed should be between 50 and 101%\r\n");
      xSemaphoreGive(consoleSem);
    }
  }
  else if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ((char*) "Warning speed should be between 50 and 101%\r\n");
    xSemaphoreGive(consoleSem);
  }
}
#endif

bool showPinConfig()  // Display pin out selection
{
  #ifndef keynone
  uint8_t rows[] = { MEMBR_ROWS };
  uint8_t cols[] = { MEMBR_COLS };
  #endif
  char prtTemplate[16];
  uint8_t limit = sizeof(pinVars)/sizeof(pinVar_s);
  uint8_t maxWidth = 0;
  bool retVal = true;

  for (uint8_t n=0; n<limit; n++) if (maxWidth<strlen(pinVars[n].pinDesc)) maxWidth = strlen(pinVars[n].pinDesc);
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println ((char*) "Hardware configuration check:");
    sprintf (prtTemplate, "\t%%-%ds = EN\r\n", maxWidth);
    Serial.printf (prtTemplate, "Reset");
    sprintf (prtTemplate, "\t%%-%ds = %%2d", maxWidth);
    for (uint8_t n=0; n<limit; n++) {
      Serial.printf (prtTemplate, pinVars[n].pinDesc, pinVars[n].pinNr);
      pinEquiv (pinVars[n].pinNr);
      for (uint8_t i=0; i<limit; i++) {
        if (i!=n && pinVars[n].pinNr == pinVars[i].pinNr && pinVars[n].pinNr!=-1) {
          retVal = false;
          Serial.printf (", Clash with \"%s\"", pinVars[i].pinDesc);
        }
      }
      #ifndef keynone
      for (uint8_t i=0; i<sizeof(rows); i++) {
        if (pinVars[n].pinNr == rows[i]) {
          retVal = false;
          Serial.printf (", Clash with Keypad-Row \"%d\"", rows[i]);
        }
      }
      for (uint8_t i=0; i<sizeof(cols); i++) {
        if (pinVars[n].pinNr == cols[i]) {
          retVal = false;
          Serial.printf (", Clash with Keypad-Column \"%d\"", cols[i]);
        }
      }
      #endif
      Serial.printf ("\r\n");
    }
    #ifdef keynone
    Serial.printf  ("No Keyboard Pins\r\n");
    #else
    Serial.printf ("\tKeyboard Rows (%d rows) = ", sizeof(rows));
    for (uint8_t n=0; n<sizeof(rows); n++) {
      if (n>0) Serial.print (", ");
      Serial.printf ("%2d", rows[n]);
      pinEquiv (rows[n]);
    }
    for (uint8_t n=0; n<sizeof(rows); n++) {
      for (uint8_t i=0; i<sizeof(rows); i++) if (i!=n && rows[i] == rows[n]) {
        retVal = false;
        Serial.printf (", row pin %d used twice", rows[n]);
      }
      for (uint8_t i=0; i<sizeof(cols); i++) if (cols[i] == rows[n]) {
        retVal = false;
        Serial.printf (", row pin %d also used as keypad column", rows[n]);
      }
    }
    Serial.println ("");
    Serial.printf ("\tKeyboard Cols (%d cols) = ", sizeof(cols));
    for (uint8_t n=0; n<sizeof(cols); n++) {
      if (n>0) Serial.print (", ");
      Serial.printf ("%2d", cols[n]);
      pinEquiv (cols[n]);
    }
    for (uint8_t n=0; n<sizeof(cols); n++) {
      for (uint8_t i=0; i<sizeof(cols); i++) if (i!=n && cols[i] == cols[n]) {
        retVal = false;
        Serial.printf (", column pin %d used twice", cols[n]);
      }
      for (uint8_t i=0; i<sizeof(rows); i++) if (rows[i] == cols[n]) {
        retVal = false;
        Serial.printf (", column pin %d also used as keypad row", cols[n]);
      }
    }
    Serial.println ("");
    #endif
    xSemaphoreGive(consoleSem);
  }
  return (retVal);
}

void pinEquiv(uint8_t pin)
{
  switch (pin) {
    case 1:
      Serial.printf (" (Txd)");
      break;
    case 3:
      Serial.printf (" (Rxd)");
      break;
    case 17:
      Serial.printf (" (Txd2)");
      break;
    case 16:
      Serial.printf (" (Rxd2)");
      break;
    case 36:
      Serial.printf (" (SensVP)");
      break;
    case 39:
      Serial.printf (" (SensVN)");
      break;
  }
}


void showMemory()
{
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.print   ("     Free Heap: "); Serial.print (util_ftos (ESP.getFreeHeap(), 0)); Serial.print (" bytes, "); Serial.print (util_ftos ((ESP.getFreeHeap()*100.0)/ESP.getHeapSize(), 1)); Serial.println ("%");
    Serial.print   (" Min Free Heap: "); Serial.print (util_ftos (ESP.getMinFreeHeap(), 0)); Serial.print (" bytes, "); Serial.print (util_ftos ((ESP.getMinFreeHeap()*100.0)/ESP.getHeapSize(), 1)); Serial.println ("%");
    Serial.print   ("     Heap Size: "); Serial.print (util_ftos (ESP.getHeapSize(), 0)); Serial.println (" bytes");
    Serial.print   ("NVS Free Space: "); Serial.print (nvs_get_freeEntries()); Serial.println (" x 32 byte blocks");
    Serial.print   ("        Uptime: "); Serial.print (util_ftos (esp_timer_get_time() / (uS_TO_S_FACTOR * 60.0), 2)); Serial.println (" mins");
    Serial.print   ("      CPU Freq: "); Serial.print (util_ftos (ESP.getCpuFreqMHz(), 0)); Serial.println (" MHz");
    Serial.print   ("  Crystal Freq: "); Serial.print (util_ftos (getXtalFrequencyMhz(), 0)); Serial.println (" MHz");
    #ifdef FILESUPPORT
    Serial.printf  ("   File System: %d bytes used of %d available (%s%% used)\r\n", SPIFFS.usedBytes(), SPIFFS.totalBytes(), util_ftos((float)(SPIFFS.usedBytes()*100)/(float)SPIFFS.totalBytes(), 2));
    #endif
    xSemaphoreGive(consoleSem);
  }
}

void help(int nparam, char **param)  // show help data
{
  bool summary = false;
  bool all = false;

  if (nparam==2) {
    if (strcmp(param[1], "summary") == 0) {
      summary = true;
      all = true;
    }
    else if (strcmp(param[1], "all") == 0) all = true;
  }
  else all = true;
  if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println ((const char*) "Running permanent settings without parameters prints current settings:");
    if (all || strcmp(param[1], "add")==0) {
      Serial.println ((const char*) "add loco <dcc-address> <description>");
      Serial.println ((const char*) "add turnout <name> {DCC|SERVO|VPIN} <dcc-definition>");
      Serial.println ((const char*) "add route <name> {<turnout-name> <state>} {<turnout-name> <state>}...");
      if (!summary) {
        Serial.println ((const char*) "    Add a locomotive, turnout or route to DCC-Ex roster");
        Serial.println ((const char*) "    where for turnouts, definitions may be:");
        Serial.println ((const char*) "       DCC <linear-address>");
        Serial.println ((const char*) "       DCC <address> <sub-address>");
        Serial.println ((const char*) "       SERVO <pin> <throw-posn> <closed-posn> <profile>");
        Serial.println ((const char*) "       VPIN <pin-number>");
        Serial.println ((const char*) "    Servo profile: 0=immediate, 1=0.5 secs, 2=1 sec, 3=2 secs, 4=bounce");
        Serial.println ((const char*) "    And where for routes, turn-out state is one of:");
        Serial.println ((const char*) "       C - Closed");
        Serial.println ((const char*) "       T - Thrown");
        Serial.println ((const char*) "    permanent setting, DCC-Ex only, restart required");
      }
    }
    #ifdef BACKLIGHTPIN
    #ifndef BACKLIGHTREF
    if (all || strcmp(param[1], "backlight")==0) {
      Serial.println ((const char*) "backlight [0-255]");
      if (!summary) {
        Serial.println ((const char*) "    Display backlight intensity setting");
        Serial.println ((const char*) "    permanent setting, default 200");
      }
    }
    #endif
    #endif
    if (all || strcmp(param[1], "bidirectional")==0) {
      Serial.println ((const char*) "[no]bidirectional");
      if (!summary) {
        Serial.println ((const char*) "    turn bidirectional (AKA \"train set\") mode on/off as default setting");
        Serial.println ((const char*) "    permanent setting, default nobidirectional");
      }
    }
    #ifdef BRAKEPRESPIN
    if (all || strcmp(param[1], "brake")==0) {
      Serial.println ((const char*) "brake [<up-rate> <down-rate>]");
      if (!summary) {
        Serial.println ((const char*) "    Set \"brake pressure\" increase and decrease");
        Serial.println ((const char*) "    Increase is constant rate over time");
        Serial.println ((const char*) "    Decrease is (1/n * pressure) per step decrease in speed");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    #endif
    if (all || strcmp(param[1], "buttonstop")==0) {
      Serial.println ((const char*) "[no]buttonstop");
      if (!summary) {
        Serial.println ((const char*) "    permit/prevents encoder button doing a stop operation when driving locos");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    if (all || strcmp(param[1], "config")==0) {
      Serial.println ((const char*) "config");
      if (!summary) {
        Serial.println ((const char*) "    Show current configuration settings");
        Serial.println ((const char*) "    info only");
      }
    }
    if (all || strcmp(param[1], "cpuspeed")==0) {
      // Serial.println ((const char*) "cpuspeed [{240|160|80|0}]");
      Serial.print   ((const char*) "cpuspeed [{240|160|80");
      #ifndef USEWIFI
      // Cannot use WiFi if using speeds of less than 80.
      switch ((int) getXtalFrequencyMhz()) {
        case 40: Serial.print ((const char*) "|40|20|10");
             break;
        case 26: Serial.print ((const char*) "|26|13");
             break;
        case 24: Serial.print ((const char*) "|24|12");
             break;
      }
      #endif
      Serial.println ((const char*) "|0}]");
      if (!summary) {
        Serial.println ((const char*) "    Set CPU speed in MHz, lower speeds conserve power but are less responsive");
        Serial.println ((const char*) "    Use zero to use speed selected at install time");
        Serial.println ((const char*) "    permanent setting, restart required");
      }
    }
    if (all || strcmp(param[1], "debug")==0) {
      Serial.println ((const char*) "debug [<0-3>]");
      if (!summary) {
        Serial.println ((const char*) "    Set console message verboseness between 0 - terse, 3 - verbose");
      }
    }
    if (all || strcmp(param[1], "del")==0) {
      Serial.println ((const char*) "del {loco|turnout|route} <dcc-address|name>");
      if (!summary) {
        Serial.println ((const char*) "    Delete a locomotive or turnout from DCC-Ex roster");
        Serial.println ((const char*) "    permanent setting, DCC-Ex only, restart required");
      }
    }
    if (all || strcmp(param[1], "detentcount")==0) {
      Serial.println ((const char*) "detentcount [<n>]");
      if (!summary) {
        Serial.println ((const char*) "    Set the number or rotary encoder clicks per up/down movement");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    if (all || strcmp(param[1], "diag")==0) {
      Serial.println ((const char*) "diag");
      if (!summary) {
        Serial.println ((const char*) "    Start diagnostic port if not already started");
        Serial.println ((const char*) "    temporary setting");
      }
    }
    if (all || strcmp(param[1], "dump")==0) {
      Serial.println ((const char*) "dump {loco|turnout|turnoutstate|route|routestate}");
      if (!summary) {
        Serial.println ((const char*) "    Dump memory structures of objects");
        Serial.println ((const char*) "    info only");
      }
    }
    if (all || strcmp(param[1], "export")==0) {
      Serial.println ((const char*) "export");
      if (!summary) {
        Serial.println ((const char*) "    export commands required to replicate this throttle's configuration");
        Serial.println ((const char*) "    info only");
      }
    }
    #ifdef FILESUPPORT
    if (all || strcmp(param[1], "file")==0) {
      Serial.println ((const char*) "file dir");
      Serial.println ((const char*) "file del <filename>");
      #ifdef USEWIFI
      Serial.println ((const char*) "file download <URL> <filename>");
      #endif
      Serial.println ((const char*) "file list <filename>");
      Serial.println ((const char*) "file replay <filename>");
      Serial.println ((const char*) "file write <filename>");
      if (!summary) {
        Serial.println ((const char*) "    Various file system management commands to:");
        Serial.println ((const char*) "    dir: get directory listing");
        Serial.println ((const char*) "    del: delete the named file. NB: filenames are case sensitive");
        #ifdef USEWIFI
        Serial.println ((const char*) "    download: download the named file from the URL, no spaces in URL");
        #endif
        Serial.println ((const char*) "    list: list the contents of the named file.");
        Serial.println ((const char*) "    replay: replay the contents of the named file as a set of configuration commands");
        Serial.println ((const char*) "    write: write the named file from the command prompt, NB no editing facility.");
      }
    }
    #endif
    #ifndef NODISPLAY
    if (all || strcmp(param[1], "font")==0) {
      Serial.println ((const char*) "font [index]");
      if (!summary) {
        Serial.println ((const char*) "    Display or set font");
        Serial.println ((const char*) "    Permanent setting");
      }
    }
    #endif
    if (all || strcmp(param[1], "help")==0) {
      Serial.println ((const char*) "help [summary|all|<command>]");
      if (!summary) {
        Serial.println ((const char*) "    Print this help menu");
        Serial.println ((const char*) "    info only");
      }
    }
    if (all || strcmp(param[1], "kill")==0 || strncmp(param[1], "auto", 4)==0) {
      Serial.println ((const char*) "kill <proc-id>");
      if (!summary) {
        Serial.println ((const char*) "    signals an automation script to stop on reaching next step");
        Serial.println ((const char*) "    also see \"run\" and \"procs\"");
      }
    }
    if (all || strcmp(param[1], "locos")==0 || strcmp(param[1], "turnouts")==0) {
      Serial.println ((const char*) "locos|turnouts");
      if (!summary) {
        Serial.println ((const char*) "    List known locomotives or turnouts");
        Serial.println ((const char*) "    info only");
      }
    }
    #ifdef USEWIFI
    if (all || strcmp(param[1], "mdns")==0) {
      Serial.println ((const char*) "mdns [on|off|<name>]");
      if (!summary) {
        Serial.println ((const char*) "    Use mDNS to search for wiThrottle on network, or locate service <name>");
        Serial.println ((const char*) "    Permanent setting or info");
      }
    }
    #endif
    if (all || strcmp(param[1], "memory")==0) {
      Serial.println ((const char*) "memory");
      if (!summary) {
        Serial.println ((const char*) "    Show system memory usage");
        Serial.println ((const char*) "    info only");
      }
    }
    if (all || strcmp(param[1], "name")==0) {
      Serial.println ((const char*) "name [<name>]");
      if (!summary) {
        Serial.println ((const char*) "    Set the name of the throttle");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    if (all || strcmp(param[1], "nvs")==0) {
      Serial.println ((const char*) "nvs [{<namespace>|*}]");
      if (!summary) {
        Serial.println ((const char*) "    Show contents of Non-Volatile-Storage (NVS) namespace(s)");
        Serial.println ((const char*) "    info only");
      }
    }
    #ifdef OTAUPDATE
    if (all || strcmp(param[1], "ota")==0) {
      Serial.println ((const char*) "ota [status|update|revert]");
      if (!summary) {
        Serial.println ((const char*) "    Manage Over The Air (OTA) updates to firmware");
        Serial.println ((const char*) "    NB: does not check hardware compatibility, use with care!");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    #endif
    if (all || strcmp(param[1], "partitions")==0) {
      Serial.println ((const char*) "partitions");
      if (!summary) {
        Serial.println ((const char*) "    Show eeprom partitioning scheme");
        Serial.println ((const char*) "    info only");
      }
    }
    if (all || strcmp(param[1], "pins")==0) {
      Serial.println ((const char*) "pins");
      if (!summary) {
        Serial.println ((const char*) "    Show processor pin assignment");
        Serial.println ((const char*) "    info only");
      }
    }
    if (all || strcmp(param[1], "procs")==0 || strncmp(param[1], "auto", 4)==0) {
      Serial.println ((const char*) "procs");
      if (!summary) {
        Serial.println ((const char*) "    Lists running automation scripts");
        Serial.println ((const char*) "    also see \"kill\" and \"run\"");
      }
    }
    #ifndef SERIALCTRL
    if (all || strcmp(param[1], "protocol")==0) {
      Serial.println ((const char*) "protocol {withrot|dccex}");
      if (!summary) {
        Serial.println ((const char*) "    Select default protocol to use");
        Serial.println ((const char*) "    permanent setting, default withrottle");
      }
    }
    #endif
    if (all || strcmp(param[1], "restart")==0) {
      Serial.println ((const char*) "restart");
      if (!summary) {
        Serial.println ((const char*) "    Restart throttle");
        Serial.println ((const char*) "    temporary setting");
      }
    }
    #ifdef SCREENROTATE
    if (all || strcmp(param[1], "rotatedisplay")==0) {
      Serial.println ((const char*) "rotatedisplay [direction]");
      if (!summary) {
        Serial.println ((const char*) "    Rotate screen orientation");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    #endif
    if (all || strcmp(param[1], "routedelay")==0) {
      Serial.println ((const char*) "routedelay [mS_delay]");
      if (!summary) {
        Serial.println ((const char*) "    Set interval between setting each turnout in a route");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    if (all || strcmp(param[1], "rpn")==0 || strncmp(param[1], "auto", 4)==0) {
      Serial.println ((const char*) "rpn <expression>");
      if (!summary) {
        Serial.println ((const char*) "    Evaluates a rpn expression displaying each step");
      }
    }
    if (all || strcmp(param[1], "run")==0 || strncmp(param[1], "auto", 4)==0) {
      Serial.println ((const char*) "run <file-name>");
      if (!summary) {
        Serial.println ((const char*) "    Starts an automation script");
        Serial.println ((const char*) "    also see \"kill\" and \"procs\"");
      }
    }
    if (all || strcmp(param[1], "sendcmd")==0) {
      Serial.println ((const char*) "sendcmd <data for CS>");
      if (!summary) {
        Serial.println ((const char*) "    Send a packet of data to the command station");
        Serial.println ((const char*) "    For testing and diagnotics only, use with care!");
      }
    }
    #ifndef SERIALCTRL
    if (all || strcmp(param[1], "server")==0) {
      Serial.println ((const char*) "server [<index> <IP-address|DNS-name> [<port-number>]]");
      if (!summary) {
        Serial.println ((const char*) "    Set the server address and port");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    #endif
    if (all || strcmp(param[1], "set")==0) {
      Serial.println ((const char*) "set [<variable> <value>]");
      Serial.println ((const char*) "set shreg<0-9> <expression>");
      if (!summary) {
	Serial.println ((const char*) "    Set most NVS variables to a value, use with care - limited checking");
        Serial.println ((const char*) "    Without parameters set will dump all settable settings");
        Serial.println ((const char*) "    Can also be used to store a value in a shared register");
      }
    }
    #ifndef SERIALCTRL
    if (all || strcmp(param[1], "showkeepalive")==0) {
      Serial.println ((const char*) "[no]showkeepalive");
      if (!summary) {
        Serial.println ((const char*) "    Show network packets received and transmitted");
        Serial.println ((const char*) "    temporary setting");
      }
    }
    #endif
    if (all || strcmp(param[1], "showkeypad")==0) {
      Serial.println ((const char*) "[no]showkeypad");
      if (!summary) {
        Serial.println ((const char*) "    Show keypad keystrokes to console");
        Serial.println ((const char*) "    temporary setting");
      }
    }
    if (all || strcmp(param[1], "showpackets")==0) {
      Serial.println ((const char*) "[no]showpackets");
      if (!summary) {
        Serial.println ((const char*) "    Show network packets received and transmitted");
        Serial.println ((const char*) "    temporary setting");
      }
    }
    #ifdef WEBLIFETIME
    if (all || strcmp(param[1], "showweb")==0) {
      Serial.println ((const char*) "[no]showweb");
      if (!summary) {
        Serial.println ((const char*) "    Show web server headers received and transmitted");
        Serial.println ((const char*) "    temporary setting");
      }
    }
    #endif
    if (all || strcmp(param[1], "sortdata")==0) {
      Serial.println ((const char*) "[no]sortdata");
      if (!summary) {
        Serial.println ((const char*) "    sort lists of locos and turnouts");
        Serial.println ((const char*) "    permanent setting, default sortdata");
      }
    }
    if (all || strcmp(param[1], "speedstep")==0) {
      Serial.println ((const char*) "speedstep [<1-9>]");
      if (!summary) {
        Serial.println ((const char*) "    Set the speed change set be each thottle click");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    if (all || strcmp(param[1], "trace")==0) {
      Serial.println ((const char*) "trace <Process-ID>");
      if (!summary) {
        Serial.println ((const char*) "    Traces the execuation of and automation script");
      }
    }
    #ifdef WARNCOLOR
    if (all || strcmp(param[1], "warnspeed")==0) {
      Serial.println ((const char*) "warnspeed [<50-101>]");
      if (!summary) {
        Serial.println ((const char*) "    change speed graph color when percentage warnspeed is exceeded");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    #endif
    #ifdef WEBLIFETIME
    if (all || strcmp(param[1], "webpass")==0) {
      Serial.println ((const char*) "webpass [password]");
      if (!summary) {
        Serial.println ((const char*) "    change web user password for web authentication");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    if (all || strcmp(param[1], "webuser")==0) {
      Serial.println ((const char*) "webuser [User-Name]");
      if (!summary) {
        Serial.println ((const char*) "    change web user for web authentication");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    #endif
    #ifdef USEWIFI
    if (all || strcmp(param[1], "wifi")==0) {
      Serial.println ((const char*) "wifi [<index> <ssid> [<password>]]");
      if (!summary) {
        Serial.println ((const char*) "    Set WiFi SSID and password for a known network");
        Serial.println ((const char*) "    Leave password blank for open access points");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    if (all || strcmp(param[1], "wifimode")==0) {
      Serial.println ((const char*) "wifimode [ap|sta|both]");
      if (!summary) {
        Serial.println ((const char*) "    Set WiFi mode to Access Point (AP), Station (sta) or both (both)");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    if (all || strcmp(param[1], "wifiscan")==0) {
      Serial.println ((const char*) "wifiscan [list|strength|any]");
      if (!summary) {
        Serial.println ((const char*) "    set wifi scanning technique");
        Serial.println ((const char*) "    - list: use the list of known WiFi networks as listed by preference");
        Serial.println ((const char*) "    - strength: use the strongest signal of known WiFi networks");
        Serial.println ((const char*) "    - any: use the strongest signal even if unknown open access point");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    #endif
    xSemaphoreGive(consoleSem);
  }
}
