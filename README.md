# 1 Vital Sign Panel Part
Deskripsi
File ini adalah bagian dari sistem pemantauan tanda vital yang menggunakan sensor MAX30105 untuk mengukur detak jantung dan tingkat oksigen dalam darah (SpO2). Data yang diperoleh kemudian dikirimkan menggunakan Wi-Fi ke server untuk pemantauan jarak jauh.

Fitur Utama
Pengukuran detak jantung dan SpO2 menggunakan sensor MAX30105.

Komunikasi melalui Wi-Fi untuk mengirimkan data ke server atau perangkat lain.

Penggunaan HTTPClient untuk mengirimkan data ke endpoint server.

Persyaratan
Board Arduino ESP32 atau ESP8266

Sensor MAX30105

Library yang dibutuhkan:

WiFi.h (untuk koneksi Wi-Fi)

Wire.h (untuk komunikasi I2C)

MAX30105.h (untuk mengontrol sensor MAX30105)

spo2_algorithm.h dan heartRate.h (untuk menghitung detak jantung dan SpO2)

HTTPClient.h (untuk pengiriman data melalui HTTP)


# 2 Vital Sign Steering Part
