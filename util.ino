int util_str2int(char *string)
{
  char *tptr;
  long result;
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
