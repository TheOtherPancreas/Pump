void doRemoteCalibration() {
  motor[0]->completeRewind();
  motor[0]->drive(true, 1.0f); 
  delay(100);
  motor[0]->stop();
  calibrateEncoders();
  Settings::getInstance().save();
  int offset = Settings::getInstance().getEncoderOffset(0);
  motor[0]->completeRewind();
  char result[4];
  //default 464
  result[0] = (char) offset & 0xFF;
  result[1] = (char) (offset >> 8) & 0xFF;
  result[2] = (char) (offset >> 16) & 0xFF;
  result[3] = (char) (offset >> 24) & 0xFF;
  
  sendResponse(0xDC, result, 2);
}


void doRemoteSelfTest() {
  
  doRemoteSelfTest(false);
}

void doRemoteSelfTest(bool skipRewinds) {
  motor[0]->reset();
  if (!skipRewinds) {
    Serial.println("Complete Rewind...");
    motor[0]->completeRewind();
  }

  Serial.println("Test Dose 5 units...");
  if (!testDose(5))
    return;
  
  for (int i = 0; i < 5; i++) {
    Serial.println("Test Dose 1 unit...");
    if (!testDose(1))
      return;
  }

  delay(50);
  Serial.println("Test Dose -5 units...");
  if(!testDose(-5))
    return;
  delay(50);

  long singleUnitDoseTime[5] = {0, 0, 0, 0, 0};
  for (int i = 0; i < 5; i++) {
    Serial.println("Time 1 unsafe unit...");
    long start = millis();
    testDose(1, false);
    long end = millis();
    singleUnitDoseTime[i] = end - start;
  }
  Serial.println("Verify Dose Times...");
  if (!verifyDoseTimes(singleUnitDoseTime)) {
    sendTestResult(false, 0, 0, false, false, true);  
    return;
  }

  if (!skipRewinds) {
    Serial.println("Complete Rewind...");
    motor[0]->completeRewind();
  }
  
  delay(50);

  Serial.println("Test Dose 10 units...");
  if (!testDose(10))
    return;

  delay(50);

  Serial.println("Test Dose -9 units...");
  if (!testDose(-9)) //we just go back 9 because we don't want to fight with the very end of the rewind
    return;
    
  sendTestResult(true, 0, 0, false, false, false);
  
  if (!skipRewinds) {
    motor[0]->completeRewind();
    Serial.println("Complete Rewind...");
  }
  
  motor[0]->reset();
}

bool testDose(float amount) {
  return testDose(amount, true);
}

bool testDose(float amount, bool safe) {
  float injected = motor[0]->dose(amount, safe);
  if (motor[0]->isOccluded() || motor[0]->isEncoderError()) {
      sendTestResult(false, amount, injected, motor[0]->isOccluded(), motor[0]->isEncoderError(), false);
      return false;
  }
  else if (safe) {
    float diff = injected - amount;
    if (diff > 0.05) {
      sendTestResult(false, amount, injected, false, false, false);
      return false;
    }
    else if (diff < -0.05) {
      sendTestResult(false, amount, injected, false, false, false);
      return false;
    }
  }
  return true;
}


void sendTestResult(bool success, float dose, float injection, bool occlusion, bool encoderError, bool variation) {

  char bytes[8];

  for (int i=0; i < 8; i++) {
    bytes[i] = 0;
  }

  bytes[0] = success ? 1 : 0;
  encodeDose(dose, bytes, 1);
  encodeDose(injection, bytes, 3);
  bytes[5] = occlusion ? 1 : 0;
  bytes[6] = encoderError ? 1 : 0;
  bytes[7] = variation ? 1 : 0;

  Serial.println("+-------------------+");
  Serial.println("|     Self Test     |");
  Serial.println("+===================+");
  if (success)
    Serial.println("|      SUCCESS      |");
  else
    Serial.println("|      FAILURE      |");
  Serial.println("+-------------------+");

  Serial.print("SelfTestResult{success=");
  Serial.print(success ? "true" : "false");
  Serial.print(", dose=");
  Serial.print(dose);
  Serial.print(", injection=");
  Serial.print(injection);
  Serial.print(", occlusion=");
  Serial.print(occlusion ? "true" : "false");
  Serial.print(", encoderError=");
  Serial.print(encoderError ? "true" : "false");
  Serial.print(", variation=");
  Serial.print(variation ? "true" : "false");
  Serial.println("}");
  sendResponse(0xDE, bytes, 8);
}

bool verifyDoseTimes(long * singleUnitDoseTime) {
  long max = 0;
  long min = 0x0FFFFFFF;
  for (int i = 0; i < 5; i++) {
    if (max < singleUnitDoseTime[i])
      max = singleUnitDoseTime[i];
    if (min > singleUnitDoseTime[i])
      min = singleUnitDoseTime[i];
  }
  double ratio = (double)max / (double)min;

  Serial.print("Max 1u Duration: ");
  Serial.print(max);
  Serial.print("\tMin 1u Duration: ");
  Serial.print(min);
  Serial.print("\tVariation: ");
  Serial.println(ratio);
  
  //if max is 50% larger than min we fail
  return ratio < 1.5;
}
