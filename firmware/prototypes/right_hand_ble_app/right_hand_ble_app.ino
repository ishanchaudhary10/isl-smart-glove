#include <Wire.h>
#include <math.h>
#include <esp_now.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- BLE SETUP ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

// --- RIGHT HAND PINS ---
const int pThumb = 17;
const int pIndex = 8;
const int pMid   = 6;
const int pRing  = 4;
const int pPinky = 15;
#define SDA_PIN 2
#define SCL_PIN 1
#define MPU_ADDR 0x68 

// --- CALIBRATION ---
int R_Thumb_cal[2] = {3850, 4095}; 
int R_Index_cal[2] = {800, 90}; 
int R_Mid_cal[2]   = {440, 100};
int R_Ring_cal[2]  = {690, 170};
int R_Pinky_cal[2] = {840, 260};

float R_pitch = 0; float R_roll = 0;

typedef struct struct_message {
    int f1; int f2; int f3; int f4; int f5;
    float pitch; float roll;
} struct_message;

struct_message leftHand; 

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; };
    void onDisconnect(BLEServer* pServer) { 
        deviceConnected = false; 
        pServer->getAdvertising()->start(); // Restart advertising so phone can find it again
    }
};

// Function to send message to BOTH Serial and the App
void sendBleMessage(String msg) {
    Serial.println("\n⭐⭐⭐ GESTURE: " + msg + " ⭐⭐⭐\n");
    if (deviceConnected) {
        pCharacteristic->setValue(msg.c_str());
        pCharacteristic->notify();
    }
}

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
    analogReadResolution(12);

    // 1. Initialize BLE
    BLEDevice::init("ESP32 BLE Monitor");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    pServer->getAdvertising()->start();

    // 2. Initialize ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) { esp_now_register_recv_cb(OnDataRecv); }

    // 3. Initialize MPU6050
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0); Wire.endTransmission();
    
    Serial.println("✅ Master Glove with BLE Ready!");
}

