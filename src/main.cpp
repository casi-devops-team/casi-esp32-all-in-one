#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <I2C_Search.h>

// variable declarations
byte i2c_address;
uint8_t buffer[40];
uint8_t i2c_buffer[40];
String serial_number;
const char* ssid = "Dialog 4G 707";
const char* password = "1JhLgena";
unsigned long previousMillis = 0;
unsigned long interval = 30000;

// MQTT Broker
const char *mqtt_broker = "mqtt.casi.io";
const char *topic_in_prefix = "casi/production/in/";
const char *topic_out_prefix = "casi/production/out/";
const char *mqtt_username = "hellotest2";
const char *mqtt_password = "1234asd2f";
const int mqtt_port = 1883;
String topic_in;
String topic_out;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// Analog Reading
// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6) 
const int AI1 = 34;
const int AI2 = 35;
const int AO3 = 2;
const int AO4 = 4;

// put function declarations here:
void initWiFi();
bool is_buffer_empty(const uint8_t *buffer, size_t size);
void mqttCallback(char *topic, byte *payload, unsigned int length);

void setup() {
  Wire.begin();
  Serial.begin(9600);
  Serial.println("\nI2C Scanner");
  initWiFi();
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  serial_number = String(WiFi.macAddress());
  serial_number.replace(":", "");
  topic_in = String(topic_in_prefix) + serial_number;
  topic_out = String(topic_out_prefix) + serial_number;

  //connecting to a mqtt broker
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);

  while (!mqtt_client.connected()) {
    String client_id = "esp32-client-";
    client_id += serial_number;
    Serial.printf("The client %s connects to the CASI MQTT broker\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
        Serial.println("CASI EMQX MQTT broker connected");
        // mqtt_client.subscribe((topic_out + "/+").c_str());
        if (mqtt_client.subscribe("casi/production/out/0CB815D62698/ao3")){
          Serial.println("Subcribed: casi/production/out/0CB815D62698/ao3");
        }
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

  //Analog Writing
  pinMode(A3, OUTPUT);
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String client_id = "esp32-client-";
    client_id += serial_number;
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Subscribe
      if (mqtt_client.subscribe("casi/production/out/0CB815D62698/ao3"))
      {
        Serial.println("Subcribed: casi/production/out/0CB815D62698/ao3");
      }
      
      
      // mqtt_client.subscribe((topic_out + "/+").c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
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
  if (!mqtt_client.connected()) {
    reconnect();
  }

  mqtt_client.loop();

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
        mqtt_client.publish((topic_in + "/i2c").c_str(), i2c_buffer, sizeof(i2c_buffer));
      }

      delay(500);
    }
  }
  
  // Analog Reading
  // variable for storing the potentiometer value
  int A1Value = analogRead(AI1);
  Serial.println(A1Value);
  mqtt_client.publish((topic_in + "/a1").c_str(), String(A1Value).c_str(), sizeof(String(A1Value)));

  int A2Value = analogRead(AI2);
  Serial.println(A2Value);
  mqtt_client.publish((topic_in + "/a2").c_str(), String(A2Value).c_str(), sizeof(String(A2Value)));

  
  
  delay(4000);
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

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  Serial.println(messageTemp);

  if (String(topic) == topic_out+"/ao3")
  {
    Serial.print("Changing output AO3 to ");
    Serial.println(messageTemp);
    analogWrite(AO3, messageTemp.toInt());
    
  } else if(String(topic) == topic_out+"/ao4"){
    Serial.print("Changing output AO4 to ");
    Serial.println(messageTemp);
    analogWrite(AO4, messageTemp.toInt());
  }
  
}