

void serialConsole(void *pvParameters)
// This is the console task.
{
  (void) pvParameters;
  uint8_t inChar;
  uint8_t bufferPtr = 0;
  uint8_t inBuffer[BUFFSIZE];

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s serialConsole(NULL)\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }

  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s Console ready\r\n", getTimeStamp());
    Serial.printf ("Type \"help\" or \"help summary\" or \"help <command>\" for more information.\r\n\r\n");
    xSemaphoreGive(displaySem);
  }
  // Serial.print ("> ");
  while ( true ) {
    if (Serial.available()) {
      inChar = Serial.read();
      if (inChar == 8 || inChar == 127) {
        if (bufferPtr > 0) {
          bufferPtr--;
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.print ((char)0x08);
            Serial.print ((char) ' ');
            Serial.print ((char)0x08);
            xSemaphoreGive(displaySem);
          }
        }
      }
      else {
        if (inChar == '\n' || inChar == '\r') {
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.println ("");
            xSemaphoreGive(displaySem);
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
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.print ((char)inChar);
            xSemaphoreGive(displaySem);
          }
        }
      }
    }
    delay(10);
  }
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s All console services stopping\r\n", getTimeStamp());
    xSemaphoreGive(displaySem);
  }
  vTaskDelete( NULL );
}

void processSerialCmd (uint8_t *inBuffer)
{
  char   *param[MAXPARAMS];
  uint8_t nparam;
  uint8_t n;
  bool inSpace = false;

  if (debuglevel>2 && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("%s processSerialCmd(%s)\r\n", getTimeStamp(), inBuffer);
    xSemaphoreGive(displaySem);
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
  else if (nparam==4 && strcmp (param[1], "download") == 0 && strcmp (param[0], "file") == 0) getHttp2File (SPIFFS, param[2], param[3]);
  #endif
  #ifndef NODISPLAY
  else if (nparam<=2 && strcmp (param[0], "font") == 0)          mt_set_font         (nparam, param);
  #endif
  else if (nparam<=2 && strcmp (param[0], "help") == 0)          help                (nparam, param);
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
  #ifndef SERIALCTRL
  else if (nparam<=2 && strcmp (param[0], "protocol") == 0)      mt_set_protocol     (nparam, param);
  #endif
  else if (nparam==1 && strcmp (param[0], "restart") == 0)       mt_sys_restart      ("command line request");
  else if (nparam<=2 && strcmp (param[0], "routedelay") == 0)    mt_set_routedelay   (nparam, param);
  #ifdef SCREENROTATE
  else if (nparam<=2 && strcmp (param[0], "rotatedisplay") == 0) mt_set_rotateDisp   (nparam, param);
  #endif
  #ifndef SERIALCTRL
  else if (nparam<=4 && strcmp (param[0], "server") == 0)        mt_set_server       (nparam, param);
  #endif
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
  else if (nparam==1 && strcmp (param[0], "turnouts") == 0)      displayTurnouts     ();
  else if (nparam==1 && strcmp (param[0], "routes") == 0)        displayRoutes       ();
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
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT))   == pdTRUE) {
    Serial.println ("Command not recognised.");
    xSemaphoreGive(displaySem);
  }
}

