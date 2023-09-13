int expectedOposites[1025];

int count = 0;
long lastUpdate = 0;
int lastReportedCount = 0;

int encoderPinA = 5;
int encoderPinB = 3;
int enablePinA = 8;
int enablePinB = 9;

int motorPinA = 29;
int motorPinB = 30;

void setup() {
  pinMode(3, INPUT_NOPULL);
  pinMode(5, INPUT_NOPULL);

  
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);

  pinMode(9, OUTPUT);
  digitalWrite(9, HIGH);
  
  Serial.begin(9600);

  for (int i = 0; i < 1025; i++) {
    expectedOposites[i] = 0;
  }

  pinMode(motorPinA, OUTPUT);
  pinMode(motorPinB, OUTPUT);

  digitalWrite(motorPinA, HIGH);
  analogWrite(motorPinB, (int)(255 * 0.2f));
  lastUpdate = millis();
}

void loop() {
  
  int a = analogRead(5);
  int b = analogRead(3);

  if (expectedOposites[a] == 0) {
    expectedOposites[a] = b;
    count++;
  }

  if (lastUpdate < millis() - 1000) {
    lastUpdate = millis();
    if (lastReportedCount != count) {
      Serial.println(count);
      lastReportedCount = count;
    }
    if (count >= 1015) {
      digitalWrite(motorPinB, LOW);
      printArray();
      delay(1000000);
    }
  }

}

void printArray() {
  for (int i = 0; i < 1025; i++) {
    if (i%10 == 0)
      Serial.println();
    if (i%100 == 0) 
      Serial.println();
    Serial.print(expectedOposites[i]);
    Serial.print(",");
  }
}

