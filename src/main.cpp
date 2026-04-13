#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

#define SENSOR_PIN 1  // KY-010 光遮斷傳感器的輸出腳位
#define LED_PIN 3   // WS2812 LED 的數據腳位
#define NUM_LEDS 24

//呼吸燈亮度在 26~28行調整，燈的亮度在404行調整
CRGB leds[NUM_LEDS];

namespace {
const char* AP_SSID = "ESP32-Counter";
const char* AP_PASSWORD = "12345678";

WebServer server(80);
volatile unsigned long counter = 0;
int targetCount = 0;
int lastSensorState = HIGH;

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
      font-size: 4rem;
      font-weight: 700;
      color: #60a5fa;
    }

    .target-display {
      margin-top: 8px;
      color: #bfdbfe;
      font-size: 0.98rem;
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
    <div id="count" class="count">0</div>
    <div id="targetDisplay" class="target-display">目標人數：0</div>
    <div class="actions">
      <button id="incrementButton" class="button" type="button">+1</button>
      <button id="decrementButton" class="button" type="button">-1</button>
      <button id="resetButton" class="button" type="button">歸零</button>
    </div>
    <div class="target-row">
      <input id="targetInput" type="number" min="0" step="1" placeholder="輸入目標人數">
      <button id="setTargetButton" class="button" type="button">設定</button>
    </div>
  </main>

  <script>
    let hasEnteredApp = false;
    let pollingId = null;

    function updateUi(state) {
      document.getElementById('count').textContent = state.count;
      document.getElementById('targetDisplay').textContent = `目標人數：${state.target}`;
      document.getElementById('targetInput').value = state.target;
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
    document.getElementById('setTargetButton').addEventListener('click', () => {
      const target = document.getElementById('targetInput').value || '0';
      postAction('/set-target', `target=${encodeURIComponent(target)}`);
    });

    fetchState();
  </script>
</body>
</html>
)rawliteral";
}  // namespace

String stateJson() {
  return "{\"count\":" + String(counter) + ",\"target\":" + String(targetCount) + "}";
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

bool hasReachedTarget() {
  return targetCount > 0 && counter >= static_cast<unsigned long>(targetCount);
}

void refreshTargetAlert() {
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
    renderBaseColor();
  }

  Serial.print(reason);
  Serial.print(" -> Count: ");
  Serial.println(counter);
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

  WiFi.softAP(AP_SSID, AP_PASSWORD);

  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/increment", HTTP_POST, handleIncrement);
  server.on("/decrement", HTTP_POST, handleDecrement);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/set-target", HTTP_POST, handleSetTarget);
  server.begin();

  Serial.println();
  Serial.println("Wi-Fi AP started");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("Open: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  refreshTargetAlert();

  const int sensorState = digitalRead(SENSOR_PIN);

  if (lastSensorState == HIGH && sensorState == LOW && !hasReachedTarget()) {
    applyCounterChange(counter + 1, "Sensor +1");
    delay(50);
  }

  lastSensorState = sensorState;
}