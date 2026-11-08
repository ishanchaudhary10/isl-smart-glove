// ============================================================
// VAKYA SMART GLOVE
// ESP32-S3 + Edge Impulse + BLE + ESP-NOW
//
// MATCHED TO NEW EDGE IMPULSE LIBRARY:
// VAKYA_ISL_Smart_Glove_inferencing
//
// MODEL CONFIGURATION:
// 14 features/sample
// 60 samples/window
// 840 total features
// 20 Hz sampling
// 50 ms interval
// ============================================================

#include <Wire.h>
#include <math.h>

#include <esp_now.h>
#include <WiFi.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// NEW EDGE IMPULSE LIBRARY
// ============================================================

#include <VAKYA_ISL_Smart_Glove_inferencing.h>


// ============================================================
// BLE
// ============================================================

#define DEVICE_NAME "ESP32-S3-BLE"

#define SERVICE_UUID \
"4fafc201-1fb5-459e-8fcc-c5c9c331914b"

#define CHARACTERISTIC_UUID \
"beb5483e-36e1-4688-b7f5-ea07361b26a8"


// ============================================================
// RIGHT HAND FLEX SENSOR PINS
// ============================================================

const int pThumb = 17;
const int pIndex = 8;
const int pMid   = 6;
const int pRing  = 4;
const int pPinky = 15;


// ============================================================
// MPU6050
// ============================================================

#define SDA_PIN 2
#define SCL_PIN 1
#define MPU_ADDR 0x68


// ============================================================
// RIGHT HAND CALIBRATION
// {STRAIGHT, BENT}
// ============================================================

int R_Thumb_cal[2] = {3485, 3159};
int R_Index_cal[2] = {990, 1172};
int R_Mid_cal[2]   = {1103, 1166};
int R_Ring_cal[2]  = {859, 1069};
int R_Pinky_cal[2] = {1061, 1572};


// ============================================================
// BLE GLOBALS
// ============================================================

BLEServer* pServer = nullptr;

BLECharacteristic* pCharacteristic = nullptr;

bool deviceConnected = false;
bool wasConnected = false;


// ============================================================
// LEFT HAND ESP-NOW DATA
// ============================================================

typedef struct struct_message {

    int f1;
    int f2;
    int f3;
    int f4;
    int f5;

    float pitch;
    float roll;

} struct_message;


struct_message leftHand = {
    0, 0, 0, 0, 0,
    0.0,
    0.0
};


// ============================================================
// ESP-NOW RECEIVE CALLBACK
// ============================================================

void OnDataRecv(
    const esp_now_recv_info *info,
    const uint8_t *incomingData,
    int len
) {

    if (len == sizeof(leftHand)) {

        memcpy(
            &leftHand,
            incomingData,
            sizeof(leftHand)
        );
    }
}


// ============================================================
// BLE STATE
// ============================================================

String lastSentMsg = "";

unsigned long lastSendTime = 0;

const unsigned long SEND_INTERVAL_MS = 500;


// ============================================================
// EDGE IMPULSE MODEL CONFIGURATION
// MATCHED TO YOUR NEW ZIP
// ============================================================

#define NUM_FEATURES 14
#define NUM_SAMPLES 60

#define TOTAL_FEATURES \
(NUM_FEATURES * NUM_SAMPLES)


// ============================================================
// MAKE SURE THE MODEL REALLY EXPECTS 840
// ============================================================

#if EI_CLASSIFIER_RAW_SAMPLE_COUNT != 60
#error "Edge Impulse model does not use 60 samples."
#endif

#if EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 14
#error "Edge Impulse model does not use 14 features per sample."
#endif

#if EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE != 840
#error "Edge Impulse model does not expect 840 raw features."
#endif


// ============================================================
// FEATURE BUFFER
// ============================================================

float featureBuffer[TOTAL_FEATURES];

int sampleIndex = 0;


// ============================================================
// IMPORTANT:
// YOUR MODEL USES 20 Hz
//
// 1000 ms / 20 = 50 ms
// ============================================================

const unsigned long SAMPLE_INTERVAL_MS = 50;

unsigned long lastSampleTime = 0;


// ============================================================
// BLE CALLBACKS
// ============================================================

class ServerCallbacks : public BLEServerCallbacks {

