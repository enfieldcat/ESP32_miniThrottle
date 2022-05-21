int util_str2int(char *string)
{
  char *tptr;
  long result;

  if (string==NULL || strlen(string)==0) return(0);
  result = strtol (string, &tptr, 10);

  return ((int) result);
}


/*
 * Routine to check if a string contains an integer.
 */
bool util_str_isa_int (char *inString)
{
  bool retval = true;
  int  howlong = strlen(inString);
  if (inString==NULL || strlen(inString)==0) return(false);
  for (int n=0; n<howlong; n++) {
    if ((inString[n]>='0' && inString[n]<='9') || (n==0 && (inString[n]=='+' || inString[n]=='-'))) {
      // it looks integer!
    }
    else retval = false;
  }
  return (retval);
}


/*
 * Homespun float to string function with dp decimal points
 */
char* util_ftos (float value, int dp)
{
  static char retval[32];
  //float mult;
  //int ws;
  //int64_t intPart;
  char format[15];

  if (dp<=0) sprintf(format, "%%1.0lf");
  else sprintf (format, "%%%d.%dlf", (dp+2), dp);
  sprintf (retval, format, value);
  return (retval);
}

/*
 * Return a string value of uint8_t
 */
char* util_bin2str (uint8_t inVal)
{
  static char retVal[10];

  retVal[0] = '\0';
  for (uint8_t mask=128; mask>0; mask = mask >> 1) {
    if ((inVal & mask) > 0) strcat (retVal, "1");
    else strcat (retVal, "0");
    if (mask == 16) strcat (retVal, "-");
  }
  return (retVal);
}

/*
 * Simple swap routine for sorting
 */
void swapRecord (void *lowRec, void *hiRec, int recSize)
{
  uint8_t recBuffer[recSize];

  memcpy (recBuffer, (uint8_t*) lowRec, recSize);
  memcpy ((uint8_t*) lowRec, (uint8_t*) hiRec, recSize);
  memcpy ((uint8_t*) hiRec, recBuffer, recSize);
}

/*
 * Simple bubble sort for locos, not most efficient sort but simple to debug
 */
void sortLoco()
{
  bool hasSwapped = true;
  uint8_t limit = locomotiveCount - 1;

  if (locomotiveCount > 1) {
    while (hasSwapped) {
      hasSwapped = false;
      for (uint8_t n=0; n<limit; n++) {
        if (strcmp (locoRoster[n].name, locoRoster[n+1].name) > 1) {
          hasSwapped = true;
          swapRecord (&locoRoster[n], &locoRoster[n+1], sizeof(struct locomotive_s));
        }
      }
    }
  }
}


/*
 * Simple bubble sort for turnouts, not most efficient sort but simple to debug
 */
void sortTurnout()
{
  bool hasSwapped = true;
  uint8_t limit = turnoutCount - 1;

  if (turnoutCount > 1) {
    while (hasSwapped) {
      hasSwapped = false;
      for (uint8_t n=0; n<limit; n++) {
        if (strcmp (turnoutList[n].userName, turnoutList[n+1].userName) > 1) {
          hasSwapped = true;
          swapRecord (&turnoutList[n], &turnoutList[n+1], sizeof(struct turnout_s));
        }
      }
    }
  }
}


/*
 * Simple bubble sort for turnouts, not most efficient sort but simple to debug
 */
void sortRoute()
{
  if (routeCount > 1) {
    bool hasSwapped = true;
    uint8_t limit = routeCount - 1;
    
    while (hasSwapped) {
      hasSwapped = false;
      for (uint8_t n=0; n<limit; n++) {
        if (strcmp (routeList[n].userName, routeList[n+1].userName) > 1) {
          hasSwapped = true;
          swapRecord (&routeList[n], &routeList[n+1], sizeof(struct route_s));
        }
      }
    }
  }
}