#ifdef BRAKEPRESPIN
void mt_brake (int nparam, char **param)
{
  if (nparam==1) {
    int brakeup = nvs_get_int ( "brakeup", 1);
    int brakedown = nvs_get_int ("(char*) brakedown", 20);
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("brake-up = ");
      Serial.print (brakeup);
      Serial.print (", brake-down = ");
      Serial.println (brakedown);
      xSemaphoreGive(displaySem);
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("brake-up and brake-down values should both be integer");
        xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("backlight = %d\r\n", val);
      xSemaphoreGive(displaySem);
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
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.println ("backlight value should be less than 256");
          xSemaphoreGive(displaySem);
        }
      }
    }
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("backlight value should be less than 256");
        xSemaphoreGive(displaySem);
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
  char *nvsData, *nvsPtr;
  char varName[16];
  char msgBuffer[BUFFSIZE];

  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
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
    xSemaphoreGive(displaySem);
  }
  for (uint8_t n=0; n<3; n++) {
    count = nvs_count ((char*)namespaces[n], "all");
    if (count > 0) {
      nvsData = (char*) nvs_extractStr ((char*)namespaces[n], count, BUFFSIZE);
      nvsPtr = nvsData;
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
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
        xSemaphoreGive(displaySem);
      }
      free (nvsData);
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print   ("Error: ");
        Serial.print   (param[1]);
        Serial.println (" number must be between 1 and 10239");
        xSemaphoreGive(displaySem);
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print   ("Error: ");
        Serial.print   (param[1]);
        Serial.println (" control method must be one of DCC, SERVO, VPIN");
        xSemaphoreGive(displaySem);
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
            if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.print   ("Error: ");
              Serial.print   (param[n]);
              Serial.println (" does not match any defined turnout name, names are case sensitive");
              xSemaphoreGive(displaySem);
            }
          }
        }
        else {
          isOK = false;
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
            Serial.print   ("Error: Turnout state should be either C or T, found ");
            Serial.print   (param[n+1]);
            Serial.println ("instead");
            xSemaphoreGive(displaySem);
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Route definition should be followed by turnout-name and state pairs");
        Serial.print   ("Maximum pair count is: ");
        Serial.println ((MAXPARAMS - 3) / 2);
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print   ("Error: ");
      Serial.print   (param[1]);
      Serial.println (" not a recognised parameter, should be: loco, turnout, route");
      xSemaphoreGive(displaySem);
    }
  }
}


void mt_del_gadget (int nparam, char **param)
{
  if (strcmp (param[1], "loco") == 0 || strcmp (param[1], "turnout") == 0 || strcmp (param[1], "route") == 0) {
    if (util_str_isa_int(param[2]) && util_str2int(param[2])<=10239 && util_str2int(param[2])>0) {
      nvs_del_key (param[1], param[2]);
    }
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print   ("Error: ");
        Serial.print   (param[1]);
        Serial.println (" number must be between 1 and 10239");
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print   ("Error: ");
      Serial.print   (param[1]);
      Serial.println (" not a recognised parameter, should be: loco, turnout, route");
      xSemaphoreGive(displaySem);
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
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println("--- Throttle config -------------------------");
    xSemaphoreGive(displaySem);
  }
  mt_set_name         (1, NULL);
  #ifndef SERIALCTRL
  mt_set_server       (1, NULL);
  mt_set_wifi         (1, NULL);
  #endif
  mt_set_debounceTime (1, NULL);
  mt_set_detentCount  (1, NULL);
  showPinConfig       ();
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println("--- Server side data ------------------------");
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
    Serial.println("--- Local mode data -------------------------");
    Serial.print  (" Bidirectional: O");
    if (bidirectionalMode) Serial.println ("n");
    else Serial.println ("ff");
    Serial.print  ("Sort Menu Data: ");
    if (nvs_get_int ("sortData", SORTDATA) == 1) Serial.println ("Yes");
    else Serial.println ("No");
    Serial.print ("Enc Button-Stop: ");
    if (nvs_get_int ("buttonStop", 1) == 1) Serial.println ("Yes");
    else Serial.println ("No");    
    xSemaphoreGive(displaySem);
  }
  #ifndef SERIALCTRL
  mt_set_protocol (1, NULL);
  #endif
  mt_set_speedStep (1, NULL);
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println("---------------------------------------------");
    xSemaphoreGive(displaySem);
  }
  for (uint8_t n=0; n<4; n++) {
    if (dccLCD[n][0] != '\0') Serial.printf ("LCD%d: %s\r\n", n, dccLCD[n]);
  }
}

