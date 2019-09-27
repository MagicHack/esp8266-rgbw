#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "NeoPixelBus.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include "animations.hpp"

#define LED_OFF HIGH
#define LED_ON LOW

#include "secrets.h"

const unsigned long MAX_CLIENT_TIMEOUT = 120 * 1000;
const unsigned long MAX_WIFI_TIMEOUT = 30 * 1000;
const unsigned long ANIMATION_DELAY = 50;
const auto FADING_TIME = 2.0;

const char* SUBSCRIPTION = "salon/strip/#";

bool brightnessUpdated = false;
bool saturationUpdated = false;
bool hueUpdated = false;
bool isOn = false;
bool isFading = false;
bool rainbowMode = true;
uint8_t brightness = 0;
uint8_t saturation = 0;
float hue = 0;

// LED strip output is on Rx/D0
const uint16_t pixelCount = 187;
NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> strip(pixelCount);

// Put to false when connected to led strip to not send random data
const bool PRINT_DEBUG = false;

template<typename T>
void printDebug(const T& val){
  if(PRINT_DEBUG){
    Serial.begin(115200);
    Serial.print(val);
  }
}

template<typename T>
void printlnDebug(const T& val){
  if(PRINT_DEBUG){
    Serial.begin(115200);
    Serial.println(val);
  }
}

void toggle(uint8_t pin) {
  digitalWrite(pin, !digitalRead(pin));
}

// NTP for time of the day
const auto utcOffsetInSeconds = -4 * 60 * 60; // UTC -4
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "north-america.pool.ntp.org", utcOffsetInSeconds);


// MQTT client
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void printTime(){
  printlnDebug(timeClient.getFormattedTime());
}

