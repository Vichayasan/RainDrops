const char serverOTA[] = "raw.githubusercontent.com";
const int port = 443;
#define TINY_GSM_MODEM_SIM7600
#define SerialMon Serial
#define D0 21
#define countPing 5      // Count for offline ping to restart
#define sensMB_ID 1      // Modbus env.Gas ID
#define pingCount 5 // Error time count 5 to reset

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <EEPROM.h>
// #include "BluetoothSerial.h"
// #include "REG_SDM120.h"
#include <ModbusMaster.h>
#include <HardwareSerial.h>
// #include <MAGELLAN_MQTT.h>
// #include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <SSLClient.h>
#include <HTTPClientESP32.h>
#include <ArduinoOTA.h>
#include "SPIFFS.h"
#include <PubSubClient.h>
#include <ESPUI.h>
#include <SIM76xx.h>
#include <GSMNetwok.h>
#include <GSMClientSecure.h>
#include <RS485.h>
#include <Wire.h>

WiFiManager wifiManager;
WiFiClient WiFi_client;
GSMClientSecure GSMsecure;
HardwareSerial modbus(2);
ModbusMaster node;

//AIS Magellan
// MAGELLAN_MQTT magelWiFi(WiFi_client);
// MAGELLAN_MQTT magelGSM(gsm_mqtt_client);

// HardwareSerial modbus(2);
// HardwareSerial SerialAT(1);
// GSM Object
// TinyGsm modem(SerialAT);

// HTTPS Transport MQTT
// TinyGsmClient gsm_mqtt_client(modem, 0);

//PubSubclient
PubSubClient mqttWiFi(WiFi_client);
PubSubClient mqttGSM(GSMsecure);

// HTTPS Transport OTA
// TinyGsmClient base_client(modem, 1);
// SSLClient secure_layer(&base_client);
HttpClient GSMclient = HttpClient(GSMsecure, serverOTA, port);

bool connectWifi = false;

//AIS Magellan
bool resultSub = false;

String new_version;
const char version_url[] = "/Vichayasan/RainDrops/main/bin_version.txt";//"/Vichayasan/BMA/refs/heads/main/TX/bin_version.txt"; // "/IndustrialArduino/OTA-on-ESP/release/version.txt";  https://raw.githubusercontent.com/:owner/:repo/master/:path
const char* version_url_WiFiOTA = "https://raw.githubusercontent.com/Vichayasan/RainDrops/main/bin_version.txt";//"https://raw.githubusercontent.com/Vichayasan/BMA/refs/heads/main/TX/bin_version.txt"; // "/IndustrialArduino/OTA-on-ESP/release/version.txt";  https://raw.githubusercontent.com/:owner/:repo/master/:path

String firmware_url;
String current_version = "0.0.2";

//For PubSub
const char *magellanServer = "device-entmagellan.ais.co.th"; //"magellan.ais.co.th""device-entmagellan.ais.co.th"
String user_mqtt = "EKALUK";
String key = ""; //3C61056B4894
String secret = "";
String mqttStatus = "";
bool pubReqStatus, subRespStatus;
String token, hostUI;
float lat, lon;
String topicRes, topicReq;
int count_ping = 0;
uint32_t lastReconnectAttempt = 0;
unsigned int CountPing = 0;

String detectRain;

unsigned long previousMillis, periodOTA, periodGPS, periodRain;

uint16_t mb_buffer[7];
bool sensErr = 0;

String json, deviceToken;
String digitsOnlyToken = ""; // Create a new string for the digits

int gsm_count_fail = 0;

struct Meter
{
  // readSensor
  int sens_CO2;
  int sens_CH2O;
  int sens_TVOC;
  int sens_PM25;
  int sens_PM10;
  float sens_Temp;
  float sens_Humi;
};
Meter meter;

// put function declarations here:
int myFunction(int, int);
void t3onlinePing();

