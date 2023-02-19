// Zegar VFD na lampie IV-27 (IW-27)
// 02.2023 Robert "revox" Piotrowski
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
bool stoj = false;
#include <OneWire.h>
#include <DallasTemperature.h>

// GPIO where the DS18B20 is connected to
const int oneWireBus = 22;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensor(&oneWire);


#pragma GCC optimize ("-Ofast")

const byte znaki[8] = { 7, 6, 5, 4, 3, 2, 1, 0};
const byte wysw[8] = {23, 16, 18, 4, 5, 15, 17, 2};
//                    o  1  2   3   4  5   6   7
#include <Adafruit_PCF8574.h>
Adafruit_PCF8574 pcf;
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
WiFiClientSecure client;
HTTPClient http;
const char* WEATHER_URL = "https://api.openweathermap.org/data/2.5/weather?lat=53.23166&lon=17.8818&appid=818757f389d69bc602fc5cb47b03fa29&units=metric";
float temperatura = -100.0;
float temperatura_odczuwalna = -100.0;
unsigned int cisnienie = -100;
unsigned int wilgotnosc = -100;
float temperatureC = 0;


#include <WiFiManager.h>
WiFiManager wifiManager;

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ) )
#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String timeStamp;

String wifiText = "net...";
bool czasUstawiony = false;
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;

byte lastN = 0;
unsigned int li = 0;
unsigned int ustawienieJasnosci = 0;

unsigned long lastWeather = 1;
bool pogodaPobrana = false;
void setup() {
  Serial.begin(115200);
  Serial.println("Zegar VFD by revox, 02.2023");

  pinMode(19, OUTPUT);
  digitalWrite(19, LOW);

  sensor.begin();
  sensor.requestTemperatures();
  float temperatureC = sensor.getTempCByIndex(0);
  Serial.print(temperatureC);
  Serial.println("*C");

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Wire.begin(12, 14);

  Wire.setClock(400000);

  if (!pcf.begin(0x38, &Wire)) {
    Serial.println("Nie znaleziono PCF8574");
    while (1);
  }
  for (uint8_t p = 0; p < 8; p++) {
    pcf.pinMode(znaki[p], OUTPUT);
  }

  for (int i = 0; i < 8; i++) {
    pinMode(wysw[i], OUTPUT);
  }

  unsigned long animationStart = millis();
  while (millis() - animationStart < 1000) {
    displayText("czesc");
  }

  Serial.println("Lacznie z WiFi...");
  wifiManager.setConnectTimeout(60);
  wifiManager.setConfigPortalBlocking(false);
  xTaskCreatePinnedToCore(wlaczManagera, "wlaczManagera", 20000, NULL, 1, &Task1, 0);

  animationStart = millis();
  while (wifiManager.getWLStatusString() != "WL_CONNECTED") {
    displayText(wifiText);
  }

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.setHostname("Zegar VFD IV-27");
  ArduinoOTA.begin();

  Serial.println("Pobieranie czasu...");
  xTaskCreatePinnedToCore(ustawCzas, "ustawCzas", 10000, NULL, 1, &Task3, 0);
  while (!czasUstawiony) {
    displayText("czas");
  }

  Serial.println("Ustawianie pogody...");
  xTaskCreatePinnedToCore(updateWeather, "updateWeather", 10000, NULL, 1, &Task4, 0);
  while (!pogodaPobrana) {
    displayText("pogoda");
  }
}

void ustawCzas(void * pvParameters ) {
  timeClient.begin();
  timeClient.setTimeOffset(3600);
  timeClient.forceUpdate();
  czasUstawiony = true;
  for (;;) {
    sensor.requestTemperatures();
    temperatureC = sensor.getTempCByIndex(0);
    delay(1000);
  }
}

void wlaczManagera( void * pvParameters ) {
  wifiManager.autoConnect("Zegar VFD IV-27");
  wifiText = "192.168.4.1";
  for (;;) {
    yield();
    wifiManager.process();
    delay(10);
  }
}

