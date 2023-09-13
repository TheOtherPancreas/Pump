/**
 * Because Simblee is dead we have resorted to using OpenBCI Ganglion stuff to provide the simblee files
 */

#define DEBUG
#define INSULIN 0
#define SYMLIN 1
#define EPSILON 0.00001f




#include "aes.h"
#include <SimbleeBLE.h>

#include <limits.h>
#include "Settings.h"
#include "Util.h"
#include "EmptyMotor.h"
#include "BasalMessageProcessor.h"
#include "BasalModel.h"
#include "ClientTime.h"
#include "DCMotorDualEncoderPiston.h"

String possibleKeyChars = "!#$%&'()*+,-./23456789:;<=>?@ABCDEFGHIJKLMNPQRSTUVWXYZ[]^_abcdefghijkmnpqrstuvwxyz{|}~";
DoseMotor *motor[3];
BasalModel *basalModel[3];
BasalMessageProcessor basalMessageProcessor;
bool initialized = false;
bool setupMode = false;


struct DoseResult {
  float injection[3];
  char verificationId;
};

DoseResult lastDose;

void setup() {

//  pinMode(19, INPUT);
//  randomSeed(analogRead(19));
  Serial.begin(9600);
  Serial.println();
  Serial.print("Battery: ");
  Serial.print(getBatteryVoltage());
  Serial.println("V");
  //delay(1000);

  SimbleeBLE.advertisementData = createAdvertisementData(Settings::getInstance().getSecurityKey());

  //Serial.println(Settings::getInstance().getSecurityKey());
  //Serial.println(SimbleeBLE.advertisementData);
  SimbleeBLE.deviceName = "TOPancreas";
  SimbleeBLE.customUUID = "dd43aac7-4ea3-498b-a572-ca33a06707c7";
  SimbleeBLE.advertisementInterval = 2500;
  
  // -20, -16, -12, -8, -4, 0, +4
  SimbleeBLE.txPowerLevel = 4;

  initMotors();
  initBasalModels();

  SimbleeBLE.begin();

  #ifdef DEBUG
  Serial.println();
  Serial.println("                ?77777?                  _____ _");
  Serial.println("                I77777?                 |_   _| |__   ___");
  Serial.println("                ?77777?                   | | | '_ \\ / _ \\");
  Serial.println("                ?77777?                   | | | | | |  __/");
  Serial.println("      ~7777777777777777777777777=         |_| |_| |_|\\___|");
  Serial.println(" ,7777,         ?77777?          7777:");
  Serial.println("777:            ?77777?             777");
  Serial.println("777              ~I7I~              777=  ___  _   _");
  Serial.println(" $777                             7777   / _ \\| |_| |__   ___ _ __");
  Serial.println("    :777777I=,           ,~?777777,     | | | | __| '_ \\ / _ \\ '__|");
  Serial.println("       777777777777777777777777?        | |_| | |_| | | |  __/ |");
  Serial.println("    77:   7777777777777777777     ,      \\___/ \\__|_| |_|\\___|_|");
  Serial.println("   777      777777777777777       7");
  Serial.println("    7777~    ,77777777777+     ,7I");
  Serial.println("        ,?77777         $777++           ____");
  Serial.println("                7777777                 |  _ \\ __ _ _ __   ___ _ __ ___  __ _ ___");
  Serial.println("          ~7     77777                  | |_) / _` | '_ \\ / __| '__/ _ \\/ _` / __|");
  Serial.println("         77:      777       =           |  __/ (_| | | | | (__| | |  __/ (_| \\__ \\");
  Serial.println("           :777I=:= +=?7I:              |_|   \\__,_|_| |_|\\___|_|  \\___|\\__,_|___/");
  Serial.println("                   7");
  #endif

int sizeofint = sizeof(int);
Serial.println("sizeofint:");
Serial.println(sizeofint);



//  char key[16] = {')','I','8','r','H','b','M','3','%','[','R','{','>','#',':','_'};
//  Settings::getInstance().setSecurityKey(key);
  //TODO clear encoder error


  
  if (Settings::getInstance().isDefault()) {
    setupMode = true;
    printSetupMenu();
  }
}


