#ifdef OTAUPDATE
/*
 * ota_control
 * 
 * Decription: routine to perform ota updates
 * 
 */

class ota_control {
  
  private:

    uint64_t sequence = 0;
    uint64_t newSequence = 0;
    char message[120];
    char image_name[80];
    char chksum[65];
    char ota_label[50];
    uint32_t image_size;
    HTTPClient *http = NULL;
    
    /*
     * Get and put sequence of last OTA update
     * typically this would just be a time sequence number (seconds since 01-Jan-1970)
     */
    uint64_t get_sequence_id() {  
      Preferences otaprefs;

      otaprefs.begin ("otaprefs");
      sequence = otaprefs.getULong64 ("sequence", 0);
      otaprefs.end();
      return (sequence);
    }

    void put_sequence_id(uint64_t id)
    {
      sequence = id; // should not be required if we restart after update
      Preferences otaprefs;

      otaprefs.begin ("otaprefs");
      otaprefs.putULong64 ("sequence", id);
      otaprefs.end();
      return;
    }

    /*
     * Read metadata about the update
     */
    bool get_meta_data(char *url, const char *cert)
    {
      bool retVal = false;
      int32_t inByte;
      char inPtr;
      char inBuffer[80];
      WiFiClient *inStream = getHttpStream(url, cert, http);
      ota_label[0] = '\0';
      if (inStream != NULL) {
        inPtr = 0;
         // process the character stream from the http/s source
        while ((inByte = inStream->read()) >= 0) {
          if (inPtr < sizeof(inBuffer)){
            if (inByte=='\n' || inByte=='\r') {
              inBuffer[inPtr] = '\0';
              inPtr = 0;
              processParam (inBuffer);
            }
            else inBuffer[inPtr++] = (char) inByte;
          } else {
            strcpy (message, "Line too long: ");
            if ((strlen(message)+strlen(url)) < sizeof(message)) strcat (message, url);
          }
          if ((inPtr%20) == 0) delay (20); //play nice in multithreading environment
        }
        inBuffer[inPtr] = '\0';
        processParam (inBuffer);
        closeHttpStream(http);
        // Test for things to be set which should be set
        retVal = true;
        if (image_size == 0) {
          strcpy (message, "No image size specified in metadata");
          retVal = false;
        }
        else if (image_size > get_next_partition_size()) {
          strcpy (message, "Image size exceeds available OTA partition size");
          retVal = false;
        }
        if (newSequence == 0) {
          strcpy (message, "Missing sequence number in metadata");
          retVal = false;
        }
        else if (sequence >= newSequence) {
          strcpy (message, "Image is up to date.");
          retVal = false;
        }
        if (strlen (chksum) == 0) {
          strcpy (message, "Missing SHA256 checksum in metadata");
          retVal = false;
        }
        if (strlen (ota_label) == 0) {
          sprintf (ota_label, "Sequence %d", newSequence);
        }
      }
      return (retVal);
    }

    /*
     * A faily crude parser for earch line of the metadata file
     */
    void processParam (char* inBuffer)
    {
      char *paramName, *paramValue;
      char ptr, lim;

      lim = strlen (inBuffer);
      strcpy (image_name, "esp32.img"); // default value
      if (lim==0) return;
      paramName = NULL;
      paramValue = NULL;
      ptr = 0;
      // Move to first non space character
      while (ptr<lim && (inBuffer[ptr]==' ' || inBuffer[ptr]=='\t')) ptr++;
      paramName = &inBuffer[ptr];
      // Move to the end of the first word
      while (ptr<lim && inBuffer[ptr]!=' ' && inBuffer[ptr]!='\t') ptr++; // move to end of non-space chars
      if (ptr<lim) inBuffer[ptr] = '\0';
      ptr++;
      // Move to the next non-space character
      while (ptr<lim && (inBuffer[ptr]==' ' || inBuffer[ptr]=='\t')) ptr++;
      paramValue = &inBuffer[ptr];
      // Store the data we want
      if (paramValue == NULL || paramName[0] == '#' || strlen(paramName) == 0 || strcmp(paramName, "---") == 0 || strcmp(paramName, "...") == 0) {}  // Ignore comments and empty lines
      else if (strcmp (paramName, "size:") == 0) {
        image_size = atol (paramValue);
      }
      else if (strcmp (paramName, "sequence:") == 0) {
        newSequence = atol (paramValue);
      }
      else if (strcmp (paramName, "sha256:") == 0) {
        if (strlen(paramValue) < sizeof(chksum)) strcpy (chksum, paramValue);
      }
      else if (strcmp (paramName, "name:") == 0) {
        if (strlen(paramValue) < sizeof(image_name)) strcpy (image_name, paramValue);
      }
      else if (strcmp (paramName, "label:") == 0) {
        if (strlen(paramValue) < sizeof(ota_label)) strcpy (ota_label, paramValue);
      }
      else sprintf (message, "Parameter %s not recognised", paramName);
    }

