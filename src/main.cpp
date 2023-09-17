/*

*/

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
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

// An IR detector/demodulator is connected to GPIO pin 14(D5 on a NodeMCU board).
// ir receiver, interrupt capable
#define IR_PIN        D5

// buttons
#define BTN_1         D0
#define BTN_2         D6
#define BTN_3         D7
#define BTN_4         D8  // IO, 10k Pull-down, SS

IRrecv irrecv(IR_PIN);
decode_results results;

DM8BA10* LCD;

char appname[] = "INFO: home_bro start";
char host[] = "192.168.147.1";
char topicResult[] = "home_bro/RESULT";
char topicCommand[] = "tele/tasmota_A6C75F/SENSOR";

WiFiClient wificlient;
PubSubClient mqttclient(wificlient);

const uint32_t IR_REMOTE_MELICONI_SAMSUNG_CODE[33] = {
    0xE0E020DF,
    0xE0E0A05F,
    0xE0E0609F,
    0xE0E010EF,
    0xE0E0906F,
    0xE0E050AF,
    0xE0E030CF,
    0xE0E0B04F,
    0xE0E0708F,
    0xE0E08877,
    0xE0E0F807,
    0xE0E0807F,
    0xE0E036C9,
    0xE0E028D7,
    0xE0E0A857,
    0xE0E06897,
    0xE0E040BF,
    0xE0E0D02F,
    0xE0E0E01F,
    0xE0E008F7,
    0xE0E048B7,
    0xE0E058A7,
    0xE0E0F00F,
    0xE0E0CF30,
    0xE0E09E61,
    0xE0E0F20D,
    0xE0E006F9,
    0xE0E0A659,
    0xE0E046B9,
    0xE0E08679,
    0xE0E016E9,
    0xE0E01AE5,
    0xE0E0B44B
};

const char* IR_REMOTE_MELICONI_SAMSUNG_SYMBOL[33] = {
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "0",
    "I",
    "CH_IN",
    "ROSSO",
    "VERDE",
    "GIALLO",
    "BLU",
    "ON_OFF",
    "VOL -",
    "VOL +",
    "PROG -",
    "PROG +",
    "MENU",
    "MUTE",
    "APP",
    "HOME",
    "GUIDE",
    "UP",
    "LEFT",
    "RIGHT",
    "DOWN",
    "OK",
    "BACK",
    "EXIT"
};

const char *irDecode(uint32_t code) {
  for (uint8_t i = 0; i < 33; i++) {
    if (IR_REMOTE_MELICONI_SAMSUNG_CODE[i] == code) {
        return IR_REMOTE_MELICONI_SAMSUNG_SYMBOL[i];
    }
  }
  return "-N/D-";
}

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

  irrecv.enableIRIn();

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(BTN_4, INPUT);            // IO, 10k Pull-down, SS 

  wifiConnect();
  wifiData();
  mqttConnect();

  LCD->println("HOME BRO", DM8BA10::Both);
}

void loop() {
  if (!mqttclient.connected()) {
    mqttReconnect();
  }
  mqttclient.loop();

  if (irrecv.decode(&results)) {
    LCD->println(irDecode(results.value), DM8BA10::Both);
    //
    // only for debug
    // serialPrintUint64(results.value, HEX);
    // Serial.println("");
    // Serial.print(resultToHumanReadableBasic(&results));
    // Serial.println("");
    //
    irrecv.resume();  // Receive the next value
  }

  if (digitalRead(BTN_1) == LOW) {
    LCD->println("BTN 1", DM8BA10::Both);
  }
  if (digitalRead(BTN_2) == LOW) {
    LCD->println("BTN 2", DM8BA10::Both);
  }
  if (digitalRead(BTN_3) == LOW) {
    LCD->println("BTN 3", DM8BA10::Both);
  }
  if (digitalRead(BTN_4) == HIGH) {
    LCD->println("BTN 4", DM8BA10::Both);
  }

}
