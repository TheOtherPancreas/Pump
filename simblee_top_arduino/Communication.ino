

const int SALT_LENGTH = 4;
const int OPCODE_CRC_LENGTH = 3;
char salt[SALT_LENGTH];

extern bool verifyCRC(char* data, int len);
extern float decodeDose(short val);
extern void encodeDose(float dose, char* buf, int offset);

void SimbleeBLE_onAdvertisement(bool start)
{
  // turn the green led on if we start advertisement, and turn it
  // off if we stop advertisement
  if (start)
    Serial.println( "Advertising Start" );
  else
    Serial.println( "Advertising Stop" );
}

void SimbleeBLE_onConnect()
{
  Serial.println( "onConnect" );
}

void disconnect() {
  SimbleeBLE.send((char)0x09);
}


void SimbleeBLE_onDisconnect()
{
  Serial.println( "onDisconnect" );
  SimbleeBLE.advertisementInterval = 2500;
}

void SimbleeBLE_onReceive(char *data, int len) {
  Serial.println("SimbleeBLE_onReceive()");
  if (len <= 0)
    return;
  
  Serial.print("onReceive: ");
  for (int i = 0; i < len; i++) {
    Serial.print((int)data[i], HEX);
    Serial.print(",");
  }
  Serial.println();

  if (data[0] == 0x00) {
    onAck();
    return;
  }
  if (handleNonEncryptedRequests(data, len))
    return;
  handleEncryptedRequests(data, len);
}

bool handleNonEncryptedRequests(char *data, int len) {
  int opcode = data[0];

  switch (opcode) {
    case 0x01:
      onRequestHash();
      return true;
    case 0xBC:
      onBasalModelReceipt(data, len);
      return true;
    case 0xFF:
      if (setupMode)
        return false;
      if (millis() < 10000) {
        setupMode = true;
        return false;
      }
      break;
    default:
      return false;
  }
}

void handleEncryptedRequests(char *data, int len) {
  
  char *decoded = decodeData(data, len);
  if (decoded == NULL) {
    Serial.println("Failed to decode");
    return;
  }
  int opcode = data[0];

  if (setupMode) {
    Serial.println("handleSetupPacket()");
    handleSetupPacket(opcode, decoded, len);
  }
  else {
    handleDecryptedPacket(opcode, decoded, len);
    extendConnectionExpiration();
  }
  initialized = true;
  delete[] decoded;
}
  
void handleDecryptedPacket(int opcode, char *decoded, int len) {
  
  switch (opcode) {
    case 0xBA:
      onBasalInitiateTransaction(decoded, len);
      break;
    case 0xBC:
      onBasalInitiateTransaction(decoded, len);
      break;
    case 0xBE:
      onBasalActivate(decoded, len);
      break;
    case 3:
      onDoseCommand(decoded, len, false);
      break;
    case 0x33:
      onDoseCommand(decoded, len, true);
      break;
      
    case 5:
      onRequestPreviousInjection(decoded, len);
      break;

    case 7:
      onClearOcclusion();
      break;

    case 9:
      onResetEncoder(decoded, len);
      break;

    case 11:
      onSleepFor(decoded, len);
      break;

    case 13:
      onStayAwake(decoded, len);
      break;

    case 15:
      onMotorPowerLevel(decoded, len);
      break;

    case 17:
      sendPumpState();
      break;

    case 19:
      onConfirmBasal(decoded, len);
      break;

    case 21:
      onClearEncoderError();
      break;
    
    case 0x97:
      onDriveCommand(decoded, len);
      break;

    case 0xDD:
      doRemoteSelfTest();
      break;
      
    case 0xDC:
      doRemoteCalibration();
      break;

  }      
  
  
}

void onFailedSetup() {
  char resp[1] = {1};
  sendResponse(0xFF, resp, 1);
}

void onSuccessfulSetup() {
  char resp[1] = {0};
  sendResponse(0xFF, resp, 1);
}

void onAck() {
   char resp[3] = {0, 0, 0};
   SimbleeBLE.send(resp, 3);
}





void onSleepFor(char* decoded, int len) {
  long sleepForMillis = *(long*) (decoded);
  sleepFor(sleepForMillis);
  char empty[0];
  sendResponse(12, empty, 0);
}

