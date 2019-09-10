#include <Arduino.h>
#include <ESP8266WiFi.h>


// Credentials for the network
const String SSID = "***REMOVED***";
const String PSK  = "***REMOVED***";

bool isConnected = false;

void toggle(uint8_t pin) {
  digitalWrite(pin, !digitalRead(pin));
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  // Connecting to Wifi
  Serial.print("\n\nConnecting to ");
  Serial.println(SSID);

  WiFi.mode(WIFI_STA); // Client only, no ap
  WiFi.begin(SSID, PSK);

  int connectionAttemps = 0;
  while (!isConnected && connectionAttemps < 40) {
    delay(500);
    Serial.print('.');
    toggle(LED_BUILTIN);
    connectionAttemps++;
    if(WiFi.isConnected()) {
      isConnected = true;
    }
  }

  if(isConnected) {
    Serial.print("\nWifi successfully connected.\nIP : ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("\nCould not connect to the wifi ");
    Serial.println(SSID);
  }
  
  
}

void loop() {
  static int count = 0;
  static int numberOfLoops = 0;
  // put your main code here, to run repeatedly:
  toggle(LED_BUILTIN);
  delay(100);
  toggle(LED_BUILTIN);
  delay(100);
  count = (count + 1) % 10 + 1;
  if(count == 0) {
    Serial.println(numberOfLoops);
  }
  numberOfLoops++;
}
