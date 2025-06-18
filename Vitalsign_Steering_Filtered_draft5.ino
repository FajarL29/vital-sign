#include <Wire.h>  
#include "MAX30105.h"  
#include "spo2_algorithm.h"   
#include "heartRate.h"  

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>

// #define SDA 19   
// #define SCL 5

#define SDA 9
#define SCL 8  

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

MAX30105 particleSensor;  
BLEClient *pClient;
BLERemoteCharacteristic *pRemoteCharacteristic;

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

const int MAF_SIZE = 5;  
float irValues[MAF_SIZE] = {0};  
float redValues[MAF_SIZE] = {0};    
int mafIndex = 0;  
//bool isFingerDetected = false;
bool redOn = false;

bool scanMessageDisplayed2 = false; 
String healthMessage = " ";

unsigned long lastServerUpdate = 0;
const unsigned long serverUpdateInterval = 5000;

const byte RED_LED_BRIGHT = 0xFF; // Intensitas terang, disesuaikan dengan kebutuhan Anda (0x00 - 0xFF)
const byte RED_LED_LOW = 0x00;   // Intensitas rendah
void sendHealthMessage();
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA, SCL);

  setupSensor();
  setupBLE();

  // RTOS Tasks
  xTaskCreatePinnedToCore(sensorTask, "SensorTask", 4096, NULL, 1, NULL, 1);  // Increase stack size if needed
  xTaskCreatePinnedToCore(bleTask, "BLETask", 8192, NULL, 1, NULL, 0);  // Increase stack size if needed
}

void loop() {
  // RTOS tasks handle everything, main loop is empty
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
// ===================== RTOS TASK ===========================
void sensorTask(void *parameter) {
  constexpr int SAMPLE_INTERVAL = 5;  // Interval pengambilan sampel dalam milidetik
  unsigned long lastSampleTime = 0;
  bool isSPO2Calculated = false;  
  bool isBodyTempCalculated = false;  
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

      if (mafIR < 40000 || mafRed == 0 ) {
        resetMeasurements();
        isSPO2Calculated = false;
        isBodyTempCalculated = false;
        isRespiratoryRateCalculated = false;
        particleSensor.setPulseAmplitudeRed(RED_LED_LOW);
        Serial.println("Letakkan jari di sensor");
        if (scanMessageDisplayed2) { // Hanya kirim pesan jika sebelumnya jari terdeteksi
          healthMessage = "Letakkan jari di sensor"; // Update pesan
          sendHealthMessage(); // Kirim pesan ini segera
          scanMessageDisplayed2 = false; // Reset flag
        }

        redOn = true;
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Cegah overloading dengan delay ringan
        continue;
      }
      particleSensor.setPulseAmplitudeRed(RED_LED_BRIGHT);
      //bool isFingerDetected = true;
      redOn = false;

      if (!scanMessageDisplayed2) {
        healthMessage = "Scanning Your Health Condition";
        Serial.println(healthMessage);
        sendHealthMessage();
        scanMessageDisplayed2 = true; 
      }

      // Hanya menghitung SpO2 jika belum dihitung sebelumnya
      if (!isSPO2Calculated) {
        calculateSpO2(mafIR, mafRed);
        isSPO2Calculated = true;
      }

      // Hanya menghitung Body Temperature jika belum dihitung sebelumnya
      if (!isBodyTempCalculated) {
        isBodyTempCalculated = true;
      }

      // Hanya menghitung Respiratory Rate jika belum dihitung sebelumnya
      if (!isRespiratoryRateCalculated) {
        calculateRespiratoryRate(mafIR);
        isRespiratoryRateCalculated = true;
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
          dataDisplayed = true;
        }
      } else {
        stableCount = 0;
        dataDisplayed = false;
      }
    }

    vTaskDelay(1);  // Memberikan waktu lebih banyak bagi task lain
  }
}

// ===================== RESET FUNCTION ======================
void resetMeasurements() {
  collectingData = false;
  dataDisplayed = false;
  stableCount = 0;
  beatAvg = estimatedSBP = estimatedDBP = spo2 = 0;
  respiratoryRate = bodyTemperature = 0;
  // When resetting, if scanMessageDisplayed2 was true, send a "finger not detected" message
  if (scanMessageDisplayed2) {
    healthMessage = "Letakkan jari di sensor";
    sendHealthMessage(); // Send the message indicating finger removal
    scanMessageDisplayed2 = false; // Reset the flag
  }
}