bool WiFicheckForUpdate(String &firmware_url)
{
  // HeartBeat();
  Serial.println("Making WiFi GET request securely...");

  HTTPClient http;
  
  http.begin(version_url_WiFiOTA);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    new_version = http.getString();
    new_version.trim();

    Serial.print("Response: ");
    Serial.println(new_version);
    Serial.println("Current version: " + current_version);
    Serial.println("Available version: " + new_version);

    if (new_version != current_version)
    {
      Serial.println("New version available. Updating...");
      firmware_url = String("https://raw.githubusercontent.com/Vichayasan/RainDrops/main/firmware") + new_version + ".bin";
      Serial.println("Firmware URL: " + firmware_url);
      return true;
    }
    else
    {
      Serial.println("Already on the latest version");
    }
  }
  else
  {
    Serial.println("Failed to check for update, HTTP code: " + String(httpCode));
  }

  http.end();
  return false;
}

void WiFiperformOTA(const char *firmware_url)
{
  // HeartBeat();

  // Initialize HTTP
  Serial.println("Making WiFi GET fiemware OTA request securely...");

  HTTPClient http;

  http.begin(firmware_url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);
    long contentlength_real = contentLength;

    Serial.print("Contentlength: ");
    Serial.println(contentLength);

    if (canBegin)
    {
      // HeartBeat();

      Serial.println("WiFi OTA Updating..");
      String OtaStat = "WiFi OTA Updating...";

      size_t written = Update.writeStream(http.getStream());

      if (written == contentLength)
      {
        Serial.println("Written : " + String(written) + " successfully");
      }
      else
      {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
      }

      if (Update.end())
      {
        Serial.println("OTA done!");
        if (Update.isFinished())
        {
          Serial.println("Update successfully completed. Rebooting.");
          delay(300);
          ESP.restart();
        }
        else
        {
          Serial.println("Update not finished? Something went wrong!");
        }
      }
      else
      {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    }
    else
    {
      Serial.println("Not enough space to begin OTA");
    }
  }
  else
  {
    Serial.println("Cannot download firmware. HTTP code: " + String(httpCode));
  }

  http.end();
}

void WiFi_OTA()
{
  Serial.println("---- WiFi OTA Check version before update ----");

  if (WiFicheckForUpdate(firmware_url))
  {

    WiFiperformOTA(firmware_url.c_str());
  }
}

bool checkForUpdate(String &firmware_url)
{
  // HeartBeat();

  Serial.println("Making GSM GET request securely...");
  GSMclient.get(version_url);
  int status_code = GSMclient.responseStatusCode();
  delay(1000);
  String response_body = GSMclient.responseBody();
  delay(1000);

  Serial.print("Status code: ");
  Serial.println(status_code);
  Serial.print("Response: ");
  Serial.println(response_body);

  response_body.trim();
  response_body.replace("\r", ""); // Remove carriage returns
  response_body.replace("\n", ""); // Remove newlines

  // Extract the version number from the response
  new_version = response_body;

  Serial.println("Current version: " + current_version);
  Serial.println("Available version: " + new_version);

  GSMclient.stop();

  if (new_version != current_version)
  {
    Serial.println("New version available. Updating...");
    firmware_url = String("/Vichayasan/RainDrops/main/firmware") + new_version + ".bin";// ***WITHOUT "/raw"***
    Serial.println("Firmware URL: " + firmware_url);

    return true;
  }
  else
  {
    Serial.println("Already on the latest version");
  }

  return false;
}

