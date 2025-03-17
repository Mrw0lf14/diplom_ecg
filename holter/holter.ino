#include <Wire.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_sleep.h>

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
#define SD_CS 15
#define SD_MOSI 13
#define SD_CLK 14
#define SD_MISO 12

// --- Wi-Fi ---
#define WIFI_SSID "MGTS_GPON_AC25"
#define WIFI_PASS "JPMGHLF4"
#define SERVER_URL "http://192.168.1.65:5000/data"

const unsigned long measurementInterval = 5 * 1000;  // 5 секунд (для тестов)
const unsigned long measurementDuration = 20 * 1000; // 20 секунд

bool wifiConnected = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    initializeSD();
    if (!initializeMMA8452()) Serial.println("Ошибка MMA8452");
    initializeI2S();
    
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
}

void loop() {
    unsigned long startTime = millis();

    while (millis() - startTime < measurementDuration) {
        int16_t x, y, z;
        readAccelerometer(x, y, z);
        int16_t micSample = readMicrophone();

        Serial.printf("X: %d Y: %d Z: %d Mic: %d\n", x, y, z, micSample);
        
        saveToSD(x, y, z, micSample);
        if (wifiConnected) {
            sendDataToServer(x, y, z, micSample);
        }
        delay(100);
    }

    Serial.println("Переход в сон...");
    esp_sleep_enable_timer_wakeup(measurementInterval * 1000);
    esp_deep_sleep_start();
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
    if (!SD.begin(SD_CS)) {
        Serial.println("Ошибка SD-карты!");
        while (1);
    }
    Serial.println("SD-карта готова!");
    
    File file = SD.open("/data.csv", FILE_READ);
    if (!file) {
        File newFile = SD.open("/data.csv", FILE_WRITE);
        if (newFile) {
            newFile.println("Дата,Время,АЦП,Гироскоп,Микрофон");
            newFile.close();
        }
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
        Serial.println("Ошибка записи в SD!");
    }
}

void sendDataToServer(int16_t x, int16_t y, int16_t z, int16_t micSample) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(SERVER_URL);
        http.addHeader("Content-Type", "application/json");

        String json = "{\"adc\":" + String(x) +
                      ",\"gyro\":" + String(y) +
                      ",\"mic\":" + String(micSample) + "}";

        int httpResponseCode = http.POST(json);
        Serial.printf("Ответ сервера: %d\n", httpResponseCode);
        http.end();
    } else {
        Serial.println("Wi-Fi отключен!");
    }
}