void mt_sys_restart (const char *reason) // restart the throttle
{
  int maxLocoArray = locomotiveCount + MAXCONSISTSIZE;

  setDisconnected();
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ("Restarting system: %s\r\n", reason);
    xSemaphoreGive(displaySem);
  }
  for (uint8_t n=0; n<maxLocoArray; n++) if (locoRoster[n].owned) {
    setLocoSpeed (n, 0, STOP);
    setLocoOwnership (n, false);
    if (cmdProtocol==DCCEX) locoRoster[n].owned = false;
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("-> Stopping loco: %s\r\n", locoRoster[n].name);
      xSemaphoreGive(displaySem);
    }
  }
  esp_restart();  
}

void mt_set_detentCount (int nparam, char **param) //set count of clicks required on rotary encoder
{
  if (nparam == 1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Detent Count  = ");
      Serial.println (nvs_get_int ("detentCount", 2));
      xSemaphoreGive(displaySem);
    }
  }
  else {
    char *tptr;
    int count;
    count = strtol (param[1], &tptr, 10);
    if (count<1 || count>10) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Detent count should be between 1 and 10");
        xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Debounce time = %dmS\r\n", nvs_get_int ("debounceTime", DEBOUNCEMS));
      xSemaphoreGive(displaySem);
    }
  }
  else {
    char *tptr;
    int count;
    count = strtol (param[1], &tptr, 10);
    if (count<1 || count>200) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Debounce time should be between 1 and 200");
        xSemaphoreGive(displaySem);
      }
    }
    else {
      nvs_put_int ("debounceTime", count);
      if (configHasChanged) {
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.println ("Change becomes effective on restart");
          xSemaphoreGive(displaySem);
        }
      }
    }
  }
}

void mt_set_debug (int nparam, char **param) //set debug level
{
  if (nparam == 1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Debug Level = %d\r\n", nvs_get_int ("debuglevel", DEBOUNCEMS));
      xSemaphoreGive(displaySem);
    }
  }
  else {
    char *tptr;
    int count;
    count = strtol (param[1], &tptr, 10);
    if (count<0 || count>3) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Debug level is between 0 (terse) and 3 (verbose)");
        xSemaphoreGive(displaySem);
      }
    }
    else {
      nvs_put_int ("debuglevel", count);
      debuglevel = count;
      if (configHasChanged) {
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.println ("Debug level becomes effective immediately and remains effective on restart");
          xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Current font %d: %s\n", fontIndex, fontLabel[fontIndex]);
      Serial.println ("Font options are:");
      for (uint8_t n=0; n<sizeof(fontWidth); n++) {
        Serial.printf ("%d: %s (%dx%d chars)\r\n", n, fontLabel[n], (screenWidth/fontWidth[n]), (screenHeight/fontHeight[n]));
      }
      xSemaphoreGive(displaySem);
    }
  }
  else {
    if (util_str_isa_int (param[1])) {
      fontIndex = util_str2int(param[1]);
      if (fontIndex >= 0 && fontIndex < sizeof(fontWidth)) {
        nvs_put_int ("fontIndex", fontIndex);
      }
      else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("Font index should be between 0 and %d\r\n", sizeof(fontWidth) - 1);
        xSemaphoreGive(displaySem);
      }
    }
    else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Font index should be between 0 and %d\r\n", sizeof(fontWidth) - 1);
      xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Screen orientation is: %d, %s\r\n", rotateIndex, rotateOpts[rotateIndex]);
      Serial.printf ("Rotation Options are:\r\n");
      for (uint8_t n=0; n<opts; n++) {
        Serial.printf ("%d: %s\r\n", n, rotateOpts[n]);
      }
      xSemaphoreGive(displaySem);
    }
  }
  else {
    if (util_str_isa_int (param[1])) {
      rotateIndex = util_str2int(param[1]);
      if (rotateIndex >= 0 && rotateIndex < opts) {
        nvs_put_int ("screenRotate", rotateIndex);
      }
      else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.printf ("Rotate index should be between 0 and %d\r\n", opts - 1);
        xSemaphoreGive(displaySem);
      }
    }
    else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ("Rotate index should be between 0 and %d\r\n", opts - 1);
      xSemaphoreGive(displaySem);
    }
  }

}
#endif