// Update the latest firmware which has uploaded to Github
void performOTA(const char *firmware_url)
{
  // HeartBeat();

  // Initialize HTTP
  Serial.println("Making GSM GET firmware OTA request securely...");
  GSMclient.get(firmware_url);
  int status_code = GSMclient.responseStatusCode();
  delay(1000);
  long contentlength = GSMclient.contentLength();
  delay(1000);

  Serial.print("Contentlength: ");
  Serial.println(contentlength);

  if (status_code == 200)
  {

    if (contentlength <= 0)
    {
      SerialMon.println("Failed to get content length");
      GSMclient.stop();
      return;
    }

    // Begin OTA update
    bool canBegin = Update.begin(contentlength);
    size_t written;
    long totalBytesWritten = 0;
    uint8_t buffer[1024];
    int bytesRead;
    long contentlength_real = contentlength;

    if (canBegin)
    {
      // HeartBeat();

      while (contentlength > 0)
      {
        // HeartBeat();

        bytesRead = GSMclient.readBytes(buffer, sizeof(buffer));
        if (bytesRead > 0)
        {
          written = Update.write(buffer, bytesRead);
          if (written != bytesRead)
          {
            Serial.println("Error: written bytes do not match read bytes");
            Update.abort();
            return;
          }
          totalBytesWritten += written; // Track total bytes written

          Serial.printf("Write %.02f%% (%ld/%ld)\n", (float)totalBytesWritten / (float)contentlength_real * 100.0, totalBytesWritten, contentlength_real);

          String OtaStat = "OTA Updating: " + String((float)totalBytesWritten / (float)contentlength_real * 100.0) + " % ";
          

          contentlength -= bytesRead; // Reduce remaining content length
        }
        else
        {
          Serial.println("Error: Timeout or no data received");
          break;
        }
      }

      if (totalBytesWritten == contentlength_real)
      {
        Serial.println("Written : " + String(totalBytesWritten) + " successfully");
      }
      else
      {
        Serial.println("Written only : " + String(written) + "/" + String(contentlength_real) + ". Retry?");

      }

      if (Update.end())
      {
        SerialMon.println("OTA done!");

        if (Update.isFinished())
        {
          SerialMon.println("Update successfully completed. Rebooting.");
          delay(300);
          ESP.restart();
        }
        else
        {
          SerialMon.println("Update not finished? Something went wrong!");
        }
      }
      else
      {
        SerialMon.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    }
    else
    {
      Serial.println("Not enough space to begin OTA");
    }
  }
  else
  {
    Serial.println("Cannot download firmware. HTTP code: " + String(status_code));
  }

  GSMclient.stop();
}

void GSM_OTA()
{
  Serial.println("---- GSM OTA Check version before update ----");

  if (checkForUpdate(firmware_url))
  {
    performOTA(firmware_url.c_str());
  }
}

void GSMinfo(){
  Serial.printf("ICCID: %s \n", GSM.getICCID().c_str());
  Serial.printf("IMEI: %s \n", GSM.getIMEI().c_str());
  Serial.printf("IMSI: %s \n", GSM.getIMSI().c_str());
  Serial.printf("getCurrentCarrier: %s \n", Network.getCurrentCarrier());
  Serial.printf("SignalStrength: %d \n", Network.getSignalStrength());
  Serial.printf("DeviceIP: %s \n", Network.getDeviceIP().toString());
  t3onlinePing();

}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("debud call back");
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // Convert payload to a String
  String jsonString;
  for (int i = 0; i < length; i++) {
    jsonString += (char)payload[i];
  }
  Serial.println(jsonString);  // Debugging: Print the received JSON

  // Parse JSON
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print("JSON Parsing failed: ");
    Serial.println(error.f_str());
    return;
  }

  token = doc["ThingToken"].as<String>();

  Serial.println("Token: " + token);
}

bool requestThingToken(String topic){
  String json;
  Serial.println("request Topic: " + topic);

  int str_len = 30;
  char char_array[str_len];
  json.toCharArray(char_array, str_len);
  
  // if (connectWifi){
  //   pubReqStatus = mqttWiFi.publish(topic.c_str(), char_array);
  //   Serial.print("requestThingToken: ");
  //   // Serial.println(pubReqStatus);
  //   Serial.println(pubReqStatus?"Succeed":"Fail");
  // }else{
    pubReqStatus = mqttGSM.publish(topic.c_str(), char_array);
    Serial.print("requestThingToken: ");
    // Serial.println(pubReqStatus);
    Serial.println(pubReqStatus?"Succeed":"Fail");
  // }
  return pubReqStatus;
}

