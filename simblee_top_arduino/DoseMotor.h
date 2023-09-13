#ifndef DoseMotor_h
#define DoseMotor_h

class DoseMotor 
{
  public:
    virtual void stop()=0;
    virtual void drive(bool forward, float speedPercent)=0;
    virtual float dose(float amount, bool safe)=0;
    virtual float completeRewind()=0;
    virtual bool isOccluded()=0;
    virtual void clearOcclusion()=0;
    virtual bool isEncoderError()=0;
    virtual void clearEncoderError()=0;
    virtual void reset()=0;
};




#endif
