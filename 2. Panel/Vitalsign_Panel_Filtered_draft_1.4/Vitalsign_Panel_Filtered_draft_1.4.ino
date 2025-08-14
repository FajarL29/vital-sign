#include <WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SDA 9
#define SCL 8

//#define SDA 5
//#define SCL 19

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

const char* ssid = "monumnen tugu";
const char* password = "maulanaa123";
String serverURL = "http://203.100.57.59:3000/api/v1/health";

// Objek
MAX30105 particleSensor;
WiFiClient client;
BLEServer *pServer = nullptr;
BLECharacteristic *pCharacteristic = nullptr;

// Variabel pembacaan
float bodyTemperature = 0;
const float slope = 0.00002;
const float intercept = 34.5;

const byte RATE_SIZE = 5;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;
int estimatedSBP = 0;
int estimatedDBP = 0;

int spo2 = 0;
float respiratoryRate = 0;

bool collectingData = false;
bool dataDisplayed = false;
int stableCount = 0;

// Filter
const int MAF_SIZE = 5;
float irValues[MAF_SIZE] = {0};
float redValues[MAF_SIZE] = {0};
int mafIndex = 0;

// WiFi
unsigned long lastWifiAttempt = 0;
const unsigned long wifiRetryInterval = 5000;
bool wifiConnected = false;

bool scanMessageDisplayed = false; 
String healthMessage = " ";

const byte RED_LED_BRIGHT = 0xFF; // Intensitas terang, disesuaikan dengan kebutuhan Anda (0x00 - 0xFF)
const byte RED_LED_LOW = 0x00;   // Intensitas rendah
bool redOn = false;

// === BLE CALLBACK ===
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
      String value = (pCharacteristic->getValue().c_str());
      Serial.println(value);
  }
};

// === SETUP UTAMA ===
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA, SCL);

  setupBLE();
  setupWiFi();
  setupSensor();

  // Jalankan task RTOS
  xTaskCreatePinnedToCore(sensorTask, "SensorTask", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(wifiTask, "WiFiTask", 2048, NULL, 1, NULL, 0);
}

void loop() {
  // Kosong, semua dikerjakan oleh task
}

void setupSensor() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 tidak ditemukan.");
    while (1);  // Berhenti jika sensor tidak terdeteksi
  }
  particleSensor.setup();
  
  // Mengatur kecerahan LED merah pada awalnya (lebih redup saat sensor pertama kali dijalankan)
  particleSensor.setPulseAmplitudeRed(RED_LED_LOW);  // Mengatur LED merah dengan kecerahan rendah pada awalnya
  particleSensor.setPulseAmplitudeGreen(0); 
  redOn = false; // Set kecerahan LED hijau (tidak digunakan di sini, tapi dapat disesuaikan jika perlu)
}

