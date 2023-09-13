#ifndef ClientTime_h
#define ClientTime_h


class ClientTime 
{
  public:

    static ClientTime& getInstance() {
      static ClientTime instance;
      return instance;
    };

    ClientTime(ClientTime const&) = delete;
    void operator=(ClientTime const&) = delete;
    
  
    void updateClientTime(long millisSinceMidnight) {
      long now = millis();
      midnight = now - millisSinceMidnight;      
      Serial.print("Just set midnight to: ");
      Serial.println(midnight);
    };

    long getClientTime() {
      return clientTimeOffset + millis();
    };

    int getFiveMinuteIndex() {
      if (midnight == 0)
        return 0;
      long now = millis();
      while (midnight < now)
        midnight += DAYS(1);
      while (midnight > now)
        midnight -= DAYS(1);
        
      long millisIntoDay = now - midnight;
      Serial.print("Millis into day: ");
      Serial.println(millisIntoDay);
      int index = (int)(millisIntoDay / MINUTES(5));
      return index;
    };

    bool isInitialized() {
      return midnight != 0;
    }
    
  private:
    ClientTime() { 
      midnight = 0;
      clientTimeOffset = 0;
    };
    long midnight;
    long clientTimeOffset;
  
};








#endif
