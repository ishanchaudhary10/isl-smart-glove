#include <Wire.h>
#include <math.h>
#include <esp_now.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define DEVICE_NAME         "ESP32-S3-BLE"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

const int pThumb = 17, pIndex = 8, pMid = 6, pRing = 4, pPinky = 15;
#define SDA_PIN 2
#define SCL_PIN 1
#define MPU_ADDR 0x68

// YOUR NEW CALIBRATED VALUES
int R_Thumb_cal[2] = {4095, 3230};
int R_Index_cal[2] = {489, 95};
int R_Mid_cal[2]   = {264, 95};
int R_Ring_cal[2]  = {461, 175};
int R_Pinky_cal[2] = {331, 220};

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

typedef struct struct_message {
    int f1; int f2; int f3; int f4; int f5;
    float pitch; float roll;
} struct_message;

struct_message leftHand;

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    memcpy(&leftHand, incomingData, sizeof(leftHand));
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* s) override { deviceConnected = true; }
    void onDisconnect(BLEServer* s) override { deviceConnected = false; }
};

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

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) esp_now_register_recv_cb(OnDataRecv);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B); Wire.write(0);
    Wire.endTransmission();

    BLEDevice::init(DEVICE_NAME);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("Ready for Python Script to connect...");
}

void loop() {
    if (!deviceConnected) {
        delay(500);
        return;
    }

    float R_pitch = 0, R_roll = 0;
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
    if (Wire.requestFrom(MPU_ADDR, 6, true) == 6) {
        float ax = (Wire.read() << 8 | Wire.read()) / 16384.0;
        float ay = (Wire.read() << 8 | Wire.read()) / 16384.0;
        float az = (Wire.read() << 8 | Wire.read()) / 16384.0;
        R_pitch = atan2(ay, sqrt(ax * ax + az * az)) * 57.29;
        R_roll  = atan2(-ax, az) * 57.29;
    }

    int R_f1 = getFingerPercent(analogRead(pThumb), R_Thumb_cal[0], R_Thumb_cal[1]);
    int R_f2 = getFingerPercent(analogRead(pIndex), R_Index_cal[0], R_Index_cal[1]);
    int R_f3 = getFingerPercent(analogRead(pMid),   R_Mid_cal[0],   R_Mid_cal[1]);
    int R_f4 = getFingerPercent(analogRead(pRing),  R_Ring_cal[0],  R_Ring_cal[1]);
    int R_f5 = getFingerPercent(analogRead(pPinky), R_Pinky_cal[0], R_Pinky_cal[1]);

    // Format: 14 comma-separated values for the Python script
    String dataStr = String(R_f1) + "," + String(R_f2) + "," + String(R_f3) + "," + 
                     String(R_f4) + "," + String(R_f5) + "," + String(R_pitch, 1) + "," + String(R_roll, 1) + "," +
                     String(leftHand.f1) + "," + String(leftHand.f2) + "," + String(leftHand.f3) + "," + 
                     String(leftHand.f4) + "," + String(leftHand.f5) + "," + String(leftHand.pitch, 1) + "," + String(leftHand.roll, 1);

    pCharacteristic->setValue(dataStr.c_str());
    pCharacteristic->notify();
    
    Serial.println(dataStr);
    delay(50); // Send data fast enough for the script to catch 30 samples quickly
}