// === TASK WiFi ===
void wifiTask(void *pvParameters) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      if (millis() - lastWifiAttempt >= wifiRetryInterval) {
        Serial.println("[WiFi] Reconnecting...");
        WiFi.begin(ssid, password);
        lastWifiAttempt = millis();
      }
      wifiConnected = false;
    } else {
      if (!wifiConnected) {
        Serial.println("[WiFi] Connected!");
        wifiConnected = true;
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// === TASK Sensor ===
void sensorTask(void *pvParameters) {
  constexpr int SAMPLE_INTERVAL = 10;  // Interval pengambilan sampel dalam milidetik
  unsigned long lastSampleTime = 0;
  bool isSPO2Calculated = false;  // Flag untuk memastikan SpO2 hanya dihitung sekali
  bool isBodyTempCalculated = false;  // Flag untuk memastikan Body Temperature hanya dihitung sekali
  bool isRespiratoryRateCalculated = false; 
  while (true) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastSampleTime >= SAMPLE_INTERVAL) {
      lastSampleTime = currentMillis;

      long ir = particleSensor.getIR();
      long red = particleSensor.getRed();

      // Terapkan filter pada nilai IR dan Red
      float mafIR = applyFilters(ir, irValues);
      float mafRed = applyFilters(red, redValues);

      bodyTemperature = (mafIR * slope) + intercept;

      if (mafIR < 50000 || mafRed == 0) {
        resetMeasurements();
        isSPO2Calculated = false;
        isBodyTempCalculated = false;
        isRespiratoryRateCalculated = false;
        particleSensor.setPulseAmplitudeRed(RED_LED_LOW);
        Serial.println("Letakkan jari di sensor");
        scanMessageDisplayed = false;
        redOn = true;
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Cegah overloading dengan delay ringan
        continue;  // Langsung melanjutkan ke iterasi berikutnya 
      }
      particleSensor.setPulseAmplitudeRed(RED_LED_BRIGHT);
      redOn = false;
      if (!scanMessageDisplayed) {
        healthMessage = "Scanning Your Health Condition";  // Menyimpan pesan ke variabel
        Serial.println(healthMessage);  // Menampilkan pesan ke Serial Monitor
        scanMessageDisplayed = true;  // Set flag untuk mencegah tampilan berulang
      }

      // Hanya menghitung SpO2 jika belum dihitung sebelumnya
      if (!isSPO2Calculated) {
        calculateSpO2(mafIR, mafRed);
        isSPO2Calculated = true;  // Set flag setelah dihitung
      }

      // Hanya menghitung Body Temperature jika belum dihitung sebelumnya
      if (!isBodyTempCalculated) {
        isBodyTempCalculated = true;  // Set flag setelah dihitung
      }

      // Hanya menghitung Respiratory Rate jika belum dihitung sebelumnya
      if (!isRespiratoryRateCalculated) {
        calculateRespiratoryRate(mafIR);
        isRespiratoryRateCalculated = true;  // Set flag setelah dihitung
      }

      // Menghitung data sensor hanya sekali ketika data stabil
      if (!collectingData) {
        collectingData = true;
        stableCount = 0;
        dataDisplayed = false;
        memset(rates, 0, sizeof(rates));
        rateSpot = 0;
      }

      // Menghitung data kesehatan
      calculateHeartRate(mafIR);
   

      // Validasi rentang BPM, SpO2, dan tekanan darah untuk memastikan data valid
      if (beatAvg >= 60 && beatAvg <= 120) {
        stableCount++;
        if (stableCount >= 1 && !dataDisplayed) {
          displayData();
          if (wifiConnected) sendDataToServer();
          dataDisplayed = true;
        }
      } else {
        stableCount = 0;
        dataDisplayed = false;
      }
    }

    vTaskDelay(1);  // Memberi waktu untuk task lain agar dapat berjalan dengan baik
  }
}

// === SETUP BLE ===
void setupBLE() {
  BLEDevice::init("ESP32S3_BLE_Server");
  pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE 
  );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("Hello Client!");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println(BLEDevice::getAddress().toString().c_str());
}

// === SETUP WiFi ===
void setupWiFi() {
  Serial.println("[WiFi] Connecting...");
  WiFi.begin(ssid, password);
}

// === UTILITIES ===

void resetMeasurements() {
  collectingData = false;
  dataDisplayed = false;
  stableCount = 0;
  beatAvg = estimatedSBP = estimatedDBP = spo2 = 0;
  respiratoryRate = bodyTemperature = 0;
}

float applyFilters(long raw, float values[]) {
  values[mafIndex] = raw;
  mafIndex = (mafIndex + 1) % MAF_SIZE;
  float sum = 0;
  for (int i = 0; i < MAF_SIZE; i++) sum += values[i];
  return sum / MAF_SIZE;
}