    void onConnect(BLEServer* s) override {

        deviceConnected = true;

        Serial.println();
        Serial.println(
            "[BLE] PHONE CONNECTED"
        );
    }


    void onDisconnect(BLEServer* s) override {

        deviceConnected = false;

        Serial.println();
        Serial.println(
            "[BLE] PHONE DISCONNECTED"
        );
    }
};


// ============================================================
// FLEX SENSOR CALIBRATION
// ============================================================

int getFingerPercent(
    int raw,
    int straight_edge,
    int bent_edge
) {

    int percentage;


    if (straight_edge < bent_edge) {

        if (raw <= straight_edge)
            return 0;

        if (raw >= bent_edge)
            return 100;

    }

    else {

        if (raw >= straight_edge)
            return 0;

        if (raw <= bent_edge)
            return 100;
    }


    percentage =
        map(
            raw,
            straight_edge,
            bent_edge,
            0,
            100
        );


    if (percentage < 0)
        percentage = 0;

    if (percentage > 100)
        percentage = 100;


    return percentage;
}


// ============================================================
// READ MPU6050
// ============================================================

void readMPU(
    float &pitch,
    float &roll
) {

    pitch = 0.0;
    roll = 0.0;


    Wire.beginTransmission(
        MPU_ADDR
    );

    Wire.write(0x3B);

    Wire.endTransmission(false);


    if (
        Wire.requestFrom(
            MPU_ADDR,
            6,
            true
        ) == 6
    ) {

        int16_t rawAx =
            (Wire.read() << 8) |
            Wire.read();

        int16_t rawAy =
            (Wire.read() << 8) |
            Wire.read();

        int16_t rawAz =
            (Wire.read() << 8) |
            Wire.read();


        float ax =
            rawAx / 16384.0;

        float ay =
            rawAy / 16384.0;

        float az =
            rawAz / 16384.0;


        pitch =
            atan2(
                ay,
                sqrt(
                    ax * ax +
                    az * az
                )
            ) * 57.29;


        roll =
            atan2(
                -ax,
                az
            ) * 57.29;
    }
}


// ============================================================
// SEND GESTURE TO PHONE
// ============================================================

void sendBLE(
    String msg
) {

    if (!deviceConnected) {

        Serial.println(
            "[BLE] Phone not connected"
        );

        return;
    }


    unsigned long now = millis();


    if (
        now - lastSendTime <
        SEND_INTERVAL_MS
    ) {

        return;
    }


    lastSendTime = now;


    pCharacteristic->setValue(
        msg.c_str()
    );


    pCharacteristic->notify();


    Serial.println();
    Serial.println(
        "********************************"
    );

    Serial.print(
        "[BLE TX] SENT: "
    );

    Serial.println(
        msg
    );

    Serial.println(
        "********************************"
    );
}


// ============================================================
// SETUP
// ============================================================

