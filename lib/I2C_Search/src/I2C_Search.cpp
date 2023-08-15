#include <I2C_Search.h>
#include <Arduino.h>
#include <Wire.h>

byte searchI2C(){
    byte error, address;

    for(address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
        Serial.print("I2C device found at address 0x");
        if (address<16) {
            Serial.print("0");
        }
        Serial.println(address,HEX);
        return address;
        }  
    }

    Serial.println("No I2C devices found\n");
    return 0;
}