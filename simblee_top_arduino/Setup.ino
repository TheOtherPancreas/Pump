

void handleSetupPacket(int opcode, char *decoded, int len) {

  switch (opcode) {
    case 0xFF:
      onSetupInit();
      break;
    case 0xFE:
      onEndSetup();
      break;
      
//    case 0xFD:
//      onDriveCommand(decoded, len);
//      break;
//    case 0xFC:
//      onFindCenter(decoded, len);
//      break;
//    case 0xFB:
//      onPreciseCenterResponse(decoded, len);
//      break;

    case 0xF0:
      onPasswordChange(decoded, len, true);
      break;
    case 0xF1:
      onPasswordChange(decoded, len, false);
      break;
  }

}

void onSetupInit() {
  char resp[1] = {0};
  sendResponse(0xFF, resp, 1);
}

void onEndSetup() {
  setupMode = false;
  char resp[1] = {0};
  sendResponse(0xFE, resp, 1);
}

void onSaveSettings() {
  Settings::getInstance().save();
}

char tempPass[16];

void onPasswordChange(char *decoded, int len, bool isFirstHalf) {
  Serial.println("Password change");
  if (isFirstHalf) {
    for (int i = 0; i < 8; i++) {
      tempPass[i] = decoded[i];  
    }
    sendResponse(0xF0);
  }
  else {
    for (int i = 0; i < 8; i++) {
      tempPass[i+8] = decoded[i];  
    }
    Settings::getInstance().setSecurityKey(tempPass);
    Settings::getInstance().save();
    Serial.println(Settings::getInstance().getSecurityKey());
    sendResponse(0xF1);
  }
}

void onUpdatePassword(char *decoded, int len) {
  Settings::getInstance().setSecurityKey(decoded);
  Settings::getInstance().save();
  sendResponse(0xFE);
}

