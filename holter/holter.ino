#include <Wire.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_adc/adc_cali.h"

// --- Пины ---
#define MMA8452_ADDRESS 0x1C
#define REG_WHO_AM_I 0x0D
#define REG_CTRL_REG1 0x2A
#define REG_OUT_X_MSB 0x01

#define SDA_PIN 21
#define SCL_PIN 22

#define I2S_WS 25
#define I2S_SD 26
#define I2S_SCK 27

// --- SD-карта ---
#define SD_CS 15    //MOSI
#define SD_MOSI 13  //MISO
#define SD_CLK 14   //
#define SD_MISO 12  //CS

// --- ADC (используем ADC2) ---
// На ESP32, ADC2_CHANNEL_0 -> GPIO0, ADC2_CHANNEL_1 -> GPIO2, ADC2_CHANNEL_2 -> GPIO4
#define ADC_CHANNEL_0 ADC2_CHANNEL_0
#define ADC_CHANNEL_1 ADC2_CHANNEL_1
#define ADC_CHANNEL_2 ADC2_CHANNEL_2

// --- Wi-Fi ---
#define WIFI_SSID "Odeyalo"
#define WIFI_PASS "20012005"
#define SERVER_URL "http://192.168.3.6:5000/data"

// Имя устройства для отправки на сервер
#define DEVICE_NAME "esp_device"

const unsigned long MEASUREMENT_INTERVAL = 40;  // 4 мс = 250 Гц
const unsigned long UPLOAD_INTERVAL = 10000;    // 10 минут (600000 мс)
unsigned long startMeasureTime = 0;
unsigned long startUploadTime = 0;
bool wifiConnected = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    initializeSD(); // очищает файл при запуске
    if (!initializeMMA8452()) Serial.println("Ошибка MMA8452");
    initializeI2S();
    // Конфигурация ADC2 (новый API)
    adc2_config_channel_atten(ADC_CHANNEL_0, ADC_ATTEN_DB_0);
    adc2_config_channel_atten(ADC_CHANNEL_1, ADC_ATTEN_DB_0);
    adc2_config_channel_atten(ADC_CHANNEL_2, ADC_ATTEN_DB_0);
    // Попытка подключения к Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Подключение к Wi-Fi");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // 20 попыток по 500 мс
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWi-Fi подключен!");
        wifiConnected = true;
    } else {
        Serial.println("\nWi-Fi не подключился, запись только на SD-карту!");
    }

    startMeasureTime = millis();
    startUploadTime = millis();
}

void loop() {
    if (millis() - startMeasureTime > MEASUREMENT_INTERVAL) {
        int16_t x, y, z;
        readAccelerometer(x, y, z);
        int16_t micSample = readMicrophone();

        int raw0, raw2, raw4;
        // Считываем показания ADC через новый API ADC2
        adc2_get_raw(ADC_CHANNEL_0, ADC_WIDTH_BIT_12, &raw0);
        adc2_get_raw(ADC_CHANNEL_1, ADC_WIDTH_BIT_12, &raw2);
        adc2_get_raw(ADC_CHANNEL_2, ADC_WIDTH_BIT_12, &raw4);

        Serial.printf("X: %d Y: %d Z: %d Mic: %d ADC1_0: %d ADC1_1: %d ADC1_2: %d\n",
                      x, y, z, micSample, raw0, raw2, raw4);
        
        saveToSD(x, y, z, micSample);

        startMeasureTime = millis();
    }
    if (millis() - startUploadTime > UPLOAD_INTERVAL) {
      if (wifiConnected) {
        sendDataFromSDToServer();
      }
      startUploadTime = millis();
      Serial.println("Отправка данных на сервер");
    }
}

bool initializeMMA8452() {
    Wire.beginTransmission(MMA8452_ADDRESS);
    Wire.write(REG_WHO_AM_I);
    Wire.endTransmission(false);
    Wire.requestFrom(MMA8452_ADDRESS, 1);
    
    if (Wire.available()) {
        uint8_t whoAmI = Wire.read();
        if (whoAmI != 0x2A) return false;
    } else return false;
    
    Wire.beginTransmission(MMA8452_ADDRESS);
    Wire.write(REG_CTRL_REG1);
    Wire.write(0x01);
    Wire.endTransmission();
    return true;
}

