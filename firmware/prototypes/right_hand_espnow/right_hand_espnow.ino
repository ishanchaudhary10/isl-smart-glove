#include <Wire.h>
#include <math.h>
#include <esp_now.h>
#include <WiFi.h>

// --- RIGHT HAND PINS ---
const int pThumb = 17;
const int pIndex = 8;
const int pMid   = 6;
const int pRing  = 4;
const int pPinky = 15;
#define SDA_PIN 2
#define SCL_PIN 1
#define MPU_ADDR 0x68 

// ==========================================
// --- RIGHT HAND CALIBRATION ---
int R_Thumb_cal[2] = {3850, 4095}; 
int R_Index_cal[2] = {800, 90}; 
int R_Mid_cal[2]   = {440, 100};
int R_Ring_cal[2]  = {690, 170};
int R_Pinky_cal[2] = {840, 260};
// ==========================================

String lastMessage = "";
unsigned long lastSendTime = 0;

float R_pitch = 0; float R_roll = 0;

typedef struct struct_message {
    int f1; int f2; int f3; int f4; int f5;
    float pitch; float roll;
} struct_message;

struct_message leftHand; 

// Callback function to receive data from Left Glove
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&leftHand, incomingData, sizeof(leftHand));
}

int getFingerPercent(int raw, int straight_edge, int bent_edge) {
    if (straight_edge < bent_edge) {
        if (raw <= straight_edge) return 0;
        if (raw >= bent_edge) return 100;
    } else {
        if (raw >= straight_edge) return 0;
        if (raw <= bent_edge) return 100;
    }
    return map(raw, straight_edge, bent_edge, 0, 100);
}

void setup() {
  Serial.begin(115200);
  delay(3000); // Give the USB port 3 seconds to wake up

  analogReadResolution(12);

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) { esp_now_register_recv_cb(OnDataRecv); }

  // Initialize MPU6050
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0); Wire.endTransmission();
  
  Serial.println("✅ Serial Master Glove Ready! Make a sign...");
}

