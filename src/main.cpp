#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <SPIFFS.h>
#include <Update.h>

#include "webpage.h"  // HTML UI

// ============================================================
// Created by: Yilmaz Yurdakul
// https://yilmazyurdakul.com
// ============================================================

// ============================================================
// HARDWARE CONFIG
// ============================================================

Servo steerServo;
const int steerPin = 18;
const int leftAngle = 45;
const int rightAngle = 135;
const int centerAngle = (leftAngle + rightAngle) / 2;

// H-Bridge
const int IN1 = 26;
const int IN2 = 27;

// Lights
const int headLightPin = 33;
const int tailLightPin = 25;

// Battery sense
const int vinPin = 34;

// PWM channels
const int pwmChannelFWD  = 2;
const int pwmChannelBWD  = 3;
const int pwmChannelHEAD = 4;
const int pwmChannelTAIL = 5;

const int pwmFreqMotor = 20000;
const int pwmFreqLED   = 5000;
const int pwmRes       = 8;

// ============================================================
// RUNTIME STATE
// ============================================================

int currentThrottle = 0;
int maxPower = 255;

bool headlightsOn = false;
bool tailManualOn = false;

bool brakeActive = false;
unsigned long brakeEndTime = 0;

// FAILSAFE
unsigned long lastControlTime = 0;
const unsigned long controlTimeout = 200;  // stop only if REAL loss

const uint8_t TAIL_DIM_LEVEL = 100;

// OTA state
bool otaInProgress = false;
unsigned long otaBlinkTimer = 0;
bool otaBlinkState = false;

// WebSocket manual ping
unsigned long lastPingSent = 0;

// ============================================================
// WIFI
// ============================================================

const char *ssid = "Rc-Car";
const char *pass = "12345678";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ============================================================
// BATTERY MEASUREMENT
// 1400 raw = 8.4V full
// ============================================================

float readVinAveraged(uint8_t samples = 20) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(vinPin);
    delayMicroseconds(200);
  }

  float raw = sum / (float)samples;
  float vin = raw * (8.4f / 1400.0f);
  return vin;
}

float getBatteryPercent() {
  float vin = readVinAveraged();
  vin = constrain(vin, 4.0f, 8.4f);
  float pct = ((vin - 4.0f) / (8.4f - 4.0f)) * 100.0f;
  return constrain(pct, 0.0f, 100.0f);
}

// ============================================================
// LIGHT CONTROL
// ============================================================

void setTailPWM(uint8_t v) {
  ledcWrite(pwmChannelTAIL, v);
}

void updateTailLightState() {
  // OTA has top priority – blinking is handled in otaBlinkLoop()
  if (otaInProgress) {
    return;
  }

  unsigned long now = millis();

  // Timed brake
  if (brakeActive && now < brakeEndTime) {
    setTailPWM(255);
    return;
  }

  if (brakeActive && now >= brakeEndTime) {
    brakeActive = false;
  }

  // Reverse = full
  if (currentThrottle < -5) {
    setTailPWM(255);
    return;
  }

  // Running lights (dim)
  if (headlightsOn || tailManualOn) {
    setTailPWM(TAIL_DIM_LEVEL);
    return;
  }

  setTailPWM(0);
}

// ============================================================
// MOTOR CONTROL
// ============================================================

void stopMotor() {
  ledcWrite(pwmChannelFWD, 0);
  ledcWrite(pwmChannelBWD, 0);
}

void forwardMotor(int speed) {
  speed = constrain(speed, 0, maxPower);
  ledcWrite(pwmChannelBWD, 0);
  ledcWrite(pwmChannelFWD, speed);
}

void backwardMotor(int speed) {
  speed = constrain(speed, 0, maxPower);
  ledcWrite(pwmChannelFWD, 0);
  ledcWrite(pwmChannelBWD, speed);
}

// ============================================================
// JOYSTICK
// ============================================================

void handleJoy(int steer, int throttle) {
  lastControlTime = millis();  // ALIVE

  steer = constrain(steer, -100, 100);
  double half = (rightAngle - leftAngle) / 2.0;
  int angle = centerAngle + (steer / 100.0) * half;
  angle = constrain(angle, leftAngle, rightAngle);
  steerServo.write(angle);

  throttle = constrain(throttle, -100, 100);

  // Brake trigger
  if (currentThrottle != 0 && throttle == 0) {
    brakeActive = true;
    brakeEndTime = millis() + 2000;
  }

  currentThrottle = throttle;
  updateTailLightState();

  int mag = abs(throttle);
  if (mag < 5) {
    stopMotor();
    return;
  }

  int speed = map(mag, 0, 100, 0, maxPower);

  if (throttle > 0) forwardMotor(speed);
  else backwardMotor(speed);
}

// ============================================================
// COMMAND HANDLING
// ============================================================