#ifdef FILESUPPORT
// directory listing of SPiffs filesystem
// cf: https://github.com/espressif/arduino-esp32/blob/master/libraries/SPIFFS/examples/SPIFFS_Test/SPIFFS_Test.ino
void util_listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  char msgBuffer[80];
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.printf("Listing directory: %s\r\n", dirname);
    xSemaphoreGive(displaySem);
  }

  File root = fs.open(dirname);
  if(!root){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("- failed to open directory");
      xSemaphoreGive(displaySem);
    }
    return;
  }
  if(!root.isDirectory()){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println(" - not a directory");
      xSemaphoreGive(displaySem);
    }
    return;
  }

  File file = root.openNextFile();
  while(file){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      for (uint8_t space=0; space<levels; space++) Serial.print ("  ");
      xSemaphoreGive(displaySem);
    }
    if(file.isDirectory()){
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print  ("  DIR : ");
        Serial.println((char*)file.name());
        xSemaphoreGive(displaySem);
      }
      util_listDir(fs, file.name(), levels +1);
    } 
    else {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print  ("  FILE: ");
        Serial.print  ((char*)file.name());
        sprintf (msgBuffer, "%d", (uint)file.size());
        Serial.print  ("\tSIZE: ");
        Serial.println(msgBuffer);
        xSemaphoreGive(displaySem);
      }
    }
    file = root.openNextFile();
  }
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    sprintf (msgBuffer, "%d bytes used of %d available (%d%% used)", SPIFFS.usedBytes(), SPIFFS.totalBytes(), (SPIFFS.usedBytes()*100)/SPIFFS.totalBytes());
    xSemaphoreGive(displaySem);
  }
  Serial.println (msgBuffer);
}

void util_deleteFile(fs::FS &fs, const char * path){
  if (!fs.exists(path)) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("  - File does not exist");
      xSemaphoreGive(displaySem);
    }
    return;
  }
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   ("Deleting file: ");
    Serial.println ((char*) path);
    if(fs.remove(path)){
      Serial.println("  - file deleted");
    } else {
      Serial.println("  - delete failed");
    }
    xSemaphoreGive(displaySem);
  }
}

char* util_loadFile(fs::FS &fs, const char* path)
{
  util_loadFile (fs, path, NULL);
}


char* util_loadFile(fs::FS &fs, const char* path, int* sizeOfFile)
{
  char *retval = NULL;
  int fileSize = 0;
  int ptr = 0;

  if (sizeOfFile!=NULL) *sizeOfFile = 0;
  if (!fs.exists(path)) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("  - File does not exist");
      xSemaphoreGive(displaySem);
    }
    return (retval);
  }
  File file = fs.open(path);
  if(!file){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println("  - failed to open file for reading");
      xSemaphoreGive(displaySem);
    }
    return (retval);
  }
  if(file.isDirectory()){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println("  - Cannot open directory for reading");
      xSemaphoreGive(displaySem);
    }
    return (retval);
  }
  fileSize = file.size() + 1;
  retval = (char*) malloc (fileSize);
  if (retval != NULL) {
    while(file.available() && ptr<fileSize){
      retval[ptr++] = file.read();
    }
    file.close();
    retval[fileSize-1] = '\0';  // Ensure data ends with a null terminator character
  }
  if (retval!=NULL && sizeOfFile!=NULL) *sizeOfFile = fileSize;

  return (retval);
}


void util_readFile(fs::FS &fs, const char * path, bool replay) {
  uint8_t inChar;
  uint8_t bufPtr = 0;
  uint8_t cmdBuffer[BUFFSIZE];
  
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   ("Reading file: ");
    Serial.println ((char*) path);
    xSemaphoreGive(displaySem);
  }

  if (!fs.exists(path)) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("  - File does not exist");
      xSemaphoreGive(displaySem);
    }
    return;
  }
  File file = fs.open(path);
  if(!file){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println("  - failed to open file for reading");
      xSemaphoreGive(displaySem);
    }
    return;
  }
  if(file.isDirectory()){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println("  - Cannot open directory for reading");
      xSemaphoreGive(displaySem);
    }
    return;
  }

  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {  
    Serial.println ("--- read from file -----------------------");
    Serial.println ("");
    while(file.available()){
      inChar = (uint8_t) file.read();
      if (inChar == '\n') {
        Serial.print('\r');
        if (replay || bufPtr == BUFFSIZE){
          cmdBuffer[bufPtr] = '\0';
          if (cmdBuffer[0] != '#') process(cmdBuffer); // if 1st char in line is a '#' it is a comment
          bufPtr = 0;
        }
      }
      else if (replay) {
        cmdBuffer[bufPtr++] = inChar;
        if (bufPtr == BUFFSIZE) bufPtr = 0; // overflow, line too long so truncate
      }
      Serial.print((char) inChar);
    }
    file.close();
    Serial.println ("");
    Serial.println ("--- end of file --------------------------");
    xSemaphoreGive(displaySem);
  }
}

