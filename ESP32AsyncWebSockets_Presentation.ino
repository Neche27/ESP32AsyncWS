#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <FastLED.h>

AsyncWebServer server(80); // Create a Asyncwebserver object that listens for HTTP request on port 80

AsyncWebSocket ws("/ws"); // create a websocket server on port 81

JSONVar readings; // JSON Variable to hold sensor readings
JSONVar buttonsJ;
JSONVar lights;

//Holds the light states
bool buttons[4] = {false, false, false, false};


//Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

//WiFi ssid and password
const char* ssid = "";
const char* password = "";

const char* apSSID = "ESP8266_AP"; // The name of the WiFi network that will be created
const char* apPassword = ""; // The password required to connect to it, leave blank for an open network

// specify the pins with an RGB LED connected
#define LED_D4 19
#define LED_D2 23
#define DHT_D1 17
#define DHTTYPE    DHT11
#define NUMLED 60

const char* mdnsName = "esp8266LAN"; // Domain name for the mDNS responder

int strSize = 8;
CRGB colors[8] = {CRGB::Red,CRGB::Blue,CRGB::Green,CRGB::Cyan,CRGB::Violet,CRGB::Orange,CRGB::Black,CRGB::White};
String strCom[8] = {"Red","Blue","Green","Cyan","violet","Orange","Black","White"};
CRGB leds[NUMLED];

// Setup function declarations
void initWiFi();
void initWebSocket();
void startMDNS();
String getSensorReadings();
void notifyClients(String sensorReadings);


//Server Handler declarations
void handleWebSocketMessage(void *arg, uint8_t* data, size_t len);
void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

//Helper functions declarations
void ChangeBtn(bool* A, const int& size, const int& btn);
String formatBytes(size_t bytes);
String getContentType(String filename);

void turnOffLED();
void ledAnimation1();
void ledAnimation2();
void ledAnimation3();
void turnONLEDSolid(CRGB&);

DHT_Unified dht(DHT_D1, DHTTYPE);
uint32_t delayMS;
char clientVal = 'a';
String val = "";
unsigned int receivedVal = 0;

void setup() {
  // put your setup code here, to run once:
  // the pins with LEDs connected are outputs
  pinMode(LED_D2, OUTPUT);
  pinMode(LED_D4, OUTPUT);
 
  Serial.begin(9600);
  delay(250);
  Serial.println("\r\n");

  initWiFi(); // Start a Wi-Fi access point

  initWebSocket(); // Start a WebSocket server

  startMDNS(); // Start the mDNS responder

  server.begin(); //Start server

  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;

  FastLED.addLeds<WS2812B, LED_D2, GRB>(leds, NUMLED);
  FastLED.setBrightness(50);
  Serial.println("ENSC PROJECT......");
}

void loop() {
  // put your main code here, to run repeatedly:
  if(Serial.available()){
    Serial.println("Data From SU-03T");

    byte highBits = Serial.read();
    byte lowBits = Serial.read();

    receivedVal = (highBits << 8) | lowBits;

    Serial.print("Received HEX values: ");
    Serial.println(receivedVal, HEX);
  }
  if(receivedVal == 0xAA11){
    fill_solid(leds, NUMLED, CRGB::Green);
    FastLED.show();
   } else if(receivedVal == 0xAA12) {
    turnOffLED();
  } else if(receivedVal == 0xAA13) {
    ledAnimation1();
  } else if(receivedVal == 0xAA14) {
    ledAnimation2();
  }
  
 delay(10);
 receivedVal = 0;
  
  switch(clientVal)
      {
        case 'A':
          Serial.println(clientVal);
          ChangeBtn(buttons, 4, 0);
          ledAnimation1();
          break;
        case 'B':
          Serial.println(clientVal);
          ChangeBtn(buttons, 4, 1);
          ledAnimation2();
          break;
        case 'C':
          Serial.println(clientVal);
          ChangeBtn(buttons, 4, 2);
          ledAnimation3();
          break;
        case 'D':
          Serial.println(clientVal);
          ChangeBtn(buttons, 4, 3);
          ledAnimation1();
          break;
        case 'N':
          Serial.println(clientVal);
          ChangeBtn(buttons, 4, 4);
          ledAnimation1();
          break;
      }
      clientVal = 'a';
      if(val.length() > 2){
        for(int i = 0; i < strSize; i++)
        {
          if(strCom[i] == val)
          {
            turnONLEDSolid(colors[i]);
          }
        }
      }

  if((millis() - lastTime) > timerDelay){
    String sensorReadings = getSensorReadings();
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

//Server handlers

void handleWebSocketMessage(void* arg, uint8_t* data, size_t len){
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    clientVal = (char)data[0];
    val = (char*)data;
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

void turnONLEDSolid(CRGB& color){
  fill_solid(leds, NUMLED, color);
    FastLED.show();
}
void turnOffLED(){
  fill_solid(leds, NUMLED, CRGB::Black);
    FastLED.show();
}
void ledAnimation1(){
  for (int i = 0; i < NUMLED; i++)
    {
      if(ws.availableForWriteAll()){
      leds[i] = CRGB::Green;
      FastLED.show();
      delay(250);
      leds[i] = CRGB::Black;
      FastLED.show();
    }
    }
}
void ledAnimation2(){
  for (int i = 0; i < NUMLED; i++)
    {
      if(ws.availableForWriteAll()){
      leds[i] = CRGB::Blue;
      FastLED.show();
      delay(250);
      leds[i] = CRGB::Green;
      FastLED.show();
      delay(250);
      leds[i] = CRGB::Red;
      FastLED.show();
      delay(250);
      leds[i] = CRGB::Black;
      FastLED.show();
    }
    }
}
void ledAnimation3(){
  for (int i = 0; i < NUMLED; i++)
    {
      if(ws.availableForWriteAll()){
      leds[i] = CRGB::BlueViolet;
      FastLED.show();
      delay(250);
      leds[i] = CRGB::Yellow;
      FastLED.show();
      delay(250);
      leds[i] = CRGB::Purple;
      FastLED.show();
      delay(250);
      leds[i] = CRGB::Black;
      FastLED.show();
    }
    }
}