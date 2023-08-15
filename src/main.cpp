#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "i2c_search.h"

// variable declarations
byte i2c_address;
uint8_t buffer[20];
uint8_t i2c_buffer[20];
String serial_number;
const char* ssid = "Dialog 4G 707";
const char* password = "1JhLgena";
unsigned long previousMillis = 0;
unsigned long interval = 30000;

// MQTT Broker
const char *mqtt_broker = "mqtt.casi.io";
char *topic_in = "casi/production/in/";
const char *mqtt_username = "hellotest2";
const char *mqtt_password = "1234asd2f";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// put function declarations here:
void initWiFi();
bool is_buffer_empty(const uint8_t *buffer, size_t size);
void callback(char *topic, byte *payload, unsigned int length);

void setup() {
  Wire.begin();
  Serial.begin(9600);
  Serial.println("\nI2C Scanner");
  initWiFi();
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  serial_number = String(WiFi.macAddress());
  serial_number.replace(":", "");

  //connecting to a mqtt broker
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(callback);

  while (!mqtt_client.connected()) {
    String client_id = "esp32-client-";
    client_id += serial_number;
    Serial.printf("The client %s connects to the CASI MQTT broker\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
        Serial.println("CASI EMQX MQTT broker connected");
    } else {
        Serial.print("failed with state ");
        Serial.print(mqtt_client.state());
        delay(2000);
    }
  }

  // set i2c buffers 0
  memset(buffer, 0, sizeof(buffer));
  memset(i2c_buffer, 0, sizeof(i2c_buffer));

  i2c_address = searchI2C();
}
 
void loop() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }

  Serial.println("Reading I2C...");
  // i2c_address = 0x68; // default slave address for MPU6050
  if (i2c_address) {

    byte bytes_returned = Wire.requestFrom(i2c_address, 14);
    if (bytes_returned>0  ) {
      int i = 0;

      while (Wire.available()) { // peripheral may send less than requested
          byte c = Wire.read(); // receive a byte as character
          buffer[i] = c;
          // Serial.print("(");
          // Serial.print(i);
          // Serial.print(")");
          // Serial.print(c, HEX);         // print the character
          i++;
      }

      if (!is_buffer_empty(buffer, sizeof(buffer)))
      {
        memcpy(i2c_buffer, buffer, sizeof(buffer));
        
      }
      if (!is_buffer_empty(i2c_buffer, sizeof(i2c_buffer))){
        // Publish and subscribe
        String topic(topic_in);
        topic += serial_number + "/i2c";
        Serial.println(topic);
        mqtt_client.publish(topic.c_str(), i2c_buffer, sizeof(i2c_buffer));
      }

      delay(500);
    }
  }
  
  
  delay(5000);
}

// put function definitions here:
bool is_buffer_empty(const uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (buffer[i] != 0) { // You can also check for any other specific value here
            return false;     // If any non-empty value is found, the buffer is not empty
        }
    }
    return true; // If no non-empty value is found, the buffer is empty
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length) {}