#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#include "LGFX.h"
#include "WiFiManagerHelpers.h"
#include "ConfigurationWebServer.h"
#include "HttpRequestManager.h"
#include "OpenSkyAuthTokenHandler.h"
#include "AircraftManager.h"
#include "DrawHelpers.h"
#include "models/Aircraft.h"
#include "models/TrackedAircraft.h"
#include <ESP32Encoder.h>
#include <Adafruit_NeoPixel.h>

#define ENCODER_A 9
#define ENCODER_B 8
#define ENCODER_SW 7

#define RGB_LED_PIN 21

Adafruit_NeoPixel statusLed(
    1,
    RGB_LED_PIN,
    NEO_GRB + NEO_KHZ800);

constexpr int SCREEN_SIZE = 240;
constexpr int SCREEN_SIZE_DIV_2 = (SCREEN_SIZE / 2);

LGFX tft;
LGFX_Sprite backbuffer(&tft);

WiFiManager wm;
ConfigurationWebServer configServer;
HttpRequestManager http;
OpenSkyAuthTokenHandler authHandler(http);

ESP32Encoder encoder;
int64_t lastEncoderPos = 0;

AircraftManager aircraftManager(configServer, authHandler, http, tft);

void SetLed(uint8_t r, uint8_t g, uint8_t b)
{
  statusLed.setPixelColor(
      0,
      statusLed.Color(r, g, b));

  statusLed.show();
}

void setup()
{
  Serial.begin(115200);
  // delay(1000); // avoids immediate serial output being cut off - uncomment if needed

  statusLed.begin();
  statusLed.setBrightness(200);
  statusLed.clear();
  statusLed.show();

  SetLed(0, 0, 255); // Booting

  // initialise LGFX + screen
  tft.init();
  tft.invertDisplay(true);
  tft.setRotation(2);
  // pinMode(3, OUTPUT);
  // digitalWrite(3, HIGH);

  backbuffer.setColorDepth(8);
  backbuffer.createSprite(SCREEN_SIZE, SCREEN_SIZE);

  // establish WiFi connection
  tft.fillScreen(lgfx::color888(0, 0, 0));
  tft.setTextColor(lgfx::color888(0, 255, 0));
  tft.drawCentreString("Connecting to WiFi...", SCREEN_SIZE / 2, SCREEN_SIZE / 2);

  SetLed(255, 255, 0); // WiFi connecting

  WiFiManagerHelpers::ConfigureWiFiManager(wm, tft);
  wm.autoConnect(WiFiManagerHelpers::WiFiManagerName);

  SetLed(0, 255, 0); // Running

  // begin background server for configuration
  configServer.Initialise();

  // initialise aircraft manager
  aircraftManager.Initialise();

  ESP32Encoder::useInternalWeakPullResistors = puType::up;

  encoder.attachSingleEdge(ENCODER_A, ENCODER_B);
  encoder.setCount(0);

  pinMode(ENCODER_SW, INPUT_PULLUP);
}

void loop()
{
  int64_t pos = encoder.getCount();

  if (pos != lastEncoderPos)
  {
    if (pos > lastEncoderPos)
    {
      aircraftManager.SelectNextAircraft();
    }
    else
    {
      aircraftManager.SelectPreviousAircraft();
    }

    lastEncoderPos = pos;
  }

  static bool lastButtonState = HIGH;

  bool currentButtonState = digitalRead(ENCODER_SW);

  if (lastButtonState == HIGH &&
      currentButtonState == LOW)
  {
    aircraftManager.EncoderClick();
  }

  lastButtonState = currentButtonState;

  SetLed(0, 255, 255); // Fetching
  aircraftManager.Update();

  // draw cycle
  backbuffer.fillScreen(lgfx::color888(0, 0, 0));

  String renderScanlines = configServer.GetStoredString("scanline");
  if (renderScanlines.isEmpty() || renderScanlines == "true")
  {
    DrawScanLines(backbuffer,
                  SCREEN_SIZE_DIV_2 - 1,
                  SCREEN_SIZE_DIV_2 - 1,
                  SCREEN_SIZE_DIV_2 - 1 + (std::cos(millis() / 3000.0f) * SCREEN_SIZE_DIV_2),
                  SCREEN_SIZE_DIV_2 - 1 + (std::sin(millis() / 3000.0f) * SCREEN_SIZE_DIV_2),
                  20, 128, 5);
  }

  aircraftManager.Draw(backbuffer);
  backbuffer.pushSprite(0, 0);

  SetLed(0, 255, 0); // Running
}
