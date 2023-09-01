#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <Ticker.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <map>
#include <esp_now.h>

#include <I2C_Search.h>

typedef struct struct_message
{
  char a[32];
  int b;
  float c;
  bool d;
} struct_message;

typedef struct datapoint
{
  uint8_t pinType;
  uint8_t key;
  int value;
  uint8_t i2c_buffer[40];
} datapoint;

typedef struct net_message
{
  int msg_type;
  char master_mac[17];
  int slave_pins[20];
  int slave_pin_config[20];
  uint8_t slave_id;
  datapoint data;
} net_message;

// ESP NET Device Types
#define MASTER_MODE 0
#define SLAVE_MODE 1

// ESP NET Message Types
#define SLAVE_CONFIG 0
#define MASTER_IN 1
#define MASTER_OUT 2

// GPIO Pin Modes
#define AI 0
#define AO 1
#define DI 2
#define DO 3
#define I2C 4
#define NA 10

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

// for MQTT
unsigned long previousMillisMQTT = 0;
unsigned long mqttInterval = 5000;

bool wifi_failed = false;
IPAddress ip;

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

// GPIO Pins
const int pins[20] = {2, 5, 12, 13, 14, 15, 16, 17, 18, 19, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39};
const int slave_pins[20] = {2, 5, 12, 13, 14, 15, 16, 17, 18, 19, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39};
int slave_pin_mode[20];

std::map<int, String> pin_modes;

// ESP NET Variables
int deviceMode;
esp_now_peer_info_t peers[10];
esp_now_peer_info_t peerMaster;
uint8_t peersCount = 0;
net_message dataReceived;

//=============================================================
const char *PARAM_INPUT_1 = "output";
const char *PARAM_INPUT_2 = "state";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Initialize SPIFFS
void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

String outputState(int output)
{
  if (digitalRead(output))
  {
    return "checked";
  }
  else
  {
    return "";
  }
}

// Replaces placeholder with button section in your web page
String processor(const String &var)
{
  if (var == "WIFISTATUSPLACEHOLDER")
  {
    String statusText = "";
    if ((WiFi.status() == WL_CONNECTED))
    {
      String client_ip = WiFi.localIP().toString();
      statusText = "Status: <span style=\"color: #00FF00;\">Connected</span> to <b>" + ssid + "</b>. Go to <b>" + client_ip + "</b>.";
    }
    else
    {
      statusText = "Status: <span style=\"color: #FF0000;\">Not Connected</span>";
    }
    return statusText;
  }

  if (var == "MACADDRESS")
  {
    String mac = "MAC Address: " + WiFi.macAddress();
    return mac;
  }

  //  if(var == "BUTTONPLACEHOLDER"){
  //    String buttons = "";
  //    buttons += "<h4>Output - GPIO 26</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"26\" " + outputState(26) + "><span class=\"slider\"></span></label>";
  //    buttons += "<h4>Output - GPIO 4</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"4\" " + outputState(4) + "><span class=\"slider\"></span></label>";
  //    buttons += "<h4>Output - GPIO 33</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"33\" " + outputState(33) + "><span class=\"slider\"></span></label>";
  //    return buttons;
  //  }
  return String();
}

//=============================================================

// put function declarations here:
void initWiFi();
bool is_buffer_empty(const uint8_t *buffer, size_t size);
void setRoutes();
void notifyGPIOStatus(int pin_id, String value);
void setPeers(String mac_pref_string);
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void setSlavePins();
void configSlave(net_message slaveConfigMsg);
void handleMasterIn(net_message masterInMsg);

