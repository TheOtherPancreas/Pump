#ifndef Settings_h
#define Settings_h

#define FLASH_STRUCT_PAGE 237
#define MAGIC 0xACE

struct FlashStruct { //Page 251
  int motorCount;
  bool maxonMotors = false;
  char powerLevel[3];
  char securityKey[16];
  int encoderOffset[3];
  int magicNumber;
};


class Settings {
  public:
    static Settings& getInstance() {
      static Settings instance;
      return instance;
    };

    Settings(Settings const&) = delete;
    void operator=(Settings const&) = delete;

    bool isDefault() {
      return settings.magicNumber != MAGIC;
    }

    char* getSecurityKey() {
      //Lazy initialize security key
      if (settings.magicNumber != MAGIC) {
        char password[] = "SetupIsRequired!";
        
        setSecurityKey(password);
      }
      return settings.securityKey;
    };

    void setSecurityKey(char* key) {
      for (int i = 0; i<16; i++)
        settings.securityKey[i] = key[i];
    }

    int getMotorCount() {
      return settings.motorCount;
    };

    void setMotorCount(int count) {
      settings.motorCount = count;
    }

    bool isMaxonMotors() {
      return false;
//      return settings.maxonMotors;
    };

    void setMaxonMotors(bool isMaxonMotors) {
      settings.maxonMotors = isMaxonMotors;
    }

    char* getPowerLevels() {
      return settings.powerLevel;
    };

    char getPowerLevel(int index) {
      return (char)settings.powerLevel[index];
    }

    void setPowerLevel(int index, char powerLevel) {
      settings.powerLevel[index] = powerLevel;
    }

    float getSpeedPercent(int index) {
      return settings.powerLevel[index] / 100.0f;
    }

    int getEncoderOffset(int index) {
      return settings.encoderOffset[index];
    }

    void setEncoderOffset(int index, int offset) {
      settings.encoderOffset[index] = offset;
    }

    void save() {
      settings.magicNumber = MAGIC;
      FlashStruct *flash = (FlashStruct*)ADDRESS_OF_PAGE(FLASH_STRUCT_PAGE);
    
      flashPageErase(PAGE_FROM_ADDRESS(flash));
      
      int result = flashWriteBlock(flash, &settings, sizeof(settings));
      if (result == 0) {
        Serial.println("Successfully written");
      }
      else if (result == 1) {
        Serial.println("Error - the flash page is reserved");
      }
      else if (result == 2) {
        Serial.println("Error - the flash page is used by the sketch");
      }
    }

    
  private:
    Settings() {
      flash = (FlashStruct*)ADDRESS_OF_PAGE(FLASH_STRUCT_PAGE);
      settings = *flash;

      bool notSet = settings.magicNumber != MAGIC;

      settings.motorCount = notSet || settings.motorCount > 3 ? 1 : settings.motorCount;
      
      settings.powerLevel[0] = notSet ? 50 : settings.powerLevel[0];
      settings.powerLevel[1] = notSet ? 50 : settings.powerLevel[1];
      settings.powerLevel[2] = notSet ? 50 : settings.powerLevel[2];

      settings.encoderOffset[0] = notSet ? 464 : settings.encoderOffset[0];
      settings.encoderOffset[1] = notSet ? 464 : settings.encoderOffset[1];
      settings.encoderOffset[2] = notSet ? 464 : settings.encoderOffset[2];
    };
    FlashStruct* flash;
    FlashStruct settings;
};


#endif