    /*
     * Read the image into OTA update partition
     */
    bool get_ota_image(char *url, const char *cert)
    {
      bool retVal = false;
      const esp_partition_t *targetPart;
      esp_ota_handle_t targetHandle;
      int32_t inByte, totalByte;
      uint8_t *inBuffer;
      char bin2hex[3];
      mbedtls_sha256_context sha256ctx;
      int sha256status, retryCount;

      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("Loading new over the air image");
        xSemaphoreGive(displaySem);
      }
      targetPart = esp_ota_get_next_update_partition(NULL);
      if (targetPart == NULL) {
        sprintf (message, "Cannot identify target partition for update");
        return (false);
      }
      WiFiClient *inStream = getHttpStream(url, cert, http);
      if (inStream != NULL) {
        if (esp_ota_begin(targetPart, image_size, &targetHandle) == ESP_OK) {
          inBuffer = (uint8_t*) malloc (WEB_BUFFER_SIZE);
          mbedtls_sha256_init(&sha256ctx);
          sha256status = mbedtls_sha256_starts_ret(&sha256ctx, 0);
          totalByte = 0;
          retryCount = image_size / WEB_BUFFER_SIZE;
          while (totalByte < image_size && retryCount > 0) {
            inByte = inStream->read(inBuffer, WEB_BUFFER_SIZE);
            if (inByte > 0) {
              if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
                Serial.printf (".");
                xSemaphoreGive(displaySem);
              }
              totalByte += inByte;
              if (sha256status == 0) sha256status = mbedtls_sha256_update_ret(&sha256ctx, (const unsigned char*) inBuffer, inByte);
              esp_ota_write(targetHandle, (const void*) inBuffer, inByte);
            }
            else retryCount--;
            if (inByte < WEB_BUFFER_SIZE && totalByte < image_size) {
              if (inByte < (WEB_BUFFER_SIZE/8)) delay(1000);   // We are reading faster than the server can serve, so slow down
              else if (inByte < (WEB_BUFFER_SIZE/4)) delay(500);  // read rate, rather than just spin through small reads
              else if (inByte < (WEB_BUFFER_SIZE/2)) delay(250);
              else delay(100);
            }
            else delay (10); //play nice in multithreading environment
          }
          if (sha256status == 0) {
            if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
              Serial.printf ("-end-\r\n");
              xSemaphoreGive(displaySem);
            }
            sha256status = mbedtls_sha256_finish_ret(&sha256ctx, (unsigned char*) inBuffer);
            message[0] = '\0'; // Truncate message buffer, then use it as a temporary store of the calculated sha256 string
            for (retryCount=0; retryCount<32; retryCount++) {
              sprintf (bin2hex, "%02x", inBuffer[retryCount]);
              strcat  (message, bin2hex);
            }
          }
          mbedtls_sha256_free (&sha256ctx);
          if (strcmp (message, chksum) == 0) {
            if (esp_ota_end(targetHandle) == ESP_OK) {
              retVal = true;
              sprintf ((char*) inBuffer, "ota_%s", get_next_partition_label());
              if (esp_ota_set_boot_partition(targetPart) == ESP_OK) {
                put_sequence_id(newSequence);
                nvs_put_string ((char*) inBuffer, ota_label);
                strcpy (message, "Reboot to run updated image.");
              }
              else strcpy (message, "Failed to update boot partition ID.");
            }
            else strcpy (message, "Could not finalise writing of over the air update");
          }
          else if (sha256status ==0) strcat (message, " <-- sha256 checksum mismatch");
          else strcpy (message, "Warning: SHA256 checksum not calculated");
          free (inBuffer);
        }
      }
      closeHttpStream (http);
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.println ("\r\nOver the air image - load complete");
        xSemaphoreGive(displaySem);
      }
      return (retVal);
    }


  public:
    ota_control()
    {
      strcpy (message, "No OTA transfer attempted");
      chksum[0] = '\0';
      image_size = 0;
    }

    /*
     * Use the base URL to get
     *   1. Metadata about the update file
     *   2. The binary package
     * 
     * Fail if: 
     *   1. either meta data or image do not exist, or
     *   2. existing image same or newer the offered package or
     *   3. insufficient space for storing image
     *   4. image transfer failed or mismatches sha256 checksum
     *   5. cannot setup connection to server
     */
    bool update(const char *baseurl, const char *cert, const char *metadata)
    {
      bool retVal = false;
      char url[132];

      strcpy (url, baseurl);
      strcat (url, metadata);
      if (sequence == 0) get_sequence_id();
      if (get_meta_data(url, cert)) {
        strcpy (url, baseurl);
        strcat (url, image_name);
        if (get_ota_image(url, cert)) retVal = true;
      }
      return (retVal);
    }

    /*
     * Newer ESP IDE may support rollback options
     * We'll assume we have 2 OTA type partitions and if update has been
     * run previously then roll back can toggle boot partition between the two.
     * Id last sequence is zero then return with an error
     */
    bool revert()
    {
      bool retVal = false;
      if (sequence == 0) get_sequence_id();
      // if (esp_ota_check_rollback_is_possible()) {
      if (sequence > 0) {
        // esp_ota_mark_app_invalid_rollback_and_reboot();
        esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
        strcpy (message, "Reboot to run previous image.");
        retVal = true;
      }
      else {
        strcpy (message, "No previous image to roll back to");
      }
      return (retVal);
    }

    /*
     * Return data about the current image.
     */
    const char* get_boot_partition_label()
    {
       const esp_partition_t *runningPart;
       
       runningPart = esp_ota_get_running_partition();
       return (runningPart->label);
    }

    uint32_t get_boot_partition_size()
    {
       const esp_partition_t *runningPart;
       
       runningPart = esp_ota_get_running_partition();
       return (runningPart->size);      
    }

    /*
     * Get data about the next partition
     */
    const char* get_next_partition_label()
    {
       const esp_partition_t *nextPart;
       
       nextPart = esp_ota_get_next_update_partition(NULL);
       return (nextPart->label);
    }

    uint32_t get_next_partition_size()
    {
       const esp_partition_t *nextPart;
       
       nextPart = esp_ota_get_next_update_partition(NULL);
       return (nextPart->size);      
    }

    /*
     * Get verbal description of failed OTA update.
     */
    char* get_status_message()
    {
      return (message);
    }
};


