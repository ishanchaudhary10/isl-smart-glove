// ============================================================
// VAKYA - RIGHT GLOVE CALIBRATION
// ESP32-S3
// ============================================================

const int pThumb = 17;
const int pIndex = 6;
const int pMid   = 8;
const int pRing  = 4;
const int pPinky = 15;

const int NUM_READINGS = 100;


// ============================================================
// AVERAGE SENSOR
// ============================================================

int readAverage(int pin)
{
    long total = 0;

    for (int i = 0; i < NUM_READINGS; i++)
    {
        total += analogRead(pin);
        delay(10);
    }

    return total / NUM_READINGS;
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1500);

    analogReadResolution(12);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("        VAKYA RIGHT GLOVE CALIBRATION");
    Serial.println("==============================================");
    Serial.println();

    Serial.println("RIGHT HAND PINS:");
    Serial.println("Thumb : GPIO 17");
    Serial.println("Index : GPIO 8");
    Serial.println("Middle: GPIO 6");
    Serial.println("Ring  : GPIO 4");
    Serial.println("Pinky : GPIO 15");
    Serial.println();

    Serial.println("Each position will use 100 readings.");
    Serial.println("Do not force the fingers.");
    Serial.println();


    // ========================================================
    // STRAIGHT POSITION
    // ========================================================

    Serial.println("==============================================");
    Serial.println("STEP 1 - STRAIGHT");
    Serial.println("==============================================");
    Serial.println();

    Serial.println("Keep ALL FIVE fingers naturally STRAIGHT.");
    Serial.println();

    Serial.println("Starting in 3 seconds...");
    delay(1000);

    Serial.println("2...");
    delay(1000);

    Serial.println("1...");
    delay(1000);

    Serial.println();
    Serial.println("READING STRAIGHT VALUES...");
    Serial.println();


    int thumbStraight = readAverage(pThumb);
    int indexStraight = readAverage(pIndex);
    int midStraight   = readAverage(pMid);
    int ringStraight  = readAverage(pRing);
    int pinkyStraight = readAverage(pPinky);


    Serial.println("STRAIGHT VALUES:");
    Serial.println();

    Serial.print("Thumb : ");
    Serial.println(thumbStraight);

    Serial.print("Index : ");
    Serial.println(indexStraight);

    Serial.print("Middle: ");
    Serial.println(midStraight);

    Serial.print("Ring  : ");
    Serial.println(ringStraight);

    Serial.print("Pinky : ");
    Serial.println(pinkyStraight);

    Serial.println();


    // ========================================================
    // BENT POSITION
    // ========================================================

    Serial.println("==============================================");
    Serial.println("STEP 2 - BENT");
    Serial.println("==============================================");
    Serial.println();

    Serial.println("Now bend ALL FIVE fingers.");
    Serial.println();
    Serial.println("Bend them naturally as far as you normally");
    Serial.println("would during your ISL signs.");
    Serial.println();
    Serial.println("DO NOT force the sensors.");
    Serial.println();

    Serial.println("Starting in 3 seconds...");
    delay(1000);

    Serial.println("2...");
    delay(1000);

    Serial.println("1...");
    delay(1000);

    Serial.println();
    Serial.println("READING BENT VALUES...");
    Serial.println();


    int thumbBent = readAverage(pThumb);
    int indexBent = readAverage(pIndex);
    int midBent   = readAverage(pMid);
    int ringBent  = readAverage(pRing);
    int pinkyBent = readAverage(pPinky);


    Serial.println("BENT VALUES:");
    Serial.println();

    Serial.print("Thumb : ");
    Serial.println(thumbBent);

    Serial.print("Index : ");
    Serial.println(indexBent);

    Serial.print("Middle: ");
    Serial.println(midBent);

    Serial.print("Ring  : ");
    Serial.println(ringBent);

    Serial.print("Pinky : ");
    Serial.println(pinkyBent);

    Serial.println();


    // ========================================================
    // FINAL CALIBRATION VALUES
    // ========================================================

    Serial.println("==============================================");
    Serial.println("       FINAL RIGHT CALIBRATION");
    Serial.println("==============================================");
    Serial.println();

    Serial.println("// Copy these into RIGHT ESP32 code:");
    Serial.println();


    Serial.print("int R_Thumb_cal[2] = {");
    Serial.print(thumbStraight);
    Serial.print(", ");
    Serial.print(thumbBent);
    Serial.println("};");


    Serial.print("int R_Index_cal[2] = {");
    Serial.print(indexStraight);
    Serial.print(", ");
    Serial.print(indexBent);
    Serial.println("};");


    Serial.print("int R_Mid_cal[2] = {");
    Serial.print(midStraight);
    Serial.print(", ");
    Serial.print(midBent);
    Serial.println("};");


    Serial.print("int R_Ring_cal[2] = {");
    Serial.print(ringStraight);
    Serial.print(", ");
    Serial.print(ringBent);
    Serial.println("};");


    Serial.print("int R_Pinky_cal[2] = {");
    Serial.print(pinkyStraight);
    Serial.print(", ");
    Serial.print(pinkyBent);
    Serial.println("};");


    Serial.println();
    Serial.println("==============================================");
    Serial.println("       CALIBRATION COMPLETE");
    Serial.println("==============================================");
    Serial.println();

    Serial.println("SAVE THESE VALUES.");
    Serial.println();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // Calibration runs only once.
}