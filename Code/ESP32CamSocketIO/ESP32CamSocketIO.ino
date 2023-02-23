#include <Arduino.h>
#include <AccelStepper.h>
#include <ArduinoJson.h>
#include <esp_camera.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <WiFi.h>
#include <LITTLEFS.h>
#define FILESYSTEM LITTLEFS
/*#include "camera_pins.h"*/

//#define STEPS 2048  // One revolution for the Z-Stage motor

// states
bool busyState = false;
bool wasRunning = false;
bool ledState= true;  // LED starts on

// definition for stepper motor
const int stepperOneA = 15;
const int stepperOneB = 13;
const int stepperOneC = 14;
const int stepperOneD = 12;
AccelStepper stepperOne(AccelStepper::HALF4WIRE, stepperOneA, stepperOneB, stepperOneC, stepperOneD);

// setup of LED Array
#define LED_PIN 4
#define NUM_LEDS 16
Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// switch for stream start
bool streamRunning = false;
bool isBusy = false;
String binaryLeadFrame;

//modified socketIO client, added methode to send binary data
class SocketIOclientMod : public SocketIOclient {
  public:
  bool sendBIN(uint8_t * payload, size_t length, bool headerToPayload = false);
  bool sendBIN(const uint8_t * payload, size_t length);
};

bool SocketIOclientMod::sendBIN(uint8_t * payload, size_t length, bool headerToPayload) {
  bool ret = false;
  if (length == 0) {
    length = strlen((const char *) payload);
  }
  ret = sendFrame(&_client, WSop_text, (uint8_t *) binaryLeadFrame.c_str(), strlen((const char*) binaryLeadFrame.c_str()), true, headerToPayload);
  if (ret) {
    ret = sendFrame(&_client, WSop_binary, payload, length, true, headerToPayload);
  }
  return ret;
}

bool SocketIOclientMod::sendBIN(const uint8_t * payload, size_t length) {
    return sendBIN((uint8_t *)payload, length);
}

SocketIOclientMod socketIO;

struct {
  String ssid;
  String password;
  String socketIP;
  int socketPort;
  String socketURL;
  String componentID;
  String stepperOneName;
  long stepperOnePos;
} settings;

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

void saveSettings() {
  DynamicJsonDocument doc(1024);

  doc["ssid"] = settings.ssid;
  doc["password"] = settings.password;
  doc["socketIP"] = settings.socketIP;
  doc["socketPort"] = settings.socketPort;
  doc["socketURL"] = settings.socketURL;
  doc["componentID"] = settings.componentID;
  doc["stepperOneName"] = settings.stepperOneName;
  doc["stepperOnePos"] = stepperOne.currentPosition();

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

void startStreaming() {
  binaryLeadFrame = "451-[\"pic\",{\"component\":\"";
  binaryLeadFrame += settings.componentID;
  binaryLeadFrame += "\",\"image\":{\"_placeholder\":true,\"num\":0}}]";
  streamRunning = true;
  reportState();
}

void stopStreaming() {
  streamRunning = false;
  reportState();
}

void camLoop() {
  if (streamRunning && (WiFi.status() == WL_CONNECTED) && (socketIO.isConnected())) {
    camera_fb_t *fb = esp_camera_fb_get();
    socketIO.sendBIN(fb->buf,fb->len);
    esp_camera_fb_return(fb);
    Serial.printf("frame send!\n");
  }
}

// ESP32Cam (AiThinker) PIN Map

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 20000000,//EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_SVGA,//UXGA,//QQVGA-QXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12,//10, //0-63 lower number means higher quality
    .fb_count = 2, //if more than one, i2s runs in continuous mode. Use only with JPEG
    //.grab_mode = CAMERA_GRAB_WHEN_EMPTY//CAMERA_GRAB_LATEST. Sets when buffers should be filled
};

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case sIOtype_DISCONNECT:
      Serial.printf("[IOc] Disconnected!\n");
      break;
    case sIOtype_CONNECT:
      Serial.printf("[IOc] Connected to URL: %s\n", payload);

      // join default namespace
      socketIO.send(sIOtype_CONNECT, "/");
      reportState();
      break;
    case sIOtype_EVENT:
      eventHandler(payload,length);
      /*-----------------------Add Stepper Code Here??? -> or do it in eventHandler function??? ------------------------ */
      break;
    case sIOtype_ACK:
      Serial.printf("[IOc] get ack: %u\n", length);
      break;
    case sIOtype_ERROR:
      Serial.printf("[IOc] get error: %u\n", length);
      break;
    case sIOtype_BINARY_EVENT:
      Serial.printf("[IOc] get binary: %u\n", length);
      break;
    case sIOtype_BINARY_ACK:
      Serial.printf("[IOc] get binary ack: %u\n", length);
      break;
  }
}