//--------------------------------------------------------
void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}
void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  for (int i : pins)
  {
    if (pin_modes[i] == "do")
    {
      uint16_t packetIdSub = mqttClient.subscribe((topic_out + "/do" + String(i)).c_str(), 1);
      Serial.print("Subscribing at QoS 1, packetId: ");
      Serial.println(packetIdSub);
    }
    else if (pin_modes[i] == "ao")
    {
      uint16_t packetIdSub = mqttClient.subscribe((topic_out + "/ao" + String(i)).c_str(), 1);
      Serial.print("Subscribing at QoS 1, packetId: ");
      Serial.println(packetIdSub);
    }
  }
}
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId)
{
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
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

  for (int i = 0; i < len; i++)
  {
    messageTemp += (char)payload[i];
  }
  Serial.println(messageTemp);

  // Witing to pins
  for (int i : pins)
  {
    if (pin_modes[i] == "do") // Digital write
    {
      if (String(topic) == topic_out + "/do" + String(i))
      {
        Serial.print("Changing output DO" + String(i) + " to ");
        Serial.println(messageTemp);
        if (messageTemp == "1" || messageTemp == "true")
        {
          digitalWrite(i, HIGH);
        }
        else if (messageTemp == "0" || messageTemp == "false")
        {
          digitalWrite(i, LOW);
        }
      }
    }
    else if (pin_modes[i] == "ao") // Analog write
    {
      if (String(topic) == topic_out + "/ao" + String(i))
      {
        Serial.print("Changing output AO" + String(i) + " to ");
        Serial.println(messageTemp);
        if (messageTemp.toInt() > 255)
        {
          messageTemp = "255";
        }
        else if (messageTemp.toInt() < 0)
        {
          messageTemp = "0";
        }
        analogWrite(i, messageTemp.toInt());
      }
    }
  }
}

void onMqttPublish(uint16_t packetId)
{
  // Serial.println("Publish acknowledged.");
  // Serial.print("  packetId: ");
  // Serial.println(packetId);
}

