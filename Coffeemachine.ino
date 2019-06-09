#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <SoftwareSerial.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define WIFINAME  "JURA"
#define GPIORX    3
#define GPIOTX    1
#define GPIOLEDIO     4
#define GPIOLEDPULSE  0
#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "europe.pool.ntp.org"

SoftwareSerial softserial(GPIORX, GPIOTX);
ESP8266WebServer webserver(5000);

int runAPI = 0;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

String cmd2jura(String outbytes) {
  String inbytes;
  int w = 0;

  while (softserial.available()) {
    softserial.read();
  }

  outbytes += "\r\n";
  for (int i = 0; i < outbytes.length(); i++) {
    for (int s = 0; s < 8; s += 2) {
      char rawbyte = 255;
      bitWrite(rawbyte, 2, bitRead(outbytes.charAt(i), s + 0));
      bitWrite(rawbyte, 5, bitRead(outbytes.charAt(i), s + 1));
      softserial.write(rawbyte);
    }
    delay(8);
  }

  int s = 0;
  char inbyte;
  while (!inbytes.endsWith("\r\n")) {
    if (softserial.available()) {
      byte rawbyte = softserial.read();
      bitWrite(inbyte, s + 0, bitRead(rawbyte, 2));
      bitWrite(inbyte, s + 1, bitRead(rawbyte, 5));
      if ((s += 2) >= 8) {
        s = 0;
        inbytes += inbyte;
      }
    } else {
      delay(10);
    }
    if (w++ > 500) {
      return "";
    }
  }

  return inbytes.substring(0, inbytes.length() - 2);
}

void handle_api() {
  String result;
  if (webserver.method() != HTTP_POST) {
    webserver.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String json = webserver.arg("plain");
  StaticJsonBuffer<200> jsonBuffer;
 JsonObject& root = jsonBuffer.parseObject(json);
 if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }
 String cmd = root["koffie_type"];
  if (cmd.length() < 3) {
    webserver.send(400, "text/plain", "Bad Request");
    return;
  }

  digitalWrite(GPIOLEDIO, HIGH);

  Serial.println(cmd);
  result = cmd2jura(cmd);
  HTTPClient client;
  client.begin("**********************");
  int httpCode = client.GET();
  if(httpCode > 0) {
    String payload  = client.getString();
    Serial.println(payload);
  }
  client.end();
  
  digitalWrite(GPIOLEDIO, LOW);

  if (result.length() < 3) {
    webserver.send(503, "text/plain", "Service Unavailable");
    return;
  }

  webserver.send(200, "text/plain", result);
}

void handle_web() {
  String html;

  html  = "<!DOCTYPE html><html><body><h1>&#9749; Jura Coffee Machine Gateway</h1>";
  html += "</body></html>";

  webserver.send(200, "text/html", html);
}

void setup() {
  // put your setup code here, to run once:

  wifi_station_set_hostname(WIFINAME);
  WiFiManager wifimanager;
  wifimanager.autoConnect(WIFINAME);
  timeClient.begin();
  
  webserver.on("/", handle_web);
  webserver.on("/api", handle_api);
  webserver.begin();

  softserial.begin(9600);

  pinMode(GPIOLEDIO, OUTPUT);
  pinMode(GPIOLEDPULSE, OUTPUT);

}
int i = 0;
void loop() {
  // put your main code here, to run repeatedly:

  webserver.handleClient();
  timeClient.update();
  i = (i + 1) % 200000;
  digitalWrite(GPIOLEDPULSE, (i < 100000) ? LOW : HIGH);
}