void onStayAwake(char* decoded, int len) {
  long awakeForMillis = *(long*) (decoded);
  setConnectionExpiration(millis() + awakeForMillis);
  char empty[0];
  sendResponse(14, empty, 0);
}

void onResetEncoder(char* decoded, int len) {
  int encoderIndex = *(int*) (decoded);
  motor[encoderIndex]->reset();
  char empty[0];
  sendResponse(10, empty, 0);
}

void onRequestPreviousInjection(char* decoded, int len) {
  char verificationId = decoded[0];
  if (lastDose.verificationId != verificationId) {
    for (int i = 0; i < 3; i++)
      lastDose.injection[i] = 0.0f;
    lastDose.verificationId = verificationId;
  }  
  sendLastDose();
}


void onClearOcclusion() {
  for (int i=0; i < 3; i++)
    motor[i]->clearOcclusion();
  char empty[0];
  sendResponse(8, empty, 0);
}

void onClearEncoderError() {
  for (int i=0; i < 3; i++)
    motor[i]->clearEncoderError();
  char empty[0];
  sendResponse(22, empty, 0);
}


/*
 * 
 * Tx Opcode 18
 * +------------------+---+----------------------+---+----------------------+---------------------------+---+---+---+----+----+
 * | 0                | 1 | 2                    | 3 | 4                    | 5 | 6                     | 7 | 8 | 9 | 10 | 11 |
 * +------------------+---+----------------------+---+----------------------+---------------------------+---+---+---+----+----+
 * | verification_id  | unreported_basal_insulin | unreported_basal_symlin  | unreported_basal_glucagon |
 * +------------------+--------------------------+--------------------------+---------------------------+
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * +------------------+--------------------------+--------------------------+---------------------------+
 */
void sendPumpState() {
  char bytes[7];
  bytes[0] = lastDose.verificationId;

  encodeDose(basalModel[0]->getUnreportedDose(), bytes, 1);
  encodeDose(basalModel[1]->getUnreportedDose(), bytes, 3);
  encodeDose(basalModel[2]->getUnreportedDose(), bytes, 5);
  
  Serial.print("Synchronizing phone state: ");
  printHex(bytes, 7);
  
  sendResponse(18, bytes, 7);
}

/*
 * Rx Opcode 19
 * +---+------------------------+---+-----------------------+---+-------------------------+
 * | 0 | 1                      | 2 | 3                     | 4 | 5                       |
 * +----------------------------+---+-----------------------+---+-------------------------+
 * | insulin_basal_confirmation | symlin_basal_confirmation | glucagon_basal_confirmation |
 * +----------------------------+---------------------------+-----------------------------+
 * |                            |                           |                             |
 * |                            |                           |                             |
 * |                            |                           |                             |
 * |                            |                           |                             |
 * |                            |                           |                             |
 * |                            |                           |                             |
 * |                            |                           |                             |
 * |                            |                           |                             |
 * +----------------------------+---------------------------+-----------------------------+
 */
void onConfirmBasal(char* decoded, int len) {
  short encodedDoses[3];
  encodedDoses[0] = bytesToShort(decoded[0], decoded[1]);
  encodedDoses[1] = bytesToShort(decoded[2], decoded[3]);
  encodedDoses[2] = bytesToShort(decoded[4], decoded[5]);

  float dose[3];
  for (int i = 0; i < 3; i++) {
      dose[i] = 0;
  }
  
  for (int i = 0; i < 3; i++) {
    dose[i] = decodeDose(encodedDoses[i]);
    basalModel[i]->confirmDose(dose[i]);
  }    

  sendPumpState();
}