//=======================================Setup=========================================
void setup()
{
  preferences.begin("MQTT", false);
  // preferences.putString("ssid", "Dialog 4G 707");
  // preferences.putString("password", "1JhLgena");
  // preferences.putString("ssid", "Dialog 4G");
  // preferences.putString("password", "Dul8622@");
  // preferences.putString("mqtt_broker", "mqtt.casi.io");
  // preferences.putString("mqtt_username", "hellotest2");
  // preferences.putString("mqtt_password", "1234asd2f");
  // preferences.putString("mqtt_port", "1883");

  ssid = preferences.getString("ssid");
  password = preferences.getString("password");
  mqtt_broker = preferences.getString("mqtt_broker");
  mqtt_username = preferences.getString("mqtt_username");
  mqtt_password = preferences.getString("mqtt_password");
  mqtt_port = preferences.getString("mqtt_port").toInt();

  Wire.begin();
  Serial.begin(115200);
  Serial.println("\nI2C Scanner");
  initSPIFFS();

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  mqttClient.setServer(mqtt_broker.c_str(), mqtt_port);
  mqttClient.setCredentials(mqtt_username.c_str(), mqtt_password.c_str());
  // mqttClient.setSecure(true);

  initWiFi();
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  if (wifi_failed)
  {
    /* code */
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP32", "12345678");

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    ip = IP;

    setRoutes();

    // server.serveStatic("/", SPIFFS, "/");
    // // Web Server Root URL
    // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    //           {
    //             request->send(SPIFFS, "/index.html", "text/html", false, processor);
    //             // request->send_P(200, "text/html", "/index.html", processor);
    //             // request->send(200, "text/html","OK");
    //           });
    // server.on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send(SPIFFS, "/mqtt.html", "text/html"); });
    // server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request)
    //           {
    //   int params = request->params();
    //   for(int i=0;i<params;i++){
    //     AsyncWebParameter* p = request->getParam(i);
    //     if(p->isPost()){
    //       // HTTP POST ssid value
    //       if (p->name() == "ssid") {
    //         String new_ssid = p->value();
    //         Serial.print("SSID set to: ");
    //         Serial.println(new_ssid);
    //         // Write file to save value
    //         preferences.putString("ssid", new_ssid);
    //       }
    //       // HTTP POST pass value
    //       if (p->name() == "password") {
    //         String new_pass = p->value();
    //         Serial.print("Password set to: ");
    //         Serial.println(new_pass);
    //         // Write file to save value
    //         preferences.putString("password", new_pass);
    //       }
    //     }
    //   }
    //   request->send(200, "text/plain", "Done. ESP will restart.");
    //   delay(3000);
    //   ESP.restart(); });
    // server.on("/mqtt", HTTP_POST, [](AsyncWebServerRequest *request)
    //           {
    //   int params = request->params();
    //   for(int i=0;i<params;i++){
    //     AsyncWebParameter* p = request->getParam(i);
    //     if(p->isPost()){
    //       // HTTP POST ssid value
    //       if (p->name() == "mqtt_broker") {
    //         String new_mqtt_broker = p->value();
    //         Serial.print("SSID set to: ");
    //         Serial.println(new_mqtt_broker);
    //         // Write file to save value
    //         preferences.putString("mqtt_broker", new_mqtt_broker);
    //       }
    //       // HTTP POST mqtt_username value
    //       if (p->name() == "mqtt_username") {
    //         String new_mqtt_username = p->value();
    //         Serial.print("Password set to: ");
    //         Serial.println(new_mqtt_username);
    //         // Write file to save value
    //         preferences.putString("password", new_mqtt_username);
    //       }
    //       // HTTP POST mqtt_username value
    //       if (p->name() == "mqtt_password") {
    //         String new_mqtt_password = p->value();
    //         Serial.print("Password set to: ");
    //         Serial.println(new_mqtt_password);
    //         // Write file to save value
    //         preferences.putString("password", new_mqtt_password);
    //       }
    //       // HTTP POST mqtt_username value
    //       if (p->name() == "mqtt_port") {
    //         String new_mqtt_port = p->value();
    //         Serial.print("Password set to: ");
    //         Serial.println(new_mqtt_port);
    //         // Write file to save value
    //         preferences.putString("password", new_mqtt_port);
    //       }
    //     }
    //   }
    //   request->send(200, "text/plain", "Done. ESP will restart.");
    //   delay(3000);
    //   ESP.restart(); });

    server.begin();
  }
  else
  {
    server.addHandler(&ws);
    setRoutes();

    // Start server
    server.begin();

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

    // Digital Read/Write
    // Digital Pin Configuartion
    for (int i : pins)
    {
      String mode = preferences.getString(("p" + String(i)).c_str());
      pin_modes[i] = mode;
      if (mode == "di")
      {
        pinMode(i, INPUT);
      }
      else if (mode == "do")
      {
        pinMode(i, OUTPUT);
      }
    }
  }

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP NET");
    // return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // ESP NET Setup
  deviceMode = preferences.getInt("device_mode", SLAVE_MODE);
  if (deviceMode == MASTER_MODE)
  {
    setSlavePins();
    String slave_mac_list_pref = preferences.getString("mac_list");
    if (slave_mac_list_pref)
    {
      setPeers(slave_mac_list_pref);
    }

    // ESP NET adding peers
    for (size_t i = 0; i < peersCount; i++)
    {
      esp_now_peer_info_t peerInfo = peers[i];
      if (esp_now_add_peer(&peerInfo) != ESP_OK)
      {
        Serial.println("Failed to add peer.");
        // return;
      }
      else
      {
        Serial.println("Peer added successfully.");
      }
    }
  }
  else if (deviceMode == SLAVE_MODE)
  {
    uint8_t temp_mac[6];
    char *macBit = strtok((char *)preferences.getString("master_mac").c_str(), ":");
    int k = 0;
    while (macBit != NULL)
    {
      int intBit = (int)strtol(macBit, 0, 16);
      temp_mac[k] = intBit;
      k++;
      macBit = strtok(NULL, ":");
    }

    memcpy(peerMaster.peer_addr, temp_mac, 6);
    peerMaster.channel = 0;
    peerMaster.encrypt = false;

    // Add Master Peer
    if (esp_now_add_peer(&peerMaster) != ESP_OK)
    {
      Serial.println("Failed to add master peer.");
    }
    else
    {
      Serial.println("Master peer added successfully.");
    }
  }

  // Slave config message
  if (deviceMode == MASTER_MODE)
  {
    net_message msg;
    msg.msg_type = SLAVE_CONFIG;
    strcpy(msg.master_mac, WiFi.macAddress().c_str());
    memcpy(&msg.slave_pins, slave_pins, sizeof(slave_pins));
    memcpy(&msg.slave_pin_config, slave_pin_mode, sizeof(slave_pin_mode));

    for (size_t i = 0; i < peersCount; i++)
    {
      msg.slave_id = i;
      esp_err_t result = esp_now_send(peers[i].peer_addr, (uint8_t *)&msg, sizeof(msg));
      if (result == ESP_OK)
      {
        Serial.println("Slave config message sent with success");
      }
    }
  }
}