void initMotors() 
{
  switch(Settings::getInstance().getMotorCount())
  {
    default:
    case 0:
    case 1:
      Serial.println("Initializing Single Motor");
      motor[0] = DCMotorDualEncoderPiston::createSingle();
      motor[1] = new EmptyMotor();
      motor[2] = new EmptyMotor();
      break;
    case 2:
      Serial.println("Initializing Dual Motors");
      motor[0] = DCMotorDualEncoderPiston::createDual(true);
      motor[1] = DCMotorDualEncoderPiston::createDual(false);
      motor[2] = new EmptyMotor();
      break;
    case 3:
      Serial.println("Initializing Tripple Motors");
      motor[0] = DCMotorDualEncoderPiston::createDual(true);
      motor[1] = DCMotorDualEncoderPiston::createDual(false);
      motor[2] = new EmptyMotor();
      break;
  }
}

void initBasalModels() {
  basalModel[0] = new BasalModel(motor[0]);
  basalModel[1] = new BasalModel(motor[1]);
  basalModel[2] = new BasalModel(motor[2]);
}

bool connected = false;
unsigned long awakeTime = 0;
unsigned long connectionExpiration = 0;

void loop() {
 
  if (awakeTime != 0)
    sleepUntilNextBasal();
  else {
    Simblee_ULPDelay(SECONDS(1));
    if (setupMode) {
      sampleMenuInput();
    }
  }
    
}

void printSetupMenu() {
  Serial.print("\n+-------------------------------------------------------+"
               "\n|              The pump is in setup mode.               |"
               "\n+===+===================================================+"
               "\n| r | Run Self Test without rewind                      |"
               "\n+---+---------------------------------------------------+"
               "\n| R | Run Self Test WITH rewind                         |"
               "\n+---+---------------------------------------------------+"
               "\n| C | Calibrate Encoders                                |"
               "\n+---+---------------------------------------------------+"
               "\n| K | Set Pump Security Key                             |"
               "\n+---+---------------------------------------------------+"
               "\n| D | Drive Motors                                      |"
               "\n+===+===================================================+"
               "\n| X | Save Changes and Exit Setup Mode                  |"
               "\n+---+---------------------------------------------------+\n");
}

void sampleMenuInput() {
  int command = Serial.read();
  if (command == -1)
    return;
    
  while (Serial.available()) {    
    Serial.read();
  }
 
  switch (command) {
    case 'r':
      doRemoteSelfTest(true);
      break;
    case 'R':
      doRemoteSelfTest(false);
      break;
    case 'c':
    case 'C':
      calibrateEncoders();
      break;
    case 'k':
    case 'K':
      setPumpKey();
      break;
    case 'd':      
    case 'D':
      driveMotorMenu();
      break;
    case 'x':
    case 'X':
      saveAndEndSetup();
      break;
  }
   printSetupMenu();
}

void saveAndEndSetup() {
  Settings::getInstance().save();
  setupMode = false;
  Serial.println("Setup Complete!\nYou may now connect your phone to your pump...");
  delay(1000);
  Simblee_systemReset();
}

void setPumpKey() {  
  randomSeed(micros());
  Serial.println("\n"
                 "\n+----------------------------------------+ "
                 "\n| Please input 16 character security key | "
                 "\n+---+------------------------------------+ "
                 "\n| G | or type G to generate a random key | "
                 "\n+---+------------------------------------+ ");
  
  while (!Serial.available()) {
    delay(10);
  }

  String inputKey;
  inputKey = Serial.readString();
  if (inputKey.length() == 2 && (inputKey.charAt(0) == 'G' || inputKey.charAt(0) == 'g')) { // 1 plus null terminator
    inputKey = "\0";
  }

  char key[16];
  for (int i = 0; i < 16; i++) {
    if (i > inputKey.length() || inputKey.charAt(i) == '\0' || inputKey.charAt(i) == '\n') {
       //add random character
       int charIndex = random(possibleKeyChars.length());
       key[i] = possibleKeyChars.charAt(charIndex);
    }
    else {
      key[i] = inputKey.charAt(i); 
    }    
  }
  Serial.print("New Key: ");
  for (int i = 0; i < 16; i++) {
    Serial.print(key[i]);
  }
  Serial.println();
  
  Settings::getInstance().setSecurityKey(key);
}

