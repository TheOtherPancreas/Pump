#ifndef EmptyMotor_h
#define EmptyMotor_h

#include "DoseMotor.h"

class EmptyMotor : public DoseMotor
{
  public:
    void stop(){};
    void drive(bool forward, float speedPercent){};
    float dose(float amount, bool safe){
      Serial.println("Empty Dosing");
      return 0.0f;
    };
    float completeRewind() {return 0;};
    bool isOccluded()
    {
      return false;
    };
    void reset() {};
    void clearOcclusion() {};
    bool isEncoderError() {return false;};
    void clearEncoderError() {};
};

#endif
