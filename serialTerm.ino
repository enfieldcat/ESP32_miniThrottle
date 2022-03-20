
#define MAXPARAMS 10


void serialConsole(void *pvParameters)
// This is the console task.
{
  (void) pvParameters;
  uint8_t inChar;
  uint8_t bufferPtr = 0;
  uint8_t inBuffer[BUFFSIZE];

  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println ("Console ready");
    Serial.println ("Type \"help\" for more information.");
    Serial.println ("");
    xSemaphoreGive(displaySem);
  }
  // Serial.print ("> ");
  while ( true ) {
    if (Serial.available()) {
      inChar = Serial.read();
      if (inChar == 7 || inChar == 127) {
        if (bufferPtr > 0) {
          bufferPtr--;
          Serial.print (7);
        }
      }
      else {
        if (inChar == '\n' || inChar == '\r') {
          Serial.println ("");
          if (bufferPtr>0) {
            inBuffer[bufferPtr] = '\0';
            process(inBuffer);
            bufferPtr = 0;
          }
          // Serial.print ("> ");
        }
        else if (bufferPtr < BUFFSIZE-1) {
          inBuffer[bufferPtr++] = inChar;
          Serial.print ((char)inChar);
        }
      }
    }
    delay(debounceTime);
  }
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println ("All console services stopping");
    xSemaphoreGive(displaySem);
  }
  vTaskDelete( NULL );
}

void process (uint8_t *inBuffer)
{
  char   *param[MAXPARAMS];
  uint8_t nparam;
  uint8_t n;
  bool inSpace = false;

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
  else if (nparam==1 && strcmp (param[0], "config")  == 0)       mt_sys_config       ();
  else if (nparam<=2 && strcmp (param[0], "cpuspeed") == 0)      mt_set_cpuspeed     (nparam, param);
  else if (nparam==3 && (strcmp (param[0], "del") == 0 || strcmp (param[0], "delete") == 0)) mt_del_gadget (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "name") == 0)          mt_set_name         (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "debouncetime") == 0)  mt_set_debounceTime (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "detentcount") == 0)   mt_set_detentCount  (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "dump") == 0)          mt_dump_data        (nparam, param);
  else if (nparam==1 && strcmp (param[0], "help") == 0)          help                ();
  else if (nparam==1 && strcmp (param[0], "memory") == 0)        showMemory          ();
  else if (nparam<=2 && strcmp (param[0], "nvs") == 0)           mt_dump_nvs         (nparam, param);
  else if (nparam==1 && strcmp (param[0], "pins")  == 0)         showPinConfig       ();
  else if (nparam<=2 && strcmp (param[0], "protocol") == 0)      mt_set_protocol     (nparam, param);
  else if (nparam<=4 && strcmp (param[0], "server") == 0)        mt_set_server       (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "speedstep") == 0)     mt_set_speedStep    (nparam, param);
  else if (nparam==1 && strcmp (param[0], "restart") == 0)       mt_sys_restart      ("command line request");
  else if (nparam<=4 && strcmp (param[0], "wifi") == 0)          mt_set_wifi         (nparam, param);
  else if (nparam<=2 && strcmp (param[0], "wifiscan") == 0)      mt_set_wifiscan     (nparam, param);
  else if (nparam==1 && strcmp (param[0], "locos") == 0)         displayLocos        ();
  else if (nparam==1 && strcmp (param[0], "turnouts") == 0)      displayTurnouts     ();
  else if (nparam==1 && strcmp (param[0], "routes") == 0)        displayRoutes       ();
  else if (nparam==1 && strcmp (param[0], "showpackets")     == 0) showPackets   = true;
  else if (nparam==1 && strcmp (param[0], "noshowpackets")   == 0) showPackets   = false;
  else if (nparam==1 && strcmp (param[0], "showkeepalive")   == 0) showKeepAlive = true;
  else if (nparam==1 && strcmp (param[0], "noshowkeepalive") == 0) showKeepAlive = false;
  else if (nparam==1 && strcmp (param[0], "showkeypad")      == 0) showKeypad    = true;
  else if (nparam==1 && strcmp (param[0], "noshowkeypad")    == 0) showKeypad    = false;
  else if (nparam==1 && strcmp (param[0], "trainsetmode")    == 0) mt_settrainset (true);
  else if (nparam==1 && strcmp (param[0], "notrainsetmode")  == 0) mt_settrainset (false);
  else Serial.println ("Command not recognised.");
}

