/*
 * https://www.wemos.cc/en/latest/d1/d1_mini_lite.html
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
#define PLACES_COUNT  10
#define REFRESH_RATE  500           // display update speed in ms
#define BANNER_TIMER  (1000 * 45)   // seconds of banner display

// An IR detector/demodulator is connected to GPIO pin 14(D5 on a NodeMCU board).
// ir receiver, interrupt capable
#define IR_PIN        D5

char appname[]        = "HOME BRO START";
char host[]           = "192.168.147.1";
char topicResult[]    = "home_bro/RESULT";
String topicCommand   = "home_bro/CMD/";

IRrecv irrecv(IR_PIN);
decode_results results;

DM8BA10*  LCD;
String    msg = appname;
word      strPos =  0;
uint32_t  lastUpd = 0;
uint32_t  bannerTimer = 0;
bool      banner = true;
char      buf[64];

WiFiClient    wificlient;
PubSubClient  mqttclient(wificlient);
wl_status_t   wifiStatus = WL_IDLE_STATUS;

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

wl_status_t wifiConnect() {
  uint8_t counts = 10;
  WiFi.begin(ssid, pass);
  msg = "WIFI CONN";
  // Serial.println(msg);
  LCD->println(msg, DM8BA10::Both);

  while ((WiFi.status() != WL_CONNECTED) && (counts-- > 0)) {
    delay(1000);
  }

  if (WiFi.status() != WL_CONNECTED) {
    msg = "WIFI DISC";
    // Serial.println(msg);
    LCD->println(msg, DM8BA10::Both);
  }
  return WiFi.status();
}

void wifiData() {
  // Serial.println();

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  msg = "IP: " + ip.toString();

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  msg += " MAC: " + 
         String(mac[5], HEX) + ":" + 
         String(mac[4], HEX) + ":" + 
         String(mac[3], HEX) + ":" + 
         String(mac[2], HEX) + ":" + 
         String(mac[1], HEX) + ":" + 
         String(mac[0], HEX);
  // print the received signal strength:
  msg += " RSSI: " + String(WiFi.RSSI()) + " DB";

  // topic cmd name
  topicCommand += String(mac[2], HEX) + 
                  String(mac[1], HEX) +
                  String(mac[0], HEX);
  topicCommand.toUpperCase();
  msg += " TOPIC CMD: " + topicCommand;
  msg.toUpperCase();
  // Serial.println(msg);
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
      // Serial.println("WIFI: not connected");
      LCD->println("WIFI DISC", DM8BA10::Both);
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

  if (!banner) {
    msg = "";
    for (unsigned int i=0;i<length;i++) {
      msg += (char)payload[i];
    }
  }
}

void mqttConnect() {
  // Serial.print("MQTT: Connecting to mqtt ");
  // Serial.print(host);
  // Serial.println(" broker...");
  mqttclient.setServer(host, 1883);
  mqttclient.setCallback(mqttCallback);
}

void publishHello() {
  if (mqttclient.connected()) mqttclient.publish(topicResult, appname);
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttclient.connected()) {
    Serial.print("MQTT: Attempting MQTT connection...");
    Serial.println(host);
    // Attempt to connect
    if (mqttclient.connect("home_show_alone")) {
      Serial.println("MQTT: connected");
      // Once connected, publish an announcement...
      mqttclient.publish(topicResult,"MQTT: connected: hello world");
      // ... and resubscribe
      topicCommand.toCharArray(buf, 64);
      mqttclient.subscribe(buf);
    } else {
      Serial.print("MQTT: failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

uint8_t decodeButton(int button_v) {
  if (button_v >= 1000) {
    return 0;
  } else if (button_v >= 800 && button_v <= 900) {
    return 1;
  } else if (button_v >= 600 && button_v <= 680) {
    return 2;
  } else if (button_v >= 400 && button_v <= 450) {
    return 3;
  } else if (button_v >= 190 && button_v <= 250) {
    return 4;
  }
  return 0;
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

  LCD->println("HOME BRO", DM8BA10::Both);
  delay(2000);

  wifiStatus = wifiConnect();
  if (wifiStatus == WL_CONNECTED) {
    wifiData();
    mqttConnect();
  }
}

void loop() {
  if (wifiStatus == WL_CONNECTED) {
    if (!mqttclient.connected()) mqttReconnect();
    mqttclient.loop();
  }

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

  int button_v = analogRead(A0);
  // debug analog keyboard
  // sprintf(buf, "V: %d *%d*", button_v, decodeButton(button_v));
  // Serial.println(buf);

  if (decodeButton(button_v) == 1) {
    // LCD->println("BTN 1", DM8BA10::Both);
    msg = "  BTN 1  ";
  } else if (decodeButton(button_v) == 2) {
    // LCD->println("BTN 2", DM8BA10::Both);
    msg = "  BTN 2  ";
  } else if (decodeButton(button_v) == 3) {
    // LCD->println("BTN 3", DM8BA10::Both);
    msg = "  BTN 3  ";
  } else if (decodeButton(button_v) == 4) {
    // LCD->println("BTN 4", DM8BA10::Both);
    msg = "  BTN 4  ";
  }

  if (millis() - lastUpd > REFRESH_RATE) {
    LCD->scroll(msg, strPos++);
    if (strPos == msg.length()) {
      strPos = 0;
    }
    lastUpd = millis();
  }

  if (banner && (millis() - bannerTimer > BANNER_TIMER)) {
    banner = false;
    Serial.println("banner end");
  }
}
