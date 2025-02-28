#include <Wire.h>

#define MMA8452_ADDRESS 0x1C  // Адрес датчика
#define REG_WHO_AM_I 0x0D     // Регистр идентификатора
#define REG_CTRL_REG1 0x2A    // Регистр управления
#define REG_OUT_X_MSB 0x01    // Выходной регистр X

#define SDA_PIN 21  // SDA для ESP32 WROOM
#define SCL_PIN 22  // SCL для ESP32 WROOM

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    
    if (!initializeMMA8452()) {
        Serial.println("Ошибка инициализации MMA8452");
        while (1);
    }
}

void loop() {
    int16_t x, y, z;
    readAccelerometer(x, y, z);
    
    Serial.print("X: "); Serial.print(x);
    Serial.print(" Y: "); Serial.print(y);
    Serial.print(" Z: "); Serial.println(z);
    
    delay(500);
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
