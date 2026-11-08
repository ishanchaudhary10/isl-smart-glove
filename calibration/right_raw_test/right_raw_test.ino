// --- RIGHT HAND PINS ---
const int pThumb = 17;
const int pIndex = 8;
const int pMid   = 6;
const int pRing  = 4;
const int pPinky = 15;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // Sets the ESP32 to read from 0 to 4095
  Serial.println("--- RIGHT GLOVE CALIBRATION READY ---");
}

void loop() {
  int r1 = analogRead(pThumb);
  int r2 = analogRead(pIndex);
  int r3 = analogRead(pMid);
  int r4 = analogRead(pRing);
  int r5 = analogRead(pPinky);

  Serial.printf("RIGHT RAW -> Thumb: %d | Index: %d | Mid: %d | Ring: %d | Pinky: %d \n", r1, r2, r3, r4, r5);
  
  delay(500); 
}