void mt_add_gadget (int nparam, char **param)
{
  char dataBuffer[256];
  
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
            if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
              Serial.print   ("Error: ");
              Serial.print   (param[n]);
              Serial.println (" does not match any defined turnout name, names are case sensitive");
              xSemaphoreGive(displaySem);
            }
          }
        }
        else {
          isOK = false;
          if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("Route definition should be followed by turnout-name and state pairs");
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print   ("Error: ");
        Serial.print   (param[1]);
        Serial.println (" number must be between 1 and 10239");
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println("--- Throttle config -------------------------");
    xSemaphoreGive(displaySem);
  }
  mt_set_name         (1, NULL);
  mt_set_server       (1, NULL);
  mt_set_wifi         (1, NULL);
  mt_set_debounceTime (1, NULL);
  mt_set_detentCount  (1, NULL);
  showPinConfig       ();
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println("--- Server side data ------------------------");
    if (strlen(remServerType)>0) {
      Serial.print   ("   Server Type: ");
      Serial.println (remServerType);      
    }
    if (strlen(remServerType)>0) {
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
    Serial.print  ("Train Set Mode: O");
    if (trainSetMode) Serial.println ("n");
    else Serial.println ("ff");
    xSemaphoreGive(displaySem);
  }
  mt_set_protocol (1, NULL);
  mt_set_speedStep (1, NULL);
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println("---------------------------------------------");
    xSemaphoreGive(displaySem);
  }
}

void mt_sys_restart (char *reason) // restart the throttle
{
  setDisconnected();
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print ("Restarting system: ");
    Serial.println (reason);
    xSemaphoreGive(displaySem);
  }
  esp_restart();  
}

void mt_set_detentCount (int nparam, char **param) //set count of clicks required on rotary encoder
{
  if (nparam == 1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.print ("Detent Count = ");
      Serial.println (nvs_get_int ("detentCount", 2));
      xSemaphoreGive(displaySem);
    }
  }
  else {
    char *tptr;
    int count;
    count = strtol (param[1], &tptr, 10);
    if (count<1 || count>10) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.print ("Debounce time = ");
      Serial.print (nvs_get_int ("debounceTime", DEBOUNCEMS));
      Serial.println ("mS");
      xSemaphoreGive(displaySem);
    }
  }
  else {
    char *tptr;
    int count;
    count = strtol (param[1], &tptr, 10);
    if (count<1 || count>200) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("Debounce time should be between 1 and 200");
        xSemaphoreGive(displaySem);
      }
    }
    else {
      nvs_put_int ("debounceTime", count);
      if (configHasChanged) {
        if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
          Serial.println ("Change becomes effective on restart");
          xSemaphoreGive(displaySem);
        }
      }
    }
  }
}

void mt_set_wifi (int nparam, char **param)  // set WiFi parameters
{
  char *tptr;
  char myssid[33];
  char passwd[33];
  char paramName[15];
  char outBuffer[80];
  
  if (nparam<=2) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
      else Serial.println ("WiFi not connected");
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
    sprintf (outBuffer, "WiFi index must be between 0 and %d", (WIFINETS - 1));
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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

void mt_set_name (int nparam, char **param)   // Set name of throttle
{
  if (nparam == 1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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


void mt_set_speedStep(int nparam, char **param)
{
  uint8_t speedStep = 4;

  if (nparam==1) {
    speedStep = nvs_get_int ("speedStep", 4);
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print ("Speed adjust per click must be  between 1 and 9");
        xSemaphoreGive(displaySem);
      }
    }
  }
  else {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.print ("Speed adjust per click must be numeric");
      xSemaphoreGive(displaySem);
    }    
  }
}


