#ifndef DualEncoder_h
#define DualEncoder_h


class DualEncoder {
  private: 
   
    int expectedOposite[1025];
       
    const int MAX_READ = 1024;
    const int ANALOG_GAP = 66; //the difference between the oposite max non-dead-zone read and the oposite min non-deadzone read
    const int ANOLOG_360 = MAX_READ + ANALOG_GAP; //1090
    const float UNITS_PER_ROTATION = 4.4118; // 4.4117647058823529411764705882353 based on 11.85mm inner diameter of cartridge;
    
    const float UNITS_PER_ANALOG_TICK = UNITS_PER_ROTATION / (ANOLOG_360); //0.0040
    const int INVALID_DELTA = 0xFFFF;

    int motorIndex;
    int readPinA;
    int readPinB;
    int enablePinA;
    int enablePinB;
    
    int analogReadingA;
    int analogReadingB;
    
    boolean aIsSafest = true;
    float location = 0;

    void initExpectedOposite() {
      int value = Settings::getInstance().getEncoderOffset(motorIndex);
      Serial.print("Encoder Offset: ");
      Serial.println(value);
      for (int i = 0; i < 1024; i++) {
        expectedOposite[i] = value;
        if (value == 0) {
          for (int j = 0; j < 32; j++) {
            expectedOposite[++i] = 0;
          }
          for (int j = 0; j < 33; j++) {
            expectedOposite[++i] = 1024;
          }
          value = 1024;
        }
        value--;        
      }

//      for (int i = 0; i < 1024; i++) {
//        Serial.print(expectedOposite[i]);
//        Serial.print(",\t");
//        if ((i+1)%10 == 0)
//          Serial.println();
//        if ((i+1)%100 == 0)
//          Serial.println();
//      }
      
    }

    int readAnalogDelta() {
      int previousAReading = analogReadingA;
      int previousBReading = analogReadingB;
      analogReadingB = analogRead(readPinB);
      analogReadingA = analogRead(readPinA);
      
      int DEAD_ZONE_MARGIN = 100;
      int aDelta = previousAReading - analogReadingA;
      int bDelta = -(previousBReading - analogReadingB);
      
      bool bothValidReadings = abs(expectedOposite[analogReadingA] - analogReadingB) <  DEAD_ZONE_MARGIN;
      if (bothValidReadings) {
        aIsSafest = abs(analogReadingA - 512) < abs(analogReadingB - 512);        
      }      
      else {
        //is this an unexpected invalid reading? 
        bool expectedBDeadZone = expectedOposite[analogReadingA] <= DEAD_ZONE_MARGIN || expectedOposite[analogReadingA] >= (1024-DEAD_ZONE_MARGIN);
        bool expectedADeadZone = expectedOposite[analogReadingB] <= DEAD_ZONE_MARGIN || expectedOposite[analogReadingB] >= (1024-DEAD_ZONE_MARGIN);

        bool invalidReadingsOutsideOfDeadZone = !expectedBDeadZone && !expectedADeadZone;
        if (invalidReadingsOutsideOfDeadZone) {
          return INVALID_DELTA; 
        }
      }

      int acceptableThreshold = 12;

      #ifdef DEBUG
        acceptableThreshold = 36;
      #endif  
      
      if (aIsSafest && abs(aDelta) < acceptableThreshold) {
          #ifdef DEBUG
            Serial.print(analogReadingA);
            Serial.print("*\t");
            Serial.print(analogReadingB);
            Serial.print("\t");
            Serial.println(aDelta);
          #endif
          return aDelta;
      }
      else if (abs(bDelta) < acceptableThreshold) {//to get here either !aIsSafest, or aIsSafest but aDelta was too big, thus aIs(NoLonger)Safest
          #ifdef DEBUG
            Serial.print(analogReadingA);
            Serial.print("\t");
            Serial.print(analogReadingB);
            Serial.print("*\t");
            Serial.println(bDelta);
          #endif
        aIsSafest = false;
        return bDelta;
      }
      else {
          #ifdef DEBUG
            Serial.print("a");
            Serial.print(analogReadingA);
            Serial.print("\tb");
            Serial.print(analogReadingB);
            Serial.print("\tdA");
            Serial.print(aDelta);
            Serial.print("*\tdB");
            Serial.print(bDelta);
            Serial.println("\t->INVALID");
          #endif
        return INVALID_DELTA;
      }
    }
    
  public:
    DualEncoder(int motorIndex, int readPinA, int readPinB, int enablePinA, int enablePinB) {
      this->motorIndex = motorIndex;
      this->readPinA = readPinA;
      this->readPinB = readPinB;
      this->enablePinA = enablePinA;
      this->enablePinB = enablePinB;

      pinMode(readPinA, INPUT_NOPULL);
      pinMode(readPinB, INPUT_NOPULL);
      pinMode(enablePinA, OUTPUT);
      pinMode(enablePinB, OUTPUT);

      initExpectedOposite();

      enablePower(true);
      reset();
      enablePower(false);

    }

    float getLocation() {
      int analogDelta = readAnalogDelta();
      if (analogDelta == INVALID_DELTA) {
        return INFINITY;
      }
      else {
        location += (analogDelta * UNITS_PER_ANALOG_TICK);
        return location;
      }
    }

    float getLocationWithoutRead() {
      return location;
    }

    void reset() {
      analogReadingA = analogRead(readPinA);
      analogReadingB = analogRead(readPinB);
      #ifdef DEBUG
          Serial.print("Initial encoder reads: a:");
          Serial.print(analogReadingA);
          Serial.print("\tb:");
          Serial.println(analogReadingB);
        #endif
      
      aIsSafest = true;
      location = 0;
    }

    void enablePower(bool enable) {
      digitalWrite(enablePinA, enable ? HIGH : LOW);
      digitalWrite(enablePinB, enable ? HIGH : LOW);      
      delay(10);
    }

    float getLocationWithDelay(long delay) {
      long time = millis();
      float location = getLocation();
      if (location == INFINITY)
        return INFINITY;
      while (millis() < time + delay) {
        location = getLocation();
        if (location == INFINITY)
          return INFINITY;
      }
      return location;
    }
};









#endif