void readAccelerometer(int16_t &x, int16_t &y, int16_t &z) {
    Wire.beginTransmission(MMA8452_ADDRESS);
    Wire.write(REG_OUT_X_MSB);
    Wire.endTransmission(false);
    Wire.requestFrom(MMA8452_ADDRESS, 6);
    
    if (Wire.available() == 6) {
        x = (Wire.read() << 8) | Wire.read();
        y = (Wire.read() << 8) | Wire.read();
        z = (Wire.read() << 8) | Wire.read();
        x >>= 4; y >>= 4; z >>= 4;
    }
}

void initializeI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

int16_t readMicrophone() {
    int16_t sample_buffer[64];
    size_t bytes_read;
    i2s_read(I2S_NUM_0, sample_buffer, sizeof(sample_buffer), &bytes_read, portMAX_DELAY);
    return sample_buffer[0];
}

void initializeSD() {
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    SPI.setDataMode(SPI_MODE3);  // Idle High (CPOL=1, CPHA=1)
    if (!SD.begin(SD_CS)) {
        // Serial.println("Ошибка SD-карты!");
        // while (1);
        delay(100);
    }
    Serial.println("SD-карта готова!");
    
    // Очищаем файл при запуске
    SD.remove("/data.csv");
    File newFile = SD.open("/data.csv", FILE_WRITE);
    if (newFile) {
        newFile.println("Date,Time,ADC,Gyro,Mic");
        newFile.close();
    }
}

void saveToSD(int16_t x, int16_t y, int16_t z, int16_t micSample) {
    File dataFile = SD.open("/data.csv", FILE_APPEND);
    if (dataFile) {
        time_t now = time(nullptr);
        struct tm *timeinfo = localtime(&now);
        char dateStr[20], timeStr[10];

        strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", timeinfo);
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);

        dataFile.printf("%s,%s,%d,%d,%d\n", dateStr, timeStr, x, y, micSample);
        dataFile.close();
    } else {
        Serial.println("Запись в SD!");
    }
}

void sendDataFromSDToServer() {
    // Подключаем Wi‑Fi для отправки
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Подключаюсь к Wi‑Fi для отправки...");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWi‑Fi подключён, начинаю отправку данных.");
    } else {
        Serial.println("\nНе удалось подключиться к Wi‑Fi, отмена отправки.");
        return;
    }
  
    File file = SD.open("/data.csv", FILE_READ);
    if (!file) {
        Serial.println("Не удалось открыть файл для отправки");
        return;
    }
  
    // Пропускаем строку заголовка
    String header = file.readStringUntil('\n');
  
    // Считываем все строки и сохраняем последнюю непустую запись
    String lastLine = "";
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() > 0) {
            lastLine = line;
        }
    }
    file.close();
  
    if (lastLine.length() == 0) {
        Serial.println("Нет данных для отправки.");
    } else {
        // Ожидаемый формат: Дата,Время,АЦП,Гироскоп,Микрофон
        int idx1 = lastLine.indexOf(',');
        int idx2 = lastLine.indexOf(',', idx1 + 1);
        int idx3 = lastLine.indexOf(',', idx2 + 1);
        int idx4 = lastLine.indexOf(',', idx3 + 1);
        if (idx1 == -1 || idx2 == -1 || idx3 == -1 || idx4 == -1) {
            Serial.println("Неверный формат данных.");
        } else {
            String adcStr = lastLine.substring(idx2 + 1, idx3);
            String gyroStr = lastLine.substring(idx3 + 1, idx4);
            String micStr = lastLine.substring(idx4 + 1);
          
            // Формирование JSON-пакета
            String payload = "{";
            payload += "\"device_name\":\"" + String(DEVICE_NAME) + "\",";
            payload += "\"adc\":" + adcStr + ",";
            payload += "\"gyro\":" + gyroStr + ",";
            payload += "\"mic\":" + micStr;
            payload += "}";
          
            HTTPClient http;
            http.begin(SERVER_URL);
            http.addHeader("Content-Type", "application/json");
            int httpResponseCode = http.POST(payload);
            Serial.printf("Ответ сервера: %d\n", httpResponseCode);
            http.end();
        }
    }
  
    // После отправки очищаем файл: удаляем и создаём заново с заголовком
    SD.remove("/data.csv");
    File newFile = SD.open("/data.csv", FILE_WRITE);
    if (newFile) {
        newFile.println("Date,Time,ADC,Gyro,Mic");
        newFile.close();
    }
  
    // Отключаем Wi‑Fi для экономии энергии
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}