void mt_set_wifiscan(int nparam, char **param)
{
  int use_multiwifi = 99;
  
  if (nparam==1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.print ("wifi scan: ");
      use_multiwifi = nvs_get_int ("use_multiwifi", 0);
      if (use_multiwifi == 0) Serial.println ("disabled");
      else Serial.println ("enabled");
      xSemaphoreGive(displaySem);
    }
  }
  else {
    if (strcmp (param[1], "disabled") == 0) use_multiwifi = 0;
    else if (strcmp (param[1], "enabled") == 0) use_multiwifi = 1;
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("wifiscan should be enabled or disabled");
        xSemaphoreGive(displaySem);
      }
      return;
    }
    if (use_multiwifi==0 || use_multiwifi==1) nvs_put_int ("use_multiwifi", use_multiwifi);
  }
}

void displayLocos()  // display locomotive data
{
  const static char dirIndic[3][5] = { "Fwd", "Stop", "Rev" };
  char outBuffer[80];
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   ("Locomotive Count = ");
    Serial.println (locomotiveCount);
    if (locomotiveCount > 0) {
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
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   ("TurnOut (switch/point) statuses = ");
    Serial.println (turnoutStateCount);
    if (turnoutStateCount>0) {
      Serial.println ("   ID | State");
      for (uint8_t n=0; n<turnoutStateCount; n++) {
        sprintf (outBuffer, "  %3d | %s", turnoutState[n].state, turnoutState[n].name);
        Serial.println (outBuffer);
      }
    }
    Serial.print   ("Turnout (switch/point) count = ");
    Serial.println (turnoutCount);
    if (turnoutCount > 0) {
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
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   ("Route statuses = ");
    Serial.println (routeStateCount);
    if (routeStateCount>0 && routeState != NULL) {
      Serial.println ("   ID | State");
      for (uint8_t n=0; n<routeStateCount; n++) {
        sprintf (outBuffer, "  %3d | %s", routeState[n].state, routeState[n].name);
        Serial.println (outBuffer);
      }
    }
    Serial.print   ("Routes count = ");
    Serial.println (routeCount);
    if (routeCount > 0 && routeList != NULL) {
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

void mt_settrainset (bool setting)
{
  trainSetMode = setting;
  if (setting) nvs_put_int ("trainSetMode", 1);
  else nvs_put_int ("trainSetMode", 0);
}

void mt_set_server (int nparam, char **param)  // set details about remote servers
{
  char *tptr;
  int  port;
  char host[65];
  char paramName[15];
  char outBuffer[80];
  
  if (nparam<=2) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
      if (WiFi.status() != WL_CONNECTED) {
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
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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



void mt_dump_data (int nparam, char **param)  // set details about remote servers
{
  if (nparam == 1) {
    char *newParams[] = {"void", "loco", "turnout", "turnoutstate", "route", "routestate"};
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
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("dump parameter not recognised");
        xSemaphoreGive(displaySem);
      }
      return;
    }
    if (blk_start == NULL || blk_count == 0) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print   (param[1]);
        Serial.println (": No data available to dump");
        xSemaphoreGive(displaySem);
      }
      return;
    }
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
  // if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
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
  char msgBuffer[80];
  int storedSpeed;
  int xtal;

  if (nparam==1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      storedSpeed = nvs_get_int ("cpuspeed", 0);
      sprintf (msgBuffer, " * CPU speed: %dMHz, Xtal freq: %dMHz", getCpuFrequencyMhz(), getXtalFrequencyMhz());
      Serial.println (msgBuffer);
      if (storedSpeed==0) Serial.println (" * Using default CPU speed");
      else if (storedSpeed!=getCpuFrequencyMhz()) {
        sprintf (msgBuffer, " * WARNING: configured CPU %dMHz speed mismatch with actual speed", storedSpeed);
        Serial.println (msgBuffer);
      }
      xSemaphoreGive(displaySem);
    }
  }
  else if (util_str_isa_int (param[1])) {
    storedSpeed = util_str2int(param[1]);
    xtal = getXtalFrequencyMhz();
    if (storedSpeed==240 || storedSpeed==160 || storedSpeed==80 || storedSpeed==0 ) {
      nvs_put_int ("cpuspeed", storedSpeed);
      // wait for reboot to reset speed, or we may have unwanted WiFi errors
    }
    else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("Invalid CPU speed specified");
      xSemaphoreGive(displaySem);
    }
  }
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println ("Use integer value to set CPU speed.");
    xSemaphoreGive(displaySem);
  }
}