/*
 * +---+--------------+---+-------------+---+---------------+------------------------------------+----------------+---+-----------+----+---------------------+
 * | 0 | 1            | 2 | 3           | 4 | 5             | 6                                  | 7              | 8 | 9         | 10 | 11                  |
 * +------------------+---+-------------+---+---------------+------------------------------------+----------------+---+-----------+----+---------------------+
 * | insulin_injected | symlin_injected | glucagon_injected | bitwiseFlags                       | verificationId | batteryLevel  | unreported_basal_insulin |
 * +------------------+-----------------+-------------------+------------------------------------+----------------+---------------+--------------------------+
 * |                  |                 |                   | 0b00000001-insulinOcclusion        |                |               |                          | 
 * |                  |                 |                   | 0b00000010-symlinOcclusion         |                |               |                          | 
 * |                  |                 |                   | 0b00000100-glucagonOcclusion       |                |               |                          |
 * |                  |                 |                   | 0b00001000-unreportedBasalInsulin  |                |               |                          |
 * |                  |                 |                   | 0b00010000-unreportedBasalSymlin   |                |               |                          |
 * |                  |                 |                   | 0b00100000-unreportedBasalGlucagon |                |               |                          |
 * |                  |                 |                   | 0b01000000-insulinEncoderError     |                |               |                          |
 * |                  |                 |                   | 0b10000000-symlinEncoderError      |                |               |                          |
 * +------------------+-----------------+-------------------+------------------------------------+----------------+---------------+--------------------------+
 */
void sendLastDose() {

  char bytes[12];

  for (int i=0; i < 12; i++) {
    bytes[i] = 0;
  }

  encodeDose(lastDose.injection[0], bytes, 0);
  encodeDose(lastDose.injection[1], bytes, 2);
  encodeDose(lastDose.injection[2], bytes, 4);

  int bitwise = 0;
  if (motor[0]->isOccluded())
    bitwise |= 0b00000001;
  if (motor[1]->isOccluded())
    bitwise |= 0b00000010;
  if (motor[2]->isOccluded())
    bitwise |= 0b00000100;
  
  if (basalModel[0]->hasUnreportedDose())
    bitwise |= 0b00001000;
  if (basalModel[1]->hasUnreportedDose())
    bitwise |= 0b00010000;
  if (basalModel[2]->hasUnreportedDose())
    bitwise |= 0b00100000;
  
  if (motor[0]->isEncoderError())
    bitwise |= 0b01000000;
  if (motor[1]->isEncoderError())
    bitwise |= 0b10000000;
  
    
  bytes[6] = bitwise;

  bytes[7] = lastDose.verificationId;  

  encodeBatteryVoltage(getBatteryVoltage(), bytes, 8);

  encodeDose(basalModel[0]->getUnreportedDose(), bytes, 10);
  

  Serial.print("Last dose turned into byte array: ");
  for (int i = 0; i < 12; i++) {
    Serial.print((int)bytes[i], HEX);
    Serial.print(",");
  }
  Serial.println();
  
  sendResponse(4, bytes, 12);
}



void onDriveCommand(char *decoded, int len) {
  int motorIndex = decoded[0];
  char direction = decoded[1];
  if (direction == 0) {
    motor[motorIndex]->stop();
    motor[motorIndex]->reset();
  }    
  else {
    float speedPercent = decodePowerLimit(decoded[2]);
    bool forward = (direction == 1);
    if (forward) {
      return; //never drive forward
    }
    motor[motorIndex]->drive(forward, speedPercent);
  }
}