void util_writeFile (fs::FS &fs, const char * path)
{
  if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   ("Writing file: ");
    Serial.print   ((char*) path);
    Serial.println ("  -  Use \".\" on a line of its own to stop writing.");
    xSemaphoreGive(displaySem);
  }

  if (fs.exists(path)) fs.remove(path);
  writeFile = fs.open(path, FILE_WRITE);
  if(!writeFile){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println("  - failed to open file for writing");
      xSemaphoreGive(displaySem);
    }
  }
  else writingFile = true;
}

void util_closeWriteFile()
{
  writeFile.close();
  writingFile = false;
}

void util_appendWriteFile (char* content)
{
  writeFile.println(content);
}

void util_format_spiffs()
{
  SPIFFS.format();
}


void getHttp2File (fs::FS &fs, char *url, char *fileName)
{
  HTTPClient *httpClient = NULL;
  char http_certFile[42];
  uint8_t *inBuffer = NULL;
  int16_t inLength = 100;

  if (url == NULL || fileName == NULL) return;
  if (strncmp (url, "http://", 7) != 0 && strncmp (url, "https://", 8) != 0) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("URL should start with \"http://\" or \"https://\"");
      xSemaphoreGive(displaySem);
    }
  }
  if (fileName[0] != '/') {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.println ("Filename should start with \"/\"");
      xSemaphoreGive(displaySem);
    }
  }
  nvs_get_string ("web_certFile", http_certFile, CERTFILE, sizeof(http_certFile));
  if (http_certFile[0]!='\0') rootCACertificate = util_loadFile(SPIFFS, http_certFile);
  else rootCACertificate = NULL;
  WiFiClient *inStream = getHttpStream(url, rootCACertificate, httpClient);
  if (inStream != NULL) {
    if (fs.exists(fileName)) fs.remove(fileName);
    writeFile = fs.open(fileName, FILE_WRITE);
    if(!writeFile){
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.print  (fileName);
        Serial.println(" - failed to open file for writing");
        xSemaphoreGive(displaySem);
      }
    }
    else {
      inBuffer = (uint8_t*) malloc (WEB_BUFFER_SIZE);
      if (inBuffer!=NULL) {
        while (inLength>0) {
          inLength = inStream->read(inBuffer, WEB_BUFFER_SIZE);
          if (inLength>0) writeFile.write (inBuffer, inLength);
        }
        free (inBuffer);
      }
      writeFile.close();
    }
  }
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    Serial.print   ("Failed to open URL: ");
    Serial.println (url);
    xSemaphoreGive(displaySem);
  }
  if (httpClient != NULL) closeHttpStream (httpClient);
}


/*
 * Open a stream to read http/s data
 */
WiFiClient* getHttpStream (char *url, const char *cert, HTTPClient *http)
{
  WiFiClient *retVal = NULL;
  int httpCode;

  http = new HTTPClient;
  if (strncmp (url, "https://", 8) == 0 && cert != NULL) {
    http->begin (url, cert);
  }
  else {
    http->begin (url);
  }
  httpCode = http->GET();
  if (httpCode == HTTP_CODE_OK) {
    retVal = http->getStreamPtr();
  }
  else if (httpCode<0) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.printf ("Error connecting to web server: %d on %s", httpCode, url);
      xSemaphoreGive(displaySem);
    }
  }
return (retVal);
}

/*
 * Close http stream once done
 */
void closeHttpStream(HTTPClient *http)
{
  if (http!= NULL) {
    http->end();
    http->~HTTPClient();
    http = NULL;
  }
}

void defaultCertExists(fs::FS &fs)
{
  if(!fs.exists(CERTFILE)){
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
      Serial.print ("Missing default certificate file, creating ");
      Serial.println (CERTFILE);
      xSemaphoreGive(displaySem);
    }
    // file.close();
    File defCertFile = fs.open( CERTFILE, FILE_WRITE);
    if(!defCertFile){
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(2000)) == pdTRUE) {
        Serial.println("  - failed to open file for writing");
        xSemaphoreGive(displaySem);
      }
    }
    else {
      defCertFile.print (defaultCertificate);
      defCertFile.close();
    }
  }
}
#endif