boolean reconnectGSMMqtt()
{
  Serial.println("user: " + user_mqtt);
  Serial.println("Key: " + key);
  Serial.println("Secret: " + secret);

  topicReq = "api/v2/thing/" + String(GSM.getICCID().c_str()) + "/" + String(GSM.getIMSI().c_str()) + "/auth/req";
  topicRes = "api/v2/thing/" + String(key) + "/" + String(secret) + "/auth/resp";

  Serial.print("Connecting to ");
  Serial.print(String(magellanServer));
  boolean status = mqttGSM.connect(user_mqtt.c_str(), key.c_str(), secret.c_str());

  while (!mqttGSM.connect(user_mqtt.c_str(), key.c_str(), secret.c_str()))
  {
    Serial.println(" ...fail");
     Serial.printf("SignalStrength: %d \n", Network.getSignalStrength());
    Serial.println(mqttGSM.state());
    gsm_count_fail++;
      if (gsm_count_fail == 10){
        GSM.shutdown();
        ESP.restart();
      }
      delay(2000);
    
  }
    Serial.println(" ..success");
    Serial.println(F("Connect MQTT Success."));
    Serial.print("State after connect ");
    Serial.println(mqttGSM.state());
    
  
  
  Serial.println("topic Response: " + topicRes);
  delay(3000);
  subRespStatus = mqttGSM.subscribe(topicRes.c_str());

  delay(3000);
  Serial.print("Token Response:");
  // Serial.println(subRespStatus);
  Serial.println(subRespStatus?"Succeed":"Fail");
  Serial.print("State after sub tk ");
  Serial.println(mqttGSM.state());

  delay(3000);
  requestThingToken(topicReq);
  delay(3000);
  
  // String AAA = "thing";
  // mqttWiFi.subscribe(("api/v2/thing/"+ key + "/" + secret + "/auth/resp").c_str());
  return mqttGSM.connected();

}

void t3onlinePing()
{
  Serial.println();
  Serial.print(F("Ping www.google.com ... "));
  // SerialBT.println();
  // SerialBT.print(F("Ping www.google.com ... "));
  if (Network.pingIP("www.google.com"))
  {
    Serial.print(F("OK"));
    // SerialBT.print(F("OK"));
    count_ping = 0;
  }
  else
  {
    count_ping++;
    Serial.println(F("===================================="));
    Serial.print(F("Ping google.com FAIL count to RST: "));
    Serial.print(count_ping);
    Serial.print(F("/"));
    Serial.println(countPing);
    Serial.println(F("===================================="));
    if (count_ping >= countPing)
    {
      count_ping = 0;
      Serial.println(F("Shutting Down GSM."));
      // SerialBT.println(F("Shutting Down GSM."));
      GSM.shutdown();
      for (int a = 0; a < 10; a++)
      {
        GSM.shutdown();
        delay(1000);
      }
      ESP.restart();
    }
  }
  Serial.println();
}

void _initGSMmqtt(){

  // key.concat(digitsOnlyToken.c_str());
  // secret.concat(digitsOnlyToken.c_str());

  //Test Enterprise

  key = GSM.getICCID().c_str();
  secret = GSM.getIMSI().c_str();

  mqttGSM.setServer(magellanServer, 8883);
  mqttGSM.setCallback(callback);
  mqttGSM.setClient(GSMsecure);
  mqttGSM.setKeepAlive(36000);
  mqttGSM.setSocketTimeout(36000);

  // You can also add a check to see if the buffer was allocated successfully
  if (!mqttGSM.setBufferSize(512)) {
    Serial.println("Failed to allocate MQTT buffer");
    // SerialBT.println("Failed to allocate MQTT buffer");

  }

  if (user_mqtt != ""){
    reconnectGSMMqtt();
  }
  delay(3000);

  GSM_OTA();

}

void readModbus()
{
  // Read Modbus ENV _ M701 RS485
  delay(200);
  for (int a = 1; a <= 7; a++)
  {
    // delay(200);
    mb_buffer[a - 1] = RS485.holdingRegisterRead(sensMB_ID, a * 2);
    delay(1000);
  }
}


