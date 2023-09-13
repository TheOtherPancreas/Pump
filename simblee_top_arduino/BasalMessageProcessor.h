#ifndef BasalMessageProcessor_h
#define BasalMessageProcessor_h

#include <Arduino.h>
#include "Util.h"
#include "BasalModel.h"
#include "ClientTime.h"

class BasalMessageProcessor {
  public:
    void initiateTransaction(BasalModel* basalModel) {
      #ifdef DEBUG
        Serial.println("Initiating Basal Transaction");      
      #endif

      startFlag = true;
      currentModel = basalModel;
      
      for (int i = 0; i < 288; i++) {
        incomingBasal[i] = -1.0f;
        encodedDoses[i] = 0;
      }
    };

    void receiveChunk(char* bytes, int len) {
      #ifdef DEBUG
        Serial.println("Receiving Basal Chunk");
      #endif
      if (!startFlag)
        return;
      int opcode = bytes[0];
      int count = bytes[1];
      for (int i = 0; i < count; i++) {
        short index = bytesToShort(bytes[i*4+2], bytes[i*4+3]);
        short encodedDose = bytesToShort(bytes[i*4+4], bytes[i*4+5]);
        float dose = decodeBasal(encodedDose);
        encodedDoses[index] = encodedDose;
        incomingBasal[index] = dose;
        #ifdef DEBUG
          Serial.print(index);
          Serial.println(dose);
        #endif
      }
    };

    long finalizeTransaction(short howManySecondsFromNowShouldPumpDoBasal, int expectedHash) {
      #ifdef DEBUG
        Serial.println("Finalizing Basal");
      #endif
      
      long now = millis();
      long wakeTime = now + howManySecondsFromNowShouldPumpDoBasal * SECONDS(1);
      startFlag = false;
      int actualHash = calculateShortArrayHashCode(encodedDoses, 288);
      if (expectedHash == actualHash) {
        interpolate(incomingBasal);
        #ifdef DEBUG
//          for (int i = 0; i < 288; i++) {
//            int hour = i/12;
//            int minute = (i%12)*5;
//            Serial.print(hour);
//            Serial.print(":");
//            if (minute < 10)
//              Serial.print("0");
//            Serial.print(minute);
//            Serial.print("--- ");
//            Serial.println(incomingBasal[i]*12);
//          }
        #endif
        currentModel->setEntries(incomingBasal);
        completeFlag = true;
        
        
        return wakeTime;
      }
      else {
        Serial.println("                                             ERROR, basal hashes don't match");
        Serial.print(expectedHash);
        Serial.print(" vs ");
        Serial.print(actualHash);
        return 0;
      }
    };

    int getState() {
      if (completeFlag)
        return 5;
      else if (startFlag)
        return 4;
      else
        return 3;
    };
    
  private:
    BasalModel* currentModel;
    float incomingBasal[288];
    short encodedDoses[288];
    int nextFiveMinuteIndex;
    bool startFlag;
    bool completeFlag;

    void interpolate(float* incomingBasal) {
      float current = -1.0f;
      for (int i = 0; i < 288; i++) {
        if (incomingBasal[i] > 0) {
          current = incomingBasal[i];
        }
        else {
          incomingBasal[i] = current;
        }
      }
      //handle wrap: where the first key was not midnight
      for (int i = 0; i < 288; i++) {
        if (incomingBasal[i] > 0)
          break;
        else
          incomingBasal[i] = current;
      }
    }
    
};





#endif