void calculateHeartRate(long ir) {
  if (checkForBeat(ir)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);
    //if (beatsPerMinute < 30 || beatsPerMinute > 180) return;

    //rates[rateSpot++] = (byte)beatsPerMinute;
    //rateSpot %= RATE_SIZE;
        // Menghitung weighted average (rata-rata berbobot) dengan nilai terbaru lebih diprioritaskan
    //float weight = 0.9;  // Memberikan bobot lebih besar pada pembacaan terbaru
    //float weightedSum = 0;
    //float sum = 0;
    //for (byte i = 0; i < RATE_SIZE; i++) {
      //sum += rates[i];  // Menjumlahkan semua nilai BPM dalam array
    //}
    //float averageBPM = sum / RATE_SIZE; 
    // Menghitung weighted average dari array rates[]
    //for (byte i = 0; i < RATE_SIZE; i++) {
      //float currentWeight = (i == rateSpot) ? weight : (1 - weight) / (RATE_SIZE - 1);
      //weightedSum += currentWeight * rates[i];  // Bobot lebih tinggi untuk nilai terbaru
    //}

    // Update beatAvg dengan nilai rata-rata berbobot
    //beatAvg = weightedSum;
    //float weight = 0.2;
    beatAvg = (0.5 * beatsPerMinute) + ((1 - 0.3) * beatAvg) + 20;

    //beatAvg = 0;
    //for (byte x = 0; x < RATE_SIZE; x++)
      //  beatAvg += rates[x];
    //beatAvg /= RATE_SIZE;
    //beatAvg = averageBPM + 10;
    estimatedSBP = 60 + 0.5 * beatsPerMinute + 0.2 * beatAvg;
    estimatedDBP = 60 + 0.3 * beatsPerMinute - 0.1 * beatAvg;
    Serial.println("AVG :"); Serial.println(beatAvg);
    Serial.println("SBP :"); Serial.println(estimatedSBP);
    Serial.println("DBP :"); Serial.println(estimatedDBP);
    Serial.println("Heartrate Done");
  }
}

void calculateSpO2(long ir, long red) {
  static long irDC = 0, redDC = 0;
  static bool first = true;
  if (first) {
    irDC = ir;
    redDC = red;
    first = false;
  } else {
    irDC = 0.9 * irDC + 0.1 * ir;
    redDC = 0.9 * redDC + 0.1 * red;
  }

  long irAC = ir - irDC;
  long redAC = red - redDC;
  float R = (float(redAC) / redDC) / (float(irAC) / irDC);
  spo2 = 130 - 25 * R;
  spo2 = constrain(spo2, 0, 99);
  Serial.println("SpO2 Done");
}

void calculateRespiratoryRate(float ir) {
  static long lastRRTime = millis();
  long now = millis();
  float delta = now - lastRRTime;
  if (delta > 1000 && delta < 5000) {
    respiratoryRate = 60.0 / (delta / 1000.0);
    lastRRTime = now;
  }
  respiratoryRate = constrain(respiratoryRate, 12, 20);
  Serial.println("RR Done");
}

void sendDataToServer() {
  HTTPClient http;
  http.setTimeout(5000);
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/json");

  String httpRequestData = "{\"user_id\":\"9\",\"respiratoryRate\":\"" + String(respiratoryRate) +
                "\",\"diastole_pressure\":\"" + String(estimatedDBP) +
                "\",\"systole_pressure\":\"" + String(estimatedSBP) +
                "\",\"heartRate\":\"" + String(beatAvg) +
                "\",\"spo2\":\"" + String(spo2) +
                "\",\"body_temp\":\"" + String(bodyTemperature) + "\"}";

  int httpResponseCode = http.POST(httpRequestData);
  Serial.println("[HTTP] Status: " + String(httpResponseCode));
  if (httpResponseCode > 0) Serial.println("[HTTP] Response: " + http.getString());
  http.end();
}


void displayData() {
  Serial.println("===== HEALTH DATA =====");
  Serial.print("BPM: "); Serial.println(beatAvg);
  Serial.print("SpO2: "); Serial.println(spo2);
  Serial.print("Body Temp: "); Serial.println(bodyTemperature, 1);
  Serial.print("SBP: "); Serial.println(estimatedSBP);
  Serial.print("DBP: "); Serial.println(estimatedDBP);
  Serial.print("Respiratory Rate: "); Serial.println(respiratoryRate, 0);
  Serial.println("========================");
}