void OTAcheck4update(char* retVal)
{
  ota_control theOtaControl;
  char ota_url[120];
  char web_certFile[42];

  if ((!APrunning) && (!WiFi.status() == WL_CONNECTED)) return;
  nvs_get_string ("ota_url", ota_url, OTAUPDATE, sizeof(ota_url));
  nvs_get_string ("web_certFile", web_certFile, CERTFILE, sizeof(web_certFile));
  if (strncmp (ota_url, "https://", 8) == 0) {
    rootCACertificate = util_loadFile(SPIFFS, web_certFile);
    if (rootCACertificate == NULL) {
      if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
        Serial.print ("When using https for OTA, place root certificate in file ");
        Serial.println (CERTFILE);
        xSemaphoreGive(displaySem);
      }
    }
  }
  else if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
    Serial.println ("WARNING: using unencrypted link for OTA update. https:// is preferred.");
    xSemaphoreGive(displaySem);
  }
  if (theOtaControl.update (ota_url, rootCACertificate, "metadata.php")) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (theOtaControl.get_status_message());
      Serial.println ("OTA update successful, will reboot.");
      xSemaphoreGive(displaySem);
    }
    nvs_put_int ("ota_initial", 1);
    esp_restart();
  }
  else {
    // Do not reboot if OTA area is not updated
    Serial.println (theOtaControl.get_status_message());
  }
  if (retVal != NULL) strcpy (retVal, theOtaControl.get_status_message());
}

void OTAcheck4rollback()
{
  ota_control theOtaControl;
  if (theOtaControl.revert ()) {
    if (xSemaphoreTake(displaySem, pdMS_TO_TICKS(TIMEOUT)) == pdTRUE) {
      Serial.println (theOtaControl.get_status_message());
      Serial.println ("OTA revert successful, will reboot.");
      xSemaphoreGive(displaySem);
    }
    esp_restart();
  }
  else {
    // Do not reboot if OTA area is not updated
    Serial.println (theOtaControl.get_status_message());
  }
}

const char* OTAstatus()
{
  static char message[200];
  char msgBuffer[80];
  char web_certFile[42];
  
  ota_control theOtaControl;
  nvs_get_string ("ota_url", msgBuffer, OTAUPDATE, sizeof(msgBuffer));
  nvs_get_string ("web_certFile", web_certFile, CERTFILE, sizeof(web_certFile));
  sprintf (message, " * boot partition: %s, next partition: %s, partition size: %d\r\n   ota URL: ", theOtaControl.get_boot_partition_label(), theOtaControl.get_next_partition_label(), theOtaControl.get_next_partition_size());
  strcat  (message, msgBuffer);
  strcat  (message, "\r\n   ota cert file: ");
  strcat  (message, web_certFile);
  return (message);
}

#endif
