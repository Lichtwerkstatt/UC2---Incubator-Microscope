#include <Arduino.h>
#include <ArduinoJson.h>
#include <LITTLEFS.h>
#define FILESYSTEM LITTLEFS

struct {
  String ssid = "lichtwerkstatt-dev";
  String password = "OPMSJena";
  String socketIP = "192.168.1.1";
  int socketPort = 5008;
  String socketURL = "/socket.io/?EIO=5"; // Changed EIO=4 to EIO=5 -> does this matter??
  String componentID = "ESP32Cam_1";
  String stepperOneName = "z_stage";
  long stepperOnePos = 0;
} settings;

void saveSettings() {
  DynamicJsonDocument doc(1024);

  doc["ssid"] = settings.ssid;
  doc["password"] = settings.password;
  doc["socketIP"] = settings.socketIP;
  doc["socketPort"] = settings.socketPort;
  doc["socketURL"] = settings.socketURL;
  doc["componentID"] = settings.componentID;
  doc["stepperOneName"] = settings.stepperOneName;
  doc["stepperOnePos"] = settings.stepperOnePos;

  if(!FILESYSTEM.begin()){
    Serial.println("An Error has occurred while mounting LITTLEFS");
    return;
  }
  
  File file = FILESYSTEM.open("/settings.txt", "w");
  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println("Failed to write file");
  }
  file.close();
}

void loadSettings() {
  if(!FILESYSTEM.begin()){
    Serial.println("An Error has occurred while mounting LITTLEFS");
    return;
  }
  File file = FILESYSTEM.open("/settings.txt","r");
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.printf("deserializeJson() while loading settings: ");
    Serial.println(error.c_str());
  }
  file.close();

  Serial.println("=======================");
  Serial.println("loading settings");
  Serial.println("=======================");
  settings.ssid = doc["ssid"].as<String>();
  Serial.printf("SSID: %s\n", settings.ssid.c_str());
  settings.password = doc["password"].as<String>();
  Serial.printf("password: %s\n", settings.password.c_str());
  settings.socketIP = doc["socketIP"].as<String>();
  Serial.printf("socket IP: %s\n", settings.socketIP.c_str());
  settings.socketPort = doc["socketPort"];
  Serial.printf("Port: %i\n", settings.socketPort);
  settings.socketURL = doc["socketURL"].as<String>();
  Serial.printf("URL: %s\n", settings.socketURL.c_str());
  settings.componentID = doc["componentID"].as<String>();
  Serial.printf("component ID: %s\n", settings.componentID.c_str());
  settings.stepperOneName = doc["stepperOneName"].as<String>();
  Serial.printf("Stepper One Name: %s\n", settings.stepperOneName.c_str());
  settings.stepperOnePos = doc["stepperOnePos"].as<String>();
  Serial.printf("Stepper One Position: %f\n", settings.stepperOnePos);
  Serial.println("=======================");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;//wait for serial interface
  }
  if (!LITTLEFS.begin(false)) {
    Serial.println("LittleFS mount failed");
    Serial.println("file system not found; formating now");
    // try again if mounting fails
    if (!LITTLEFS.begin(true)) {
      Serial.println("LittleFs mount failed again");
      Serial.println("unable to format LittleFs");
      return;
    }
    else {
      Serial.println("LittleFs formated successfully");
      Serial.println("trying to write settings file");
      saveSettings();
    }
  }
  else {
    Serial.println("LittleFs mounted successfully");
    Serial.println("trying to write settings file");
    saveSettings();
  }
}

void loop() {
  Serial.println("loading existing settings...");
  loadSettings();
  delay(10000);
}
