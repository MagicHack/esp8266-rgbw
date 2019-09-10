#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "NeoPixelBus.h"

#include "animations.hpp"

// Credentials for the network
const String SSID = "***REMOVED***";
const String PSK  = "***REMOVED***";

// LED strip output is on Rx/D0
const uint16_t pixelCount = 100;
NeoPixelBus<NeoRgbwFeature, NeoSk6812Method> strip(pixelCount);

void toggle(uint8_t pin) {
  digitalWrite(pin, !digitalRead(pin));
}




// TODO: split setup in parts
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  // Seting up LED strip
  strip.Begin();
  strip.Show(); // Put the strip at off at the beginning


  // Connecting to Wifi
  Serial.print("\n\nConnecting to ");
  Serial.println(SSID);

  WiFi.mode(WIFI_STA); // Client only, no ap
  WiFi.begin(SSID, PSK);

  int connectionAttemps = 0;
  while (!WiFi.isConnected() && connectionAttemps < 40) {
    delay(500);
    Serial.print('.');
    toggle(LED_BUILTIN);
    connectionAttemps++;
  }
  
  Serial.println();

  if(WiFi.isConnected()) {
    Serial.print("Wifi successfully connected.\nIP : ");
    Serial.println(WiFi.localIP());

    fillColor(strip, pixelCount, RgbColor(0, 0, 255));
    strip.Show();
    delay(500);
  } else {
    Serial.print("Could not connect to the wifi ");
    Serial.println(SSID);

    fillColor(strip, pixelCount, RgbColor(255, 0, 0));
    strip.Show();
    delay(500);
  }


  rainbowFill(strip, pixelCount);
}


void loop() {
  rotateStrip(strip, pixelCount);
  
  strip.Show();
  delay(50);

  toggle(LED_BUILTIN);
}