void handleCommand(const String &cmd) {

  // Heartbeat
  if (cmd == "ALIVE") {
    lastControlTime = millis();
    return;
  }

  lastControlTime = millis();
  Serial.println(cmd);

  if (cmd.startsWith("JOY:")) {
    int mid = cmd.indexOf(",");
    int steer = cmd.substring(4, mid).toInt();
    int throttle = cmd.substring(mid + 1).toInt();
    handleJoy(steer, throttle);
    return;
  }

  if (cmd.startsWith("MAXPOWER:")) {
    maxPower = constrain(cmd.substring(9).toInt(), 20, 255);
    return;
  }

  // Lights
  if (cmd == "HEAD_LOW" || cmd == "HEAD_ON") {
    headlightsOn = true;
    ledcWrite(pwmChannelHEAD, 90);
    updateTailLightState();
    return;
  }

  if (cmd == "HEAD_HIGH") {
    headlightsOn = true;
    ledcWrite(pwmChannelHEAD, 255);
    updateTailLightState();
    return;
  }

  if (cmd == "HEAD_OFF") {
    headlightsOn = false;
    ledcWrite(pwmChannelHEAD, 0);
    updateTailLightState();
    return;
  }
}

// ============================================================
// WEBSOCKET EVENTS
// ============================================================

void onWsEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len)
{
  if (type == WS_EVT_DATA) {
    String msg = String((char *)data).substring(0, len);
    handleCommand(msg);
  }

  if (type == WS_EVT_DISCONNECT) {
    // Hard disconnect: stop throttle immediately
    stopMotor();
    currentThrottle = 0;
    brakeActive = false;
    updateTailLightState();
  }
}

// ============================================================
// OTA BLINKING
// ============================================================

void otaBlinkLoop() {
  if (!otaInProgress) return;

  unsigned long now = millis();
  if (now - otaBlinkTimer >= 300) {
    otaBlinkTimer = now;
    otaBlinkState = !otaBlinkState;

    // FORCE tail output
    ledcWrite(pwmChannelTAIL, otaBlinkState ? 255 : 20);
  }
}

void otaFinalFlash() {
  for (int i = 0; i < 6; i++) {
    ledcWrite(pwmChannelTAIL, 255);
    delay(120);
    ledcWrite(pwmChannelTAIL, 0);
    delay(120);
  }
}

// ============================================================
// OTA UPLOAD PAGE
// ============================================================

void setupOTA() {

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html",
      "<html><body style='background:#111;color:#eee;text-align:center;font-family:Arial;'>"
      "<h2>Firmware Update</h2>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update'><br><br>"
      "<input type='submit' value='Upload Firmware' "
      "style='padding:12px 20px;background:#0af;color:#fff;border:none;border-radius:6px;'>"
      "</form></body></html>");
  });

  server.on(
    "/update", HTTP_POST,
    [](AsyncWebServerRequest *req)
    {
      otaInProgress = false;
      otaFinalFlash();

      bool ok = !Update.hasError();
      req->send(200, "text/plain", ok ? "OK" : "FAIL");
      delay(500);
      ESP.restart();
    },
    [](AsyncWebServerRequest *req,
       const String &filename, size_t index,
       uint8_t *data, size_t len, bool final)
    {
      if (index == 0) {
        otaInProgress = true;
        otaBlinkTimer = millis();
        otaBlinkState = false;
        Serial.printf("OTA Start: %s\n", filename.c_str());
        Update.begin();
      }

      Update.write(data, len);

      if (final) {
        Update.end(true);
        Serial.println("OTA DONE");
      }
    });
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);

  ESP32PWM::allocateTimer(0);
  steerServo.setPeriodHertz(50);
  steerServo.attach(steerPin, 1000, 2000);
  steerServo.write(centerAngle);

  ledcSetup(pwmChannelFWD, pwmFreqMotor, pwmRes);
  ledcSetup(pwmChannelBWD, pwmFreqMotor, pwmRes);
  ledcAttachPin(IN1, pwmChannelFWD);
  ledcAttachPin(IN2, pwmChannelBWD);

  ledcSetup(pwmChannelHEAD, pwmFreqLED, pwmRes);
  ledcAttachPin(headLightPin, pwmChannelHEAD);

  ledcSetup(pwmChannelTAIL, pwmFreqLED, pwmRes);
  ledcAttachPin(tailLightPin, pwmChannelTAIL);
  setTailPWM(0);

  analogReadResolution(12);
  analogSetPinAttenuation(vinPin, ADC_11db);

  SPIFFS.begin(true);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, pass);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", webpageHTML);
  });

  server.on("/vin", HTTP_GET, [](AsyncWebServerRequest *req) {
    float vin = readVinAveraged();
    float pct = getBatteryPercent();
    String json = "{";
    json += "\"voltage\":" + String(vin, 2) + ",";
    json += "\"percent\":" + String(pct, 0);
    json += "}";
    req->send(200, "application/json", json);
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  setupOTA();

  server.begin();
  lastControlTime = millis();

  Serial.println("SERVER READY");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  ws.cleanupClients();
  otaBlinkLoop();

  // Manual WebSocket ping every 5 seconds to keep connection alive
  if (millis() - lastPingSent > 5000) {
    lastPingSent = millis();
    ws.textAll("PING");
  }

  // FAILSAFE: stop ONLY motor (NOT steering)
  if (millis() - lastControlTime > controlTimeout) {
    stopMotor();
    currentThrottle = 0;
    brakeActive = false;
    updateTailLightState();
  }
}
