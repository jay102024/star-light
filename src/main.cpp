#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <FastLED.h>

#define SENSOR_PIN 1  // KY-010 光遮斷傳感器的輸出腳位
#define LED_PIN 3   // WS2812 LED 的數據腳位
#define NUM_LEDS 24

//呼吸燈亮度在 26~28行調整，燈的亮度在404行調整
CRGB leds[NUM_LEDS];

namespace {
const char* WIFI_SSID = "counter";
const char* WIFI_PASSWORD = "88888888";
const char* SERVER_BASE_URL = "http://192.168.66.101:3000";
const char* TEAM_ID = "team-1";
const char* DEVICE_ID = "esp32-table-1";

WebServer server(80);
volatile unsigned long counter = 0;
int targetCount = 0;
int lastSensorState = HIGH;
unsigned long lastHeartbeatMs = 0;
unsigned long lastWifiRetryMs = 0;
unsigned long lastRemoteSyncMs = 0;
unsigned long testLightEndMs = 0;
long lastTestLightSeq = 0;

constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 5000;
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 5000;
constexpr unsigned long REMOTE_SYNC_INTERVAL_MS = 1200;
constexpr unsigned long TEST_LIGHT_DURATION_MS = 1200;

bool targetAlertActive = false;
uint8_t alertBreathBrightness = 80;
bool alertBreathGoingUp = true;
unsigned long alertBreathLastStepMs = 0;

constexpr unsigned long ALERT_BREATH_STEP_MS = 20;
constexpr uint8_t ALERT_BREATH_MIN = 0;
constexpr uint8_t ALERT_BREATH_MAX = 225;

const CRGB COLOR_PALETTE[] = {
  CRGB::Lavender,
  CRGB::LightPink,
  CRGB::PaleTurquoise,
  CRGB::PowderBlue,
  CRGB::Thistle,
  CRGB::Violet,
  CRGB::CornflowerBlue,
  CRGB::MediumAquamarine,
  CRGB::Orchid,
  CRGB::SteelBlue,
  CRGB::MediumPurple,
  CRGB::Coral,
  CRGB::SkyBlue,
  CRGB::LightSeaGreen,
  CRGB::MediumSlateBlue,
  CRGB::Aquamarine,
  CRGB::Pink,
  CRGB::Turquoise,
  CRGB::Plum,
  CRGB::DarkTurquoise
};

const size_t COLOR_COUNT = sizeof(COLOR_PALETTE) / sizeof(COLOR_PALETTE[0]);

size_t colorOrder[COLOR_COUNT];
size_t colorOrderPos = COLOR_COUNT;  // 初始值 == COLOR_COUNT，觸發第一次 shuffle
size_t lastColorIdx = COLOR_COUNT;   // 哨兵：尚無上一個顏色
CRGB  currentDisplayColor = CRGB::Black;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-Hant">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>圓滿餐會星星燈控制器</title>
  <style>
    :root {
      color-scheme: dark;
      font-family: "Segoe UI", sans-serif;
    }

    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      background: linear-gradient(135deg, #111827, #1f2937);
      color: #f9fafb;
    }

