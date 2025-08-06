const char serverOTA[] = "raw.githubusercontent.com";
const int port = 443;
#define TINY_GSM_MODEM_SIM7600
#define SerialMon Serial
#define D0 21
#define countPing 5      // Count for offline ping to restart
#define sensMB_ID 1      // Modbus env.Gas ID
#define pingCount 5 // Error time count 5 to reset

#include <Arduino.h>
#include <MAGELLAN_SIM7600E_MQTT.h>
#include <WiFi.h>
#include <RS485.h>
#include <Wire.h>
// #include <GSMClientSecure.h>
// #include <HttpClient.h>
// #include <WiFiManager.h>
// #include <ESPUI.h>
#include <BluetoothSerial.h>


MAGELLAN_SIM7600E_MQTT magel;
// GSMClientSecure secureLayer;
// HttpClient GSMclient = HttpClient(secureLayer, serverOTA, port);
BluetoothSerial SerialBT;

bool connectWifi = false;

//AIS Magellan
bool resultSub = false;

String new_version;
const char version_url[] = "/Vichayasan/RainDrops/main/bin_version.txt";//"/Vichayasan/BMA/refs/heads/main/TX/bin_version.txt"; // "/IndustrialArduino/OTA-on-ESP/release/version.txt";  https://raw.githubusercontent.com/:owner/:repo/master/:path
const char* version_url_WiFiOTA = "https://raw.githubusercontent.com/Vichayasan/RainDrops/main/bin_version.txt";//"https://raw.githubusercontent.com/Vichayasan/BMA/refs/heads/main/TX/bin_version.txt"; // "/IndustrialArduino/OTA-on-ESP/release/version.txt";  https://raw.githubusercontent.com/:owner/:repo/master/:path

String firmware_url;
String current_version = "0.0.4";

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
int checkStatusUpdate = UNKNOWN;

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

// bool checkForUpdate(String &firmware_url)
// {
//   // HeartBeat();

//   Serial.println("Making GSM GET request securely...");
//   GSMclient.get(version_url);
//   int status_code = GSMclient.responseStatusCode();
//   delay(1000);
//   String response_body = GSMclient.responseBody();
//   delay(1000);

//   Serial.print("Status code: ");
//   Serial.println(status_code);
//   Serial.print("Response: ");
//   Serial.println(response_body);

//   response_body.trim();
//   response_body.replace("\r", ""); // Remove carriage returns
//   response_body.replace("\n", ""); // Remove newlines

//   // Extract the version number from the response
//   new_version = response_body;

//   Serial.println("Current version: " + current_version);
//   Serial.println("Available version: " + new_version);

//   GSMclient.stop();

//   if (new_version != current_version)
//   {
//     Serial.println("New version available. Updating...");
//     firmware_url = String("/Vichayasan/RainDrops/main/firmware") + new_version + ".bin";// ***WITHOUT "/raw"***
//     Serial.println("Firmware URL: " + firmware_url);

//     return true;
//   }
//   else
//   {
//     Serial.println("Already on the latest version");
//   }

//   return false;
// }

// // Update the latest firmware which has uploaded to Github
// void performOTA(const char *firmware_url)
// {
//   // HeartBeat();

//   // Initialize HTTP
//   Serial.println("Making GSM GET firmware OTA request securely...");
//   GSMclient.get(firmware_url);
//   int status_code = GSMclient.responseStatusCode();
//   delay(1000);
//   long contentlength = GSMclient.contentLength();
//   delay(1000);

//   Serial.print("Contentlength: ");
//   Serial.println(contentlength);

//   if (status_code == 200)
//   {

//     if (contentlength <= 0)
//     {
//       SerialMon.println("Failed to get content length");
//       GSMclient.stop();
//       return;
//     }

//     // Begin OTA update
//     bool canBegin = Update.begin(contentlength);
//     size_t written;
//     long totalBytesWritten = 0;
//     uint8_t buffer[1024];
//     int bytesRead;
//     long contentlength_real = contentlength;

//     if (canBegin)
//     {
//       // HeartBeat();

//       while (contentlength > 0)
//       {
//         // HeartBeat();

//         bytesRead = GSMclient.readBytes(buffer, sizeof(buffer));
//         if (bytesRead > 0)
//         {
//           written = Update.write(buffer, bytesRead);
//           if (written != bytesRead)
//           {
//             Serial.println("Error: written bytes do not match read bytes");
//             Update.abort();
//             return;
//           }
//           totalBytesWritten += written; // Track total bytes written

//           Serial.printf("Write %.02f%% (%ld/%ld)\n", (float)totalBytesWritten / (float)contentlength_real * 100.0, totalBytesWritten, contentlength_real);

//           String OtaStat = "OTA Updating: " + String((float)totalBytesWritten / (float)contentlength_real * 100.0) + " % ";
          

//           contentlength -= bytesRead; // Reduce remaining content length
//         }
//         else
//         {
//           Serial.println("Error: Timeout or no data received");
//           break;
//         }
//       }

//       if (totalBytesWritten == contentlength_real)
//       {
//         Serial.println("Written : " + String(totalBytesWritten) + " successfully");
//       }
//       else
//       {
//         Serial.println("Written only : " + String(written) + "/" + String(contentlength_real) + ". Retry?");

