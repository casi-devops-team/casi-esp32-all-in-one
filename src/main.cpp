#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <Ticker.h>
#include <Preferences.h>

#include <I2C_Search.h>

// variable declarations
Preferences preferences;

byte i2c_address;
uint8_t buffer[40];
uint8_t i2c_buffer[40];
String serial_number;
String ssid;
String password;
// const char* ssid = "Dialog 4G 707";
// const char* password = "1JhLgena";
unsigned long previousMillis = 0;
unsigned long interval = 30000;

// MQTT Broker
String mqtt_broker;
const char *topic_in_prefix = "casi/production/in/";
const char *topic_out_prefix = "casi/production/out/";
String mqtt_username;
String mqtt_password;
int mqtt_port;
// const char *mqtt_broker = "mqtt.casi.io";
// const char *topic_in_prefix = "casi/production/in/";
// const char *topic_out_prefix = "casi/production/out/";
// const char *mqtt_username = "hellotest2";
// const char *mqtt_password = "1234asd2f";
// int mqtt_port = 1883;
String topic_in;
String topic_out;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

// Analog Reading
const int AI1 = 34;
const int AI2 = 35;
const int AO3 = 2;
const int AO4 = 4;

// put function declarations here:
void initWiFi();
bool is_buffer_empty(const uint8_t *buffer, size_t size);

//--------------------------------------------------------
void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}
void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe("casi/production/out/0CB815D62698/ao3", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
}
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  // Serial.println(topic);
  // Serial.print("  qos: ");
  // Serial.println(properties.qos);
  // Serial.print("  dup: ");
  // Serial.println(properties.dup);
  // Serial.print("  retain: ");
  // Serial.println(properties.retain);
  // Serial.print("  len: ");
  // Serial.println(len);
  // Serial.print("  index: ");
  // Serial.println(index);
  // Serial.print("  total: ");
  // Serial.println(total);
  Serial.print("  payload: ");
  Serial.println(payload);

  String messageTemp;

  for (int i = 0; i < len; i++) {
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

void onMqttPublish(uint16_t packetId) {
  // Serial.println("Publish acknowledged.");
  // Serial.print("  packetId: ");
  // Serial.println(packetId);
}

void setup() {
  preferences.begin("MQTT", false);
  preferences.putString("ssid", "Dialog 4G 707");
  preferences.putString("password", "1JhLgena");
  preferences.putString("mqtt_broker", "mqtt.casi.io");
  preferences.putString("mqtt_username", "hellotest2");
  preferences.putString("mqtt_password", "1234asd2f");
  preferences.putString("mqtt_port", "1883");

  ssid = preferences.getString("ssid");
  password = preferences.getString("password");
  mqtt_broker = preferences.getString("mqtt_broker");
  mqtt_username = preferences.getString("mqtt_username");
  mqtt_password = preferences.getString("mqtt_password");
  mqtt_port = preferences.getString("mqtt_port").toInt();

  Wire.begin();
  Serial.begin(9600);
  Serial.println("\nI2C Scanner");

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  mqttClient.setServer(mqtt_broker.c_str(), mqtt_port);
  mqttClient.setCredentials(mqtt_username.c_str(), mqtt_password.c_str());
  
  initWiFi();
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  serial_number = String(WiFi.macAddress());
  serial_number.replace(":", "");
  topic_in = String(topic_in_prefix) + serial_number;
  topic_out = String(topic_out_prefix) + serial_number;

  String client_id = "2esp32-client-";
  client_id += serial_number;
  Serial.printf("The client %s connects to the CASI MQTT broker\n", client_id.c_str());
  mqttClient.setClientId(client_id.c_str());
  mqttClient.connect();

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
          i++;
      }

      if (!is_buffer_empty(buffer, sizeof(buffer)))
      {
        memcpy(i2c_buffer, buffer, sizeof(buffer));
        
      }
      if (!is_buffer_empty(i2c_buffer, sizeof(i2c_buffer))){
        mqttClient.publish((topic_in + "/i2c").c_str(), 1, true, (char *)i2c_buffer, sizeof(i2c_buffer));
      }

    }
  }
  
  // Analog Reading
  int A1Value = analogRead(AI1);
  mqttClient.publish((topic_in + "/ai1").c_str(), 1, true, String(A1Value).c_str());

  int A2Value = analogRead(AI2);
  mqttClient.publish((topic_in + "/ai2").c_str(), 1, true, String(A2Value).c_str());

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