/*
 * 
 * +------------+----+---+-------+---+-------+---+----------------------+----------------+-----------------------------+----+----+  
 * | 0          | 1  | 2 | 3     | 4 | 5     | 6 | 7                    | 8              | 9                           | 10 | 11 |
 * +------------+----+---+-------+---+-------+---+----------------------+----------------+-----------------------------+----+----+
 * | motorIndex | amount | power | tempBasal | tempBasalDurationMinutes | verificationId | bitwiseFlags                | 
 * +------------+--------+-------+-----------+--------------------------+----------------+-----------------------------+
 * |            |        |       |           |                          |                | 0b00000001-updateTempBasal  |
 * |            |        |       |           |                          |                | 0b00000010-tempBasalActive  |
 * |            |        |       |           |                          |                | 0b00000100-tempBasalPercent |
 * |            |        |       |           |                          |                | 0b00001000-                 |
 * |            |        |       |           |                          |                | 0b00010000-                 |
 * |            |        |       |           |                          |                | 0b00100000-                 |
 * |            |        |       |           |                          |                | 0b01000000-                 |
 * |            |        |       |           |                          |                | 0b10000000-unsafe           |
 * +------------+--------+-------+-----------+--------------------------+----------------+-----------------------------+
 * 
 * +---+-----+---+----+--------------+-------------+----------------+------------------------+---+-------+----+---------+  
 * | 0 | 1   | 2 | 3  | 4            | 5           | 6              | 7                      | 8 | 9     | 10 | 11      |
 * +---+-----+---+----+--------------+-------------+----------------+------------------------+---+-------+----+---------+
 * | insulin | symlin | insulinPower | symlinPower | verificationId |      bitwiseFlags      | tempBasal | tempDuration |
 * +---------+--------+--------------+-------------+----------------+------------------------+-----------+--------------+
 * |         |        |              |             |                | 0x  1-safe             |           |              |
 * |         |        |              |             |                | 0x  2-tempBasal active |           |              |
 * |         |        |              |             |                | 0x  4-tempBasalPercent |           |              |
 * |         |        |              |             |                | 0x  8-                 |           |              |
 * |         |        |              |             |                | 0x 16                  |           |              |
 * |         |        |              |             |                | 0x 32                  |           |              |
 * |         |        |              |             |                | 0x 64                  |           |              |
 * |         |        |              |             |                | 0x128                  |           |              |
 * +---------+--------+--------------+-------------+----------------+------------------------+-----------+--------------+
 * 
 * 90 01      00 00       00 00     01              00                            00 00         00 00 
 * +---+-----+---+-----+---+------+----------------+-----------------------------+---+-------+----+----------------+  
 * | 0 | 1   | 2 | 3   | 4 | 5    | 6              | 7                           | 8 | 9     | 10 | 11             |
 * +---+-----+---+-----+---+------+----------------+-----------------------------+---+-------+----+----------------+
 * | dose[0] | dose[1] | dose[2]  | verificationId |      bitwiseFlags           | tempBasal | tempDurationMinutes |
 * +---------+---------+----------+----------------+-----------------------------+-----------+---------------------+
 * |(insulin)|(symlin) |(glucogon)|                | 0b00000001-basalIndex bit1  |           | 0xFF == forever     |
 * |         |         |          |                | 0b00000010-basalIndex bit2  |           |                     |
 * |         |         |          |                | 0b00000100-tempBasalActive  |           |                     |
 * |         |         |          |                | 0b00001000-tempBasalPercent |           |                     |
 * |         |         |          |                | 0b00010000                  |           |                     |
 * |         |         |          |                | 0b00100000                  |           |                     |
 * |         |         |          |                | 0b01000000                  |           |                     |
 * |         |         |          |                | 0b10000000-unsafe           |           |                     |
 * +---------+---------+----------+----------------+-----------------------------+-----------+---------------------+
 */
void onDoseCommand(char *decoded, int len, bool manual) {
  if (!manual) {
    setWakeTime(millis() + MINUTES(5) + SECONDS(30));
  }
  delay(1000);

  char flags = decoded[7];
  bool unsafe = flags & 0b10000000;


  int basalIndex =          flags & 0b00000011;
  bool tempBasalActive =    flags & 0b00000100;
  bool tempBasalPercent =   flags & 0b00001000;  
  
  float tempBasal = decodeDose(bytesToShort(decoded[8], decoded[9]));
  char tempDuration = bytesToShort(decoded[10], decoded[11]);
  if (basalIndex < 3)
    basalModel[basalIndex]->updateTempBasal(tempBasalActive, tempBasalPercent, tempBasal, tempDuration);
  

  char verificationId = decoded[6];
  
  short encodedDoses[3];
  encodedDoses[0] = bytesToShort(decoded[0], decoded[1]);
  encodedDoses[1] = bytesToShort(decoded[2], decoded[3]);
  encodedDoses[2] = bytesToShort(decoded[4], decoded[5]);

  float dose[3];
  float injection[3];
  for (int i = 0; i < 3; i++) {
      injection[i] = 0;
      dose[i] = 0;
  }
  
  for (int i = 0; i < 3; i++) {
    dose[i] = decodeDose(encodedDoses[i]);
    Serial.print("dose: ");
    Serial.println(dose[i]);
    
    
    if (dose[i] > 0) 
    {
      Serial.print("motor[");
      Serial.print(i);
      Serial.print("] occluded: ");
      if (motor[i]->isOccluded())
        Serial.print("true");
      else
        Serial.print("false");

      if (motor[i]->isEncoderError())
        Serial.println("\tencoderError: true");
      else
        Serial.println("\tencoderError: false");
        
      if ((!motor[i]->isOccluded() && !motor[i]->isEncoderError()) || unsafe) {
        Serial.println("*******************************************let's dose");
        injection[i] = motor[i]->dose(dose[i], !unsafe);
      }
      else {
        Serial.println("*********************************************condition failed");
      }
    }
  }

  Serial.print("insulin:");
  Serial.println(injection[0]);
  Serial.print("symlin:");
  Serial.println(injection[1]);
  Serial.print("glucogon: " );
  Serial.println(injection[2]);
  Serial.print("verifictionId:");
  Serial.println(verificationId);

  for (int i = 0; i < 3; i++)
    lastDose.injection[i] = injection[i];
  lastDose.verificationId = verificationId;

  sendLastDose();
}

