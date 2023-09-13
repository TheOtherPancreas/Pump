#ifndef DCMotorDualEncoderPiston_h
#define DCMotorDualEncoderPiston_h

#include "DoseMotor.h"
#include "DualEncoder.h"

class DCMotorDualEncoderPiston : public DoseMotor
{
  private:
    int motorIndex;
    int motorPinA;
    int motorPinB;
   
    bool occluded;
    bool encoderError;
    DualEncoder encoder;

    void enableHBridge(bool enable) {
      int hBridgeVccPin = 7;
      int hBridgeModePin = 10;
      pinMode(hBridgeVccPin, OUTPUT);
      pinMode(hBridgeModePin, OUTPUT);
      if (enable) {
        digitalWrite(hBridgeVccPin, HIGH);
        digitalWrite(hBridgeModePin, HIGH);
      }
      else {
        digitalWrite(hBridgeVccPin, LOW);
        digitalWrite(hBridgeModePin, LOW);
        digitalWrite(motorPinA, LOW);
        digitalWrite(motorPinB, LOW);        
      }
      
    }
    
  public:

    static DCMotorDualEncoderPiston* createSingle() {
//      return new DCMotorDualEncoderPiston(0, 29, 30, 5, 3, 8, 9); //v1
      return new DCMotorDualEncoderPiston(0, 13, 14, 5, 3, 8, 8); //v2
    };

    static DCMotorDualEncoderPiston* createDual(bool isInsulin) {
      if (isInsulin) {
        return new DCMotorDualEncoderPiston(0, 29, 30, 5, 2, 11, 12);
      }
      else {
        return new DCMotorDualEncoderPiston(1, 25, 28, 4, 6, 23, 24);
      }
    };
    
    DCMotorDualEncoderPiston(int motorIndex, int motorPinA, int motorPinB, int encoderPinA, int encoderPinB, int enablePinA, int enablePinB) : encoder(motorIndex, encoderPinA, encoderPinB, enablePinA, enablePinB) 
    {
      this->motorIndex = motorIndex;
      this->motorPinA = motorPinA;      
      this->motorPinB = motorPinB;      
          
      pinMode(motorPinA, OUTPUT);
      pinMode(motorPinB, OUTPUT);
      
      occluded = false;
      encoderError = false;

    };
    
    
    void clearOcclusion() {occluded = false;};
    void clearEncoderError() { encoderError = false;};
  
    
  
  
    void stop() 
    {
      digitalWrite(this->motorPinB, LOW);
      enableHBridge(false);
    };

    void pause() {
      digitalWrite(this->motorPinB, LOW);
    }

    float completeRewind() {

      encoder.enablePower(true);
      
      float initialLocation = encoder.getLocation();
      if (initialLocation > 10000) { 
        Serial.println("Invalid initial encoder read");
        encoder.enablePower(false);
        return 0;
      }
      
      enableHBridge(true);
      drive(false, 0.5f);

      float lastLocation = initialLocation;      
      while (true) {
        float location = encoder.getLocationWithDelay(100); 
        if (abs(location - lastLocation) < 0.1) {
          break;
        }
        lastLocation = location;
      }
      stop();
      reset();
      return initialLocation - lastLocation;
    }
    
    void drive(bool forward, float speedPercent) 
    {
      enableHBridge(true);
      if (speedPercent > 1)
        speedPercent = 1;
      if (speedPercent < 0)
        speedPercent = 0;
      digitalWrite(this->motorPinA, forward ? LOW : HIGH);
      analogWrite(this->motorPinB, (int)(255 * speedPercent));
    };
    
    float dose(float amount, bool safe)
    {
      if (amount >= 0)
        doseForward(amount, safe);
      else
        doseBackward(amount, safe);
    }

    float doseForward(float amount, bool safe)
    {
      Serial.print("Dosing ");
      Serial.println(amount);
      if (amount != amount) //if amount is nan
        return 0.0f;
    
      float speedLimit = Settings::getInstance().getSpeedPercent(motorIndex);
      float lastLocation = encoder.getLocationWithoutRead();
      
      encoder.enablePower(true);
      enableHBridge(true);
      
      while (SimbleeBLE_radioActive) 
      {
        //wait until radio isn't active, this will hypothetically reduce noise on the encoders
      }
      
      float initialLocation = encoder.getLocation();
      if (initialLocation > 10000) { //invalid read
        Serial.println("Invalid initial encoder read");
        encoderError = true;
        encoder.enablePower(false);
        enableHBridge(false);
  
        Serial.print("final location: ");
        Serial.println(encoder.getLocationWithoutRead());
        return 0;
      }
      
      float destination = initialLocation + amount;

      Serial.print("Current Location: ");
      Serial.println(initialLocation);
      Serial.print("Destination:      ");
      Serial.println(destination);
    
    
      float maxLocation = 0;
      bool movedForward = false;
      long lastForwardMovement = millis();
      float adjustSpeed = 0;
      float previousLocation = initialLocation;
      float locationAtTimeOfStop = initialLocation;
      long lastSpeedAdjustment = 0;
      long firstBacklash = 0;
      while(true) {
        float currentLocation = encoder.getLocation();
        if (currentLocation > 10000) { //invalid read
          encoderError = true;
          currentLocation = previousLocation;
          break;
        }
        
        if (currentLocation + EPSILON  >= destination) {
          //we've dosed enough
          pause();
          locationAtTimeOfStop = currentLocation;
          //see if there's backlash
          currentLocation = encoder.getLocationWithDelay(250);      
          Serial.print("Backlash check -- stopped at: ");
          Serial.print(locationAtTimeOfStop);
          Serial.print(", quarter second later location is: ");
          Serial.print(currentLocation);
          Serial.print(". That is a difference of: ");
          Serial.println(currentLocation - locationAtTimeOfStop);
          boolean backlashed = currentLocation + EPSILON < destination;
          if (!backlashed || millis() > firstBacklash + 2000) { //if it didn't backlash we break, otherwise, let's turn the motor some more
            break;
          }
          else {
            if (firstBacklash == 0)
              firstBacklash = millis();
            lastForwardMovement = millis(); //we don't want it to think it's an occlusion at the very end when we told it to stop moving
          }
        }
    
        bool movedForward = currentLocation > maxLocation; 
        
        if (movedForward) {
          maxLocation = currentLocation;
          lastForwardMovement = millis();
          
          //we're moving forward so let's make sure the speeds are where we want 
          
          if (safe && destination - currentLocation < 0.1f)  //we're close so slow things down
          {
            lastSpeedAdjustment = millis();
            adjustSpeed = -speedLimit;
          }
          else //things are moving normally so let's just go the normal speed limit
          { 
            adjustSpeed = safe ? 0 : 1; //not safe goes max speed
          }
        }
        else { //didn't move forward
          //might be occluded  
          if (lastForwardMovement < millis() - 500) {
            occluded = true;
            break;
          }
          else {
            if (lastSpeedAdjustment < millis() - 10) {
              lastSpeedAdjustment = millis();
              adjustSpeed += 0.05f;
            }
          }
        }
        
        drive(true, speedLimit + adjustSpeed);      
        previousLocation = currentLocation;
      }
      
      stop();
      encoder.enablePower(false);
      enableHBridge(false);

      Serial.print("final location: ");
      Serial.println(encoder.getLocationWithoutRead());
      return encoder.getLocationWithoutRead() - initialLocation;    
    };

