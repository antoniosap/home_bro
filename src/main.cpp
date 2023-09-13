/*

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//#include <json.h>
#include <string>
#include "secrets.h"
#include <DM8BA10.h>
#include <charset/latin_basic.h>

// pins
#define CS_PIN        D4
#define WR_PIN        D2
#define DATA_PIN      D3
#define BACKLIGHT_PIN D1

// display settings
#define PLACES_COUNT 10
#define REFRESH_RATE 1000 // display update speed in ms

DM8BA10* LCD;

const DM8BA10::Padding alignment[] = { DM8BA10::Padding::Right, DM8BA10::Padding::Left, DM8BA10::Padding::Both };
volatile byte curAlignmentIndex = 0;
const String texts[] = { "Left", "Right", "Center" };
uint32_t lastUpd = 0;

char appname[] = "INFO: home_bro start";
char host[] = "192.168.147.1";
char topicResult[] = "home_bro/RESULT";
char topicCommand[] = "tele/tasmota_A6C75F/SENSOR";

WiFiClient wificlient;
PubSubClient mqttclient(wificlient);

void wifiConnect() {
  WiFi.begin(ssid, pass);
  Serial.println("WIFI: Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
}

void wifiData() {
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("\nWIFI: IP Address: ");
  Serial.println(ip);
  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("WIFI: MAC address: ");
  Serial.print(mac[5], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.println(mac[0], HEX);
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("WIFI: signal strength (RSSI):");
  Serial.println(rssi);
}

void checkWiFi() {
  switch (WiFi.status()) {
    case WL_IDLE_STATUS:
    case WL_NO_SSID_AVAIL:
    case WL_SCAN_COMPLETED:
    case WL_NO_SHIELD:
    case WL_CONNECTED:
      return;
    case WL_CONNECT_FAILED:
    case WL_CONNECTION_LOST:
    case WL_DISCONNECTED:
      Serial.println("WIFI: not connected");
      WiFi.begin(ssid, pass);
      return;
  }
}

//----------------------------------------------------------------------------------

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT: Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void mqttConnect() {
  Serial.print("MQTT: Connecting to mqtt ");
  Serial.print(host);
  Serial.println(" broker...");
  mqttclient.setServer(host, 1883);
  mqttclient.setCallback(mqttCallback);
}

void publishHello() {
  if (mqttclient.connected()) mqttclient.publish(topicResult, appname);
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttclient.connected()) {
    Serial.println("MQTT: Attempting MQTT connection...");
    // Attempt to connect
    if (mqttclient.connect("home_show_alone")) {
      Serial.println("MQTT: connected");
      // Once connected, publish an announcement...
      mqttclient.publish(topicResult,"MQTT: connected: hello world");
      // ... and resubscribe
      mqttclient.subscribe(topicCommand);
    } else {
      Serial.print("MQTT: failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//----------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println(appname);
  Serial.flush();

  LCD = new DM8BA10(new LatinBasicCharset(), CS_PIN, WR_PIN, DATA_PIN, BACKLIGHT_PIN);
  LCD->backlight();

  wifiConnect();
  wifiData();
  mqttConnect();

}

void loop() {
  if (!mqttclient.connected()) {
    mqttReconnect();
  }
  mqttclient.loop();

  auto nowMs = millis();
  if (nowMs - lastUpd > REFRESH_RATE) {
    LCD->println(texts[curAlignmentIndex], alignment[curAlignmentIndex]);

    if (curAlignmentIndex++ >= 2)
        curAlignmentIndex = 0;
      lastUpd = nowMs;
  }
}