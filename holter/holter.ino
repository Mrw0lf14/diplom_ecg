#include <Wire.h>
#include <driver/i2s.h>

#define MMA8452_ADDRESS 0x1C  // Адрес датчика
#define REG_WHO_AM_I 0x0D     // Регистр идентификатора
#define REG_CTRL_REG1 0x2A    // Регистр управления
#define REG_OUT_X_MSB 0x01    // Выходной регистр X

#define SDA_PIN 21  // SDA для ESP32 WROOM
#define SCL_PIN 22  // SCL для ESP32 WROOM

#define I2S_WS 25   // Word Select (LRCLK)
#define I2S_SD 26   // Serial Data (DOUT)
#define I2S_SCK 27  // Bit Clock (BCLK)

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    
    if (!initializeMMA8452()) {
        Serial.println("Ошибка инициализации MMA8452");
        while (1);
    }
    
    initializeI2S();
}

void loop() {
    int16_t x, y, z;
    readAccelerometer(x, y, z);
    
    Serial.print("X: "); Serial.print(x);
    Serial.print(" Y: "); Serial.print(y);
    Serial.print(" Z: "); Serial.println(z);
    
    readMicrophone();
    
    delay(100);
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

void readMicrophone() {
    int16_t sample_buffer[64];
    size_t bytes_read;
    i2s_read(I2S_NUM_0, sample_buffer, sizeof(sample_buffer), &bytes_read, portMAX_DELAY);
    
    Serial.print("Mic Sample: "); Serial.println(sample_buffer[0]);
}