void mt_set_name (int nparam, char **param)   // Set name of throttle
{
  if (nparam == 1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Throttle Name: ");
      Serial.println (tname);
      xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Web user name: ");
      Serial.println (webuser);
      xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Web user password: ");
      Serial.println (webpass);
      xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Speed adjust per click: ");
      Serial.println (speedStep);
      xSemaphoreGive(displaySem);
    }
  }
  else if (util_str_isa_int(param[1])) {
    speedStep = util_str2int(param[1]);
    if (speedStep > 0 && speedStep < 10) {
      nvs_put_int ("speedStep", speedStep);
    }
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print ("Speed adjust per click must be  between 1 and 9");
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Speed adjust per click must be numeric");
      xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println ("WiFi Network table");
      sprintf (outBuffer, "%5s | %-32s | %s", "Idx", "SSID", "Password");
      Serial.println (outBuffer);
      for (uint8_t index=0; index<WIFINETS; index++) {
        sprintf (paramName, "wifissid_%d", index);
        if (index==0) nvs_get_string (paramName, myssid, MYSSID, sizeof(myssid));
        else nvs_get_string (paramName, myssid, "none", sizeof(myssid));
        if (strcmp (myssid, "none") != 0) {
          sprintf (paramName, "wifipass_%d", index);
          nvs_get_string (paramName, passwd, "", sizeof(passwd));
        }
        else strcpy (passwd, "");
        if (strcmp (passwd, "none")==0) passwd[0] = '\0';
        sprintf (outBuffer, "%5d | %-32s | %s", index, myssid, passwd);
        Serial.println (outBuffer);
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print ("Connected to SSID: ");
        Serial.print (ssid);
        Serial.print (", IP: ");
        Serial.println (WiFi.localIP());
      }
      else Serial.println ("WiFi not connected in station mode");
      if (APrunning) {
        Serial.println ("WiFi running in access point mode");
      }
      Serial.println ("Checking for non-hidden SSIDs");
      xSemaphoreGive(displaySem);
      wifi_scanNetworks();
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (outBuffer);
      xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("wifi station mode scan: ");
      staConnect = nvs_get_int ("staConnect", 0);
      if (staConnect == 0) Serial.println ("list");
      else if (staConnect == 1) Serial.println ("strength");
      else Serial.println ("any");
      xSemaphoreGive(displaySem);
    }
  }
  else {
    if (strcmp (param[1], "list") == 0) staConnect = 0;
    else if (strcmp (param[1], "strength") == 0) staConnect = 1;
    else if (strcmp (param[1], "any") == 0) staConnect = 2;
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("wifiscan should be list, strength or any");
        xSemaphoreGive(displaySem);
      }
      return;
    }
    if (staConnect>=0 || staConnect<=2) nvs_put_int ("staConnect", staConnect);
  }
}

void set_mdns(int nparam, char **param)
{
  if (nparam==1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("mDNS lookup: O");
      if (nvs_get_int ("mdns", 1) == 1) Serial.println ("n");
      else Serial.println ("ff");
      xSemaphoreGive(displaySem);
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("if searching for service prefix service name with \"_\".");
        xSemaphoreGive(displaySem);
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

  if ((nparam==1 || strcmp(param[1], "status") == 0) && xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println ((char *) OTAstatus());
    xSemaphoreGive(displaySem);
  }
  else if (nparam==2) {
    if (strcmp(param[1], "update") == 0) OTAcheck4update();
    else if (strcmp(param[1], "revert") == 0) OTAcheck4rollback();
    else if (strncmp(param[1], "http://", 7) == 0 || strncmp(param[1], "https://", 8) == 0) {
      nvs_put_string ("ota_url", param[1]);
    }
    else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println ("DCC Server Table");
      Serial.println ("  Idx | Server");
      for (uint8_t index=0; index<WIFINETS; index++) {
        sprintf (paramName, "server_%d", index);
        if (index==0) nvs_get_string (paramName, host, HOST, sizeof(host));
        else nvs_get_string (paramName, host, "none", sizeof(host));
        if (strcmp (host, "none") != 0) {
          sprintf (paramName, "port_%d", index);
          port = nvs_get_int (paramName, PORT);
          sprintf (outBuffer, "%5d | %s:%d", index, host, port);
        }
        else sprintf (outBuffer, "%5d | none", index);
        Serial.println (outBuffer);
      }
      if (!client.connected()) {
        Serial.println ("--- Server not connected ---");
      }
      xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (outBuffer);
      xSemaphoreGive(displaySem);
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Default protocol is ");
      if (nvs_get_int ("defaultProto", WITHROT) == WITHROT) Serial.println ("WiThrottle");
      else Serial.println ("DCC-Ex");
      xSemaphoreGive(displaySem);
    }
  }
  else {
    if (strcmp (param[1], "withrot") == 0 || strcmp (param[1], "withrottle") == 0 || strcmp (param[1], "dccex") == 0) {
      if (strcmp (param[1], "withrot") == 0 || strcmp (param[1], "withrottle") == 0) nvs_put_int ("defaultProto", WITHROT);
      else nvs_put_int ("defaultProto", DCCEX);
    }
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("protocol should be one of: withrot, dccex");
        xSemaphoreGive(displaySem);
      }
    }
  }
}