void loop()
{
  // Serial.println(WiFi.status());
  if (!wifi_failed || deviceMode == SLAVE_MODE)
  {
    unsigned long currentMillisMQTT = millis();
    bool mqttPublished = false;

    unsigned long currentMillis = millis();
    // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
    if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval) && !wifi_failed)
    {
      Serial.print(millis());
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
      previousMillis = currentMillis;
    }

    Serial.println("Reading I2C...");
    // i2c_address = 0x68; // default slave address for MPU6050
    if (i2c_address)
    {
      byte bytes_returned = Wire.requestFrom(i2c_address, 14);
      if (bytes_returned > 0)
      {
        int i = 0;

        while (Wire.available())
        {                       // peripheral may send less than requested
          byte c = Wire.read(); // receive a byte as character
          buffer[i] = c;
          i++;
        }

        if (!is_buffer_empty(buffer, sizeof(buffer)))
        {
          memcpy(i2c_buffer, buffer, sizeof(buffer));
        }
        if (!is_buffer_empty(i2c_buffer, sizeof(i2c_buffer)))
        {
          if (currentMillisMQTT - previousMillisMQTT >= mqttInterval)
          {
            if (!wifi_failed)
            {
              mqttClient.publish((topic_in + "/i2c").c_str(), 1, true, (char *)i2c_buffer, sizeof(i2c_buffer));
              mqttPublished = true;
              if (deviceMode == SLAVE_MODE)
              {
                net_message msg;
                msg.slave_id = preferences.getInt("slave_id");
                msg.msg_type = MASTER_IN;
                msg.data.pinType = I2C;
                msg.data.key = 100;
                memcpy(&msg.data.i2c_buffer, i2c_buffer, sizeof(i2c_buffer));
                esp_err_t result = esp_now_send(peerMaster.peer_addr, (uint8_t *)&msg, sizeof(msg));
              }
            }
          }
        }
      }
    }

    for (int i : pins)
    {
      if (pin_modes[i] == "di") // Digital Reading
      {
        int val = digitalRead(i);
        if (currentMillisMQTT - previousMillisMQTT >= mqttInterval)
        {
          mqttClient.publish((topic_in + "/di" + String(i)).c_str(), 1, true, String(val).c_str());
          mqttPublished = true;
          if (deviceMode == SLAVE_MODE)
          {
            net_message msg;
            msg.slave_id = preferences.getInt("slave_id");
            msg.msg_type = MASTER_IN;
            msg.data.pinType = DI;
            msg.data.key = i;
            msg.data.value = val;
            esp_err_t result = esp_now_send(peerMaster.peer_addr, (uint8_t *)&msg, sizeof(msg));
          }
        }

        notifyGPIOStatus(i, String(val));
      }
      else if (pin_modes[i] == "ai") // Analog Reading
      {
        int analog_reading = analogRead(i);
        if (currentMillisMQTT - previousMillisMQTT >= mqttInterval)
        {
          mqttClient.publish((topic_in + "/ai" + String(i)).c_str(), 1, true, String(analog_reading).c_str());
          mqttPublished = true;
          if (deviceMode == SLAVE_MODE)
          {
            net_message msg;
            msg.slave_id = preferences.getInt("slave_id");
            msg.msg_type = MASTER_IN;
            msg.data.pinType = AI;
            msg.data.key = i;
            msg.data.value = analog_reading;
            esp_err_t result = esp_now_send(peerMaster.peer_addr, (uint8_t *)&msg, sizeof(msg));
          }
        }

        notifyGPIOStatus(i, String(analog_reading));
      }
    }

    if (mqttPublished)
    {
      previousMillisMQTT = currentMillisMQTT;
    }

    delay(5000);
  }
}