void loop() {
  // Read Right MPU
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  if (Wire.requestFrom(MPU_ADDR, 6, true) == 6) {
     float ax = (Wire.read() << 8 | Wire.read()) / 16384.0;
     float ay = (Wire.read() << 8 | Wire.read()) / 16384.0;
     float az = (Wire.read() << 8 | Wire.read()) / 16384.0;
     R_pitch = atan2(ay, sqrt(ax * ax + az * az)) * 57.29;
     R_roll = atan2(-ax, az) * 57.29;
  }

  // Map Right Fingers
  int R_f1 = getFingerPercent(analogRead(pThumb), R_Thumb_cal[0], R_Thumb_cal[1]);
  int R_f2 = getFingerPercent(analogRead(pIndex), R_Index_cal[0], R_Index_cal[1]);
  int R_f3 = getFingerPercent(analogRead(pMid),   R_Mid_cal[0],   R_Mid_cal[1]);
  int R_f4 = getFingerPercent(analogRead(pRing),  R_Ring_cal[0],  R_Ring_cal[1]);
  int R_f5 = getFingerPercent(analogRead(pPinky), R_Pinky_cal[0], R_Pinky_cal[1]);

  // Pull Left Hand Data
  int L_f1 = leftHand.f1; int L_f2 = leftHand.f2; int L_f3 = leftHand.f3; 
  int L_f4 = leftHand.f4; int L_f5 = leftHand.f5;
  float L_pitch = leftHand.pitch; float L_roll = leftHand.roll;

  // Print Hand Snapshots
  Serial.printf("RIGHT -> F: %d,%d,%d,%d,%d | Pitch: %.1f | Roll: %.1f \n", R_f1, R_f2, R_f3, R_f4, R_f5, R_pitch, R_roll);
  Serial.printf("LEFT  -> F: %d,%d,%d,%d,%d | Pitch: %.1f | Roll: %.1f \n", L_f1, L_f2, L_f3, L_f4, L_f5, L_pitch, L_roll);
  Serial.println("---------------------------------------------------");

  // ==========================================
  // --- GESTURE DICTIONARY ---
  // ==========================================
  
  // 1. "STOP" (Two Hands) - Both hands flat, hands tilted up (Pitch > 20)
  if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && 
      L_f1 < 40 && L_f2 < 40 && L_f3 < 40 && L_f4 < 40 && L_f5 < 40 &&
      R_pitch > 20) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Stop (Two Hands) ⭐⭐⭐\n");
  }

  // 2. "FIST / ROCK" - Both hands completely closed tightly
  else if (R_f1 > 70 && R_f2 > 70 && R_f3 > 70 && R_f4 > 70 && R_f5 > 70 && 
           L_f1 > 70 && L_f2 > 70 && L_f3 > 70 && L_f4 > 70 && L_f5 > 70) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Fist / Rock ⭐⭐⭐\n");
  }

  // 3. "THUMBS UP" - Thumb open, all others closed, hand sideways
  else if (R_f1 < 30 && R_f2 > 60 && R_f3 > 60 && R_f4 > 60 && R_f5 > 60 && 
           R_roll < -30 && R_roll > -130) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Thumbs Up ⭐⭐⭐\n");
  }

  // 4. "THUMBS DOWN" - Thumb open, others closed, pitch pointing to the floor
  else if (R_f1 < 30 && R_f2 > 60 && R_f3 > 60 && R_f4 > 60 && R_f5 > 60 && 
           R_pitch < -30) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Thumbs Down (Bad) ⭐⭐⭐\n");
  }

  // 5. "PEACE / V-SIGN" - Index & Mid open, others closed, upright
  else if (R_f1 > 60 && R_f2 < 40 && R_f3 < 40 && R_f4 > 60 && R_f5 > 60 && 
           R_pitch > 20) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Peace ⭐⭐⭐\n");
  }

  // 6. "POINTING FORWARD" - Index open, others closed, hand flat/forward
  else if (R_f1 > 60 && R_f2 < 40 && R_f3 > 60 && R_f4 > 60 && R_f5 > 60 && 
           R_pitch > -30 && R_pitch < 30) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Look There (Point) ⭐⭐⭐\n");
  }

  // 7. "FINGER GUN / PISTOL" - Thumb & Index open, others closed, pointing forward
  else if (R_f1 < 40 && R_f2 < 40 && R_f3 > 60 && R_f4 > 60 && R_f5 > 60 && 
           R_pitch > -30 && R_pitch < 30) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Finger Gun / Pistol ⭐⭐⭐\n");
  }

  // 8. "CALL ME / PHONE" - Thumb & Pinky open, middle three closed
  else if (R_f1 < 40 && R_f2 > 60 && R_f3 > 60 && R_f4 > 60 && R_f5 < 40 && 
           (R_roll < -50 || R_roll > 50)) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Call Me ⭐⭐⭐\n");
  }

  // 9. "ROCK ON / HORNS" - Index & Pinky open, Thumb/Mid/Ring closed
  else if (R_f1 > 60 && R_f2 < 40 && R_f3 > 60 && R_f4 > 60 && R_f5 < 40 && 
           R_pitch > 20) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Rock On ⭐⭐⭐\n");
  }

  // 10. "OK SIGN" - Thumb & Index touching (half bent), other 3 open
  else if (R_f1 > 40 && R_f1 < 80 && R_f2 > 40 && R_f2 < 80 && 
           R_f3 < 30 && R_f4 < 30 && R_f5 < 30) { 
      Serial.println("\n⭐⭐⭐ GESTURE: OK! ⭐⭐⭐\n");
  }

  // 11. "I LOVE YOU (ASL)" - Thumb, Index, Pinky open. Mid & Ring closed.
  else if (R_f1 < 40 && R_f2 < 40 && R_f3 > 70 && R_f4 > 70 && R_f5 < 40 && 
           R_pitch > 20) { 
      Serial.println("\n⭐⭐⭐ GESTURE: I Love You ⭐⭐⭐\n");
  }

  // 12. "PALMS DOWN / CALM DOWN" - Both hands flat, hands horizontal/facing floor
  else if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && 
           L_f1 < 40 && L_f2 < 40 && L_f3 < 40 && L_f4 < 40 && L_f5 < 40 &&
           R_pitch < 10 && R_pitch > -40) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Calm Down (Palms Down) ⭐⭐⭐\n");
  }

  // 13. "PALMS UP / OFFERING" - Both hands flat, hands rolled upside down
  else if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && 
           L_f1 < 40 && L_f2 < 40 && L_f3 < 40 && L_f4 < 40 && L_f5 < 40 &&
           (R_roll > 130 || R_roll < -130)) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Palms Up / Offering ⭐⭐⭐\n");
  }

  // 14. "SPIDERMAN / WEB" - Mid & Ring fully bent, others open, pitch up
  else if (R_f1 < 40 && R_f2 < 40 && R_f3 > 80 && R_f4 > 80 && R_f5 < 40 && 
           R_pitch > 30) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Spiderman ⭐⭐⭐\n");
  }

  // 15. "NUMBER 3 (ASL)" - Thumb, Index, Middle open. Ring & Pinky closed.
  else if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 > 60 && R_f5 > 60 && 
           R_pitch > 20) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Number 3 ⭐⭐⭐\n");
  }

  // 16. "NUMBER 4" - Thumb closed across palm, other 4 fingers completely open
  else if (R_f1 > 60 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && 
           R_pitch > 20) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Number 4 ⭐⭐⭐\n");
  }

  // 17. "PINKY PROMISE" - Pinky open, all other 4 fingers closed tight
  else if (R_f1 > 60 && R_f2 > 60 && R_f3 > 60 && R_f4 > 60 && R_f5 < 40 && 
           R_pitch > -20) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Pinky Promise ⭐⭐⭐\n");
  }

  // 18. "CLAW / ZOMBIE" - All 10 fingers are half-bent, like holding a ball
  else if (R_f1 > 30 && R_f1 < 80 && R_f2 > 30 && R_f2 < 80 && R_f3 > 30 && R_f3 < 80 && 
           R_f4 > 30 && R_f4 < 80 && R_f5 > 30 && R_f5 < 80 &&
           L_f1 > 30 && L_f1 < 80 && L_f2 > 30 && L_f2 < 80 && L_f3 > 30 && L_f3 < 80) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Claw / Zombie ⭐⭐⭐\n");
  }

  // 19. "FANCY DRINK / TEA TIME" - Right pinky open, other 4 fingers gripping/bent
  else if (R_f1 > 50 && R_f2 > 50 && R_f3 > 50 && R_f4 > 50 && R_f5 < 30) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Fancy / Tea Time ⭐⭐⭐\n");
  }

  // 20. "WAIT / ONE HAND STOP" - Right hand flat, upright (Left hand doing whatever)
  else if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && 
           R_pitch > 40) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Wait (One Hand Stop) ⭐⭐⭐\n");
  }

  // 21. "MONEY / PAY ME" - Thumb, Index, Mid rubbing together (half bent). Ring & Pinky closed.
  else if (R_f1 > 30 && R_f1 < 70 && R_f2 > 30 && R_f2 < 70 && R_f3 > 30 && R_f3 < 70 && 
           R_f4 > 70 && R_f5 > 70) { 
      Serial.println("\n⭐⭐⭐ GESTURE: Money / Pay Me ⭐⭐⭐\n");
  }

  delay(500); // Wait half a second before reading the hands again
}