void setup() {

    Serial.begin(115200);

    delay(1500);


    Serial.println();
    Serial.println(
        "=========================================="
    );

    Serial.println(
        "       VAKYA SMART GLOVE"
    );

    Serial.println(
        "=========================================="
    );


    // ========================================================
    // ADC
    // ========================================================

    analogReadResolution(12);


    // ========================================================
    // ESP-NOW
    // ========================================================

    WiFi.mode(WIFI_STA);


    if (
        esp_now_init() ==
        ESP_OK
    ) {

        esp_now_register_recv_cb(
            OnDataRecv
        );

        Serial.println(
            "[ESP-NOW] Ready"
        );

    }

    else {

        Serial.println(
            "[ESP-NOW] FAILED"
        );
    }


    // ========================================================
    // MPU6050
    // ========================================================

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );


    Wire.beginTransmission(
        MPU_ADDR
    );

    Wire.write(0x6B);

    Wire.write(0);


    if (
        Wire.endTransmission() == 0
    ) {

        Serial.println(
            "[MPU6050] Connected"
        );

    }

    else {

        Serial.println(
            "[MPU6050] NOT DETECTED"
        );
    }


    // ========================================================
    // BLE
    // ========================================================

    BLEDevice::init(
        DEVICE_NAME
    );


    pServer =
        BLEDevice::createServer();


    pServer->setCallbacks(
        new ServerCallbacks()
    );


    BLEService* pService =
        pServer->createService(
            SERVICE_UUID
        );


    pCharacteristic =
        pService->createCharacteristic(

            CHARACTERISTIC_UUID,

            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY
        );


    pCharacteristic->addDescriptor(
        new BLE2902()
    );


    pCharacteristic->setValue(
        "Waiting for gesture..."
    );


    pService->start();


    BLEAdvertising* pAdv =
        BLEDevice::getAdvertising();


    pAdv->addServiceUUID(
        SERVICE_UUID
    );


    pAdv->setScanResponse(
        true
    );


    BLEDevice::startAdvertising();


    // ========================================================
    // DISPLAY MODEL INFORMATION
    // ========================================================

    Serial.println();
    Serial.println(
        "========== ML CONFIG =========="
    );


    Serial.print(
        "Features/sample: "
    );

    Serial.println(
        EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME
    );


    Serial.print(
        "Samples/window: "
    );

    Serial.println(
        EI_CLASSIFIER_RAW_SAMPLE_COUNT
    );


    Serial.print(
        "Total features: "
    );

    Serial.println(
        EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE
    );


    Serial.print(
        "Sampling frequency: "
    );

    Serial.print(
        EI_CLASSIFIER_FREQUENCY
    );

    Serial.println(
        " Hz"
    );


    Serial.print(
        "Sampling interval: "
    );

    Serial.print(
        EI_CLASSIFIER_INTERVAL_MS
    );

    Serial.println(
        " ms"
    );


    Serial.print(
        "Output classes: "
    );

    Serial.println(
        EI_CLASSIFIER_LABEL_COUNT
    );


    Serial.println(
        "=============================="
    );


    Serial.println();
    Serial.println(
        "[READY] Waiting for gesture..."
    );
}


// ============================================================
// LOOP
// ============================================================