    float doseBackward(float amount, bool safe)
    {
      Serial.print("Dosing ");
      Serial.println(amount);
      if (amount != amount) //if amount is nan
        return 0.0f;
    
      float speedLimit = Settings::getInstance().getSpeedPercent(motorIndex);
      float lastLocation = encoder.getLocationWithoutRead();
      
      encoder.enablePower(true);
      enableHBridge(true);
      
      while (SimbleeBLE_radioActive) 
      {
        //wait until radio isn't active, this will hypothetically reduce noise on the encoders
      }
      
      float initialLocation = encoder.getLocation();
      if (initialLocation > 10000) { //invalid read
        Serial.println("Invalid initial encoder read");
        encoderError = true;
        encoder.enablePower(false);
        enableHBridge(false);
  
        Serial.print("final location: ");
        Serial.println(encoder.getLocationWithoutRead());
        return 0;
      }
      
      float destination = initialLocation + amount;

      Serial.print("Current Location: ");
      Serial.println(initialLocation);
      Serial.print("Destination:      ");
      Serial.println(destination);
    
    
      float minLocation = 10000;
      //bool movedBackward = false;
      long lastBackwardMovement = millis();
      float adjustSpeed = 0;
      float previousLocation = initialLocation;
      float locationAtTimeOfStop = initialLocation;
      long lastSpeedAdjustment = 0;
      long firstBacklash = 0;
      while(true) {
        float currentLocation = encoder.getLocation();
        if (currentLocation > 10000) { //invalid read
          encoderError = true;
          currentLocation = previousLocation;
          break;
        }
        
        if (currentLocation - EPSILON  <= destination) {
          //we've dosed enough
          pause();
          locationAtTimeOfStop = currentLocation;
          //see if there's backlash
          currentLocation = encoder.getLocationWithDelay(250);      
          Serial.print("Backlash check -- stopped at: ");
          Serial.print(locationAtTimeOfStop);
          Serial.print(", quarter second later location is: ");
          Serial.print(currentLocation);
          Serial.print(". That is a difference of: ");
          Serial.println(currentLocation - locationAtTimeOfStop);
          boolean backlashed = currentLocation - EPSILON > destination;
          if (!backlashed || millis() > firstBacklash + 2000) { //if it didn't backlash we break, otherwise, let's turn the motor some more
            break;
          }
          else {
            if (firstBacklash == 0)
              firstBacklash = millis();
            lastBackwardMovement = millis(); //we don't want it to think it's an occlusion at the very end when we told it to stop moving
          }
        }
    
        bool movedBackward = currentLocation < minLocation; 
        
        if (movedBackward) {
          minLocation = currentLocation;
          lastBackwardMovement = millis();
          
          //we're moving backward so let's make sure the speeds are where we want 
          
          if (safe && currentLocation - destination < 0.1f)  //we're close so slow things down
          {
            lastSpeedAdjustment = millis();
            adjustSpeed = -speedLimit;
          }
          else //things are moving normally so let's just go the normal speed limit
          { 
            adjustSpeed = safe ? 0 : 1; //not safe goes max speed
          }
        }
        else { //didn't move backward
          //might be occluded  
          if (lastBackwardMovement < millis() - 500) {
            occluded = true;
            break;
          }
          else {
            if (lastSpeedAdjustment < millis() - 10) {
              lastSpeedAdjustment = millis();
              adjustSpeed += 0.05f;
            }
          }
        }
        
        drive(false, speedLimit + adjustSpeed);      
        previousLocation = currentLocation;
      }
      
      stop();
      encoder.enablePower(false);
      enableHBridge(false);

      Serial.print("final location: ");
      Serial.println(encoder.getLocationWithoutRead());
      return encoder.getLocationWithoutRead() - initialLocation;    
    };
    
    void reset() 
    {
      encoder.enablePower(true);
      encoder.reset();
      encoder.enablePower(false);  
      occluded = false;
      encoderError = false;
    };
    
    bool isOccluded() 
    {
      return occluded;  
    };

    bool isEncoderError()
    {
      return encoderError;
    }

};


#endif