// put function definitions here:
bool is_buffer_empty(const uint8_t *buffer, size_t size)
{
  for (size_t i = 0; i < size; i++)
  {
    if (buffer[i] != 0)
    {               // You can also check for any other specific value here
      return false; // If any non-empty value is found, the buffer is not empty
    }
  }
  return true; // If no non-empty value is found, the buffer is empty
}

void initWiFi()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    if (attempts > 10)
    {
      wifi_failed = true;
      break;
    }

    Serial.print('.');
    attempts++;
    delay(1000);
  }

  if (!wifi_failed)
  {
    ip = WiFi.localIP();
    Serial.println(ip);
  }
}

void setRoutes()
{
  server.serveStatic("/", SPIFFS, "/");
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
    // request->send_P(200, "text/html", index_html, processor);
    request->send(SPIFFS, "/index.html", "text/html", false, processor); });

  server.on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/mqtt.html", "text/html"); });

  server.on("/pins", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/pins.html", "text/html"); });

  server.on("/net", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/net.html", "text/html"); });

  server.on("/getWiFiData", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String existing_ssid = preferences.getString("ssid");
    String existing_password = preferences.getString("password");
    request->send(200, "application/json", "{ \"ssid\" : \""+existing_ssid+"\", \"password\": \""+existing_password+"\"}"); });

  server.on("/loadMQTTData", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String existing_ssid = preferences.getString("ssid");
    String existing_password = preferences.getString("password");
    request->send(200, "application/json", "{ \"mqtt_broker\" : \""+preferences.getString("mqtt_broker")+"\", \"mqtt_username\": \""+preferences.getString("mqtt_username")+"\", \"mqtt_password\": \""+preferences.getString("mqtt_password")+"\", \"mqtt_port\": \""+preferences.getString("mqtt_port")+"\"}"); });

  server.on("/loadPinsData", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String response = "{ \"pins\": {";
              size_t count = sizeof(pins) / sizeof(int);
              for (size_t i = 0; i < count; i++)
              {
                response += "\"p" + String(pins[i]) + "\": ";
                response += "\"" + preferences.getString(("p" + String(pins[i])).c_str()) + "\"";
                if (i != (count-1))
                {
                  response += ", ";
                }
                else
                {
                  response += " ";
                }
              }
              response += "}}";

              request->send(200, "application/json", response); });

  server.on("/loadSlavePinsData", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String slavePinConfig = preferences.getString("slave_pins");
              String macList = preferences.getString("mac_list");
              String deviceMode = String(preferences.getInt("device_mode"));
              
              String response = "{ \"pins\": ";
              response += slavePinConfig;
              response += ", ";
              response += "\"mac_list\": \""+macList+"\", ";
              response += "\"device_mode\": \""+deviceMode+"\"";
              response += "}";

              request->send(200, "application/json", response); });

  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    String new_ssid;
    String new_password;

    int params = request->params();
    for (int i = 0; i < params; i++){
      AsyncWebParameter* p = request->getParam(i);
      if (p->name()=="ssid")
      {
        new_ssid = p->value();
      }
      else if (p->name()=="password")
      {
        new_password = p->value();
      }
      
      Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }

    preferences.putString("ssid", new_ssid);
    preferences.putString("password", new_password);
    request->send(200, "text/plain", "Updated. Restarting the Device.");
    // request->redirect("/"); 
    delay(3000);
    ESP.restart(); });

  server.on("/mqtt", HTTP_POST, [](AsyncWebServerRequest *request)
            {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == "mqtt_broker") {
            String new_mqtt_broker = p->value();
            Serial.print("SSID set to: ");
            Serial.println(new_mqtt_broker);
            // Write file to save value
            preferences.putString("mqtt_broker", new_mqtt_broker);
          }
          // HTTP POST mqtt_username value
          if (p->name() == "mqtt_username") {
            String new_mqtt_username = p->value();
            Serial.print("Password set to: ");
            Serial.println(new_mqtt_username);
            // Write file to save value
            preferences.putString("password", new_mqtt_username);
          }
          // HTTP POST mqtt_username value
          if (p->name() == "mqtt_password") {
            String new_mqtt_password = p->value();
            Serial.print("Password set to: ");
            Serial.println(new_mqtt_password);
            // Write file to save value
            preferences.putString("password", new_mqtt_password);
          }
          // HTTP POST mqtt_username value
          if (p->name() == "mqtt_port") {
            String new_mqtt_port = p->value();
            Serial.print("Password set to: ");
            Serial.println(new_mqtt_port);
            // Write file to save value
            preferences.putString("password", new_mqtt_port);
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart.");
      delay(3000);
      ESP.restart(); });

  server.on("/pins", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    int params = request->params();
    for (int i = 0; i < params; i++){
      AsyncWebParameter* p = request->getParam(i);
      String key = p->name();
      String value = p->value();
      preferences.putString(p->name().c_str(), p->value().c_str());
    }
    request->redirect("/pins"); });

  server.on("/net", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    int device_mode = request->getParam("device_mode", true)->value().toInt();
    if (device_mode== SLAVE_MODE)
    {
      preferences.putInt("device_mode", SLAVE_MODE);
      request->redirect("/net");
    }

    if (device_mode == MASTER_MODE)
    {
      preferences.putInt("device_mode", MASTER_MODE);

      preferences.putString("mac_list", request->getParam("mac_list", true)->value());

      // Saving Slave Pin Configuaration
      int params = request->params();
      String slavePinConfig = "{";
      for (int i = 0; i < params; i++){
        AsyncWebParameter* p = request->getParam(i);
        String key = p->name();
        String value = p->value();
        if (key !="device_mode" && key != "mac_list")
        {
          int s_pin_mode = NA;
          if (value=="ai")
          {
            s_pin_mode = AI;
          } else if(value=="ao"){
            s_pin_mode = AO;
          }else if(value=="di"){
            s_pin_mode = DI;
          }else if(value=="do"){
            s_pin_mode = DO;
          }
          preferences.putInt(("s"+p->name()).c_str(), s_pin_mode);

          slavePinConfig += "\"" + key + "\": ";
          slavePinConfig += "\""+p->value()+"\"";
          if (i != (params-1))
          {
            slavePinConfig += ", ";
          }
          else
          {
            slavePinConfig += " ";
          }

        }
        
      }
      slavePinConfig += "}";
      preferences.putString("slave_pins", slavePinConfig);
    }
    
    request->redirect("/net"); });
}

