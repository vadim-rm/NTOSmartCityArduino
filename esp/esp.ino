#include <ESP8266WiFi.h>  //Библиотека для работы с WIFI
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

WiFiClient wifiClient;

const char* ssid = "ZyXEL_KEENETIC_LITE_3F992B";  //Имя точки доступа WIFI
const char* password = "123456789";  //пароль точки доступа WIFI

char* host = "192.168.1.43";
uint16_t port = 5000;

String buffer;

unsigned int lastUpdateTime = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("connecting");
    delay(5000);
    ESP.restart();
  }
    Serial.println("connected");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '!') {
      HTTPClient http;
      http.begin(wifiClient, "http://arduino.vadimrm.tk" + buffer);
      http.POST(buffer);
      http.end();
      buffer = "";
    } else if (c == ':') {
      HTTPClient http;
      http.begin(wifiClient, "http://arduino.vadimrm.tk" + buffer);
      http.GET();
      http.end();
      buffer = "";
    } else if (c == '*') {
      if (wifiClient.connect(host, port)) {
        wifiClient.print(buffer);
        wifiClient.stop();
      }
      buffer = "";
    } else {
      buffer += c;
    }
  }

  if (millis() - lastUpdateTime > 5000) {
    lastUpdateTime = millis();
    HTTPClient http;
    http.begin(wifiClient, "http://arduino.vadimrm.tk/container/getStatus");
    http.GET();
    String payload = http.getString();
    http.end();
    Serial.print(int(payload == "true"));
  }
}
