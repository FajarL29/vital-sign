# 1. Vital Sign Panel Part
## Deskripsi
File ini adalah bagian dari sistem pemantauan tanda vital yang menggunakan sensor MAX30105 untuk mengukur detak jantung dan tingkat oksigen dalam darah (SpO2). Data yang diperoleh kemudian dikirimkan menggunakan Wi-Fi ke server untuk pemantauan jarak jauh.

## Fitur Utama
- Pengukuran detak jantung dan SpO2 menggunakan sensor MAX30105.
- Komunikasi melalui Wi-Fi untuk mengirimkan data ke server atau perangkat lain.
- Penggunaan HTTPClient untuk mengirimkan data ke endpoint server.

## Persyaratan
- Board Arduino ESP32
- Sensor MAX30105
### Library yang dibutuhkan:
1. WiFi.h (untuk koneksi Wi-Fi)
2. Wire.h (untuk komunikasi I2C)
3. MAX30105.h (untuk mengontrol sensor MAX30105)
4. spo2_algorithm.h dan heartRate.h (untuk menghitung detak jantung dan SpO2)
5. HTTPClient.h (untuk pengiriman data melalui HTTP)

## Instalasi 
1. Siapkan Board: Pilih board ESP32 di Arduino IDE
2. Pasang Library :
   - Instal library WiFi, Wire, MAX30105, HTTPClient, dan lainnya yang diperlukan melalui Library Manager di Arduino IDE.
3. Hubungkan Sensor MAX30105 :
   - Sambungkan sensor ke pin SDA dan SCL pada board.
4. Sesuaikan Kredensial Wifi :
   -    const char* ssid = "your-ssid";
         const char* password = "your-password";
5. Upload ke Board Arduino  

## Penggunaan 
- Setelah kode diupload, data akan dikumpulkan oleh sensor MAX30105 dan dikirim ke server menggunakan HTTP.

## Skema Koneksi 
- SDA (pin 9) dan SCL (pin 8) pada board Arduino terhubung ke sensor MAX30105.

# 2. Vital Sign Steering Part
## Deskripsi 
File ini bertujuan untuk mengontrol sistem pemantauan tanda vital melalui Bluetooth Low Energy (BLE). Proyek ini menggunakan sensor MAX30105 untuk mengukur detak jantung dan oksigen dalam darah, kemudian mengirimkan data melalui komunikasi BLE ke perangkat lain.

## Fitur Utama
- Pengukuran detak jantung dan SpO2 menggunakan sensor MAX30105.
- Komunikasi menggunakan BLE (Bluetooth Low Energy) untuk mengirimkan data ke perangkat penerima.
- Penggunaan BLEClient untuk berinteraksi dengan perangkat BLE lainnya.

## Persyaratan
- Board Arduino ESP32
- Sensor MAX30105
- Library yang dibutuhkan:
     - Wire.h (untuk komunikasi I2C)
     - MAX30105.h (untuk mengontrol sensor MAX30105)
     - BLEDevice.h, BLEUtils.h, BLEServer.h (untuk komunikasi Bluetooth)

## Instalasi
1. Siapkan Board: Pilih board ESP32 atau ESP8266 di Arduino IDE.
2. Pasang Library:
   - Instal library Wire, MAX30105, dan BLE libraries (BLEDevice, BLEUtils, BLEServer).
3. Hubungkan Sensor MAX30105:
   - Sambungkan sensor ke pin SDA dan SCL pada board.
4. Setup BLE
   - Tentukan karakteristik BLE yang akan digunakan untuk komunikasi, seperti SERVICE_UUID dan CHARACTERISTIC_UUID.
5. Upload ke Board Arduino

## Penggunaan 
- Setelah upload selesai, board Arduino akan memulai komunikasi BLE dan mengirimkan data sensor ke perangkat yang terhubung.

## Skema Koneksi 
- SDA (pin 9) dan SCL (pin 8) pada board Arduino terhubung ke sensor MAX30105.

# 3. Air Quality Monitoring System
## Deskripsi 
Proyek ini adalah sistem pemantauan kualitas udara berbasis ESP32 yang mengukur beberapa parameter kualitas udara, seperti O2, CO, CO2, PM2.5, dan PM10, serta suhu dan kelembaban. Sistem ini dapat mengontrol relay untuk mengatur pasokan O2 dan penyaring udara (air purifier) berdasarkan level kualitas udara yang terdeteksi. Data yang diambil dari sensor-sensor ini dikirim ke server melalui WiFi.

## Fitur Utama
- Pemantauan Kualitas Udara: Mengukur O2, CO, CO2, PM2.5, dan PM10.
- Kontrol Relay: Mengatur pasokan O2 dan penyaring udara berdasarkan parameter yang terdeteksi.
- Pengiriman Data ke Server: Mengirim data kualitas udara ke server melalui HTTP POST setiap menit.
- AQI (Air Quality Index): Menghitung AQI untuk PM2.5, PM10, dan CO menggunakan standar US EPA.
  
## Persyaratan
1. Hardware :
   - ESP32S3
   - Sensor gas untuk O2, CO, dan CO2 (misalnya DFRobot MultiGas Sensor dan MQ135)
   - Sensor suhu dan kelembaban (misalnya DHT22)
   - Sensor partikel udara (misalnya PMS5003 untuk PM2.5 dan PM10)
   - Relay untuk kontrol O2 dan penyaring udara
2. Software :
   - Arduino IDE
   - Libraries:
      - DFRobot_MultiGasSensor
      - MQUnifiedsensor
      - DHT
      - WiFi
      - HTTPClient
      - ArduinoJson

## Instalasi
