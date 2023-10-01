# Welcome to CASI!
# Casi Admins'Guide for ESP32

## Initial Installation of Software
### Prerequisites
1. Visual Studio Code (VS Code)
2. Platform.io extention installed on Visual Studio Code (https://platformio.org/install/ide?install=vscode)
3. If you are using Windows, install **CP210x Universal Windows Driver**, additionally. (https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads)

### Install the software

1. Clone this repository into yor computer and open the project in VS Code
2. Remove all other serial devices except ESP32.
3. Upload the file system image
   #### Uploading Filesystem Image
   1. Click the PIO icon at the left side bar. The project tasks should open.
   2. Select env:esp32-devkit (it may be slightly different depending on the board you’re using).
   3. Expand the Platform menu.
   4. Select Build Filesystem Image.
   5. After the build is completed, Click Upload Filesystem Image.
   ![image](https://github.com/casi-devops-team/casi-esp32-all-in-one/assets/136977780/76245276-bc13-4e43-8a13-6105925aca21)
   
4. click on the Upload icon to compile and upload the code. Alternatively, you can go to the PIO Project Tasks menu and select Upload.
   ![image](https://github.com/casi-devops-team/casi-esp32-all-in-one/assets/136977780/7b76e6b0-6d2e-42ee-a930-334b809636e1)
5. If the code is successfully uploaded, you should get the following message.
   ![image](https://github.com/casi-devops-team/casi-esp32-all-in-one/assets/136977780/4cf08057-6e52-4c2d-9780-24c289b4fff9)
6. Once the upload is completed, you should press EN/RST button once on the esp32 device




# User Guide (to be moved to another repository)
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
