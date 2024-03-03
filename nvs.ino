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



/*
 * Keep NVS storage routines here
 */
Preferences prefs;

void nvs_init()
{
  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.begin ("Throttle");  
    xSemaphoreGive(nvsSem);
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

void nvs_get_string (const char *strName, char *strDest, const char *strDefault, int strSize)
{
  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    if (prefs.getString(strName, strDest, strSize) == 0) {
      strcpy (strDest, strDefault);
    }  
    xSemaphoreGive(nvsSem);
  }
  else {
    strcpy (strDest, strDefault);
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

void nvs_get_string (const char *strName, char *strDest, int strSize)
{
  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.getString(strName, strDest, strSize);
    xSemaphoreGive(nvsSem);
  } 
  else {
    strcpy (strDest, "");
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

void nvs_put_string (const char *strName, const char *value)
{
  char oldval[80];
  oldval[0] = '\0';
  nvs_get_string (strName, oldval, "", 80);
  if (strcmp (oldval, value) != 0 && xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.putString (strName, value);
    xSemaphoreGive(nvsSem);
    configHasChanged = true;
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

int nvs_get_int (const char *intName, int intDefault)
{
  int retVal = -1;
  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    retVal = prefs.getInt (intName, intDefault);
    xSemaphoreGive(nvsSem);
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
  return (retVal);
}

void nvs_put_int (const char *intName, int value)
{
  int oldval = nvs_get_int (intName, value+1);
  if (value != oldval && xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.putInt (intName, value);
    configHasChanged = true;
    xSemaphoreGive(nvsSem);
  }
}

double nvs_get_double (const char *doubleName, double doubleDefault)
{
  double retval = doubleDefault;
  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    retval = prefs.getDouble (doubleName, doubleDefault);
    xSemaphoreGive(nvsSem);
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
  if (isnan(retval)) retval = doubleDefault; 
  return (retval);
}

void nvs_put_double (const char *doubleName, double value)
{
  double oldval = nvs_get_double (doubleName, value+1.00);
  if (value != oldval && xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.putDouble (doubleName, value);
    xSemaphoreGive(nvsSem);
    configHasChanged = true;
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

float nvs_get_float (const char *floatName, float floatDefault)
{
  float retval = floatDefault;
  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    retval = prefs.getFloat (floatName, floatDefault);
    xSemaphoreGive(nvsSem);
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
  if (isnan(retval)) retval = floatDefault; 
  return (retval);
}

void nvs_put_float (const char *floatName, float value)
{
  float oldval = nvs_get_float (floatName, value+1.00);
  if (value != oldval && xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.putFloat (floatName, value);
    xSemaphoreGive(nvsSem);
    configHasChanged = true;
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

int nvs_get_freeEntries()
{
  int retVal = 0;
  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    retVal = prefs.freeEntries();
    xSemaphoreGive(nvsSem);
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
  return (retVal);
}

void nvs_put_string (const char* nameSpace, const char* key, const char* value)
{
  Preferences tpref;

  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.end();   // Should only ever have one set of preferences open at a time
    tpref.begin(nameSpace);
    tpref.putString (key, value);
    tpref.end();
    prefs.begin ("Throttle");  
    xSemaphoreGive(nvsSem);
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

void nvs_get_string (const char* nameSpace, const char *strName, char *strDest, const char *strDefault, int strSize)
{
  Preferences tpref;

  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.end();   // Should only ever have one set of preferences open at a time
    tpref.begin(nameSpace);
    if (tpref.getString(strName, strDest, strSize) == 0) {
      strcpy (strDest, strDefault);
    }  
    tpref.end();
    prefs.begin ("Throttle");  
    xSemaphoreGive(nvsSem);
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

void nvs_del_key (const char* nameSpace, const char* key)
{
  Preferences tpref;

  if (xSemaphoreTake(nvsSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    prefs.end();   // Should only ever have one set of preferences open at a time
    tpref.begin(nameSpace);
    tpref.remove (key);
    tpref.end();
    prefs.begin ("Throttle");  
    xSemaphoreGive(nvsSem);
  }
  else {
    semFailed ("nvsSem", __FILE__, __LINE__);
  }
}

// return a pointer to an array of num_entries structures with key[16], value[data_size]
void* nvs_extractStr (const char *nameSpace, int numEntries, int dataSize)
{
  int   readCnt = 0;
  int   retOffset = 0;
  int   entrySize = dataSize + 16;
  char* retVal = (char*) malloc (numEntries * entrySize);
  char* retPtr = retVal;
  nvs_page *nsbuff = NULL;
  nvs_page *dabuff = NULL;
  esp_partition_iterator_t partIt;
  const esp_partition_t* part;
  uint32_t nsoffset = 0;
  uint32_t daoffset = 0;
  uint8_t targType = 0x21;
  uint8_t i, j;
  uint8_t bitmap;
  uint8_t targetns = 0;
  
  nsbuff = (nvs_page*) malloc (sizeof (nvs_page));
  dabuff = (nvs_page*) malloc (sizeof (nvs_page));
  if (nsbuff == NULL || dabuff == NULL) {
    if (nsbuff != NULL) free (nsbuff);
    if (dabuff != NULL) free (dabuff);
    if (retVal != NULL) free (retVal);
    return(NULL);  // No space for data buffers
  }
  partIt = esp_partition_find (ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, (const char*) "nvs");
  if (partIt) {
    part = esp_partition_get (partIt);
    esp_partition_iterator_release (partIt);
    while (nsoffset < part->size) {
      if (esp_partition_read (part, nsoffset, nsbuff, sizeof(nvs_page)) != ESP_OK) {
        if (nsbuff != NULL) free (nsbuff);
        if (dabuff != NULL) free (dabuff);
        if (retVal != NULL) free (retVal);
        return(NULL); // Cannot read namespace data
      }
      i = 0 ;
      while (i < 126) {
        bitmap = ( nsbuff->Bitmap[i/4] >> ( ( i % 4 ) * 2 ) ) & 0x03 ;  // Get bitmap for this NameSpace entry
        if ( bitmap == 2 ) {
          if (nsbuff->Entry[i].Ns == 0 && strcmp(nameSpace, nsbuff->Entry[i].Key)==0) {
            targetns = (uint8_t) (nsbuff->Entry[i].Data.sixFour & 0xFF);
            daoffset = 0;
            while (daoffset < part->size) {
              if (esp_partition_read (part, daoffset, dabuff, sizeof(nvs_page)) != ESP_OK) {
                if (nsbuff != NULL) free (nsbuff);
                if (dabuff != NULL) free (dabuff);
                if (retVal != NULL) free (retVal);
                return(NULL);  // No space for content
              }
              j = 0;
              while (j < 126) {
                bitmap = ( dabuff->Bitmap[j/4] >> ( ( j % 4 ) * 2 ) ) & 0x03 ;  // Get bitmap for this Data entry
                if ( bitmap == 2 ) {
                  if (dabuff->Entry[j].Ns == targetns && dabuff->Entry[j].Type == targType && numEntries>readCnt++) {
                    strcpy (retPtr, dabuff->Entry[j].Key);
                    strcpy (retPtr+16, (char*) &(dabuff->Entry[j+1].Ns));
                    retPtr = retPtr + entrySize;
                  }
                j += dabuff->Entry[j].Span ;                                    // Next Data entry
                }
                else j++;
              }
              daoffset += sizeof(nvs_page) ;                                    // Prepare to read next Data page in nvs
            }
          }
          i += nsbuff->Entry[i].Span ;                                          // Next NameSpace entry          
        }
        else i++ ;
      }
      nsoffset += sizeof(nvs_page) ;                                            // Prepare to read next NameSpace page in nvs
    }
  }
  if (nsbuff != NULL) free (nsbuff);
  if (dabuff != NULL) free (dabuff);
  if (targetns == 0) {
    if (retVal != NULL) free (retVal);
    return(NULL);  // Name space not found
  }
  return (retVal);
}

// Count the number of entries in an NVS partition of a certain type
// type may be "all" to count all entries
int nvs_count (const char* target, const char* type)
{
  nvs_page *nsbuff = NULL;
  nvs_page *dabuff = NULL;
  esp_partition_iterator_t partIt;
  const esp_partition_t* part;
  uint32_t nsoffset = 0;
  uint32_t daoffset = 0;
  int retVal = 0;
  uint8_t targType = 255;
  uint8_t i, j;
  uint8_t bitmap;
  uint8_t targetns = 0;
  
  for (uint8_t n=0;n<sizeof(nvs_index_ref); n++) if (strcmp(type, nvs_descrip[n]) == 0) targType = nvs_index_ref[n];
  nsbuff = (nvs_page*) malloc (sizeof (nvs_page));
  dabuff = (nvs_page*) malloc (sizeof (nvs_page));
  if (nsbuff == NULL || dabuff == NULL) {
    if (nsbuff != NULL) free (nsbuff);
    if (dabuff != NULL) free (dabuff);
    return(-1);  // No space for data buffers
  }
  partIt = esp_partition_find (ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, (const char*) "nvs");
  if (partIt) {
    part = esp_partition_get (partIt);
    esp_partition_iterator_release (partIt);
    while (nsoffset < part->size) {
      if (esp_partition_read (part, nsoffset, nsbuff, sizeof(nvs_page)) != ESP_OK) {
        if (nsbuff != NULL) free (nsbuff);
        if (dabuff != NULL) free (dabuff);
        return(-2); // Cannot read namespace data
      }
      i = 0 ;
      while (i < 126) {
        bitmap = ( nsbuff->Bitmap[i/4] >> ( ( i % 4 ) * 2 ) ) & 0x03 ;  // Get bitmap for this NameSpace entry
        if ( bitmap == 2 ) {
          if (nsbuff->Entry[i].Ns == 0 && (target==NULL || strcmp(target, nsbuff->Entry[i].Key)==0)) {
            targetns = (uint8_t) (nsbuff->Entry[i].Data.sixFour & 0xFF);
            daoffset = 0;
            while (daoffset < part->size) {
              if (esp_partition_read (part, daoffset, dabuff, sizeof(nvs_page)) != ESP_OK) {
                if (nsbuff != NULL) free (nsbuff);
                if (dabuff != NULL) free (dabuff);
                return(-3);  // No space for content
              }
              j = 0;
              while (j < 126) {
                bitmap = ( dabuff->Bitmap[j/4] >> ( ( j % 4 ) * 2 ) ) & 0x03 ;  // Get bitmap for this Data entry
                if ( bitmap == 2 ) {
                  if (dabuff->Entry[j].Ns == targetns && (dabuff->Entry[j].Type == targType || strcmp(type, "all") == 0)) {
                    retVal++;
                  }
                j += dabuff->Entry[j].Span ;                                    // Next Data entry
                }
                else j++;
              }
              daoffset += sizeof(nvs_page) ;                                    // Prepare to read next Data page in nvs
            }
          }
          i += nsbuff->Entry[i].Span ;                                          // Next NameSpace entry          
        }
        else i++ ;
      }
      nsoffset += sizeof(nvs_page) ;                                            // Prepare to read next NameSpace page in nvs
    }
  }
  if (nsbuff != NULL) free (nsbuff);
  if (dabuff != NULL) free (dabuff);
  if (targetns == 0) return(-4);  // Name space not found
  return (retVal);
}


/*
 * Inspired by https://github.com/Edzelf/ESP32-Show_nvs_keys/blob/master/Show_nvs_keys.ino
 * Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
 */
void nvs_dumper(const char *target)
{
  uint32_t nsoffset = 0;
  uint32_t daoffset = 0;
  esp_partition_iterator_t partIt;
  const esp_partition_t* part;
  char outline[105];
  char dataLine[41];
  char *dataPtr;
  char *typePtr;
  uint8_t i, j;
  uint8_t bitmap;
  uint8_t targetns;
  uint8_t page;
  nvs_page *nsbuff = NULL;
  nvs_page *dabuff = NULL;

  nsbuff = (nvs_page*) malloc (sizeof (nvs_page));
  dabuff = (nvs_page*) malloc (sizeof (nvs_page));
  partIt = esp_partition_find (ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, (const char*) "nvs");
  if (partIt) {
    part = esp_partition_get (partIt);
    esp_partition_iterator_release (partIt);
    sprintf (outline, "NVS partition size: %d bytes", part->size);
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (outline);
      xSemaphoreGive(consoleSem);
    }
    while (nsoffset < part->size) {
      if (esp_partition_read (part, nsoffset, nsbuff, sizeof(nvs_page)) != ESP_OK) {
        if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
          Serial.println ("Error reading NVS data");
          xSemaphoreGive(consoleSem);
        }
        if (nsbuff != NULL) free (nsbuff);
        if (dabuff != NULL) free (dabuff);
        return;
      }
      i = 0 ;
      while (i < 126) {
        bitmap = ( nsbuff->Bitmap[i/4] >> ( ( i % 4 ) * 2 ) ) & 0x03 ;  // Get bitmap for this NameSpace entry
        if ( bitmap == 2 ) {
          if (nsbuff->Entry[i].Ns == 0 && (target==NULL || strcmp(target, nsbuff->Entry[i].Key)==0)) {
            sprintf (outline, "--- Namespace = %s ---------------------------------", nsbuff->Entry[i].Key);
            if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.println (outline);
              sprintf (outline, "%4s | %3s | %-8s | %-15s | %s", "Page", "Key", "Type", "Name", "Value");
              Serial.println (outline);
              xSemaphoreGive(consoleSem);
            }
            targetns = (uint8_t) (nsbuff->Entry[i].Data.sixFour & 0xFF);
            daoffset = 0;
            page = 0;
            while (daoffset < part->size) {
              if (esp_partition_read (part, daoffset, dabuff, sizeof(nvs_page)) != ESP_OK) {
                if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                  Serial.println ("Error reading NVS data");
                  xSemaphoreGive(consoleSem);
                }
                if (nsbuff != NULL) free (nsbuff);
                if (dabuff != NULL) free (dabuff);
                return;
              }
              j = 0;
              while (j < 126) {
                bitmap = ( dabuff->Bitmap[j/4] >> ( ( j % 4 ) * 2 ) ) & 0x03 ;  // Get bitmap for this Data entry
                if ( bitmap == 2 ) {
                  if (dabuff->Entry[j].Ns == targetns) {
                    switch (dabuff->Entry[j].Type) {
                      case 0x01:
                        sprintf (dataLine, "%d", dabuff->Entry[j].Data.eight[0]);
                        dataPtr = dataLine;
                        break;
                      case 0x02:
                        sprintf (dataLine, "%d", dabuff->Entry[j].Data.oneSix[0]);
                        dataPtr = dataLine;
                        break;
                      case 0x04:
                        sprintf (dataLine, "%d", dabuff->Entry[j].Data.threeTwo[0]);
                        dataPtr = dataLine;
                        break;
                      case 0x08:
                        sprintf (dataLine, "%ld", dabuff->Entry[j].Data.sixFour);
                        dataPtr = dataLine;
                        break;
                      case 0x11:
                        sprintf (dataLine, "%d", (int8_t) dabuff->Entry[j].Data.eight[0]);
                        dataPtr = dataLine;
                        break;
                      case 0x12:
                        sprintf (dataLine, "%d", (int16_t) dabuff->Entry[j].Data.oneSix[0]);
                        dataPtr = dataLine;
                        break;
                      case 0x14:
                        sprintf (dataLine, "%d", (int32_t) dabuff->Entry[j].Data.threeTwo[0]);
                        dataPtr = dataLine;
                        break;
                      case 0x18:
                        sprintf (dataLine, "%ld", (int64_t) dabuff->Entry[j].Data.sixFour);
                        dataPtr = dataLine;
                        break;
                      case 0x21:
                        dataPtr = (char*) &(dabuff->Entry[j+1].Ns);
                        dataPtr[dabuff->Entry[j].Data.oneSix[0]] = '\0';
                        if (strlen(dataPtr) >= 60) dataPtr[59] = 0;
                        break;
                      default:
                        sprintf (dataLine, "0x%02x%02x%02x%02x%02x%02x%02x%02x",
                          dabuff->Entry[j].Data.eight[0],
                          dabuff->Entry[j].Data.eight[1],
                          dabuff->Entry[j].Data.eight[2],
                          dabuff->Entry[j].Data.eight[3],
                          dabuff->Entry[j].Data.eight[4],
                          dabuff->Entry[j].Data.eight[5],
                          dabuff->Entry[j].Data.eight[6],
                          dabuff->Entry[j].Data.eight[7]);
                        dataPtr = dataLine;
                        break;
                    }
                    typePtr = NULL;
                    for (uint8_t n=0; n<sizeof(nvs_index_ref) && typePtr==NULL; n++)
                      if (nvs_index_ref[n] == dabuff->Entry[j].Type)
                        typePtr = (char*) nvs_descrip[n];
                    if (typePtr == NULL) typePtr = (char*) "Unknown";
                    sprintf (outline, "%4d | %03d | %-8s | %-15s | %s", page,  j,// Print page and entry nr
                       typePtr,                                                  // Print the data type
                       dabuff->Entry[j].Key,                                     // Print the key
                       dataPtr );                                                // Print the data
                    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                      Serial.println (outline);
                      xSemaphoreGive(consoleSem);
                    }
                  }
                j += dabuff->Entry[j].Span ;                                    // Next Data entry
                }
                else j++;
              }
              daoffset += sizeof(nvs_page) ;                                   // Prepare to read next Data page in nvs
              page++;
            }
          }
          i += nsbuff->Entry[i].Span ;                                          // Next NameSpace entry          
        }
        else i++ ;
      }
      nsoffset += sizeof(nvs_page) ;                                           // Prepare to read next NameSpace page in nvs
    }
  }
  else {
    if (xSemaphoreTake(consoleSem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println ("NVS partition not found");
      xSemaphoreGive(consoleSem);
    }
  }
  if (nsbuff != NULL) free (nsbuff);
  if (dabuff != NULL) free (dabuff);
}
