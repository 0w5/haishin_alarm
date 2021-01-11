/*
   Youtube Live Stream Alarm

   This program is for ESP32 and requires the libraly of ESP8266 Audio.
   https://github.com/earlephilhower/ESP8266Audio
   Note that the pin for delta-sigma audio output is 22 on ESP32-DevKitC V4.

   An MP3 file named "sound.mp3" must be placed in SPIFFS.
   Files smaller than 1 MB can be uploaded to the ESP32 with the Arduino IDE plugin of arduino-esp32fs-plugin.
   https://github.com/me-no-dev/arduino-esp32fs-plugin
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "SPIFFS.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"

//-------------------------------------------------------------------
// SSID and password of your Wi-Fi
RTC_DATA_ATTR const char* ssid     = "OhayouGozaiNa-su";
RTC_DATA_ATTR const char* password = "Ogona!Ondai!Anoko?Seiyuu?";

// Youtube Channel ID to monitor
RTC_DATA_ATTR const char* channelId = "UCIdEIHpS0TdkqRkHL5OkLtA";

// Your API key of Youtube DATA API v3
RTC_DATA_ATTR const char* apiKey = "APIKEY_ha_Google_Cloud_Platform_kara_getto_dekiru";

// The repeating number of the alarm
RTC_DATA_ATTR const int alarmLoop = 10;

// Interval time of monitoring (sec.)
RTC_DATA_ATTR const int waitTime = 20;

// Pin for Wi-Fi connection indicator
RTC_DATA_ATTR int led = 2;
//-------------------------------------------------------------------

RTC_DATA_ATTR const char* apiHost = "www.googleapis.com";
RTC_DATA_ATTR const char* rssHost = "www.youtube.com";

RTC_DATA_ATTR int videoNum = 2; // The number of recent video IDs to get

RTC_DATA_ATTR AudioGeneratorMP3 *mp3;
RTC_DATA_ATTR AudioFileSourceSPIFFS *file;
RTC_DATA_ATTR AudioOutputI2SNoDAC *out;
RTC_DATA_ATTR AudioFileSourceID3 *id3;

// The setup function to connect to the internet
void setup() {
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  Serial.begin(115200);

  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Connecting to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int wificnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (wificnt > 10) {   // Rebooting after 5 sec. if it can't connect to Wi-Fi
      ESP.restart();
    }
    wificnt++;
  }
  digitalWrite(led, HIGH);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//The main function to detect a live streaming
void loop() {
  // Reading Youtube RSS and get recent video IDs
  String rssUrl = "/feeds/videos.xml?channel_id=" + String(channelId);
  String videoId[videoNum];
  getVideoId(rssHost, rssUrl, videoId, videoNum);

  // Checking if videos are live streaming
  for (int i = 0; i < videoNum; i++) {
    String apiUrl = "/youtube/v3/videos?part=snippet&id=" + String(videoId[i]) + "&key=" + String(apiKey);
    String jsonData = getHTTPS(apiHost, apiUrl);

    if (jsonData.indexOf("\"liveBroadcastContent\": \"live\"") > -1) { // live -> none for sound test

      Serial.println(videoId[i]);
      Serial.println("live streaming detected");

      for (int i = 0; i < alarmLoop; i++) {
        /*
          Serial.print("alarm repeats:,");
          Serial.println(i);
        */
        sound();  //Sounding the alarm
      }
    }
  }
  // Entering deep sleeping for the next monitoring
  // 7 sec. to run loop()
  esp_sleep_enable_timer_wakeup((waitTime - 7) * 1000 * 1000);
  esp_deep_sleep_start();
}


void sound() {
  startsound();
  while (1) {
    if (mp3->isRunning()) {
      if (!mp3->loop()) mp3->stop();
    } else {
      /*
        Serial.printf("MP3 done\n");
      */
      delay(500);
      break;
    }
  }
}

void startsound() {
  SPIFFS.begin();
  file = new AudioFileSourceSPIFFS("/sound.mp3");
  id3 = new AudioFileSourceID3(file);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
}


void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  (void)cbData;
  /*
    Serial.printf("ID3 callback for: %s = '", type);
  */
  if (isUnicode) {
    string += 2;
  }

  while (*string) {
    char a = *(string++);
    if (isUnicode) {
      string++;
    }
    /*
      Serial.printf("%c", a);
    */
  }
  /*
    Serial.printf("'\n");
    Serial.flush();
  */
}


void getVideoId(const char* host, String url, String videoId[], int videoNum) {

  Serial.print("connecting to ");
  Serial.println(host);

  // Creating TCP connections
  WiFiClientSecure client;
  const int httpPort = 443;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    ESP.restart();  //Rebooting if connection is failed
  }

  // Requesting to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      client.stop();
      Serial.println("connection timed out");
      ESP.restart();    //Rebooting if connection is failed
    }
  }

  // Reading reply from server
  int cnt = 0;
  while (client.available()) {
    if (cnt > videoNum) {
      client.stop();
      break;
    }

    String line = client.readStringUntil('\n'); //Reading XML line by line
    line.replace('\t', '\0');
    line.trim();
    if (line.indexOf("<yt:videoId>") > -1) {  // Picking up the line if it contains video ID tags
      line.replace("<yt:videoId>", "");       // Deleting tags and only video ID will remain
      line.replace("</yt:videoId>", "");
      videoId[cnt] = line;
      cnt++;
    }

  }
}


String getHTTPS(const char* host, String url) {

  Serial.print("connecting to ");
  Serial.println(host);

  // Creating TCP connections
  WiFiClientSecure client;
  const int httpPort = 443;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    ESP.restart();  //Rebooting if connection is failed
    return "";
  }

  // Sending request
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      client.stop();
      Serial.println("connection timed out");
      ESP.restart();  //Rebooting if connection is failed
      return "";
    }
  }

  // Reading reply from server
  String content;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    content += line;
  }
  client.stop();
  return content;
}