    .card {
      width: min(90vw, 420px);
      padding: 32px;
      border-radius: 24px;
      background: rgba(255, 255, 255, 0.08);
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.35);
      text-align: center;
      backdrop-filter: blur(10px);
    }

    .hidden {
      display: none;
    }

    .welcome {
      width: min(92vw, 460px);
    }

    h1 {
      margin: 0 0 12px;
      font-size: 1.6rem;
    }

    .subtitle {
      margin: 0 0 20px;
      color: #d1d5db;
      line-height: 1.7;
    }

    .count {
      margin: 24px 0;
      display: inline-flex;
      align-items: flex-end;
      justify-content: center;
      gap: 6px;
      font-size: 4rem;
      font-weight: 700;
      color: #60a5fa;
    }

    .count-current {
      line-height: 1;
    }

    .count-target {
      color: #bfdbfe;
      font-size: 1.2rem;
      line-height: 1.1;
      transform: translateY(-0.2rem);
    }

    .actions {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-top: 12px;
    }

    .target-row {
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 10px;
      margin-top: 10px;
    }

    input[type="number"] {
      width: 100%;
      box-sizing: border-box;
      border: 1px solid #4b5563;
      border-radius: 10px;
      background: #111827;
      color: #f9fafb;
      padding: 10px 12px;
      font-size: 1rem;
    }

    .button {
      border: none;
      border-radius: 10px;
      padding: 10px 14px;
      font-size: 1rem;
      font-weight: 600;
      color: #f9fafb;
      background: linear-gradient(135deg, #2563eb, #1d4ed8);
      cursor: pointer;
      transition: transform 0.15s ease, opacity 0.15s ease;
    }

    .button:hover {
      transform: translateY(-1px);
      opacity: 0.95;
    }

    .button:active {
      transform: translateY(0);
    }

    .hint {
      margin: 18px 0 0;
      color: #d1d5db;
      line-height: 1.6;
    }
  </style>
</head>
<body>
  <section id="welcomeScreen" class="card welcome">
    <h1>設定各組參與人數</h1>
    <p class="subtitle">輸入本桌人數。</p>
    <div class="target-row">
      <input id="welcomeTargetInput" type="number" min="0" step="1" placeholder="例如：30">
      <button id="startButton" class="button" type="button">開始</button>
    </div>
  </section>

  <main id="appScreen" class="card hidden">
    <h1>目前人數</h1>
    <div class="count">
      <span id="count" class="count-current">0</span>
      <span id="targetDisplay" class="count-target">/0</span>
    </div>
    <div class="actions">
      <button id="incrementButton" class="button" type="button">+1</button>
      <button id="decrementButton" class="button" type="button">-1</button>
      <button id="resetButton" class="button" type="button">歸零</button>
    </div>
  </main>

  <script>
    let hasEnteredApp = false;
    let pollingId = null;

    function updateUi(state) {
      document.getElementById('count').textContent = state.count;
      document.getElementById('targetDisplay').textContent = `/${state.target}`;
      if (!hasEnteredApp) {
        document.getElementById('welcomeTargetInput').value = state.target > 0 ? state.target : '';
      }
    }

    async function fetchState() {
      try {
        const response = await fetch('/state');
        const data = await response.json();
        updateUi(data);
      } catch (error) {
        console.error(error);
      }
    }

    function enterApp() {
      hasEnteredApp = true;
      document.getElementById('welcomeScreen').classList.add('hidden');
      document.getElementById('appScreen').classList.remove('hidden');

      if (!pollingId) {
        pollingId = setInterval(fetchState, 500);
      }
    }

    async function confirmTargetAndEnter() {
      const input = document.getElementById('welcomeTargetInput');
      const target = input.value || '0';
      await postAction('/set-target', `target=${encodeURIComponent(target)}`);
      enterApp();
    }

    async function postAction(path, body = null) {
      try {
        const response = await fetch(path, {
          method: 'POST',
          headers: body ? { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' } : undefined,
          body
        });
        const data = await response.json();
        updateUi(data);
      } catch (error) {
        console.error(error);
      }
    }

    document.getElementById('startButton').addEventListener('click', confirmTargetAndEnter);
    document.getElementById('welcomeTargetInput').addEventListener('keydown', (event) => {
      if (event.key === 'Enter') {
        confirmTargetAndEnter();
      }
    });
    document.getElementById('incrementButton').addEventListener('click', () => postAction('/increment'));
    document.getElementById('decrementButton').addEventListener('click', () => postAction('/decrement'));
    document.getElementById('resetButton').addEventListener('click', () => postAction('/reset'));

    fetchState();
  </script>
</body>
</html>
)rawliteral";
}  // namespace

void startTargetAlert();
void renderBaseColor();
CRGB nextRandomColor();
void renderCurrentLedState();
void triggerTestLight();
void refreshTestLight();

String stateJson() {
  return "{\"count\":" + String(counter) + ",\"target\":" + String(targetCount) + "}";
}

void ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastWifiRetryMs < WIFI_RETRY_INTERVAL_MS) {
    return;
  }

  lastWifiRetryMs = now;
  Serial.println("Wi-Fi disconnected, retrying...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

long extractJsonLong(const String& json, const char* key, long defaultValue) {
  const String pattern = String("\"") + key + "\":";
  const int start = json.indexOf(pattern);
  if (start < 0) {
    return defaultValue;
  }

  int valueStart = start + pattern.length();
  while (valueStart < static_cast<int>(json.length()) &&
         (json[valueStart] == ' ' || json[valueStart] == '\t')) {
    valueStart++;
  }

  int valueEnd = valueStart;
  while (valueEnd < static_cast<int>(json.length()) &&
         (json[valueEnd] == '-' || (json[valueEnd] >= '0' && json[valueEnd] <= '9'))) {
    valueEnd++;
  }

  if (valueEnd == valueStart) {
    return defaultValue;
  }

  return json.substring(valueStart, valueEnd).toInt();
}

void applyRemoteState(unsigned long newCount, int newTarget) {
  const unsigned long previous = counter;
  counter = newCount;
  targetCount = newTarget < 0 ? 0 : newTarget;

  if (targetCount > 0 && counter == static_cast<unsigned long>(targetCount) && previous != counter) {
    startTargetAlert();
  } else {
    if (counter != static_cast<unsigned long>(targetCount)) {
      targetAlertActive = false;
      FastLED.setBrightness(80);
    }
    currentDisplayColor = (counter == 0) ? CRGB::Black : nextRandomColor();
    renderCurrentLedState();
  }

  Serial.print("Remote sync -> Count: ");
  Serial.print(counter);
  Serial.print(" / Target: ");
  Serial.println(targetCount);
}

void fetchRemoteState(bool forceNow = false) {
  const unsigned long now = millis();
  if (!forceNow && now - lastRemoteSyncMs < REMOTE_SYNC_INTERVAL_MS) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  lastRemoteSyncMs = now;

  HTTPClient http;
  const String url = String(SERVER_BASE_URL) + "/api/teams/" + TEAM_ID + "/state";
  http.begin(url);
  const int statusCode = http.GET();
  if (statusCode < 200 || statusCode >= 300) {
    http.end();
    return;
  }

  const String body = http.getString();
  http.end();

  const long syncedCount = extractJsonLong(body, "count", static_cast<long>(counter));
  const long syncedTarget = extractJsonLong(body, "target", static_cast<long>(targetCount));
  const long syncedTestLightSeq = extractJsonLong(body, "testLightSeq", lastTestLightSeq);

  if (syncedTestLightSeq > lastTestLightSeq) {
    lastTestLightSeq = syncedTestLightSeq;
    triggerTestLight();
  }

  if (syncedCount < 0) {
    return;
  }

  if (static_cast<unsigned long>(syncedCount) != counter || static_cast<int>(syncedTarget) != targetCount) {
    applyRemoteState(static_cast<unsigned long>(syncedCount), static_cast<int>(syncedTarget));
  }
}

void sendHeartbeat(bool forceNow = false, bool includeCount = false) {
  const unsigned long now = millis();
  if (!forceNow && now - lastHeartbeatMs < HEARTBEAT_INTERVAL_MS) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  lastHeartbeatMs = now;

  HTTPClient http;
  const String url = String(SERVER_BASE_URL) + "/api/devices/heartbeat";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  const String payload =
      String("{\"teamId\":\"") + TEAM_ID +
      "\",\"deviceId\":\"" + DEVICE_ID +
      (includeCount ? String("\",\"count\":") + String(counter) : String()) +
      "}";

  const int statusCode = http.POST(payload);
  if (statusCode < 200 || statusCode >= 300) {
    Serial.print("Heartbeat failed, status: ");
    Serial.println(statusCode);
  }
  http.end();
}

void showSolid(const CRGB& color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void shuffleColorOrder() {
  for (size_t i = 0; i < COLOR_COUNT; i++) colorOrder[i] = i;
  for (size_t i = COLOR_COUNT - 1; i > 0; i--) {
    const size_t j = random(i + 1);
    const size_t tmp = colorOrder[i];
    colorOrder[i] = colorOrder[j];
    colorOrder[j] = tmp;
  }
  // 避免重洗後第一個顏色與上一個相同
  if (lastColorIdx < COLOR_COUNT && colorOrder[0] == lastColorIdx && COLOR_COUNT > 1) {
    const size_t tmp = colorOrder[0];
    colorOrder[0] = colorOrder[1];
    colorOrder[1] = tmp;
  }
  colorOrderPos = 0;
}

CRGB nextRandomColor() {
  if (colorOrderPos >= COLOR_COUNT) {
    shuffleColorOrder();
  }
  lastColorIdx = colorOrder[colorOrderPos++];
  return COLOR_PALETTE[lastColorIdx];
}

void startTargetAlert() {
  targetAlertActive = true;
    alertBreathBrightness = ALERT_BREATH_MIN;
  alertBreathGoingUp = true;
  alertBreathLastStepMs = millis();
  FastLED.setBrightness(alertBreathBrightness);
  showSolid(CRGB::Yellow);
}

void renderBaseColor() {
  showSolid(currentDisplayColor);
}

void renderCurrentLedState() {
  if (testLightEndMs > millis()) {
    FastLED.setBrightness(220);
    showSolid(CRGB::White);
    return;
  }

  if (targetAlertActive) {
    FastLED.setBrightness(alertBreathBrightness);
    showSolid(CRGB::Yellow);
    return;
  }

  FastLED.setBrightness(80);
  renderBaseColor();
}

void triggerTestLight() {
  testLightEndMs = millis() + TEST_LIGHT_DURATION_MS;
  Serial.println("Test light triggered");
  renderCurrentLedState();
}

void refreshTestLight() {
  if (testLightEndMs == 0) {
    return;
  }

  const unsigned long now = millis();
  if (now < testLightEndMs) {
    return;
  }

  testLightEndMs = 0;
  renderCurrentLedState();
}

bool hasReachedTarget() {
  return targetCount > 0 && counter >= static_cast<unsigned long>(targetCount);
}

void refreshTargetAlert() {
  if (testLightEndMs > millis()) {
    return;
  }

  if (!targetAlertActive) {
    return;
  }

  const unsigned long now = millis();

  if (now - alertBreathLastStepMs >= ALERT_BREATH_STEP_MS) {
    alertBreathLastStepMs = now;
    if (alertBreathGoingUp) {
      if (alertBreathBrightness < ALERT_BREATH_MAX) {
        alertBreathBrightness++;
      } else {
        alertBreathGoingUp = false;
      }
    } else {
      if (alertBreathBrightness > ALERT_BREATH_MIN) {
        alertBreathBrightness--;
      } else {
        alertBreathGoingUp = true;
      }
    }
    FastLED.setBrightness(alertBreathBrightness);
    FastLED.show();
  }
}

void applyCounterChange(unsigned long newValue, const char* reason) {
  const unsigned long previous = counter;
  counter = newValue;

  if (targetCount > 0 && counter == static_cast<unsigned long>(targetCount) && previous != counter) {
    startTargetAlert();
  } else {
    if (counter != static_cast<unsigned long>(targetCount)) {
        targetAlertActive = false;
        FastLED.setBrightness(80);
      }
    currentDisplayColor = (counter == 0) ? CRGB::Black : nextRandomColor();
    renderCurrentLedState();
  }

  Serial.print(reason);
  Serial.print(" -> Count: ");
  Serial.println(counter);

  sendHeartbeat(true, true);
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleState() {
  server.send(200, "application/json", stateJson());
}

void handleIncrement() {
  if (!hasReachedTarget()) {
    applyCounterChange(counter + 1, "Manual +1");
  }
  server.send(200, "application/json", stateJson());
}

void handleDecrement() {
  if (counter > 0) {
    applyCounterChange(counter - 1, "Manual -1");
  }
  server.send(200, "application/json", stateJson());
}

void handleReset() {
  applyCounterChange(0, "Reset");
  server.send(200, "application/json", stateJson());
}

void handleSetTarget() {
  if (server.hasArg("target")) {
    const long value = server.arg("target").toInt();
    targetCount = static_cast<int>(value < 0 ? 0 : value);
  }

  if (targetCount > 0 && counter == static_cast<unsigned long>(targetCount)) {
    startTargetAlert();
  } else {
    targetAlertActive = false;
    FastLED.setBrightness(80);
    renderBaseColor();
  }

  Serial.print("Target set: ");
  Serial.println(targetCount);
  server.send(200, "application/json", stateJson());
}

void setup() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(150);
  showSolid(CRGB::Black);

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  delay(10);
  lastSensorState = digitalRead(SENSOR_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  unsigned long wifiStartMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartMs < 15000) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();

  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/increment", HTTP_POST, handleIncrement);
  server.on("/decrement", HTTP_POST, handleDecrement);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/set-target", HTTP_POST, handleSetTarget);
  server.begin();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");
    Serial.print("ESP32 IP: http://");
    Serial.println(WiFi.localIP());
    Serial.print("Central host: ");
    Serial.println(SERVER_BASE_URL);
    Serial.print("Team ID: ");
    Serial.println(TEAM_ID);
    sendHeartbeat(true, true);
    fetchRemoteState(true);
  } else {
    Serial.println("Wi-Fi connection failed");
    Serial.println("Please check SSID/password or router signal.");
  }
}

void loop() {
  server.handleClient();
  refreshTestLight();
  refreshTargetAlert();
  ensureWifiConnected();
  sendHeartbeat(false, false);
  fetchRemoteState(false);

  const int sensorState = digitalRead(SENSOR_PIN);

  if (lastSensorState == HIGH && sensorState == LOW && !hasReachedTarget()) {
    applyCounterChange(counter + 1, "Sensor +1");
    delay(50);
  }

  lastSensorState = sensorState;
}