// ============================================================
// LEFT GLOVE - RAW SENSOR TEST
// ============================================================

const int pThumb = 6;
const int pIndex = 4;
const int pMid   = 17;
const int pRing  = 15;
const int pPinky = 8;

void setup()
{
    Serial.begin(115200);
    delay(1500);

    analogReadResolution(12);

    Serial.println();
    Serial.println("================================");
    Serial.println("     LEFT GLOVE SENSOR TEST");
    Serial.println("================================");
    Serial.println();
}

void loop()
{
    int thumb = analogRead(pThumb);
    int index = analogRead(pIndex);
    int mid   = analogRead(pMid);
    int ring  = analogRead(pRing);
    int pinky = analogRead(pPinky);

    Serial.print("Thumb=");
    Serial.print(thumb);

    Serial.print(" | Index=");
    Serial.print(index);

    Serial.print(" | Mid=");
    Serial.print(mid);

    Serial.print(" | Ring=");
    Serial.print(ring);

    Serial.print(" | Pinky=");
    Serial.println(pinky);

    delay(500);
}