void calibrateEncoders() {
  
  int oposites[1025];
  for (int i=0; i < 1024; i++) {
    oposites[i] = -1;
  }
  motor[0]->drive(true, 0.25f); 

  int enablePinA = 8;
  int encoderA = 5;
  int encoderB = 3;
  //turn encoders on
  digitalWrite(enablePinA, HIGH);
  long startTime = millis();
  while (startTime + SECONDS(10) > millis()) {
    int a = analogRead(encoderA);
    int b = analogRead(encoderB);
    oposites[a] = b;
  }
  //turn encoders off
  digitalWrite(enablePinA, LOW);
  motor[0]->stop();

  

  int encoderOffset[300];
  for (int i = 200; i < 300; i++) {
    if (oposites[i] > 0)
      encoderOffset[i] = oposites[i] + i;    
    else
      encoderOffset[i] = -1;
  }
  int count[1024];
  for (int i = 0; i < 1024; i++) {
    count[i] = 0;
  }
  for (int i = 200; i < 300; i++) {
    if (encoderOffset[i] > 0)
    count[encoderOffset[i]]++;
  }
  int max = 0;
  int maxIndex = 0;
  for (int i = 0; i < 1024; i++) {
    if (count[i] > max)  {
      max = count[i];
      maxIndex = i;
    }    
  }

  int calculatedOffset = maxIndex;

  Serial.print("CALCULATED OFFSET: ");
  Serial.println(calculatedOffset);

  for (int i = 0; i < 1024; i++) {
    if (i%10 == 0)
      Serial.println();
    if (i%100 == 0)
      Serial.println();
    Serial.print(oposites[i]);
    Serial.print(",\t");
  }


  int expectedOposite[1024];
  int value = calculatedOffset;
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

  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println();
  
  for (int i = 0; i < 1024; i++) {
    if (i%10 == 0)
      Serial.println();
    if (i%100 == 0)
      Serial.println();
    Serial.print(expectedOposite[i]);
    Serial.print(",\t");
  }

  Settings::getInstance().setEncoderOffset(0, maxIndex);
  
}

void driveMotorMenu() {
  
  while (true) {
    Serial.print("\n+-------------------------------------------------------+"
                 "\n|              The pump is ready to drive               |"
                 "\n+===+===================================================+"
                 "\n| f | Drive forward 1 second                            |"
                 "\n+---+---------------------------------------------------+"
                 "\n| F | Drive forward 5 seconds                           |"
                 "\n+---+---------------------------------------------------+"
                 "\n| r | Drive backward 1 second                           |"
                 "\n+---+---------------------------------------------------+"
                 "\n| R | Drive backward 5 seconds                          |"
                 "\n+===+===================================================+"
                 "\n| X | Exit                                              |"
                 "\n+---+---------------------------------------------------+\n");
    
    while (!Serial.available()) {
      delay(5);
    }
  
    int command = Serial.read();
    while (Serial.available()) {
      Serial.read();
    }
    switch (command) {
      case 'f':
        motor[0]->drive(true, 1.0f); 
        delay(1000);
        motor[0]->stop();
        break;
      case 'F':
        motor[0]->drive(true, 1.0f); 
        delay(5000);
        motor[0]->stop();
        break;
      case 'r':
        motor[0]->drive(false, 1.0f); 
        delay(1000);
        motor[0]->stop();
        break;
      case 'R':
        motor[0]->drive(false, 1.0f); 
        delay(5000);
        motor[0]->stop();
        break;
      case 'X':
      case 'x':
        return;
    }
  }
}
bool disconnected = false;



void doSelfTest() {
   motor[0]->reset();      
    if (testDosing(1) && testDosing(5) && testDosing(10))
      Serial.println("\n****Self Test was a Success!****");
    else
      Serial.println("\n****Self Test failed. Please see notes above****");
}