void mt_set_wifimode(int nparam, char **param)
{
  uint8_t mode;

  if (nparam==1) {
    mode = nvs_get_int ("WiFiMode", WIFISTA);
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print ("Default WiFi mode is ");
      if (mode == WIFIAP) Serial.println ("AP");
      else if (mode == WIFIAP) Serial.println ("STA");
      else Serial.println ("BOTH");
      xSemaphoreGive(displaySem);
    }
  }
  else {
    if      (strcmp (param[1], "ap")   == 0) nvs_put_int ("WiFiMode", WIFIAP);
    else if (strcmp (param[1], "sta")  == 0) nvs_put_int ("WiFiMode", WIFISTA);
    else if (strcmp (param[1], "both") == 0) nvs_put_int ("WiFiMode", WIFIBOTH);
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("wifimode should be one of: ap, sta or both");
        xSemaphoreGive(displaySem);
      }
    }
  }
}

#endif

void displayLocos()  // display locomotive data
{
  const static char dirIndic[3][5] = { "Fwd", "Stop", "Rev" };
  char outBuffer[80];
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.print   ("Locomotive Count = ");
    Serial.println (locomotiveCount);
    if (locomotiveCount > 0 && locomotiveCount<255 && locoRoster!=NULL) {
      sprintf (outBuffer, "%6s | %-16s | %-4s | %s", "ID", "Name", "Dir", "Speed");
      Serial.println (outBuffer);
      for (uint8_t n=0; n<locomotiveCount; n++) {
        sprintf (outBuffer, "%5d%c | %-16s | %-4s | %d", locoRoster[n].id, locoRoster[n].type, locoRoster[n].name, dirIndic[locoRoster[n].direction], locoRoster[n].speed);
        Serial.println (outBuffer);
      }
    }
    xSemaphoreGive(displaySem);
  }
}

void displayTurnouts()  // display known data about switches / points
{
  char outBuffer[50];
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.print   ("TurnOut (switch/point) statuses = ");
    Serial.println (turnoutStateCount);
    if (turnoutStateCount>0 && turnoutStateCount<255) {
      Serial.println ("   ID | State");
      for (uint8_t n=0; n<turnoutStateCount; n++) {
        sprintf (outBuffer, "  %3d | %s", turnoutState[n].state, turnoutState[n].name);
        Serial.println (outBuffer);
      }
    }
    Serial.print   ("Turnout (switch/point) count = ");
    Serial.println (turnoutCount);
    if (turnoutCount > 0 && turnoutCount<255 && turnoutList!=NULL) {
      sprintf (outBuffer, "%-16s | %-16s | %s", "System-Name", "User-Name", "State");
      Serial.println (outBuffer);
      for (uint8_t n=0; n<turnoutCount; n++) {
        sprintf (outBuffer, "%-16s | %-16s | %d", turnoutList[n].sysName, turnoutList[n].userName, turnoutList[n].state);
        Serial.println (outBuffer);
      }
    }
    xSemaphoreGive(displaySem);
  }
}

