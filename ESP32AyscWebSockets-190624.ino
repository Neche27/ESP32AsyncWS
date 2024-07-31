#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <FS.h>
#include "LittleFS.h"
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

 //Create an instance of the ESP8266WifiMulti class, called 'wifiMulti'

AsyncWebServer server(80); // Create a Asyncwebserver object that listens for HTTP request on port 80

AsyncWebSocket ws("/ws"); // create a websocket server on port 81

JSONVar readings; // JSON Variable to hold sensor readings
JSONVar buttonsJ;
JSONVar lights;

//Holds the light states
bool buttons[4] = {false, false, false, false};

//File fsUploadFile;

//Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

//WiFi ssid and password
const char* ssid = "SM-G970W1746";//"WIFI-D138";//"NechePhoneWifi";
const char* password = "zwir6661";//"across6605basic";//"Neche2005";

const char* apSSID = "ESP8266_AP"; // The name of the WiFi network that will be created
const char* apPassword = ""; // The password required to connect to it, leave blank for an open network

// A name and password for the OTA service
const char* OTAName = "ESP8266";
const char* OTAPassword = "esp8266";

// specify the pins with an RGB LED connected
#define LED_D4 19
#define LED_D2 23
#define DHT_D1 17
#define DHTTYPE    DHT11

const char* mdnsName = "esp8266LAN"; // Domain name for the mDNS responder

// Setup function declarations
void initWiFi();
void initWebSocket();
void startMDNS();
String getSensorReadings();
void notifyClients(String sensorReadings);
void initLittleFS();


//Server Handler declarations
void handleWebSocketMessage(void *arg, uint8_t* data, size_t len);
void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

//Helper functions declarations
void ChangeBtn(bool* A, const int& size, const int& btn);
String formatBytes(size_t bytes);
String getContentType(String filename);
void setHue(int hue);

DHT_Unified dht(DHT_D1, DHTTYPE);
uint32_t delayMS;
char clientVal = 'a';

void setup() {
  // put your setup code here, to run once:
  // the pins with LEDs connected are outputs
  pinMode(LED_D2, OUTPUT);
  pinMode(LED_D4, OUTPUT);

  Serial.begin(115200); //Start the serial communication to send messages to the computer
  delay(250);
  Serial.println("\r\n");

  initWiFi(); // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  //initLittleFS();
  //startOTA(); // Start the OTA service

  //startSPIFFS(); // Start the SPIFFS and list all contents

  initWebSocket(); // Start a WebSocket server

  startMDNS(); // Start the mDNS responder

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){// Start a HTTP server with a file read handler and an upload handler
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");

  server.begin(); //Start server

  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;
}

void loop() {
  // put your main code here, to run repeatedly:

  //MDNS.update(); // For mDNS processing

  //ArduinoOTA.handle(); // listen for OTA events

  if((millis() - lastTime) > timerDelay){
    String sensorReadings = getSensorReadings();
    Serial.print(sensorReadings);
    notifyClients(sensorReadings);
    lastTime = millis();
  }
  // Delay between measurements.
  //delay(delayMS);
  // Get temperature event and print its value.

  ws.cleanupClients();
 
}

//Setup Functions
void initWiFi(){ // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  //WiFi.mode(WIFI_STA);
 

  WiFi.softAP(apSSID, apPassword); // Start the access point
  Serial.print("Access Point \"");
  Serial.print(apSSID);
  Serial.println("\" started\r\n");
  //WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");

  Serial.print("Connecting ...");

  int i = 0;
  while(WiFi.softAPgetStationNum() < 1) // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
  {
    delay(2500);
    Serial.print(' ');
    Serial.print(++i);
  }

  Serial.println("\r\n");
  if(WiFi.softAPgetStationNum() == 0){
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID()); // Tell us what network we're connected to
  Serial.print("IP address: \t");
  Serial.println(WiFi.localIP()); // Send the IP address to the ESP8266 to the computer
  } else { // If a station is connected to the ESP SoftAP
    Serial.print("Station connected to ESP8266 AP");
    Serial.println(WiFi.localIP()); 
  }
  Serial.print("\r\n");

  Serial.println(WiFi.localIP());
}

void initWebSocket(){ // Start a WebSocket server
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  //webSocket.onEvent(webSocketEvent); // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("Websocket server started.");
}

void startMDNS(){ // Start the mDNS responder
  MDNS.begin(mdnsName); // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startClient()
{
  
}
//Server handlers

void handleWebSocketMessage(void* arg, uint8_t* data, size_t len){
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    clientVal = (char)data[0];
    String message = (char*)data;
    if (strcmp((char*)data, "getReadings") == 0) //Check if the message is "getReadings"
    {
      //if it is send current sensor readings
      String sensorReadings = getSensorReadings();
      Serial.print(sensorReadings);
      notifyClients(sensorReadings);
    }
     
  }
}

void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len){ // When a WebSocket message is received
  switch(type){
    case WS_EVT_DISCONNECT: // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", client->id());
      break;
    case WS_EVT_CONNECT: { // if a new websocket connection is established
      Serial.printf("WebSocket #%u client Connected from url: %s\n", client->id(), client->remoteIP().toString().c_str());
       // Turn rainbow off when a new Connection is established
    }
      break;
    case WS_EVT_DATA: // if new text data is received
      handleWebSocketMessage(arg, data, len);
      switch(clientVal)
      {
        case 'A':
         digitalWrite(LED_D4, HIGH);
         Serial.println(clientVal);
         ChangeBtn(buttons, 4, 0);
         break;
        case 'B':
         digitalWrite(LED_D4, HIGH);
         Serial.println(clientVal);
         ChangeBtn(buttons, 4, 1);
         break;
        case 'C':
         digitalWrite(LED_D4, HIGH);
         Serial.println(clientVal);
         ChangeBtn(buttons, 4, 2);
         break;
        case 'D':
         digitalWrite(LED_D4, HIGH);
         Serial.println(clientVal);
         ChangeBtn(buttons, 4, 3);
         break;
        case 'N':
        digitalWrite(LED_D4, LOW);
        Serial.println(clientVal);
        ChangeBtn(buttons, 4, 4);
        break;
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void notifyClients(String sensorReadings){
  ws.textAll(sensorReadings);
}

//Helper functions
void ChangeBtn(bool* A, const int& size, const int& btn){
  for(int i =0 ; i < 4; i++)
  {
    if(i == btn)
      continue;
    buttons[i] = false;
  }
}
String getSensorReadings(){
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) 
  {
    Serial.println(F("Error reading temperature!"));
  }
  
  readings["temperature"] = String(event.temperature);
  dht.humidity().getEvent(&event);
  readings["humidity"] = String(event.relative_humidity);
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

String getLights(){
  lights["button_1"] = String(digitalRead(LED_D4));
  lights["button_2"] = String(digitalRead(LED_D2));
  String jsonString = JSON.stringify(lights);
  return jsonString;
}

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if(bytes < 1024){
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if(filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}
