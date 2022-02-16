/*
 * Send initial data after protocol detection
 */
void setInitialData()
{
  if (!initialDataSent) {
    initialDataSent = true;  // Only send once per connection
    if (cmdProtocol == JMRI) {
      uint64_t chipid = ESP.getEfuseMac();
      char myID[15];

      // Send identifying data
      sprintf (myID, "HU%02X%02X%02X%02X%02X%02X",(uint8_t)chipid, (uint8_t)(chipid>>8), (uint8_t)(chipid>>16), (uint8_t)(chipid>>24), (uint8_t)(chipid>>32), (uint8_t)(chipid>>40));
      txPacket (myID);
      txPacket ("N", tname);
    }
  }
}

/*
 * Set track power state
 * 1 = On
 * 2 = Off
 * Other = no action
 */
void setTrackPower (uint8_t desiredState)
{
  
  switch (desiredState) {
    case 1:
      if (cmdProtocol == JMRI) txPacket ("PPA1");
      break;
    case 2:
      if (cmdProtocol == JMRI) txPacket ("PPA0");
      break;
  }
}

void setTurnout (uint8_t turnoutNr, char desiredState)
{
  char outPacket[21];
  //char string[6];
  uint8_t reqState;
 
  reqState = turnoutState[desiredState].state;
  if (cmdProtocol == JMRI) {
    strcpy (outPacket, "PTA");
    if (reqState == 2) outPacket[3] = 'C';
    else if (reqState == 4) outPacket[3] = 'T';
    else outPacket[3] = '2';
    outPacket[4] = '\0';
    //if (nvs_get_int ("switchRef", 0) == 1) { //Reference by Name
    //  strcat (outPacket, turnoutList[turnoutNr].userName);
    //}
    //else {                                   // Reference by numeric ID
      //sprintf (string, "%d", turnoutList[turnoutNr].id);
      strcat (outPacket, turnoutList[turnoutNr].sysName);
    //}
    txPacket (outPacket);
  }
}

void setRoute (uint8_t routeNr)
{
  char outPacket[21];
  if (cmdProtocol == JMRI) {
    sprintf (outPacket, "PRA2%s", routeList[routeNr].sysName);
    txPacket (outPacket);
  }
}

void setLocoFunction (uint8_t locoIndex, uint8_t funcIndex, bool set)
{
  char commandPacket[40];
  if (cmdProtocol == JMRI) {
    char setVal = '0';

    if (set) setVal = '1';
    sprintf (commandPacket, "M%cA%c%d<;>F%c%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, setVal, funcIndex);
    txPacket (commandPacket);
  }
}

void setLocoOwnership(uint8_t locoIndex, bool owned)
{
  char commandPacket[40];
  if (cmdProtocol == JMRI) {
    char setVal = '-';

    if (owned) setVal = '+';
    sprintf (commandPacket, "M%c%c%c%d<;>%s", locoRoster[locoIndex].throttleNr, setVal, locoRoster[locoIndex].type, locoRoster[locoIndex].id, locoRoster[locoIndex].name);
    txPacket (commandPacket);
  }  
}

void setStealLoco(uint8_t locoIndex)
{
  char commandPacket[40];
  if (cmdProtocol == JMRI) {
    sprintf (commandPacket, "M%cS%c%d<;>%s", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, locoRoster[locoIndex].name);
    txPacket (commandPacket);
  }  
}


void setLocoSpeed (uint8_t locoIndex, uint8_t speed)
{
  char commandPacket[40];
  if (cmdProtocol == JMRI) {
    sprintf (commandPacket, "M%cA%c%d<;>V%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, speed);
    txPacket (commandPacket);
  }  
}


void setLocoDirection (uint8_t locoIndex, uint8_t direction)
{
  char commandPacket[40];
  if (cmdProtocol == JMRI) {
    if (direction != STOP) {
      int dirFlag = 1;

      if (direction == REVERSE) dirFlag = 0;
      sprintf (commandPacket, "M%cA%c%d<;>R%d", locoRoster[locoIndex].throttleNr, locoRoster[locoIndex].type, locoRoster[locoIndex].id, dirFlag);
      txPacket (commandPacket);
    }
  }
}


void setDisconnected()
{
  if (cmdProtocol == JMRI) {
    txPacket ("Q"); // quit
  }
}