void loop() {

    // ========================================================
    // RESTART BLE ADVERTISING AFTER DISCONNECT
    // ========================================================

    if (
        !deviceConnected &&
        wasConnected
    ) {

        delay(300);

        pServer->startAdvertising();

        wasConnected = false;

        lastSentMsg = "";

        Serial.println(
            "[BLE] Advertising restarted"
        );
    }


    if (
        deviceConnected &&
        !wasConnected
    ) {

        wasConnected = true;
    }


    // ========================================================
    // 20 Hz SAMPLING
    // 50 ms
    // ========================================================

    unsigned long now =
        millis();


    if (
        now - lastSampleTime <
        SAMPLE_INTERVAL_MS
    ) {

        return;
    }


    lastSampleTime =
        now;


    // ========================================================
    // RIGHT MPU
    // ========================================================

    float R_pitch = 0.0;

    float R_roll = 0.0;


    readMPU(
        R_pitch,
        R_roll
    );


    // ========================================================
    // RIGHT FLEX SENSORS
    // ========================================================

    int R_f1 =
        getFingerPercent(
            analogRead(pThumb),
            R_Thumb_cal[0],
            R_Thumb_cal[1]
        );


    int R_f2 =
        getFingerPercent(
            analogRead(pIndex),
            R_Index_cal[0],
            R_Index_cal[1]
        );


    int R_f3 =
        getFingerPercent(
            analogRead(pMid),
            R_Mid_cal[0],
            R_Mid_cal[1]
        );


    int R_f4 =
        getFingerPercent(
            analogRead(pRing),
            R_Ring_cal[0],
            R_Ring_cal[1]
        );


    int R_f5 =
        getFingerPercent(
            analogRead(pPinky),
            R_Pinky_cal[0],
            R_Pinky_cal[1]
        );


    // ========================================================
    // LEFT HAND
    // ========================================================

    int L_f1 = leftHand.f1;
    int L_f2 = leftHand.f2;
    int L_f3 = leftHand.f3;
    int L_f4 = leftHand.f4;
    int L_f5 = leftHand.f5;

    float L_pitch = leftHand.pitch;
    float L_roll = leftHand.roll;


    // ========================================================
    // STORE 14 FEATURES
    // ========================================================

    int base =
        sampleIndex *
        NUM_FEATURES;


    // RIGHT

    featureBuffer[base + 0] =
        (float)R_f1;

    featureBuffer[base + 1] =
        (float)R_f2;

    featureBuffer[base + 2] =
        (float)R_f3;

    featureBuffer[base + 3] =
        (float)R_f4;

    featureBuffer[base + 4] =
        (float)R_f5;

    featureBuffer[base + 5] =
        R_pitch;

    featureBuffer[base + 6] =
        R_roll;


    // LEFT

    featureBuffer[base + 7] =
        (float)L_f1;

    featureBuffer[base + 8] =
        (float)L_f2;

    featureBuffer[base + 9] =
        (float)L_f3;

    featureBuffer[base + 10] =
        (float)L_f4;

    featureBuffer[base + 11] =
        (float)L_f5;

    featureBuffer[base + 12] =
        L_pitch;

    featureBuffer[base + 13] =
        L_roll;


    // ========================================================
    // NEXT SAMPLE
    // ========================================================

    sampleIndex++;


    Serial.print(
        "[ML] Sample "
    );

    Serial.print(
        sampleIndex
    );

    Serial.println(
        "/60"
    );


    // ========================================================
    // WAIT UNTIL 60 SAMPLES
    // ========================================================

    if (
        sampleIndex <
        NUM_SAMPLES
    ) {

        return;
    }


    // ========================================================
    // 840 FEATURES READY
    // ========================================================

    Serial.println();
    Serial.println(
        "=========================================="
    );

    Serial.println(
        "[ML] 60 SAMPLES COMPLETE"
    );

    Serial.println(
        "[ML] 840 FEATURES READY"
    );

    Serial.println(
        "[ML] RUNNING CLASSIFIER..."
    );


    // ========================================================
    // CREATE SIGNAL
    // ========================================================

    signal_t signal;


    int err =
        numpy::signal_from_buffer(

            featureBuffer,

            EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,

            &signal
        );


    if (
        err != 0
    ) {

        Serial.print(
            "[ML ERROR] Signal error: "
        );

        Serial.println(
            err
        );


        sampleIndex = 0;

        return;
    }


    // ========================================================
    // RUN CLASSIFIER
    // ========================================================

    ei_impulse_result_t result = { 0 };


    err =
        run_classifier(
            &signal,
            &result,
            false
        );


    if (
        err != EI_IMPULSE_OK
    ) {

        Serial.print(
            "[ML ERROR] Classifier error: "
        );

        Serial.println(
            err
        );


        sampleIndex = 0;

        return;
    }


    // ========================================================
    // FIND BEST PREDICTION
    // ========================================================

    int bestIndex = -1;

    float bestConfidence = 0.0;


    Serial.println();
    Serial.println(
        "========== PREDICTIONS =========="
    );


    for (
        uint16_t i = 0;
        i <
        EI_CLASSIFIER_LABEL_COUNT;
        i++
    ) {

        float confidence =
            result.classification[i].value;


        Serial.print(
            result.classification[i].label
        );


        Serial.print(
            " : "
        );


        Serial.print(
            confidence * 100.0,
            2
        );


        Serial.println(
            "%"
        );


        if (
            confidence >
            bestConfidence
        ) {

            bestConfidence =
                confidence;

            bestIndex =
                i;
        }
    }


    // ========================================================
    // PROCESS BEST RESULT
    // ========================================================

    if (
        bestIndex >= 0
    ) {

        String gesture =
            String(
                result.classification[
                    bestIndex
                ].label
            );


        gesture.trim();

        gesture.toUpperCase();


        Serial.println();

        Serial.print(
            "[BEST] "
        );

        Serial.println(
            gesture
        );


        Serial.print(
            "[CONFIDENCE] "
        );

        Serial.print(
            bestConfidence * 100.0,
            2
        );

        Serial.println(
            "%"
        );


        // ====================================================
        // ACCEPT >= 80%
        // ====================================================

        if (
            bestConfidence >= 0.80
        ) {

            Serial.println(
                "[ACCEPTED]"
            );


            Serial.print(
                "[GESTURE] "
            );

            Serial.println(
                gesture
            );


            // =================================================
            // SEND THROUGH BLE
            // =================================================

            if (
                deviceConnected
            ) {

                sendBLE(
                    gesture
                );

            }

            else {

                Serial.println(
                    "[BLE] PHONE NOT CONNECTED"
                );
            }

        }

        else {

            Serial.println(
                "[REJECTED] Below 80%"
            );
        }
    }


    // ========================================================
    // RESET WINDOW
    // ========================================================

    sampleIndex = 0;


    Serial.println();

    Serial.println(
        "[ML] Buffer reset"
    );

    Serial.println(
        "=========================================="
    );

    Serial.println();
}