void onBasalInitiateTransaction(char* decoded, int len) { //0xBA
  int motorIndex = decoded[0];
  
  basalMessageProcessor.initiateTransaction(basalModel[motorIndex]);
  sendResponse(0xBB);
}


void onBasalModelReceipt(char* decoded, int len) { //0xBC

  basalMessageProcessor.receiveChunk(decoded, len);
  sendResponse(0xBD);
}


/*
 * +---+------------------+---+------------------------------------+---+---+---+---+
 * | 0 | 1                | 2 | 3                                  | 4 | 5 | 6 | 7 |
 * +---+------------------+---+------------------------------------+---+---+---+---+
 * | minutesSinceMidnight | howManySecondsFromNowShouldPumpDoBasal | expectedHash  |
 * +----------------------+----------------------------------------+---------------+
 */
void onBasalActivate(char* decoded, int len) { //0xBE
    long now = millis();
    short minutesSinceMidnight = bytesToShort(decoded[0], decoded[1]);
    long millisSinceMidnight = minutesSinceMidnight * MINUTES(1);
    ClientTime::getInstance().updateClientTime(millisSinceMidnight);
    
    short howManySecondsFromNowShouldPumpDoBasal = bytesToShort(decoded[2], decoded[3]);
    int expectedHash = bytesToInt(decoded, 4);

    long wakeTime = basalMessageProcessor.finalizeTransaction(howManySecondsFromNowShouldPumpDoBasal, expectedHash);
    bool hashesMatched = wakeTime != 0;

    char bytes[1];
    if (hashesMatched) {
      setWakeTime(wakeTime); 
      bytes[0] = 1;
      
    }
    else {
      bytes[0] = 0;      
    }
    sendResponse(0xBF, bytes, 1);
}

void onMotorPowerLevel(char* decoded, int len) {
  int motorIndex = decoded[0];
  char powerLevel = decoded[1];
  Settings::getInstance().setPowerLevel(motorIndex, powerLevel);
  Settings::getInstance().save();
  char bytes[1] = {1};
  sendResponse(16, bytes, 1);
}

















void sendResponse(char opcode) {
  char empty[0];
  sendResponse(opcode, empty, 0);
}

void sendResponse(char opcode, char *data, int dataLength) {

  int payloadLength = dataLength;
  
  //make it divisible by 16
  if (payloadLength % 16 != 0) {
    payloadLength = (payloadLength/16)*16 + 16;
  }
  
  int totalPayloadLength = payloadLength+3; //add opcode and CRC

  char padded[payloadLength];
  char encrypted[payloadLength];


  //copy serialized bytes into the padded array that will get encrypted
  for (int i = 0; i < payloadLength; i++) {
    if (i >= dataLength) {
      padded[i] = 0;
    }
    else {
      padded[i] = data[i];
    }
  }
    

  if(dataLength > 0)
    AES128_ECB_encrypt((unsigned char*)padded, (const unsigned char*)Settings::getInstance().getSecurityKey(), (unsigned char*)encrypted);



  char result[totalPayloadLength];
  result[0] = opcode;
  for (int i = 0; i < payloadLength; i++) {
    result[i+1] = encrypted[i];
  }
  
  short crcd = crc(result, 0, totalPayloadLength-2); //include opcode in crc
  Serial.println((int)crc, HEX);
  
  result[totalPayloadLength-2] = byteTwoOfShort(crcd);
  result[totalPayloadLength-1] = byteOneOfShort(crcd);

  
  Serial.print("Response about to send: ");
  for (int i = 0; i < totalPayloadLength; i++) {
    Serial.print((int)result[i], HEX);
    Serial.print(",");
  }
  Serial.println();
   
  SimbleeBLE.send(result, totalPayloadLength); 
}