void setupBLE() {
  Serial.println("Inisialisasi BLE Client dan Sensor...");

  BLEDevice::init(""); // Inisialisasi perangkat BLE
  pClient = BLEDevice::createClient(); // Membuat client BLE

  BLEAddress serverAddress("a0:85:e3:e7:48:85");  // Alamat server BLE yang akan dikoneksikan
  //BLEAddress serverAddress("30:ED:A0:33:A0:B1"); //ESP BACKUP LABEL B2
  // Cek koneksi pertama kali
  if (!pClient->connect(serverAddress)) {
    Serial.println("Gagal menghubungkan ke Server BLE!");
    while (1);  // Berhenti jika koneksi gagal
  }
  Serial.println("Terhubung ke Server BLE!");

  // Cek layanan dan karakteristik BLE
  BLERemoteService *pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("Layanan tidak ditemukan!");
    pClient->disconnect();
    while (1);  // Berhenti jika layanan tidak ditemukan
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Karakteristik tidak ditemukan!");
    pClient->disconnect();
    while (1);  // Berhenti jika karakteristik tidak ditemukan
  }
}

void bleTask(void *parameter) {
  unsigned long lastAttemptTime = 0;
  const unsigned long retryInterval = 10000;  // Interval waktu untuk mencoba lagi (misalnya 10 detik)
  const int maxRetryCount = 5;  // Maksimum percobaan untuk menghubungkan kembali BLE
  int retryCount = 0;

  while (true) {
    // Cek koneksi BLE, jika terputus, coba sambungkan kembali
    if (!pClient->isConnected()) {
      Serial.println("Koneksi BLE terputus. Menghubungkan ulang...");
      
      // Cek apakah sudah mencapai batas waktu untuk mencoba kembali
      if (millis() - lastAttemptTime > retryInterval && retryCount < maxRetryCount) {
        if (!pClient->connect(BLEAddress("a0:85:e3:e7:48:85"))){//"a0:85:e3:e7:48:85"34:85:18:b5:02:b9))) {
          Serial.println("Gagal menghubungkan kembali ke Server BLE!");
          retryCount++;  // Increment jumlah percobaan
          lastAttemptTime = millis();  // Reset waktu percobaan
          vTaskDelay(1000 / portTICK_PERIOD_MS);  // Tunggu sebelum mencoba lagi
          continue;  // Lanjutkan mencoba kembali
        }
        
        // Reset percobaan jika berhasil terhubung
        retryCount = 0;
        Serial.println("Berhasil menghubungkan kembali ke Server BLE.");
      }
      
      // Jika gagal setelah beberapa percobaan, keluar atau beri tanda
      if (retryCount >= maxRetryCount) {
        Serial.println("Gagal menghubungkan ke BLE setelah beberapa percobaan. Task BLE berhenti.");
        // Anda bisa menghentikan task atau memicu mekanisme error lebih lanjut
        vTaskDelete(NULL);  // Menghentikan task jika sudah gagal beberapa kali
        return;  // Keluar dari fungsi BLE Task
      }
    }

    // Pastikan koneksi BLE tetap terjaga saat pengiriman data
    if (beatAvg >= 60 && millis() - lastServerUpdate >= serverUpdateInterval) {
      sendData();  // Kirim data
      lastServerUpdate = millis();
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Memberikan jeda agar task lain bisa berjalan
  }
}

// ===================== FILTER ===========================
float applyFilters(long rawValue, float values[]) {  
  values[mafIndex] = rawValue;  
  mafIndex = (mafIndex + 1) % MAF_SIZE;  
  float sum = 0;  
  for (int i = 0; i < MAF_SIZE; i++) sum += values[i];  
  return sum / MAF_SIZE;  
}  

// ===================== KALKULASI ===========================
void calculateHeartRate(long ir) {  
  if (checkForBeat(ir)) {  
    long delta = millis() - lastBeat;  
    lastBeat = millis();  

    beatsPerMinute = 60 / (delta / 1000.0);  
    beatAvg = (0.5 * beatsPerMinute) + ((1 - 0.3) * beatAvg) + 20; 
    estimatedSBP = 60 + 0.5 * beatsPerMinute + 0.2 * beatAvg;  
    estimatedDBP = 60 + 0.3 * beatsPerMinute - 0.1 * beatAvg;  
    Serial.println("AVG :"); Serial.println(beatAvg);
    Serial.println("SBP :"); Serial.println(estimatedSBP);
    Serial.println("DBP :"); Serial.println(estimatedDBP);
    Serial.println("Heartrate Done");
  }  
}

void calculateSpO2(long ir, long red) {  
  static long irDC = 0;  
  static long redDC = 0;  
  static bool firstReading = true;  

  if (firstReading) {  
    irDC = ir;  
    redDC = red;  
    firstReading = false;  
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

void calculateRespiratoryRate(float filteredIR) {  
  static long lastRRTime = millis();  
  long currentTime = millis();  
  float deltaResp = currentTime - lastRRTime;  

  if (deltaResp > 1000 && deltaResp < 5000) {  
    respiratoryRate = 60.0 / (deltaResp / 1000.0);   
    lastRRTime = currentTime;  
  }  
  respiratoryRate = constrain(respiratoryRate, 12, 20);
  Serial.println("RR Done");
}

// ============== FUNGSI BARU UNTUK MENGIRIM PESAN KESEHATAN ==============
void sendHealthMessage() {
  // Pastikan pRemoteCharacteristic dan pClient sudah diinisialisasi dan koneksi BLE aktif
  if (pRemoteCharacteristic != nullptr && pClient != nullptr && pClient->isConnected() && pRemoteCharacteristic->canWrite()) {
    String healthMsgData = healthMessage; // Tambahkan prefix "MSG:" untuk identifikasi di sisi penerima (Flutter)
    pRemoteCharacteristic->writeValue(healthMsgData.c_str());
    Serial.println(healthMsgData);
    vTaskDelay(50 / portTICK_PERIOD_MS); // Beri sedikit jeda untuk memastikan transmisi BLE
  } else {
    // Pesan ini bisa sering muncul di Serial Monitor jika BLE belum siap/terputus.
    // Anda bisa mengomentarinya setelah debugging jika terlalu bising.
    Serial.println("Karakteristik BLE tidak dapat ditulis atau tidak terhubung saat mencoba mengirim pesan kesehatan.");
  }
}

void sendData() {
  if (pRemoteCharacteristic->canWrite()) {
    if (!pClient->isConnected()) {
      Serial.println("Koneksi BLE terputus. Menghubungkan ulang...");
      if (!pClient->connect(BLEAddress("a0:85:e3:e7:48:85"))) {  
        Serial.println("Gagal menghubungkan kembali ke Server BLE!");
        return;
      }
      Serial.println("Berhasil menghubungkan kembali ke Server BLE.");
    }
    //String healthMsgData = healthMessage;
    //pRemoteCharacteristic->writeValue(healthMsgData.c_str());
    //delay(10);

    String bpmData = "BPM:" + String(beatAvg);
    pRemoteCharacteristic->writeValue(bpmData.c_str());
    delay(10);

    String spo2Data = "SpO2:" + String(spo2);
    pRemoteCharacteristic->writeValue(spo2Data.c_str());
    delay(20);

    String tempData = "Body Temp:" + String(bodyTemperature, 1);
    pRemoteCharacteristic->writeValue(tempData.c_str());
    delay(20);

    String sbpData = "SBP:" + String(estimatedSBP);
    pRemoteCharacteristic->writeValue(sbpData.c_str());
    delay(10);

    String dbpData = "DBP:" + String(estimatedDBP);
    pRemoteCharacteristic->writeValue(dbpData.c_str());
    delay(10);

    String rrData = "Respiratory Rate:" + String(respiratoryRate, 0);
    pRemoteCharacteristic->writeValue(rrData.c_str());
    delay(30);

    Serial.println("Semua data telah dikirim.");
  } else {
    Serial.println("Karakteristik tidak mendukung penulisan.");
  }
}

// ===================== OUTPUT ===========================
void displayData() {  
  Serial.println("=================================");  
  Serial.print("BPM: "); Serial.println(beatAvg);  
  Serial.print("SpO2: "); Serial.println(spo2);  
  Serial.print("Body Temp: "); Serial.println(bodyTemperature, 1); 
  Serial.print("SBP: "); Serial.println(estimatedSBP);
  Serial.print("DBP: "); Serial.println(estimatedDBP);  
  Serial.print("Respiratory Rate: "); Serial.println(respiratoryRate, 0);  
  Serial.println("=================================");  
}
