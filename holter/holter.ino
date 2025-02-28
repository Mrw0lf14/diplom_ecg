#include <Wire.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <SD.h>
#include <esp_sleep.h>

#define MMA8452_ADDRESS 0x1C  // Адрес датчика
#define REG_WHO_AM_I 0x0D     // Регистр идентификатора
#define REG_CTRL_REG1 0x2A    // Регистр управления
#define REG_OUT_X_MSB 0x01    // Выходной регистр X

#define SDA_PIN 21  // SDA для ESP32 WROOM
#define SCL_PIN 22  // SCL для ESP32 WROOM

#define I2S_WS 25   // Word Select (LRCLK)
#define I2S_SD 26   // Serial Data (DOUT)
#define I2S_SCK 27  // Bit Clock (BCLK)

#define SD_CS 5     // Chip Select для SD-карты

const unsigned long measurementInterval = 10 * 60 * 1000; // 10 минут в миллисекундах
const unsigned long measurementDuration = 20 * 1000; // 20 секунд в миллисекундах

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    
    if (!initializeMMA8452()) {
        Serial.println("Ошибка инициализации MMA8452");
        while (1);
    }
    
    initializeI2S();
    initializeSD();
}

void loop() {
    unsigned long startTime = millis();
    while (millis() - startTime < measurementDuration) {
        int16_t x, y, z;
        readAccelerometer(x, y, z);
        
        Serial.print("X: "); Serial.print(x);
        Serial.print(" Y: "); Serial.print(y);
        Serial.print(" Z: "); Serial.println(z);
        
        int16_t micSample = readMicrophone();
        Serial.print("Mic Sample: "); Serial.println(micSample);
        
        saveToSD(x, y, z, micSample);
        delay(100);
    }
    
    Serial.println("Переход в режим сна...");
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
    } else {
        return false;
    }
    
    Wire.beginTransmission(MMA8452_ADDRESS);
    Wire.write(REG_CTRL_REG1);
    Wire.write(0x01); // Включение датчика
    Wire.endTransmission();
    
    return true;
}

void readAccelerometer(int16_t &x, int16_t &y, int16_t &z) {
    Wire.beginTransmission(MMA8452_ADDRESS);
    Wire.write(REG_OUT_X_MSB);
    Wire.endTransmission(false);
    Wire.requestFrom(MMA8452_ADDRESS, 6);
    
    if (Wire.available() == 6) {
        x = (Wire.read() << 8) | (Wire.read());
        y = (Wire.read() << 8) | (Wire.read());
        z = (Wire.read() << 8) | (Wire.read());
        
        x >>= 4; y >>= 4; z >>= 4;  // Приведение к 12-битному формату
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
    if (!SD.begin(SD_CS)) {
        Serial.println("Ошибка инициализации SD-карты");
        while (1);
    }
    Serial.println("SD-карта успешно инициализирована");
}

void saveToSD(int16_t x, int16_t y, int16_t z, int16_t micSample) {
    File dataFile = SD.open("data.txt", FILE_APPEND);
    if (dataFile) {
        dataFile.print("X: "); dataFile.print(x);
        dataFile.print(" Y: "); dataFile.print(y);
        dataFile.print(" Z: "); dataFile.print(z);
        dataFile.print(" Mic: "); dataFile.println(micSample);
        dataFile.close();
    } else {
        Serial.println("Ошибка записи на SD-карту");
    }
}