void setupOTA(){
  ArduinoOTA.setHostname("esp8266-leds-salon");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    printlnDebug("Starting OTA");
    fillColor(strip, pixelCount, RgbColor(0, 255, 0));
    strip.Show();
    delay(200);
  });

  ArduinoOTA.onEnd([]() {
    printlnDebug("OTA finished");

    for(uint16_t i = 0; i < pixelCount; i++) {
      if(i % 2) {
        strip.SetPixelColor(i, RgbColor(0, 255, 0));
      } else {
        strip.SetPixelColor(i, RgbColor(0, 0, 255));
      }
    }
    strip.Show();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percentage = (float) progress / (float) total * 100;
    static int lastUp = 0;
    if(lastUp != percentage){
      printlnDebug(percentage);
      lastUp = percentage;
      fillPercentage(strip, pixelCount, RgbColor(0, 255, 0), RgbColor(255, 0, 0), percentage);
      strip.Show();
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    if(PRINT_DEBUG){
      Serial.begin(115200);
      Serial.printf("Error[%u]: ", error);
    }
    if (error == OTA_AUTH_ERROR) {
      printlnDebug("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      printlnDebug("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      printlnDebug("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      printlnDebug("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      printlnDebug("End Failed");
    }
    fillColor(strip, pixelCount, RgbColor(255, 0, 0));
    strip.Show();
    delay(200);
  });

  ArduinoOTA.begin();
}

// Destructive operation on the colors
void applyBrightness(uint8_t brightness){
  for(int i = 0; i < pixelCount; i++){
    auto color = strip.GetPixelColor(i);
    auto red = color.R * (brightness / 100.0f);
    auto green = color.G * (brightness / 100.0f);
    auto blue = color.B * (brightness / 100.0f);
    strip.SetPixelColor(i, RgbColor(red, green, blue));
  }
}

// Show the strip while dimming it to the brightness (in percent)
void showStrip(uint8_t brightness = 100) {
  RgbColor stripBackup[pixelCount];
  // Save the original colors
  for(int i = 0; i < pixelCount; i++){
    stripBackup[i] = strip.GetPixelColor(i);
  }

  applyBrightness(brightness);
  strip.Show();

  // Put back the non dimmed colors in the strip
  for(int i = 0; i < pixelCount; i++){
    strip.SetPixelColor(i, stripBackup[i]);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  printlnDebug("Received MQTT message");

  String topicStr = String(topic);
  String payloadStr = String();
  

  payloadStr.reserve(length);
  for(unsigned int i = 0; i < length; i++){
    payloadStr += (char) payload[i];
  }

  printlnDebug("Topic : " + topicStr);
  printlnDebug("Payload : " + payloadStr);

  float value = payloadStr.toFloat();

  if(topicStr == "salon/strip/saturation") {
    saturationUpdated = true;
    saturation = value;
  }
  else if(topicStr == "salon/strip/brightness") {
    if(value <= 100) {
      brightnessUpdated = true;
      brightness = value;
      if(value > 0) {
        isOn = true;
      }
    }
  }
  else if(topicStr == "salon/strip/hue") {
    hueUpdated = true;
    hue = value;
  }
  else if(topicStr == "salon/strip/on") {
    if(payloadStr == "true"){
      isOn = true;
    } 
    else if(payloadStr == "rainbow"){
      rainbowMode = true;
    }
    else {
      isOn = false;
    }
  }
}

void restartIfClientDisconnected() {
  static unsigned long lastClientConnected = 0;
  if(!client.connected()){
    printlnDebug("Client disconnected");
    while(!client.connected()){
      if(millis() - lastClientConnected > MAX_CLIENT_TIMEOUT) {
        printlnDebug("MQTT could not connect to server, restarting");
        ESP.restart();
      }
      String clientId = "esp8266SalonLeds-";
      clientId += String(random(0xFFFF), HEX);
      client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD, "salon/strip", 0, 0, "disconnected");
      client.subscribe(SUBSCRIPTION);

      ArduinoOTA.handle();
      delay(100);
    }
  }
  else {
    lastClientConnected = millis();
  }
}

void restartIfWifiIsDiconnected(){
  // Restart the MCU if the wifi is disconnnected for too long
  static int lastWifiConnected = 0;
  if(!WiFi.isConnected()){
    printlnDebug("Wifi disconnected");
    while(!WiFi.isConnected()){
      if(millis() - lastWifiConnected > MAX_WIFI_TIMEOUT) {
        printlnDebug("Could not connect to wifi, restarting");
        ESP.restart();
      }
      delay(100);
    }
  }
  else {
    lastWifiConnected = millis();
  }
}

// TODO: split setup in parts
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  // Seting up LED strip
  strip.Begin();
  strip.Show(); // Put the strip at off at the beginning


  // Connecting to Wifi
  printDebug("\n\nConnecting to ");
  printlnDebug(WIFI_SSID);

  WiFi.mode(WIFI_STA); // Client only, no ap
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  restartIfWifiIsDiconnected();

  setupOTA();

  // Setup MQTT client
  client.setServer(MQTT_HOST, 1883);
  client.setCallback(callback);
  restartIfClientDisconnected();
  client.publish("online", "ledsSalonEsp8266"); 

  digitalWrite(LED_BUILTIN, LED_OFF);
}

void loop() {

  static unsigned long loopStart = millis();

  timeClient.update();

  restartIfWifiIsDiconnected();
  restartIfClientDisconnected();

  static uint8_t savedBrightness = 100;
  static uint8_t savedSaturation = 0;
  static float savedHue = 0;
  static float fadingBrightness = 0;

  static bool isRainbowFilled = false;
  static bool wasOn = false;

  if(brightnessUpdated) {
    savedBrightness = brightness;
    brightnessUpdated = false;
  }
  if(saturationUpdated || hueUpdated) {
    savedSaturation = saturation;
    savedHue =  hue;

    float fHue = float(savedHue) / 360.0f;
    float fSat = float(savedSaturation) / 100.0f;
    
    client.publish("salon/debug/hue", String(fHue).c_str());
    client.publish("salon/debug/sat", String(fSat).c_str());

    printDebug("hue : ");
    printlnDebug(fHue);
    printDebug("sat : ");
    printDebug(fSat);

    fillColor(strip, pixelCount, HsbColor(fHue , fSat, 1.0f));
    hueUpdated = false;
    saturationUpdated = false;
    rainbowMode = false;
    isRainbowFilled = false;
  }

  if(rainbowMode) {
    if(!isRainbowFilled) {
      rainbowFill(strip, pixelCount);
      isRainbowFilled = true;
    }
    rotateStrip(strip, pixelCount);
  }

  float fadingStep = 255.0f / ((FADING_TIME * 1000.0f) / float(ANIMATION_DELAY));
  
  if(isOn) {
    if(!wasOn){
      wasOn = true;
      fadingBrightness = 0;
      isFading = true;
    }
    if(isFading) {
      fadingBrightness += fadingStep;
      if(fadingBrightness >= savedBrightness) {
        isFading = false;
        fadingBrightness = savedBrightness;
      }
      showStrip(fadingBrightness);
    } else {
      showStrip(savedBrightness);
    }
  } else {
    if(wasOn){
      wasOn = false;
      isFading = true;
      fadingBrightness = savedBrightness;
    }
    if(isFading){
      fadingBrightness -= fadingStep;
      if(fadingBrightness <= 0) {
        isFading = false;
        fadingBrightness = 0;
      }
      showStrip(fadingBrightness);
    } else {
      showStrip(0);
    }
  }
  
  loopStart = millis();
  while(millis() - loopStart < ANIMATION_DELAY) {
    ArduinoOTA.handle();
    client.loop();
  }
}