void readSensor()
{
  readModbus();

  if ((mb_buffer[0] > 30000) || (mb_buffer[1] > 30000) || (mb_buffer[2] > 30000) || (mb_buffer[3] > 30000) || (mb_buffer[4] > 30000) || (mb_buffer[5] > 30000) || (mb_buffer[6] > 30000))
  {
    sensErr = 1;
    Serial.println(F(""));
    Serial.println(F("===================================="));
    Serial.println(F("Modbus not stable"));
    
    readModbus();
  }
  else
  {
    sensErr = 0;
    Serial.println(F(""));
    Serial.println(F("===================================="));
    Serial.println(F("Modbus OK"));

    meter.sens_CO2 = mb_buffer[0];
    meter.sens_CH2O = mb_buffer[1];
    meter.sens_TVOC = mb_buffer[2];
    meter.sens_PM25 = mb_buffer[3];
    meter.sens_PM10 = mb_buffer[4];
    meter.sens_Temp = mb_buffer[5] * 0.1;
    meter.sens_Humi = mb_buffer[6] * 0.1;
  }
}

void getMac()
{
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("Get Mac");
  Serial.println("OK");
  Serial.print("+deviceToken: ");
  Serial.println(WiFi.macAddress());
  // SerialBT.print("Get Mac");
  // SerialBT.println("OK");
  // SerialBT.print("+deviceToken: ");
  // SerialBT.println(WiFi.macAddress());
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) {
      deviceToken += "0"; // Add leading zero if needed
    }
    deviceToken += String(mac[i], HEX); // Convert byte to hex
  }
  deviceToken.toUpperCase();
  // ESPUI.updateLabel(GUI.idLabel, String(deviceToken));

  // --- NEW CODE TO EXTRACT DIGITS ---
  for (int i = 0; i < deviceToken.length(); i++) {
    // Check if the character at the current position is a digit
    if (isDigit(deviceToken.charAt(i))) {
      // If it is a digit, add it to our new string
      digitsOnlyToken += deviceToken.charAt(i);
    }
  }

}

void setup() {

  // pinMode(16, INPUT);
  // digitalWrite(16, HIGH);
  // pinMode(17, INPUT);
  // digitalWrite(17, HIGH);
  // pinMode(4, INPUT);
  // digitalWrite(4, HIGH);

  // GSM.pinMode(13, INPUT);
  // GSM.digitalWrite(13, HIGH);
  // GSM.pinMode(14, INPUT);
  // GSM.digitalWrite(14, HIGH);

  pinMode(D0, INPUT);
  digitalWrite(D0, HIGH);
  // put your setup code here, to run once:
  Serial.begin(115200);
  GSMsecure.setInsecure();
  getMac();
  
  RS485.begin(9600, SERIAL_8N1);
  Serial.print(F("Setup GSM.."));
  
  while (!GSM.begin())
  {
    Serial.println(F(" fail"));
    gsm_count_fail++;
    if (gsm_count_fail == 10){
      GSM.shutdown();
      ESP.restart();
    }
    delay(2000);
  }
  delay(1000);
  Serial.println(F(" Success"));
  Serial.println();

  GSMinfo();
  _initGSMmqtt();

  // modbus.begin(9600, SERIAL_8N1, 16, 17);
  // // communicate with Modbus slave ID 1 over Serial (port 2)
  // node.begin(1, modbus);
  
}

