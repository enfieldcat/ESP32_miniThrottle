void processDccPacket (char *packet)
{
  if (strncmp (packet, "<t ", 3) == 0) dccSpeedChange (&packet[3]);
  else if (strncmp (packet, "PPA", 3) == 0) dccPowerChange (packet[3]);
  else if (strncmp (packet, "<p",  2) == 0) dccPowerChange (packet[2]);
  else if (strncmp (packet, "<* ", 3) == 0) dccComment (&packet[3]);
}

void dccSpeedChange(char* speedSet)
{
  int dccRegister = 1;
  int dccSpeed = 0;
  int dccDirection = 0;
  int maxLocoArray = locomotiveCount + MAXCONSISTSIZE;
  char *token;
  char *remain = speedSet;

  int len = strlen (speedSet);
  while (speedSet[len-1]=='>' || speedSet[len-1]=='*' || speedSet[len-1]==' ') len--;
  speedSet[len] = '\0';
  token = strtok_r (remain, " ", &remain);
  dccRegister = util_str2int(token);
  token = strtok_r (remain, " ", &remain);
  dccSpeed = util_str2int(token);
  token = strtok_r (remain, " ", &remain);
  dccDirection = util_str2int(token);
  for (uint8_t ptr=0; ptr<maxLocoArray; ptr++) {
    if (locoRoster[ptr].owned) {
      locoRoster[ptr].speed = dccSpeed;
      if (dccDirection == 1) locoRoster[ptr].direction = FORWARD;
      else locoRoster[ptr].direction = REVERSE;
    }
  }
}

void dccPowerChange(char state)
{
  if (state == '1') {
    trackPower = true;
    #ifdef TRACKPWR
    digitalWrite(TRACKPWR, HIGH);
    #endif
    #ifdef TRACKPWRINV
    digitalWrite(TRACKPWRINV, LOW);
    #endif
  }
  else {
    trackPower = false;
    #ifdef TRACKPWR
    digitalWrite(TRACKPWR, LOW);  // Off or unknown
    #endif
    #ifdef TRACKPWRINV
    digitalWrite(TRACKPWRINV, HIGH);  // Off or unknown
    #endif
  }
}

void dccComment(char* comment)
{
  int len = strlen (comment);

  // drop trailing chars
  while (comment[len-1]=='>' || comment[len-1]=='*' || comment[len-1]==' ') len--;
  if (len>(sizeof(lastMessage)-1)) len=sizeof(lastMessage)-1;
  comment[len] = '\0';
  // drop LCDx: leader
  len = 0;
  while (comment[len]!=':' && len<strlen(comment)) len++;
  if (comment[len] == ':') len++;
  else len = 0;
  // copy to lastMessage buffer
  strcpy (lastMessage, &comment[len]);
}

void dccPopulateLoco()
{
  for (uint8_t tokenPtr=0; tokenPtr<MAXCONSISTSIZE; tokenPtr++) {
    sprintf (locoRoster[tokenPtr].name, "ad-hoc-%d", tokenPtr+1);
    locoRoster[tokenPtr].direction = STOP;
    locoRoster[tokenPtr].speed     = 0;
    locoRoster[tokenPtr].steps     = 128;
    locoRoster[tokenPtr].id        = 0;
    locoRoster[tokenPtr].owned     = false;
    locoRoster[tokenPtr].function  = 0;
    locoRoster[tokenPtr].throttleNr= 255;
  }
}
