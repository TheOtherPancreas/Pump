#ifndef BasalModel_h
#define BasalModel_h

#include <Arduino.h>
#include "ClientTime.h"

class BasalModel {

  public:
    BasalModel(DoseMotor* doseMotor) {
      this->doseMotor = doseMotor;
      amountInjected = 0;
      amountDosed = 0;
      tempBasalExpiration = 0;
      tempBasalIsPercentage = true;
      tempBasal = 1;
      for (int i = 0; i < 288; i++) {
        fiveMinuteEntries[i] = 0;
      }
    };
    
    void setEntries(float* entries) {
      for (int i = 0; i < 288; i++) {
        fiveMinuteEntries[i] = entries[i];
      }
    };
    
    void dose() {
      float fiveMinuteDose = getCurrentBasalDose();
      amountDosed += fiveMinuteDose;
      float amountToInject = amountDosed - amountInjected;    
      float injected = 0;  
      if (amountToInject > 0)
        injected = doseMotor->dose(amountToInject, true);
      amountInjected += injected;
      Serial.print("Basal injected: ");
      Serial.println(injected);
    };

    bool hasUnreportedDose() {
      return amountInjected > 0;
    }

    void confirmDose(float amount) {
      amountInjected -= amount;
      amountDosed = amountInjected;
    }

    float getUnreportedDose() {
      return amountInjected;
    }

    void updateTempBasal(bool tempBasalActive, bool tempBasalIsPercentage, float tempBasal, char tempDuration){
      if (!tempBasalActive) {
        tempBasalExpiration = millis()-1;
      }
      else {
        this->tempBasalIsPercentage = tempBasalIsPercentage;
        this->tempBasal = tempBasal;
        if (tempDuration == 0xFF)
          tempBasalExpiration = LONG_MAX;
        else 
          tempBasalExpiration = millis() + MINUTES(tempDuration);
      }
      
    }
    
  private:
    float fiveMinuteEntries[288];
    DoseMotor* doseMotor;
    float amountDosed;
    float amountInjected;
    long tempBasalExpiration;
    bool tempBasalIsPercentage;
    float tempBasal;

    
    float getCurrentBasalDose() {
      
      int index = ClientTime::getInstance().getFiveMinuteIndex();
      
      Serial.print("fiveMinuteEntries[");
      Serial.print(index, DEC);
      Serial.print("]: ");
      Serial.print(fiveMinuteEntries[index]);
      
      
      if (tempBasalExpiration > millis()) {
        Serial.print("....temp:");
        if (tempBasalIsPercentage) {
          float amount = fiveMinuteEntries[index] * tempBasal;
          Serial.println(amount);
          return amount;
        }
        else {
          Serial.println(tempBasal);
          return tempBasal;
        }
      }
      Serial.println("....notemp");
      return fiveMinuteEntries[index];
    };

    

};




#endif