void displayRoutes()  // display known data about switches / points
{
  char outBuffer[40];
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.print   ("Route statuses = ");
    Serial.println (routeStateCount);
    if (routeStateCount>0 && routeStateCount<255 && routeState != NULL) {
      Serial.println ("   ID | State");
      for (uint8_t n=0; n<routeStateCount; n++) {
        sprintf (outBuffer, "  %3d | %s", routeState[n].state, routeState[n].name);
        Serial.println (outBuffer);
      }
    }
    Serial.print   ("Routes count = ");
    Serial.println (routeCount);
    if (routeCount > 0 && routeCount<255 && routeList != NULL) {
      sprintf (outBuffer, "%-16s | %-16s | %s", "System-Name", "User-Name", "State");
      Serial.println (outBuffer);
      for (uint8_t n=0; n<routeCount; n++) {
        sprintf (outBuffer, "%-16s | %-16s | %d", routeList[n].sysName, routeList[n].userName, routeList[n].state);
        Serial.println (outBuffer);
      }
    }
    xSemaphoreGive(displaySem);
  }
}

void mt_set_routedelay (int nparam, char **param)
{
  if (nparam == 1) {
    int dval = nvs_get_int("routeDelay", 2);
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
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
      xSemaphoreGive(displaySem);
    }
  }
  else {
    if (util_str_isa_int (param[1])) {
      int dval = util_str2int (param[1]);
      if (dval>=0 && dval<(sizeof(routeDelay)/sizeof(uint16_t))) {
        nvs_put_int ("routeDelay", dval);
      }
      else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print ("routedelay value should be less than ");
        Serial.println (sizeof(routeDelay)/sizeof(uint16_t));
        xSemaphoreGive(displaySem);
      }
    }
    else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println ("routedelay value should be an integer");
      xSemaphoreGive(displaySem);
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
  if (nparam == 1) {
    const char *newParams[] = {"void", "loco", "turnout", "turnoutstate", "route", "routestate"};
    for (uint8_t n=0; n<5; n++) {
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print (MAXCONSISTSIZE);
        Serial.println (" additional slots allocated for ad-hoc loco IDs");
        xSemaphoreGive(displaySem);
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("dump parameter not recognised");
        xSemaphoreGive(displaySem);
      }
      return;
    }
    if (blk_start == NULL || blk_count == 0) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print   (param[1]);
        Serial.println (": No data available to dump");
        xSemaphoreGive(displaySem);
      }
      return;
    }
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.print   ("--- ");
      Serial.print   (param[1]);
      Serial.print   ("(n=");
      Serial.print   (blk_count);
      Serial.println (") ---");
      for (uint16_t n=0; n<blk_count; n++) {
        mt_dump (blk_start, blk_size);
        blk_start += blk_size;
      }
      for (uint8_t n = 0; n<15; n++) Serial.print ("-----");
      Serial.println ("---");
      xSemaphoreGive(displaySem);
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

  // Leave the calling routine to grab the serial display semaphore
  // if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    sprintf (message, "--- Address 0x%08x, Size %d bytes ", (uint32_t) memblk, memsize);
    for (rowoffset=strlen(message); rowoffset<77; rowoffset++) strcat (message, "-");
    Serial.println (message);
    for (memoffset=0; memoffset<memsize; ) {
      sprintf (message, "0x%04x ", memoffset);
      Serial.print (message);
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
    }
  //  xSemaphoreGive(displaySem);
  //}
}


