// --- LEFT HAND PINS ---
const int pThumb = 6;
const int pIndex = 4;
const int pMid   = 17;
const int pRing  = 15;
const int pPinky = 8;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); 
  Serial.println("--- LEFT GLOVE CALIBRATION READY ---");
}

void loop() {
  int l1 = analogRead(pThumb);
  int l2 = analogRead(pIndex);
  int l3 = analogRead(pMid);
  int l4 = analogRead(pRing);
  int l5 = analogRead(pPinky);

  Serial.printf("LEFT RAW  -> Thumb: %d | Index: %d | Mid: %d | Ring: %d | Pinky: %d \n", l1, l2, l3, l4, l5);
  
  delay(500); 
}