void notifyGPIOStatus(int pin_id, String value)
{
  ws.textAll("{ \"p" + String(pin_id) + "\": \"" + value + "\" }");
}

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  memcpy(&dataReceived, incomingData, sizeof(dataReceived));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Type: ");
  Serial.println(dataReceived.msg_type);
  // Serial.print("Master Mac: ");
  // Serial.println(dataReceived.master_mac);
  switch (dataReceived.msg_type)
  {
  case SLAVE_CONFIG:
    configSlave(dataReceived);
    break;

  case MASTER_IN:
    handleMasterIn(dataReceived);
    break;

  default:
    break;
  }
}

void configSlave(net_message slaveConfigMsg)
{
  preferences.putInt("device_mode", SLAVE_MODE);
  preferences.putInt("slave_id", slaveConfigMsg.slave_id);
  preferences.putString("master_mac", slaveConfigMsg.master_mac);
  size_t count = sizeof(slaveConfigMsg.slave_pins) / sizeof(int);
  for (size_t i = 0; i < count; i++)
  {
    // Serial.print(slaveConfigMsg.slave_pins[i]);
    // Serial.print(":");
    // Serial.println(slaveConfigMsg.slave_pin_config[i]);
    if (slaveConfigMsg.slave_pin_config[i] == AI)
    {
      preferences.putString(("p" + String(slaveConfigMsg.slave_pins[i])).c_str(), "ai");
    }
    else if (slaveConfigMsg.slave_pin_config[i] == AO)
    {
      preferences.putString(("p" + String(slaveConfigMsg.slave_pins[i])).c_str(), "ao");
    }
    else if (slaveConfigMsg.slave_pin_config[i] == DI)
    {
      preferences.putString(("p" + String(slaveConfigMsg.slave_pins[i])).c_str(), "di");
    }
    else if (slaveConfigMsg.slave_pin_config[i] == DO)
    {
      preferences.putString(("p" + String(slaveConfigMsg.slave_pins[i])).c_str(), "do");
    }
    else
    {
      preferences.putString(("p" + String(slaveConfigMsg.slave_pins[i])).c_str(), "");
    }
  }

  delay(2000);
  ESP.restart();
}

