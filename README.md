# Welcome to CASI!

## CASI-ESP32 Integration
This program automatically find the I2C device and publish data via mqtt. This program supports one I2C device only.

### ESP32 Pin Configuration
|PIN  | Usage |
|--|--|
| GPIO 21 | SDA for I2C |
| GPIO 22 | SCL for I2C |
| GND | Ground |
| 3V3 | 3V Output |

### MQTT Tags
Use the following tag identifiers to receive respective data in CASI.
|Tag Identifier  | Data Receivable |
|--|--|
| i2c | Data buffer read in i2c connection |