void updateWeather(void * pvParameters) {
  client.setInsecure();
  http.useHTTP10(true);

  for (;;) {
    Serial.println("Aktualizacja pogody");

    http.begin(client, WEATHER_URL);
    http.GET();

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.print("Blad przy deserializeJson(): ");
      Serial.println(error.c_str());
    } else {
      JsonObject main = doc["main"];
      temperatura = main["temp"];
      cisnienie = main["pressure"];
      Serial.println("Pogoda zaktualizowana");
      pogodaPobrana = true;
      delay(110 * 1000);
    }

    http.end();
    delay(10 * 1000);
  }
}

void display(String znak, int wyswietlacz) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(wysw[i], HIGH);
  }

  for (int i = 0; i < 8; i++) {
    pcf.digitalWrite(znaki[i], HIGH);
  }
  zapalSegmenty(znak);
  if (znak != "") digitalWrite(wysw[wyswietlacz], LOW);
}

void zapalSegmenty(String tekst) {
  String znak = String(tekst.charAt(0));
  String kropka = String(tekst.charAt(1));

  if (znak == "0") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "1") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[5], LOW);
  }  else if (znak == "2") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "3") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "4") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "5") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "6") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "7") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    //pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "8") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "9") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "-") {
    pcf.digitalWrite(znaki[4], LOW);
  } else if (znak == "*") {
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "C" || znak == "c") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "z") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "s") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "r") {
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
  } else if (znak == "h") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "H") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "P") {
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "a") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "@") {
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "n") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
  } else if (znak == "e") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "t") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "j") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "v") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
  } else if (znak == "f") {
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "d") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
  } else if (znak == "p") {
    pcf.digitalWrite(znaki[7], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
  } else if (znak == "o") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
  } else if (znak == "g") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "i") {
    pcf.digitalWrite(znaki[2], LOW);
  } else if (znak == ".") {
    pcf.digitalWrite(znaki[0], LOW);
  }

  if (kropka == ".") {
    pcf.digitalWrite(znaki[0], LOW);
  }
}


void displayText(String text) {
  byte dlugosc = text.length();
  byte z = 0;

  if (dlugosc < 7) {
    z = (7 - dlugosc) / 2;
  }

  for (byte i = 0; i < dlugosc; i++) {
    String aktualnyZnak = String(text.charAt(i));
    String nastepnyZnak = String(text.charAt(i + 1));

    if (nastepnyZnak == ".") {
      z++;
      display(aktualnyZnak + ".", z);
    } else if (aktualnyZnak != ".") {
      z++;
      display(aktualnyZnak, z);
    }
    if (aktualnyZnak != " " && aktualnyZnak != "") delayMicroseconds(600);
  }
}

long newPosition = 0;
long oldPosition = 0;
bool firstRun = true;
void loop() {
  if (lastWeather == 0) lastWeather = millis();

  timeStamp = timeClient.getFormattedTime();
  String hours = timeStamp.substring(0, 2);
  String minutes = timeStamp.substring(3, 5);
  String seconds = timeStamp.substring(6, 9);

  if (millis() - lastWeather < 5000) {
    display(String( (hours.toInt() / 10) % 10 ) , 1);
    delayMicroseconds(600);
    display(String( hours.toInt() % 10 ), 2);
    delayMicroseconds(600);

    display("-", 3);
    delayMicroseconds(100);

    display(String( (minutes.toInt() / 10) % 10 ) , 4);
    delayMicroseconds(600);
    display(String( minutes.toInt() % 10 ), 5);
    delayMicroseconds(600);
  } else {
    if (lastN == 0) {
      li += 1;
      if (li >= 300 && !stoj) {
        lastWeather = millis();
        li = 0;
        lastN = 1;
      } else {
        displayText(String(temperatura) + "*C");
      }
    } else if (lastN == 1 ) {
      li += 1;
      if ((li >= 300 && !stoj) || temperatureC == -127.0) {
        lastWeather = millis();
        li = 0;
        lastN = 2;
      } else {
        displayText(String(temperatureC) + "*C");
      }
    } else if (lastN == 2) {
      li += 1;
      if (li >= 300 && !stoj) {
        lastWeather = millis();
        li = 0;
        lastN = 0;
      } else {
        displayText(String(cisnienie) + "hPa");
      }
    }
  }

  if (stoj) {
    display("@", 0);
  }


  if (seconds.toInt() % 2 == 0) {
    display("@", 0);
    delayMicroseconds(100);
  } else {
    delayMicroseconds(100);
  }

  ArduinoOTA.handle();
}