void showPinConfig()  // Display pin out selection
{
  char outBuffer[50];
  #ifndef keynone
  uint8_t rows[] = { MEMBR_ROWS };
  uint8_t cols[] = { MEMBR_COLS };
  #endif

  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println ("Hardware configuration, check pin numbers are not duplicated");
    #ifdef SDA_PIN
    sprintf (outBuffer, "I2C - SDA      = %d", SDA_PIN);
    Serial.println (outBuffer);
    #endif
    #ifdef SCK_PIN
    sprintf (outBuffer, "I2C - SCK      = %d", SCK_PIN);
    Serial.println (outBuffer);
    #endif
    sprintf (outBuffer, "Encoder up     = %d", ENCODE_UP);
    Serial.println (outBuffer);
    sprintf (outBuffer, "Encoder down   = %d", ENCODE_DN);
    Serial.println (outBuffer);
    sprintf (outBuffer, "Encoder switch = %d", ENCODE_SW);
    Serial.println (outBuffer);
    sprintf (outBuffer, "Direction Fwd  = %d", DIRFWDPIN);
    Serial.println (outBuffer);
    sprintf (outBuffer, "Direction Rev  = %d", DIRREVPIN);
    Serial.println (outBuffer);
    #ifndef keynone
    sprintf (outBuffer, "Keyboard rows (%d) ", sizeof(rows));
    Serial.print (outBuffer);
    for (uint8_t n=0; n<sizeof(rows); n++) {
      if (n>0) Serial.print (", ");
      sprintf (outBuffer, "%d", rows[n]);
      Serial.print (outBuffer);
    }
    Serial.println ("");
    sprintf (outBuffer, "Keyboard cols (%d) ", sizeof(cols));
    Serial.print (outBuffer);
    for (uint8_t n=0; n<sizeof(cols); n++) {
      if (n>0) Serial.print (", ");
      sprintf (outBuffer, "%d", cols[n]);
      Serial.print (outBuffer);
    }
    Serial.println ("");
    #endif
    #ifdef TRACKPWR
    sprintf (outBuffer, "Trk Power indc = %d", TRACKPWR);
    Serial.println (outBuffer);
    #endif
    #ifdef TRACKPWRINV
    sprintf (outBuffer, "Trk Power indc = %d (Inverted)", TRACKPWRINV);
    Serial.println (outBuffer);
    #endif
    #ifdef TRAINSETLEN
    sprintf (outBuffer, "Train set indc = %d", TRAINSETPIN);
    Serial.println (outBuffer);
    #endif
    #ifdef F1LED
    sprintf (outBuffer, "Func 10x indc  = %d", F1LED);
    Serial.println (outBuffer);
    #endif
    #ifdef F2LED
    sprintf (outBuffer, "Func 20x indc  = %d", F2LED);
    Serial.println (outBuffer);
    #endif    
    #ifdef SPEEDOPIN
    sprintf (outBuffer, "Speedometer Out= %d", SPEEDOPIN);
    Serial.println (outBuffer);
    #endif
    xSemaphoreGive(displaySem);
  }
}