void mt_set_cpuspeed(int nparam, char **param)
{
  int storedSpeed;
  int xtal;

  if (nparam==1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      storedSpeed = nvs_get_int ("cpuspeed", 0);
      Serial.printf (" * CPU speed: %dMHz, Xtal freq: %dMHz\r\n", getCpuFrequencyMhz(), getXtalFrequencyMhz());
      if (storedSpeed==0) Serial.println (" * Using default CPU speed");
      else if (storedSpeed!=getCpuFrequencyMhz()) {
        Serial.printf (" * WARNING: configured CPU %dMHz speed mismatch with actual speed\r\n", storedSpeed);
      }
      xSemaphoreGive(displaySem);
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
    else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println ("Invalid CPU speed specified");
      xSemaphoreGive(displaySem);
    }
  }
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println ("Use integer value to set CPU speed.");
    xSemaphoreGive(displaySem);
  }
}

#ifdef WARNCOLOR
void mt_set_warnspeed(int nparam, char **param)
{
  int warnspeed;
  
  if (nparam==1) {
    warnspeed = nvs_get_int((char*) "warnSpeed", 90);
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ((char*) " * Warn speed: %d%%\r\n", warnspeed);
      xSemaphoreGive(displaySem);
    }
  }
  else if (util_str_isa_int (param[1])) {
    warnspeed = util_str2int(param[1]);
    if (warnspeed>=50 && warnspeed<=101) {
      nvs_put_int ( "warnSpeed", warnspeed);
    }
    else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.printf ((char*) "Warning speed should be between 50 and 101%\r\n");
      xSemaphoreGive(displaySem);
    }
  }
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.printf ((char*) "Warning speed should be between 50 and 101%\r\n");
    xSemaphoreGive(displaySem);
  }
}
#endif

void showPinConfig()  // Display pin out selection
{
  #ifndef keynone
  uint8_t rows[] = { MEMBR_ROWS };
  uint8_t cols[] = { MEMBR_COLS };
  #endif

  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println ((char*) "Hardware configuration, check pin numbers are not duplicated");
    Serial.println ((char*) "Reset          =  EN");
    Serial.println ((char*) "Console - Tx   =  1");
    Serial.println ((char*) "Console - Rx   =  3");
    #ifdef DCCTX
    Serial.printf  ((char*) "DCC - Tx       = %2d\r\n", DCCTX);
    #endif
    #ifdef DCCRX
    Serial.printf  ((char*) "DCC - Rx       = %2d\r\n", DCCRX);
    #endif
    #ifdef SDA_PIN
    Serial.printf  ((char*) "I2C - SDA      = %2d\r\n", SDA_PIN);
    #endif
    #ifdef SCK_PIN
    Serial.printf  ("I2C - SCK      = %2d\r\n", SCK_PIN);
    #endif
    #ifdef SPI_RESET
    Serial.printf  ("SPI - Reset    = %2d\r\n", SPI_RESET);
    #endif
    #ifdef SPI_CS
    Serial.printf  ("SPI - CS       = %2d\r\n", SPI_CS);
    #endif
    #ifdef SPI_DC
    Serial.printf  ("SPI - DC       = %2d\r\n", SPI_DC);
    #endif
    #ifdef SPI_SCL
    Serial.printf  ("SPI - Clock/SCL= %2d\r\n", SPI_SCL);
    #endif
    #ifdef SPI_SDA
    Serial.printf  ("SPI - Data/SDA = %2d\r\n", SPI_SDA);
    #endif
    #ifdef BACKLIGHTPIN
    Serial.printf  ("Backlight      = %2d\r\n", BACKLIGHTPIN);
    #ifdef BACKLIGHTREF
    Serial.printf  ("Backlight Ref  = %2d\r\n", BACKLIGHTREF);
    #endif
    #endif
    #ifdef ENCODE_UP
    Serial.printf  ("Encoder Up     = %2d\r\n", ENCODE_UP);
    #endif
    #ifdef ENCODE_DN
    Serial.printf  ("Encoder Down   = %2d\r\n", ENCODE_DN);
    #endif
    #ifdef ENCODE_SW
    Serial.printf  ("Encoder Switch = %2d\r\n", ENCODE_SW);
    #endif
    #ifdef DIRFWDPIN
    Serial.printf  ("Direction Fwd  = %2d\r\n", DIRFWDPIN);
    #endif
    #ifdef DIRREVPIN
    Serial.printf  ("Direction Rev  = %2d\r\n", DIRREVPIN);
    #endif
    #ifdef TRACKPWR
    Serial.printf  ("Trk Power Indc = %2d\r\n", TRACKPWR);
    #endif
    #ifdef TRACKPWRINV
    Serial.printf  ("Trk Power Indc = %2d (Inverted)\r\n", TRACKPWRINV);
    #endif
    #ifdef TRAINSETLEN
    Serial.printf  ("Bidirectional  = %2d\r\n", TRAINSETPIN);
    #endif
    #ifdef F1LED
    Serial.printf  ("Func 10x Indc  = %2d\r\n", F1LED);
    #endif
    #ifdef F2LED
    Serial.printf  ("Func 20x Indc  = %2d\r\n", F2LED);
    #endif    
    #ifdef SPEEDOPIN
    Serial.printf  ("Speedometer Out= %2d\r\n", SPEEDOPIN);
    #endif
    #ifdef BRAKEPRESPIN
    Serial.printf  ("Brake Pressure = %2d\r\n", BRAKEPRESPIN);
    #endif
    #ifdef POTTHROTPIN
    Serial.printf  ("Throttle Poten = %2d\r\n", POTTHROTPIN);
    #endif
    #ifdef keynone
    Serial.printf  ("No Keyboard Pins\r\n");
    #else
    Serial.printf ("Keyboard Rows (%d rows) ", sizeof(rows));
    for (uint8_t n=0; n<sizeof(rows); n++) {
      if (n>0) Serial.print (", ");
      Serial.printf ("%2d", rows[n]);
    }
    Serial.println ("");
    Serial.printf ("Keyboard Cols (%d cols) ", sizeof(cols));
    for (uint8_t n=0; n<sizeof(cols); n++) {
      if (n>0) Serial.print (", ");
      Serial.printf ("%2d", cols[n]);
    }
    Serial.println ("");
    #endif
    xSemaphoreGive(displaySem);
  }
}