bool testDosing(int units) {
  Serial.print("Dosing ");
  Serial.print(units);
  Serial.println(units == 1 ? " unit..." : " units...");
 
  float amountDosed = motor[0]->dose(units, true);
  Serial.println();
  if (motor[0]->isOccluded()) {
    if (amountDosed < EPSILON) {
      Serial.println("Please verify that the piston has room to move (i.e. is not fully extended).\n"
          "We did not detect any motor movement.\n"          
          "If the motor moved freely, this likely indicates an issue with your encoders.\n"
          "If the motor made noise but did not move (or the motor has an electrical odor) then this likely indicates a bad motor.\n"
          "If there was no noise, this likely indicates a faulty H-bridge or solder issues.");
    }
    else {
      Serial.println("We detected an occlussion.\n"
      "If the motor moved freely, this likely indicates an issue with your encoders.");
    }
    return false;
  }
  
  float diff = amountDosed - units;
  if (diff > 0.05) {
    Serial.print("There was more insulin delivered than expected. (");
    Serial.print(diff);
    Serial.print(" units extra.)\nThis can either mean your motor is too powerful (you may try limiting motor power in the app), or there is an issue with your encoders.");
    return false;
  }
  else if (diff < -0.05) {
    Serial.print("There was less insulin delivered than expected. (");
    Serial.print(-diff);
    Serial.print(" units less than expected).\nThis likely means there is an issue with your encoders.");
    return false;
  }
  else
    return true;
}

float getBatteryVoltage() {
  
  analogReference(VBG); // Sets the Reference to 1.2V band gap           
  analogSelection(VDD1_3_PS);  //Selects VDD with 1/3 prescaling as the analog source
  int sensorValue = analogRead(1); // the pin has no meaning, it uses VDD pin
  float batteryVoltage = sensorValue * (3.6 / 1023.0);
  analogReference(DEFAULT);
  analogSelection(AIN_1_3_PS); // Selects the analog inputs with 1/3 prescaling as the analog source
  return batteryVoltage;
}

void sleepFor(long howLongToSleep) {
  setWakeTime(millis() + howLongToSleep);
}

void setWakeTime(long wakeTime) {
  awakeTime = wakeTime;
  connectionExpiration = 0;
}


void doseBasal() {
  Serial.println("Dosing basal");
  if (!motor[0]->isOccluded() && !motor[0]->isEncoderError())
    basalModel[0]->dose();
  if (!motor[1]->isOccluded() && !motor[1]->isEncoderError())
    basalModel[1]->dose();
//  basalModel[2]->dose();
}

void sleepUntilNextBasal() {

  //if it hasn't been set then we don't want to sleep / we shouldn't be in her if it's not set
  if (awakeTime == 0) {
    return;
  }

  //if it wasn't updated then we'll just assume it's supposed to dose basal at some 5 minute interval after the previous setting
  long now = millis();
  while (awakeTime <= now) {
    awakeTime += MINUTES(5);
  }
  unsigned long sleepTime = awakeTime - now;
  
  Serial.print("About to sleep for: ");
  Serial.println(sleepTime);
  Simblee_ULPDelay(sleepTime);
  
  bool timeHasPassed = awakeTime <= millis();
  if (timeHasPassed) {
    doseBasal();
  }
}

void extendConnectionExpiration() {
  unsigned long temp = millis() + SECONDS(5);
  if (connectionExpiration < temp)
    connectionExpiration = temp;
}


void setConnectionExpiration(unsigned long expiration) {
  if (expiration > connectionExpiration)
    connectionExpiration = expiration;
}

/**
   start the bluetooth stack
   wait 30 seconds for a connection
   if a connection comes, give it 60 seconds to finish
   when it's done, or the time has elapsed end the bluetooth stack
*/
void handleBluetooth() {
  connected = false;
  unsigned long sessionEnd = millis() + SECONDS(60);
  Serial.println("turn stack on");
  SimbleeBLE.begin();

  //wait 30 seconds for somebody to connect
  //if somebody conncted then keep bluetooth alive for 1 minute
  while (awakeTime == 0 || shouldWaitForInitialConnection(sessionEnd) || shouldWaitForClientExpire()) {
    Simblee_ULPDelay(SECONDS(10));
  }
  SimbleeBLE.end();
  Serial.println("turn stack off");
}

bool shouldWaitForInitialConnection(unsigned long sessionEnd) {
  return connectionExpiration == 0 && sessionEnd > millis();
}

bool shouldWaitForClientExpire() {
  return connectionExpiration > millis();
}