void handleMasterIn(net_message masterInMsg)
{
  String mqttTopic = topic_in + "/"+masterInMsg.slave_id+"_";
  switch (masterInMsg.data.pinType)
  {
  case AI:
    mqttTopic += "ai";
    break;

  case DI:
    mqttTopic += "di";
    break;
  
  case I2C:
    mqttTopic += "i2c";
    break;
  
  default:
    break;
  }
  mqttTopic += String(masterInMsg.data.key);
  Serial.print("Topicc: ");
  Serial.print(String(masterInMsg.data.value)+" ");
  Serial.println(mqttTopic);
  mqttClient.publish((mqttTopic).c_str(), 1, true, String(masterInMsg.data.value).c_str());
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  if (status != ESP_NOW_SEND_SUCCESS)
  {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.print("Delivery Failed to ");
    for (size_t i = 0; i < (sizeof(mac_addr) / sizeof(uint8_t)); i++)
    {
      Serial.print(mac_addr[i]);
    }
    Serial.println();
  }
}

void setSlavePins()
{
  int count = sizeof(slave_pins) / sizeof(int);
  for (size_t i = 0; i < count; i++)
  {
    slave_pin_mode[i] = preferences.getInt(("sp" + String(slave_pins[i])).c_str());
  }
}

void setPeers(String mac_pref_string)
{
  String slave_mac_list[10];
  mac_pref_string.replace(" ", ""); // remove spaces
  char *slave_mac = strtok((char *)mac_pref_string.c_str(), ",");
  int i = 0;
  while (slave_mac != NULL)
  {
    slave_mac_list[i] = slave_mac;
    i++;
    slave_mac = strtok(NULL, ",");
  }
  peersCount = i;
  for (size_t j = 0; j < peersCount; j++)
  {
    uint8_t temp_mac[6];
    if (slave_mac_list[j])
    {
      char *macBit = strtok((char *)slave_mac_list[j].c_str(), ":");
      int k = 0;
      while (macBit != NULL)
      {
        int intBit = (int)strtol(macBit, 0, 16);
        temp_mac[k] = intBit;
        k++;
        macBit = strtok(NULL, ":");
      }

      memcpy(peers[j].peer_addr, temp_mac, 6);
      peers[j].channel = 0;
      peers[j].encrypt = false;
    }
  }
}