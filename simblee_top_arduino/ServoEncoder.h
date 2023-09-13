#ifndef ServoEncoder_h
#define ServoEncoder_h

class ServoEncoder
{
  public:
    ServoEncoder(int _encoderPin, int _powerPin) : encoderPin(_encoderPin) , powerPin(_powerPin) {
      init();
    };
    float readForward();
    void enablePower(bool enable);
    void init();
    
  protected:
    int smoothAnalogRead();
    int encoderPin;
    int powerPin;
    
  private:
    float currentLocation;
    int lastAnalogRead;
    int currentAnalogRead;
    
    const int MAX_READ = 1024;
    const int QUARTER = MAX_READ / 4;
    const int HALF = MAX_READ / 2;

    /*
    * 0 to 360º is 4.4118u
    * 0 to MAX_READ is 333.3º which is 4.084u
    * Noman's land is 26.7º which is 0.327u
    * if 1024 is 333.3º then 1106 is 360º which is a difference of 82
    */
    const int ANOLOG_360 = (int)((MAX_READ * 360)/333.3f); //1106
    const float UNITS_PER_ROTATION = 4.4118; // 4.4117647058823529411764705882353 based on 11.85mm inner diameter of cartridge;
    const float UNITS_PER_ANALOG_TICK = UNITS_PER_ROTATION / (ANOLOG_360);


    bool inNomansLand;
    int timesAround;

    bool checkpointA;
    bool checkpointB;


    bool lookingFor1 = false;

    static const int SMOOTHING_SIZE = 5;
    int smoothingIndex;
    int signals[SMOOTHING_SIZE];

};





void ServoEncoder::init() 
{
  currentLocation = 0;
  lastAnalogRead = 0;
  currentAnalogRead = 0;
  inNomansLand = false;
  timesAround = 0;
  checkpointA = false;
  checkpointB = false;
  lookingFor1 = false;
  smoothingIndex = -1;
  for (int i = 0; i < SMOOTHING_SIZE; i++)
    signals[i] = 0;

  pinMode(encoderPin, INPUT);
    
  enablePower(true);
  currentAnalogRead = MAX_READ - smoothAnalogRead();
  lastAnalogRead = currentAnalogRead;
  currentLocation = currentAnalogRead * UNITS_PER_ANALOG_TICK;;
  readForward();
  enablePower(false);
}


void ServoEncoder::enablePower(bool enable) 
{
  if (enable)
    digitalWrite(powerPin, HIGH);
  else
    digitalWrite(powerPin, LOW);
  delay(100);
}


float ServoEncoder::readForward()
{
  currentAnalogRead = MAX_READ - smoothAnalogRead(); //subtracted from MAX_READ so it counts up instead of down

  if (checkpointA && checkpointB && currentAnalogRead < QUARTER) //if it went around the horn
  {
    timesAround++;
    checkpointA = false;
    checkpointB = false;    
  }
  else if (!checkpointB && currentAnalogRead > MAX_READ - QUARTER) 
  {
    currentAnalogRead -= ANOLOG_360;
  }
  else if (!checkpointA && !checkpointB && currentAnalogRead > QUARTER && currentAnalogRead < MAX_READ - HALF) 
  {
    checkpointA = true;
  }
  else if (checkpointA && !checkpointB && currentAnalogRead > HALF && currentAnalogRead < MAX_READ - QUARTER) 
  {
    checkpointB = true;
  }

  float units = currentAnalogRead * UNITS_PER_ANALOG_TICK;
  lastAnalogRead = currentAnalogRead;
  currentLocation = timesAround * UNITS_PER_ROTATION + units;
  
  #ifdef DEBUG
    Serial.print("an:");
    Serial.print(currentAnalogRead);
    Serial.print("->");
    Serial.println(currentLocation);
  #endif
  
  return currentLocation;
}



/**
 * This method will average the last SMOOTHING_SIZE readings and return them. In the case of wrapped readings (i.e. some readings around 1024, and some readings around 0, it will
 * treat the lower readings as +=1024, and in such a case it may return a value greater than 1024. This is to handle noise as an encoder transitions through the dead zone
 */
int ServoEncoder::smoothAnalogRead() 
{
  int analogValue = analogRead(encoderPin);
  #ifdef DEBUG
    Serial.print("rawAnalogRead:");
    Serial.println(analogValue);
    Serial.print("encoderPin: ");
    Serial.println(encoderPin);
  #endif

  if (smoothingIndex == -1)  //first read
  {
    for (int i = 0; i < SMOOTHING_SIZE; i++) 
    {
      signals[i] = analogValue;
    }
    smoothingIndex = 0;
  }
  else 
  {
    smoothingIndex++;
    smoothingIndex %= SMOOTHING_SIZE;    
    signals[smoothingIndex] = analogValue;
    int upperSum = 0;
    int upperCount = 0;
    int lowerSum = 0;
    int lowerCount = 0;
    for (int i = 0; i < SMOOTHING_SIZE; i++) 
    {
      if (signals[i] > MAX_READ/2) 
      {
        upperSum += signals[i];
        upperCount++;
      }
      else 
      {
        lowerSum += signals[i];
        lowerCount++;
      }
    }

    int lowerAverage = lowerCount == 0 ? 0 : lowerSum / lowerCount;
    int upperAverage = upperCount == 0 ? 0 : upperSum / upperCount;

    if (lowerAverage == 0 || upperAverage == 0 ||  abs(lowerAverage - upperAverage) < abs((lowerAverage + MAX_READ) - upperAverage))  //didn't wrap
    { 
      return (upperSum + lowerSum) / (lowerCount + upperCount);
    }
    else 
    {
      lowerAverage += MAX_READ; //unwrap it
      int unwrappedSum = lowerAverage * lowerCount;
      int result = ((unwrappedSum + upperSum) / (lowerCount + upperCount));
      if (result > MAX_READ)
        result -= MAX_READ;
      return result;// we want to allow readings below 0 when we have transitioned from low to high, otherwise we'd just % MAX_READ;
    }

  }

}






#endif