void mt_set_protocol(int nparam, char **param)
{
  if (nparam==1) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.print ("Default protocol is ");
      if (nvs_get_int ("defaultProto", JMRI) == JMRI) Serial.println ("JMRI");
      else Serial.println ("DCC++");
      xSemaphoreGive(displaySem);
    }
  }
  else {
    if (strcmp (param[1], "jmri") == 0 || strcmp (param[1], "dcc++") == 0) {
      if (strcmp (param[1], "jmri") == 0) nvs_put_int ("defaultProto", JMRI);
      else nvs_put_int ("defaultProto", DCCPLUS);
    }
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println ("protocol should be one of: jmri, dcc++");
        xSemaphoreGive(displaySem);
      }
    }
  }
}

void showMemory()
{
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   ("     Free Heap: "); Serial.print (util_ftos (ESP.getFreeHeap(), 0)); Serial.print (" bytes, "); Serial.print (util_ftos ((ESP.getFreeHeap()*100.0)/ESP.getHeapSize(), 1)); Serial.println ("%");
    Serial.print   (" Min Free Heap: "); Serial.print (util_ftos (ESP.getMinFreeHeap(), 0)); Serial.print (" bytes, "); Serial.print (util_ftos ((ESP.getMinFreeHeap()*100.0)/ESP.getHeapSize(), 1)); Serial.println ("%");
    Serial.print   ("     Heap Size: "); Serial.print (util_ftos (ESP.getHeapSize(), 0)); Serial.println (" bytes");
    Serial.print   ("NVS Free Space: "); Serial.print (nvs_get_freeEntries()); Serial.println (" entries");
    Serial.println ("NB: NVS Entries = a block count, first block holds name/id, plus 1 block for each 32 bytes data");
    Serial.print   ("        Uptime: "); Serial.print (util_ftos (esp_timer_get_time() / (uS_TO_S_FACTOR * 60.0), 2)); Serial.println (" mins");
    Serial.print   ("      CPU Freq: "); Serial.print (util_ftos (ESP.getCpuFreqMHz(), 0)); Serial.println (" MHz");
    Serial.print   ("  Crystal Freq: "); Serial.print (util_ftos (getXtalFrequencyMhz(), 0)); Serial.println (" MHz");
    xSemaphoreGive(displaySem);
  }
}