//       }

//       if (Update.end())
//       {
//         SerialMon.println("OTA done!");

//         if (Update.isFinished())
//         {
//           SerialMon.println("Update successfully completed. Rebooting.");
//           delay(300);
//           ESP.restart();
//         }
//         else
//         {
//           SerialMon.println("Update not finished? Something went wrong!");
//         }
//       }
//       else
//       {
//         SerialMon.println("Error Occurred. Error #: " + String(Update.getError()));
//       }
//     }
//     else
//     {
//       Serial.println("Not enough space to begin OTA");
//     }
//   }
//   else
//   {
//     Serial.println("Cannot download firmware. HTTP code: " + String(status_code));
//   }

//   GSMclient.stop();
// }

// void GSM_OTA()
// {
//   Serial.println("---- GSM OTA Check version before update ----");

//   if (checkForUpdate(firmware_url))
//   {
//     performOTA(firmware_url.c_str());
//   }
// }

void _initGSMmqtt(){

  setting.endpoint = magellanServer; //if not set *default: magellan.ais.co.th
  magel.OTA.autoUpdate(); // this function ENABLED by default unless you set FALSE
  setting.clientBufferSize = defaultOTABuffer; // if not set *default: 1024
  magel.begin(setting);
  //* callback getControl
  // magel.getControlJSON([](String controls){ 
  //   Serial.print("# Control incoming JSON: ");
  //   Serial.println(controls);
  //   String control = magel.deserializeControl(controls);
  //   magel.control.ACK(control); //ACKNOWLEDGE control to magellan ⚠️ important to Acknowledge control value to platform
  // });

  magel.info.getBoardInfo();
  

  // GSM_OTA();

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

    SerialBT.println(F("Modbus not stable"));
    
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

void GSMinfo(){

  SerialBT.printf("Host name: %s \n", magel.info.getHostName().c_str());
  SerialBT.printf("Thing Token: %s \n",magel.info.getThingToken().c_str());
  SerialBT.printf("ICCID: %s \n",magel.info.getICCID().c_str());
  SerialBT.printf("IMEI: %s \n",magel.info.getIMEI().c_str());
  SerialBT.printf("IMSI: %s \n",magel.info.getIMSI().c_str());
  SerialBT.printf("CurrentCarrier: %s \n",Network.getCurrentCarrier().c_str());
  SerialBT.printf("Device IP: %s \n",Network.getDeviceIP().toString());
  SerialBT.printf("Signal Strength: %d \n",Network.getSignalStrength());

}

void setup() {

  pinMode(D0, INPUT);
  digitalWrite(D0, HIGH);
  // put your setup code here, to run once:
  Serial.begin(115200);
  getMac();
  String hostBT = "Thor-Debug-" + deviceToken;
  SerialBT.begin(hostBT.c_str());
  
  
  RS485.begin(9600, SERIAL_8N1);
  Serial.print(F("Setup GSM.."));
  // secureLayer.setInsecure();

  
  _initGSMmqtt();
  GSMinfo();
  
}

void loop() {
  unsigned long currentMillis = millis();
  // put your main code here, to run repeatedly:

  magel.loop();

  magel.subscribes([](){
    checkStatusUpdate = magel.OTA.checkUpdate();
       magel.subscribe.report.response(); //if using MessageId please subscribe response report to check status and MessageId is acepted from platform.
     });

  magel.interval(10, [](){

    RetransmitSetting settingReport; //decleare object variable for setting report
    ResultReport result; //decleare object variable for receive result report
    settingReport.setEnabled(true); //true: retransmit / false: report with MsgId only
    settingReport.setRepeat(5); //default: 2 attempt *retransmit max 2 time to cancel attempt
    settingReport.setDuration(7); //default: 5 sec. delay wait duration every retransmit
    settingReport.generateMsgId(); //optional: regenerateMsgId if manual using ".setMsgId(msgId)"

    readSensor();

    //{1.} auto buildJSON and reportJSON
    magel.sensor.add("ch20", meter.sens_CH2O);
    magel.sensor.add("co2", meter.sens_CO2);
    magel.sensor.add("hum", meter.sens_Humi);
    magel.sensor.add("pm10", meter.sens_PM10);
    magel.sensor.add("pm25", meter.sens_PM25);
    magel.sensor.add("Temp", meter.sens_Temp);
    magel.sensor.add("TVOC", meter.sens_TVOC);
    magel.sensor.add("Rain_detect", digitalRead(D0)?0:100);
    magel.sensor.add("4G_SignalStrength ", Network.getSignalStrength());
    magel.sensor.add("firmware_version ", current_version);

    result = magel.sensor.report(settingReport);
    Serial.print("[MsgId report]: ");
    Serial.println(result.msgId);
    Serial.print("[Status report]: ");
    Serial.println((result.statusReport)? "SUCCESS" : "FAIL");

    SerialBT.print("[MsgId report]: ");
    SerialBT.println(result.msgId);
    SerialBT.print("[Status report]: ");
    SerialBT.println((result.statusReport)? "SUCCESS" : "FAIL");

    


  });

}