void loop() {
    // Read MPU Data
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
    if (Wire.requestFrom(MPU_ADDR, 6, true) == 6) {
         float ax = (Wire.read() << 8 | Wire.read()) / 16384.0;
         float ay = (Wire.read() << 8 | Wire.read()) / 16384.0;
         float az = (Wire.read() << 8 | Wire.read()) / 16384.0;
         R_pitch = atan2(ay, sqrt(ax * ax + az * az)) * 57.29;
         R_roll = atan2(-ax, az) * 57.29;
    }

    int R_f1 = getFingerPercent(analogRead(pThumb), R_Thumb_cal[0], R_Thumb_cal[1]);
    int R_f2 = getFingerPercent(analogRead(pIndex), R_Index_cal[0], R_Index_cal[1]);
    int R_f3 = getFingerPercent(analogRead(pMid),   R_Mid_cal[0],   R_Mid_cal[1]);
    int R_f4 = getFingerPercent(analogRead(pRing),  R_Ring_cal[0],  R_Ring_cal[1]);
    int R_f5 = getFingerPercent(analogRead(pPinky), R_Pinky_cal[0], R_Pinky_cal[1]);

    int L_f1 = leftHand.f1; int L_f2 = leftHand.f2; int L_f3 = leftHand.f3; 
    int L_f4 = leftHand.f4; int L_f5 = leftHand.f5;

    // --- GESTURE DICTIONARY ---
    if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && 
        L_f1 < 40 && L_f2 < 40 && L_f3 < 40 && L_f4 < 40 && L_f5 < 40 && R_pitch > 20) { 
        sendBleMessage("Stop (Two Hands)");
    }
    else if (R_f1 > 70 && R_f2 > 70 && R_f3 > 70 && R_f4 > 70 && R_f5 > 70 && 
             L_f1 > 70 && L_f2 > 70 && L_f3 > 70 && L_f4 > 70 && L_f5 > 70) { 
        sendBleMessage("Fist / Rock");
    }
    else if (R_f1 < 30 && R_f2 > 60 && R_f3 > 60 && R_f4 > 60 && R_f5 > 60 && 
             R_roll < -30 && R_roll > -130) { 
        sendBleMessage("Thumbs Up");
    }
    else if (R_f1 < 30 && R_f2 > 60 && R_f3 > 60 && R_f4 > 60 && R_f5 > 60 && R_pitch < -30) { 
        sendBleMessage("Thumbs Down");
    }
    else if (R_f1 > 60 && R_f2 < 40 && R_f3 < 40 && R_f4 > 60 && R_f5 > 60 && R_pitch > 20) { 
        sendBleMessage("Peace");
    }
    else if (R_f1 > 60 && R_f2 < 40 && R_f3 > 60 && R_f4 > 60 && R_f5 > 60 && 
             R_pitch > -30 && R_pitch < 30) { 
        sendBleMessage("Look There (Point)");
    }
    else if (R_f1 < 40 && R_f2 < 40 && R_f3 > 60 && R_f4 > 60 && R_f5 > 60 && 
             R_pitch > -30 && R_pitch < 30) { 
        sendBleMessage("Finger Gun");
    }
    else if (R_f1 < 40 && R_f2 > 60 && R_f3 > 60 && R_f4 > 60 && R_f5 < 40 && 
             (R_roll < -50 || R_roll > 50)) { 
        sendBleMessage("Call Me");
    }
    else if (R_f1 > 60 && R_f2 < 40 && R_f3 > 60 && R_f4 > 60 && R_f5 < 40 && R_pitch > 20) { 
        sendBleMessage("Rock On");
    }
    else if (R_f1 > 40 && R_f1 < 80 && R_f2 > 40 && R_f2 < 80 && R_f3 < 30 && R_f4 < 30 && R_f5 < 30) { 
        sendBleMessage("OK!");
    }
    else if (R_f1 < 40 && R_f2 < 40 && R_f3 > 70 && R_f4 > 70 && R_f5 < 40 && R_pitch > 20) { 
        sendBleMessage("I Love You");
    }
    else if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && 
             L_f1 < 40 && L_f2 < 40 && L_f3 < 40 && L_f4 < 40 && L_f5 < 40 &&
             R_pitch < 10 && R_pitch > -40) { 
        sendBleMessage("Calm Down");
    }
    else if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && 
             L_f1 < 40 && L_f2 < 40 && L_f3 < 40 && L_f4 < 40 && L_f5 < 40 &&
             (R_roll > 130 || R_roll < -130)) { 
        sendBleMessage("Palms Up / Offering");
    }
    else if (R_f1 < 40 && R_f2 < 40 && R_f3 > 80 && R_f4 > 80 && R_f5 < 40 && R_pitch > 30) { 
        sendBleMessage("Spiderman");
    }
    else if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 > 60 && R_f5 > 60 && R_pitch > 20) { 
        sendBleMessage("Number 3");
    }
    else if (R_f1 > 60 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && R_pitch > 20) { 
        sendBleMessage("Number 4");
    }
    else if (R_f1 > 60 && R_f2 > 60 && R_f3 > 60 && R_f4 > 60 && R_f5 < 40 && R_pitch > -20) { 
        sendBleMessage("Pinky Promise");
    }
    else if (R_f1 > 30 && R_f1 < 80 && R_f2 > 30 && R_f2 < 80 && R_f3 > 30 && R_f3 < 80 && 
             R_f4 > 30 && R_f4 < 80 && R_f5 > 30 && R_f5 < 80 &&
             L_f1 > 30 && L_f1 < 80 && L_f2 > 30 && L_f2 < 80 && L_f3 > 30 && L_f3 < 80) { 
        sendBleMessage("Claw / Zombie");
    }
    else if (R_f1 > 50 && R_f2 > 50 && R_f3 > 50 && R_f4 > 50 && R_f5 < 30) { 
        sendBleMessage("Fancy / Tea Time");
    }
    else if (R_f1 < 40 && R_f2 < 40 && R_f3 < 40 && R_f4 < 40 && R_f5 < 40 && R_pitch > 40) { 
        sendBleMessage("Wait");
    }
    else if (R_f1 > 30 && R_f1 < 70 && R_f2 > 30 && R_f2 < 70 && R_f3 > 30 && R_f3 < 70 && 
             R_f4 > 70 && R_f5 > 70) { 
        sendBleMessage("Money / Pay Me");
    }

    delay(300); // Slightly faster response time
}