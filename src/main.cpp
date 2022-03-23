#include <Arduino.h>
#include <HTTPClient.h>

#include "../lib/SmartMatrix/src/MatrixHardware_ESP32_V0.h"
#include "../lib/SmartMatrix/src/SmartMatrix.h"
#include <../lib/ArduinoJson/ArduinoJson.h>
#include <Preferences.h>
#include "WiFi.h"
#include <WebServer.h>
#include <ArduinoOTA.h>

//WiFiManager wm;

String apiNews = "ef589987f29946ba8402cbb50b1d200c";
String urlNews = "https://newsapi.org/v2/top-headlines?country=id&apiKey=" + apiNews;
String Message0 = "";
int32_t scrollDelay = 15;
int32_t brightness = 255;
rgb24 fullRed = {0xff, 0, 0};

int bacaBeritaKe = 0;

WebServer server(80);
Preferences preferences;

unsigned long previousMillisUpdateBerita = 0;
unsigned long currentMillis = 0;

StaticJsonDocument<32> jsonRequest;
DynamicJsonDocument beritaJson(18000);

#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 32;      // Set to the height of your display
const uint8_t kRefreshDepth = 36;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType,
                             kMatrixOptions);

SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingBerita, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(dateLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer3, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer4, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer5, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);

SMARTMATRIX_ALLOCATE_INDEXED_LAYER(timeLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

char ssid[] = "Smart Home AP"; // Nama Wifi Anda
char pass[] = "Husna1299";     // Password Wifi Anda
#define TZ (+7 * 60 * 60)       // Timezone

void handleSetBrightness() {
    String body = server.arg("plain");
    deserializeJson(jsonRequest, body);
    int32_t red = jsonRequest["brightness"];
    brightness = red;
    preferences.putInt("brightness", red);
    server.send(200, "application/json", "success");
}

void setup_routing() {
    server.on("/brightness", HTTP_POST, handleSetBrightness);
    server.begin();
}

void setupArduinoOta() {
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

    ArduinoOTA.begin();
}

void fetchBerita() {
    HTTPClient http;
    http.begin(urlNews);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
        beritaJson.clear();
        Serial.print("HTTP Response code: ");
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        DeserializationError error = deserializeJson(beritaJson, http.getString());
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

void updateTime() {
    time_t t;
    static time_t last_t;
    struct tm *tm;
    static const char *const wd[7] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};
    t = time(nullptr);
    char timeBuffer[9];
    if (last_t == t)
        return;
    last_t = t;
    uint8_t jam, menit, detik, mday, myear;
    tm = localtime(&t);
    jam = tm->tm_hour;
    menit = tm->tm_min;
    detik = tm->tm_sec;
    mday = tm->tm_mday;
    myear = tm->tm_year;
    String wday = wd[tm->tm_wday];

    sprintf(timeBuffer, "%02d:%02d:%02d", jam, menit, detik);
    timeLayer.fillScreen(0);
    timeLayer.setFont(font8x13);
    timeLayer.drawString(0, 0, 1, timeBuffer);
    timeLayer.swapBuffers(false);

    String dateFormatShow = String(wday) + ", " + String(mday) + " " + 2022;
    int str_len = dateFormatShow.length() + 2;
    Serial.println(tm);

    char dateBuffer[str_len];
    dateFormatShow.toCharArray(dateBuffer, str_len);
    dateLayer.setColor(fullRed);
    dateLayer.setOffsetFromTop(12);
    if (dateLayer.getStatus() == 0)
        dateLayer.start(dateBuffer, 1);
}

void scheduleFetchBerita() {
    if (currentMillis - previousMillisUpdateBerita >= 3600000) {
        previousMillisUpdateBerita = currentMillis;
        fetchBerita();
    }
}

void scrollBerita() {
    if (scrollingBerita.getStatus() == 0) {
        bacaBeritaKe++;
        Message0 = beritaJson["articles"][bacaBeritaKe]["description"].as<String>() + " - " +
                   beritaJson["articles"][bacaBeritaKe]["source"]["name"].as<String>();
        String str = Message0;
        int str_len = str.length() + 1;
        char char_array[str_len];
        str.toCharArray(char_array, str_len);
        scrollingBerita.setMode(wrapForward);
        scrollingBerita.setColor(fullRed);
        scrollingBerita.setOffsetFromTop(kMatrixHeight - 12);
        scrollingBerita.start(char_array, 1);
        if (bacaBeritaKe > 36)
            bacaBeritaKe = 0;
    }
}

void setupWifi() {
    WiFi.begin(ssid, pass);
    while (WiFiClass::status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("");
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);
    configTime(TZ, 0, "id.pool.ntp.org");
}

void setupMatrix() {
    matrix.addLayer(&scrollingBerita);
    matrix.addLayer(&dateLayer);
    matrix.addLayer(&timeLayer);
    matrix.begin();

    timeLayer.fillScreen(0);
    timeLayer.setFont(font6x10);

    dateLayer.setFont(font6x10);

    scrollingBerita.setFont(gohufont11b);
    scrollingBerita.setMode(wrapForward);
}

void setup() {
    Serial.begin( 9600);
    Message0 = preferences.getString("message", Message0);
    scrollDelay = preferences.getInt("scrollDelay", scrollDelay);
    brightness = preferences.getInt("brightness", brightness);
    setupMatrix();
    setupWifi();
    setup_routing();
    setupArduinoOta();
    fetchBerita();
    delay(2000);
}

void loop() {
    updateTime();
    scheduleFetchBerita();
    scrollBerita();
    server.handleClient();
    ArduinoOTA.handle();
    uint16_t valueLux = analogRead(A0); //Reads the Value of LDR(light).
    if (valueLux < 350) matrix.setBrightness(10);
    else matrix.setBrightness(brightness);
}