void loop() {
  unsigned long currentMillis = millis();
  // put your main code here, to run repeatedly:
  if(user_mqtt != ""){

      

      if (!mqttGSM.connected())
      {
        SerialMon.println("=== GSM MQTT NOT CONNECTED ===");
        t3onlinePing();
        Serial.printf("SignalStrength: %d \n", Network.getSignalStrength());
        Serial.print("State just check = ");
        Serial.println(mqttGSM.state());
        Serial.print("getWriteError: ");
        Serial.println(mqttGSM.getWriteError());
        if (!GSM.begin())
        {
          SerialMon.println("Network disconnected");

          t3onlinePing();
        }

        // Reconnect every 10 seconds
        uint32_t t = millis() / 1000;
        if (t - lastReconnectAttempt >= 30)
        {
          lastReconnectAttempt = t;

          if (CountPing >= pingCount)
          {
            CountPing = 0;
            mqttGSM.disconnect();
            GSM.shutdown();
            ESP.restart();
          }
          CountPing++;

          if (reconnectGSMMqtt())
          {
            CountPing = 0;
            lastReconnectAttempt = 0;
            mqttGSM.subscribe(topicRes.c_str());
            requestThingToken(topicReq.c_str());
          }
        }
        delay(100);
        return;
      }

      mqttGSM.loop();

    }

  if (currentMillis - periodRain >= 500){
    periodRain = currentMillis;
    // Serial.println(digitalRead(D0));
    Serial.println();
    if (digitalRead(D0) == LOW){
      Serial.println("found_drop");
    }else{
      Serial.println("no_drop");
    }
  }

  if (currentMillis - previousMillis >= 5000){
    previousMillis = currentMillis;
    readSensor();
    // readMeter();
    // Serial.print("co2: ");
    // Serial.println(meter.sens_CO2);
    // Serial.print("CH20: ");
    // Serial.println(meter.sens_CH2O);
    // Serial.print("Hum: ");
    // Serial.println(meter.sens_Humi);
    // Serial.print("pm10: ");
    // Serial.println(meter.sens_PM10);
    // Serial.print("pm25: ");
    // Serial.println(meter.sens_PM25);
    // Serial.print("Temp: ");
    // Serial.println(meter.sens_Temp);
    // Serial.print("TVOC: ");
    // Serial.println(meter.sens_TVOC);

    Serial.println();

  
    String json = "";
    // json.concat("{\"DevicToken\":\"");
    // json.concat(deviceToken);
    // json.concat("\",\"ccid\":\"");
    // json.concat(Network.getDeviceIP());
    // json.concat("\",\"version\":\"");
    // json.concat(current_version);
    // json.concat("\",\"SignalStrength\":");
    // json.concat(Network.getSignalStrength());
    // json.concat(",\"ch20\":");
    json.concat("{\"ch20\":");
    json.concat(meter.sens_CH2O);
    json.concat(",\"co2\":");
    json.concat(meter.sens_CO2);
    json.concat(",\"hum\":");
    json.concat(meter.sens_Humi);
    json.concat(",\"pm10\":");
    json.concat(meter.sens_PM10);
    json.concat(",\"pm25\":");
    json.concat(meter.sens_PM25);
    json.concat(",\"temp\":");
    json.concat(meter.sens_Temp);
    json.concat(",\"TVOC\":");
    json.concat(meter.sens_TVOC);
    json.concat("}");
    Serial.println(json);

    int str_len = json.length() + 1;

    Serial.println();
    Serial.printf("str_len: %d \n", str_len);
    Serial.println();


    char char_array[str_len];
    json.toCharArray(char_array, str_len);

    // if(token.equals("")){
    //   Serial.println("topic Response: " + topicRes);
    //   Serial.println("request Topic: " + topicReq);
    //   mqttGSM.subscribe(topicRes.c_str());
    //   requestThingToken(topicReq.c_str());
    // }
    
    String topic = "api/v2/thing/" + token + "/report/persist";
    Serial.println("topic:" + topic);

    // Serial.printf("SignalStrength: %d \n", Network.getSignalStrength());
    Serial.print("State Before report ");
    Serial.println(mqttGSM.state());
    delay(3000);

    resultSub = mqttGSM.publish(topic.c_str(), char_array, str_len);
    Serial.print("[Status report]: ");
    Serial.println((resultSub)? "Sending via GSM SUCCESS" : "Sending via GSM FAIL");

    Serial.print("State After report ");
    Serial.println(mqttGSM.state());

  }

  if (currentMillis - periodOTA >= 60000){
    periodOTA = currentMillis;
    // Serial.print("Used space: ");
    // Serial.print(SPIFFS.usedBytes());
    // Serial.println(" Bytes");

    Serial.println("debug OTA");
    Serial.println();

    // if (connectWifi){
        // WiFi_OTA();
    //   }else{
        // GSM_OTA();
      // }
  }
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}