char* decodeData(char *data, int len) {
  if ((len - 3) % 16 != 0)
    return NULL;
  int payloadLength = len - 3;
    
  char encrypted[payloadLength];
  char decrypted[payloadLength];
  

  short crc = bytesToShort(data[len-1], data[len-2]);
  if(!verifyCRC(data, len)) {
    Serial.println("CRC doesn't match");
    return NULL;
  }
  
  for (int i = 0; i < payloadLength; i++) {
    encrypted[i] = data[i+1]; //+1 because opcode isnt' encrypted
  }
  
  AES128_ECB_decrypt((unsigned char*)encrypted, (const unsigned char*)Settings::getInstance().getSecurityKey(), (unsigned char*)decrypted);


  Serial.print("encrypted: ");
  for (int i = 0; i < payloadLength; i++) {
    Serial.print((int)encrypted[i], HEX);
    Serial.print(",");
  }
  Serial.println();

  Serial.print("decrypted: ");
  for (int i = 0; i < payloadLength; i++) {
    Serial.print((int)decrypted[i], HEX);
    Serial.print(",");
  }
  Serial.println();

  Serial.print("Salt: ");
  for (int i = 0; i < SALT_LENGTH; i++) {
    Serial.print((int)salt[i], HEX);
    Serial.print(",");
  }
  Serial.println();

  
  bool success = true;
  for (int i = 0; i < SALT_LENGTH; i++) {
    if(salt[i] != decrypted[i]) {
      Serial.println(i);
      success = false;
      break;
    }
  }
  Serial.print("salts matched: ");
  Serial.println(success);

  char* content = new char[payloadLength - SALT_LENGTH];
  for (int i = 0; i < payloadLength - SALT_LENGTH; i++) {
    content[i] = decrypted[i+SALT_LENGTH];
  }

  delay(100);
  
  if (success)
    return content;
  else
    return NULL;
}


/*
 *    0    1 - 4      5      6-7
 * ------+--------+-------+----------------------------------------------------------
 * opcode| seed   | flags | CRC
 * ------+--------+-------+----------------------------------------------------------
 *    1     4         1      2
 */
void onRequestHash() {
  //create seed
  Serial.println("onRequestHash()");
  const int FLAG_LENGTH = 1;
  char result[SALT_LENGTH + FLAG_LENGTH + OPCODE_CRC_LENGTH];
  result[0] = 0x02;
  for (int i = 0; i < SALT_LENGTH; i++) {
    salt[i] = random(256); 
    result[i+1] = salt[i];
  }

  int flag = basalMessageProcessor.getState();
  result[SALT_LENGTH + 1] = flag;
  
  //add the CRC here
  short calculated = crc(result, 0, 2 + SALT_LENGTH);

  Serial.println((int)calculated, HEX);
  
  result[SALT_LENGTH + 2] = (char)calculated;
  result[SALT_LENGTH + 3] = (char)(calculated >> 8);
  Serial.println((int)result[SALT_LENGTH + 2], HEX);
  Serial.println((int)result[SALT_LENGTH + 3], HEX);
  

  Serial.print("Salt - about to send packet: ");
  for (int i = 0; i < 1+SALT_LENGTH+OPCODE_CRC_LENGTH; i++) {
    Serial.print((int)result[i], HEX);
    Serial.print(",");
  }
  Serial.println();
  
  SimbleeBLE.send(result, SALT_LENGTH + OPCODE_CRC_LENGTH + 1);
  
  Serial.println("response sent");
}



/*
 * +---+---+---+---+---+---+---+---+---+---+----+----+
 * | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
 * +---+---+---+---+---+---+---+---+---+---+----+----+
 */