void showMemory()
{
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.print   ("     Free Heap: "); Serial.print (util_ftos (ESP.getFreeHeap(), 0)); Serial.print (" bytes, "); Serial.print (util_ftos ((ESP.getFreeHeap()*100.0)/ESP.getHeapSize(), 1)); Serial.println ("%");
    Serial.print   (" Min Free Heap: "); Serial.print (util_ftos (ESP.getMinFreeHeap(), 0)); Serial.print (" bytes, "); Serial.print (util_ftos ((ESP.getMinFreeHeap()*100.0)/ESP.getHeapSize(), 1)); Serial.println ("%");
    Serial.print   ("     Heap Size: "); Serial.print (util_ftos (ESP.getHeapSize(), 0)); Serial.println (" bytes");
    Serial.print   ("NVS Free Space: "); Serial.print (nvs_get_freeEntries()); Serial.println (" x 32 byte blocks");
    Serial.print   ("        Uptime: "); Serial.print (util_ftos (esp_timer_get_time() / (uS_TO_S_FACTOR * 60.0), 2)); Serial.println (" mins");
    Serial.print   ("      CPU Freq: "); Serial.print (util_ftos (ESP.getCpuFreqMHz(), 0)); Serial.println (" MHz");
    Serial.print   ("  Crystal Freq: "); Serial.print (util_ftos (getXtalFrequencyMhz(), 0)); Serial.println (" MHz");
    xSemaphoreGive(displaySem);
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
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
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
      Serial.println ((const char*) "nvs [status|update|revert]");
      if (!summary) {
        Serial.println ((const char*) "    Manage Over The Air (OTA) updates to firmware");
        Serial.println ((const char*) "    NB: does not check hardware compatibility, use with care!");
        Serial.println ((const char*) "    permanent setting");
      }
    }
    #endif
    if (all || strcmp(param[1], "pins")==0) {
      Serial.println ((const char*) "pins");
      if (!summary) {
        Serial.println ((const char*) "    Show processor pin assignment");
        Serial.println ((const char*) "    info only");
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
    xSemaphoreGive(displaySem);
  }
}