void eventHandler(uint8_t * eventPayload, size_t eventLength) {
  char * sptr = NULL;
  int id = strtol((char *)eventPayload, &sptr, 10);
  Serial.printf("[IOc] get event: %s     (id: %d)\n", eventPayload, id);
  DynamicJsonDocument incomingEvent(1024);
  DeserializationError error = deserializeJson(incomingEvent,eventPayload,eventLength);
  if(error) {
    Serial.printf("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  String eventName = incomingEvent[0];
  Serial.printf("[IOc] event name: %s\n", eventName.c_str());

  if (strcmp(eventName.c_str(),"command") == 0) {
    // analyze and store input
    JsonObject receivedPayload = incomingEvent[1];
    String component = receivedPayload["componentId"];
    Serial.println("identified command event");

    // act only when involving this component
    if (strcmp(component.c_str(),settings.componentID.c_str()) == 0) {
      // check for simple or extended command structure
      Serial.printf("componentId identified (me): %s\n", component);
      if (receivedPayload["command"].is<JsonObject>()) {  // stepper
        JsonObject command = receivedPayload["command"];
        Serial.println("identified nested command structure");
        String control = receivedPayload["controlId"];
        Serial.printf("identified controlID: %s\n", control);
        int steps = command["steps"];
        driveStepper(control,steps);
      }
      else if (receivedPayload["command"].is<String>()) {
        String command = receivedPayload["command"];
        Serial.printf("identified simple command: %s\n", command.c_str());
        if (strcmp(command.c_str(),"getStatus") == 0) {
          reportState();
        }
        if (strcmp(command.c_str(),"stop") == 0) {
          stepperOne.stop();
          Serial.println("Received stop command. Issuing stop at maximum decceleration.");
        }
        if (strcmp(command.c_str(),"restart") == 0) {
          Serial.println("Received restart command. Disconnecting now.");
          stopStreaming();
          delay(500);
          saveSettings();
          ESP.restart();
        }
        if (strcmp(command.c_str(),"startStreaming") == 0) {
          startStreaming();
        }
        if (strcmp(command.c_str(),"stopStreaming") == 0) {
          stopStreaming();
        }
        if (strcmp(command.c_str(),"ledOn") == 0) {
          ledState = true;
          Serial.println("Turning LEDs On");
          pixels.setBrightness(10);
          setLEDs();
          pixels.show();
        }
        if (strcmp(command.c_str(),"ledOff") == 0) {
          ledState = false;
          Serial.println("Turning LEDs Off");
          pixels.clear();
          pixels.show();
        }
      }
    }
  }
}

void reportState() {
  DynamicJsonDocument outgoingEvent(1024);
  JsonArray payload = outgoingEvent.to<JsonArray>();
  payload.add("status");
  JsonObject parameters = payload.createNestedObject();
  parameters["component"] = settings.componentID;
  JsonObject state = parameters.createNestedObject("status");
  state["streaming"] = streamRunning;
  state["busy"] = busyState;
  state[settings.stepperOneName] = stepperOne.currentPosition();
  state["led"] = ledState;
  //state["STATE"] = value_to_be_shown
  String output;
  serializeJson(payload, output);
  socketIO.sendEVENT(output);
  Serial.print("string sent: ");
  Serial.println(output);
}

static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        Serial.printf("Camera Init Failed: %s\n", err);
        return err;
    }

    return ESP_OK;
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("[System] lost connection. Trying to reconnect.\n");
  WiFi.reconnect();
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("[System] connected to WiFi\n");
}

void setLEDs()
{
  for(int i = 0; i < NUM_LEDS; i++)
  {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  }
}

void driveStepper(String stepperName, int steps) {
  busyState = true;
  reportState();
  if (stepperName == settings.stepperOneName) {
    Serial.printf("moving %s by %d steps... ", settings.stepperOneName, steps);
    stepperOne.move(steps);
    wasRunning = true;
  }
}

void depowerStepper() {
  digitalWrite(stepperOneA,0);
  digitalWrite(stepperOneB,0);
  digitalWrite(stepperOneC,0);
  digitalWrite(stepperOneD,0);
}

void setup() {
  Serial.begin(115200);
  for(uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  
  loadSettings();
  WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);

  socketIO.begin(settings.socketIP.c_str(), settings.socketPort, settings.socketURL.c_str());
  socketIO.onEvent(socketIOEvent);

  stepperOne.setMaxSpeed(15);
  stepperOne.setAcceleration(10);

  // set up LED array
  pixels.begin();
  pixels.clear();

  // turn on LED array
  pixels.setBrightness(10);
  setLEDs();
  pixels.show();
  
  init_camera();
  Serial.printf("setup done\n");
}

void loop() {
  if (Serial.available() > 0) {
    String SerialInput=Serial.readStringUntil('\n');
    eventHandler((uint8_t *)SerialInput.c_str(), SerialInput.length());
  }
  camLoop();
  socketIO.loop();
  
  if (stepperOne.isRunning) {
    stepperOne.run();
  }
  else if (wasRunning){
    depowerStepper();
    if ((socketIO.isConnected()) and (WiFi.status() == WL_CONNECTED)) {
      busyState = false;
      wasRunning = false;
      Serial.printf("done. New Position: %d\n", stepperOne.currentPosition());
      reportState();
    }
  }
}