void help()  // show help data
{
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.println ((const char*) "Running permanent settings without parameters prints current settings:");
    Serial.println ((const char*) "add loco <dcc-address> <description>");
    Serial.println ((const char*) "add turnout <name> {DCC|SERVO|VPIN} <dcc-definition>");
    Serial.println ((const char*) "add route <name> {<turnout-name> <state>} {<turnout-name> <state>}...");
    Serial.println ((const char*) "    Add a locomotive, turnout or route to DCC++ roster");
    Serial.println ((const char*) "    where definitions may be:");
    Serial.println ((const char*) "       DCC <linear-address>");
    Serial.println ((const char*) "       DCC <address> <sub-address>");
    Serial.println ((const char*) "       SERVO <pin> <throw-posn> <closed-posn> <profile>");
    Serial.println ((const char*) "       VPIN <pin-number>");
    Serial.println ((const char*) "    Servo profile: 0=immediate, 1=0.5 secs, 2=1 sec, 3=2 secs, 4=bounce");
    Serial.println ((const char*) "    And where turn-out state is one of:");
    Serial.println ((const char*) "       C - Closed");
    Serial.println ((const char*) "       T - Thrown");
    Serial.println ((const char*) "    permanent settingi, DCC++ only, restart required");
    Serial.println ((const char*) "config");
    Serial.println ((const char*) "    Show current configuration settings");
    Serial.println ((const char*) "    info only");
    Serial.println ((const char*) "cpuspeed [{240|160|80|0}]");
    Serial.println ((const char*) "    Set CPU speed in MHz, try to use the lowest viable to save power consumption");
    Serial.println ((const char*) "    Use zero to use factory default speed");
    Serial.println ((const char*) "    permanent setting, restart required");
    Serial.println ((const char*) "del {loco|turnout} <dcc-address|name>");
    Serial.println ((const char*) "    Delete a locomotive or turnout from DCC++ roster");
    Serial.println ((const char*) "    permanent setting, DCC++ only, restart required");
    Serial.println ((const char*) "detentcount [<n>]");
    Serial.println ((const char*) "    Set the number or rotary encoder clicks per up/down movement");
    Serial.println ((const char*) "    permanent setting");
    Serial.println ((const char*) "dump {loco|turnout|turnoutstate|route|routestate}");
    Serial.println ((const char*) "    Dump memory structures of objects");
    Serial.println ((const char*) "    info only");
    Serial.println ((const char*) "help");
    Serial.println ((const char*) "    Print this help menu");
    Serial.println ((const char*) "    info only");
    Serial.println ((const char*) "locos|turnouts");
    Serial.println ((const char*) "    List known locomotives or turnouts");
    Serial.println ((const char*) "    info only");
    Serial.println ((const char*) "memory");
    Serial.println ((const char*) "    Show system memory usage");
    Serial.println ((const char*) "    info only");
    Serial.println ((const char*) "name [<name>]");
    Serial.println ((const char*) "    Set the name of the throttle");
    Serial.println ((const char*) "    permanent setting");
    Serial.println ((const char*) "nvs [{<namespace>|*}]");
    Serial.println ((const char*) "    Show contents of Non-Volatile-Storage (NVS) namespace(s)");
    Serial.println ((const char*) "    info only");
    Serial.println ((const char*) "pins");
    Serial.println ((const char*) "    Show processor pin assignment");
    Serial.println ((const char*) "    info only");
    Serial.println ((const char*) "protocol {jrmi|dcc++}");
    Serial.println ((const char*) "    Select default protocol to use");
    Serial.println ((const char*) "    permanent setting");
    Serial.println ((const char*) "restart");
    Serial.println ((const char*) "    Restart throttle");
    Serial.println ((const char*) "    temporary setting");
    Serial.println ((const char*) "server [<index> <IP-address|DNS-name> [<port-number>]]");
    Serial.println ((const char*) "    Set the server address and port");
    Serial.println ((const char*) "    permanent setting");
    Serial.println ((const char*) "speedstep [<1-9>]");
    Serial.println ((const char*) "    Set the speed change set be each thottle click");
    Serial.println ((const char*) "    permanent setting");
    Serial.println ((const char*) "[no]showkeepalive");
    Serial.println ((const char*) "    Show network packets received and transmitted");
    Serial.println ((const char*) "    temporary setting");
    Serial.println ((const char*) "[no]showkeypad");
    Serial.println ((const char*) "    Show keypad keystrokes to console");
    Serial.println ((const char*) "    temporary setting");
    Serial.println ((const char*) "[no]showpackets");
    Serial.println ((const char*) "    Show network packets received and transmitted");
    Serial.println ((const char*) "    temporary setting");
    Serial.println ((const char*) "[no]trainsetmode");
    Serial.println ((const char*) "    turn train set mode on/off as default setting");
    Serial.println ((const char*) "    permanent setting");
    Serial.println ((const char*) "wifi [<index> <ssid> [<password>]]");
    Serial.println ((const char*) "    Set WiFi SSID and password");
    Serial.println ((const char*) "    permanent setting");
    Serial.println ((const char*) "wifiscan [enable|disable]");
    Serial.println ((const char*) "    set wifi to scan for non-hidden ssids and use strongest signal");
    Serial.println ((const char*) "    NB: the network used must be predefined by \"wifi\", or it will be ignored");
    Serial.println ((const char*) "    permanent setting");
    xSemaphoreGive(displaySem);
  }
}
