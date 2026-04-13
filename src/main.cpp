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

unsigned long alertBreathStepMs = 20;
uint8_t alertBreathMin = 0;
uint8_t alertBreathMax = 255;

const CRGB COLOR_PALETTE[] = {
  CRGB(230, 230, 250),  // 薰衣草
  CRGB(255, 182, 193),  // 淡粉紅
  CRGB(175, 238, 238),  // 淡土耳其藍
  CRGB(176, 224, 230),  // 粉藍
  CRGB(216, 191, 216),  // 薊色
  CRGB(238, 130, 238),  // 紫羅蘭
  CRGB(100, 149, 237),  // 矢車菊藍
  CRGB(102, 205, 170),  // 中海藍綠
  CRGB(218, 112, 214),  // 蘭花紫
  CRGB(70, 130, 180),   // 鋼鐵藍
  CRGB(147, 112, 219),  // 中紫
  CRGB(255, 127, 80),   // 珊瑚
  CRGB(135, 206, 235),  // 天藍
  CRGB(32, 178, 170),   // 淺海綠
  CRGB(123, 104, 238),  // 中板岩藍
  CRGB(127, 255, 212),  // 碧綠
  CRGB(255, 192, 203),  // 粉紅
  CRGB(64, 224, 208),   // 綠松石
  CRGB(221, 160, 221),  // 李子紫
  CRGB(0, 206, 209)     // 深土耳其藍
};

const size_t COLOR_COUNT = sizeof(COLOR_PALETTE) / sizeof(COLOR_PALETTE[0]);

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
      min-height: 100dvh;
      display: grid;
      place-items: center;
      background: linear-gradient(135deg, #111827, #1f2937);
      color: #f9fafb;
      padding: 12px;
      box-sizing: border-box;
    }

    .card {
      width: min(100%, 420px);
      padding: 32px;
      border-radius: 24px;
      background: rgba(255, 255, 255, 0.08);
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.35);
      text-align: center;
      backdrop-filter: blur(10px);
      box-sizing: border-box;
    }

    .hidden {
      display: none;
    }

    .welcome {
      width: min(100%, 460px);
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
    .tab-bar { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 20px; }
    .tab-btn { border: none; border-radius: 10px; padding: 10px 0; font-size: 0.95rem; font-weight: 600; color: #9ca3af; background: rgba(255,255,255,0.06); cursor: pointer; transition: all 0.15s; width: 100%; }
    .tab-btn.active { color: #f9fafb; background: linear-gradient(135deg, #7c3aed, #6d28d9); }
    .swatch-grid { display: grid; grid-template-columns: repeat(5, 1fr); gap: 8px; margin: 12px 0; }
    .swatch { aspect-ratio: 1; border-radius: 8px; border: 3px solid transparent; cursor: pointer; transition: transform 0.12s, border-color 0.12s; }
    .swatch:hover { transform: scale(1.08); }
    .swatch.selected { border-color: #fff; box-shadow: 0 0 0 2px rgba(255,255,255,0.45); }
    .field-block { text-align: left; margin-top: 14px; }
    .field-label { display: block; font-size: 0.82rem; color: #9ca3af; margin-bottom: 4px; }
    .back-btn { margin-top: 16px; font-size: 0.9rem; color: #9ca3af; background: transparent; border: 1px solid #374151; border-radius: 10px; padding: 8px 14px; cursor: pointer; transition: all 0.15s; width: 100%; }
    .back-btn:hover { color: #f9fafb; border-color: #6b7280; }
    .error-hint { font-size: 0.8rem; color: #f87171; margin-top: 4px; min-height: 1rem; }

    @media (max-width: 420px) {
      body {
        padding: 8px;
      }

      .card {
        padding: 20px;
        border-radius: 16px;
      }

      h1 {
        font-size: 1.35rem;
      }

      .count {
        font-size: 3.2rem;
      }

      .button {
        padding: 9px 10px;
        font-size: 0.95rem;
      }
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
    <div style="margin-top:10px">
      <button id="testButton" class="button" style="width:100%;background:linear-gradient(135deg,#374151,#1f2937)" type="button">燈光測試</button>
    </div>
  </section>

  <section id="testScreen" class="card welcome hidden">
    <h1>燈光測試</h1>
    <div class="tab-bar">
      <button class="tab-btn active" id="tabSwitch" type="button">切換燈</button>
      <button class="tab-btn" id="tabFinal" type="button">最終燈</button>
    </div>

    <div id="panelSwitch">
      <div class="swatch-grid" id="swatchGrid"></div>
      <div class="field-block">
        <label class="field-label" for="swBrightness">亮度（0–255）</label>
        <input id="swBrightness" type="number" min="0" max="255" step="1" placeholder="80">
      </div>
      <div style="margin-top:12px">
        <button class="button" style="width:100%" id="applySwitch" type="button">套用</button>
      </div>
    </div>

    <div id="panelFinal" class="hidden">
      <div class="field-block">
        <label class="field-label" for="finalMin">最小亮度（0–254）</label>
        <input id="finalMin" type="number" min="0" max="254" step="1" placeholder="0">
        <div class="error-hint" id="errMin"></div>
      </div>
      <div class="field-block">
        <label class="field-label" for="finalMax">最大亮度（1–255）</label>
        <input id="finalMax" type="number" min="1" max="255" step="1" placeholder="225">
        <div class="error-hint" id="errMax"></div>
      </div>
      <div class="field-block">
        <label class="field-label" for="finalPeriod">週期（秒，最小→最大→最小）</label>
        <input id="finalPeriod" type="number" min="0.1" step="0.1" placeholder="9">
        <div class="error-hint" id="errPeriod"></div>
      </div>
      <div style="margin-top:12px">
        <button class="button" style="width:100%" id="applyFinal" type="button">套用預覽</button>
      </div>
    </div>

    <button class="back-btn" type="button" id="backFromTest">← 返回</button>

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
  </main>

  <script>
    let hasEnteredApp = false;
    let pollingId = null;

    function updateUi(state) {
      document.getElementById('count').textContent = state.count;
      document.getElementById('targetDisplay').textContent = `目標人數：${state.target}`;
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

    // ── 燈光測試 ──────────────────────────────────────────
    const PALETTE = [
      { name: '薰衣草',     hex: '#E6E6FA' },
      { name: '淡粉紅',     hex: '#FFB6C1' },
      { name: '淡土耳其藍', hex: '#AFEEEE' },
      { name: '粉藍',       hex: '#B0E0E6' },
      { name: '薊色',       hex: '#D8BFD8' },
      { name: '紫羅蘭',     hex: '#EE82EE' },
      { name: '矢車菊藍',   hex: '#6495ED' },
      { name: '中海藍綠',   hex: '#66CDAA' },
      { name: '蘭花紫',     hex: '#DA70D6' },
      { name: '鋼鐵藍',     hex: '#4682B4' },
      { name: '中紫',       hex: '#9370DB' },
      { name: '珊瑚',       hex: '#FF7F50' },
      { name: '天藍',       hex: '#87CEEB' },
      { name: '淺海綠',     hex: '#20B2AA' },
      { name: '中板岩藍',   hex: '#7B68EE' },
      { name: '碧綠',       hex: '#7FFFD4' },
      { name: '粉紅',       hex: '#FFC0CB' },
      { name: '綠松石',     hex: '#40E0D0' },
      { name: '李子紫',     hex: '#DDA0DD' },
      { name: '深土耳其藍', hex: '#00CED1' },
    ];

    let selectedColorIdx = 0;

    function showScreen(id) {
      document.getElementById('welcomeScreen').classList.toggle('hidden', id !== 'welcomeScreen');
      document.getElementById('testScreen').classList.toggle('hidden', id !== 'testScreen');
    }

    function switchTab(name) {
      const isSw = name === 'switch';
      document.getElementById('panelSwitch').classList.toggle('hidden', !isSw);
      document.getElementById('panelFinal').classList.toggle('hidden', isSw);
      document.getElementById('tabSwitch').classList.toggle('active', isSw);
      document.getElementById('tabFinal').classList.toggle('active', !isSw);
    }

    function buildSwatchGrid() {
      const grid = document.getElementById('swatchGrid');
      PALETTE.forEach((c, i) => {
        const el = document.createElement('div');
        el.className = 'swatch' + (i === 0 ? ' selected' : '');
        el.style.background = c.hex;
        el.title = c.name;
        el.addEventListener('click', () => {
          grid.querySelectorAll('.swatch').forEach(s => s.classList.remove('selected'));
          el.classList.add('selected');
          selectedColorIdx = i;
        });
        grid.appendChild(el);
      });
    }

    document.getElementById('testButton').addEventListener('click', () => showScreen('testScreen'));
    document.getElementById('backFromTest').addEventListener('click', () => showScreen('welcomeScreen'));
    document.getElementById('tabSwitch').addEventListener('click', () => switchTab('switch'));
    document.getElementById('tabFinal').addEventListener('click', () => switchTab('final'));

    document.getElementById('applySwitch').addEventListener('click', async () => {
      const raw = document.getElementById('swBrightness').value;
      const br = Math.min(255, Math.max(0, raw !== '' ? parseInt(raw) : 80));
      await fetch('/led-test/set-color', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
        body: `idx=${selectedColorIdx}&brightness=${br}`
      });
    });

    document.getElementById('applyFinal').addEventListener('click', async () => {
      const minBr  = parseInt(document.getElementById('finalMin').value);
      const maxBr  = parseInt(document.getElementById('finalMax').value);
      const period = parseFloat(document.getElementById('finalPeriod').value);
      let valid = true;
      document.getElementById('errMin').textContent    = '';
      document.getElementById('errMax').textContent    = '';
      document.getElementById('errPeriod').textContent = '';
      if (isNaN(minBr) || minBr < 0)           { document.getElementById('errMin').textContent    = '不可小於 0';       valid = false; }
      else if (minBr >= maxBr)                  { document.getElementById('errMin').textContent    = '必須小於最大亮度'; valid = false; }
      if (isNaN(maxBr) || maxBr > 255)          { document.getElementById('errMax').textContent    = '不可大於 255';     valid = false; }
      else if (!isNaN(minBr) && maxBr <= minBr) { document.getElementById('errMax').textContent    = '必須大於最小亮度'; valid = false; }
      if (isNaN(period) || period <= 0)         { document.getElementById('errPeriod').textContent = '週期必須大於 0';   valid = false; }
      if (!valid) return;
      await fetch('/led-test/set-final', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
        body: `minBr=${minBr}&maxBr=${maxBr}&period=${period}`
      });
    });

    buildSwatchGrid();

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

CRGB colorForCounter(unsigned long value) {
  if (value == 0) {
    return CRGB::Black;
  }

  const size_t index = (value - 1) % COLOR_COUNT;
  return COLOR_PALETTE[index];
}

void startTargetAlert() {
  targetAlertActive = true;
  alertBreathBrightness = alertBreathMin;
  alertBreathGoingUp = true;
  alertBreathLastStepMs = millis();
  FastLED.setBrightness(alertBreathBrightness);
  showSolid(CRGB::Yellow);
}

void renderBaseColor() {
  showSolid(colorForCounter(counter));
}

bool hasReachedTarget() {
  return targetCount > 0 && counter >= static_cast<unsigned long>(targetCount);
}

void refreshTargetAlert() {
  if (!targetAlertActive) {
    return;
  }

  const unsigned long now = millis();

  if (now - alertBreathLastStepMs >= alertBreathStepMs) {
    alertBreathLastStepMs = now;
    if (alertBreathGoingUp) {
      if (alertBreathBrightness < alertBreathMax) {
        alertBreathBrightness++;
      } else {
        alertBreathGoingUp = false;
      }
    } else {
      if (alertBreathBrightness > alertBreathMin) {
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
    renderBaseColor();
  }

  Serial.print(reason);
  Serial.print(" -> Count: ");
  Serial.println(counter);
}

void handleLedTestSetColor() {
  if (server.hasArg("idx") && server.hasArg("brightness")) {
    const int idx = server.arg("idx").toInt();
    const int br  = server.arg("brightness").toInt();
    if (idx >= 0 && idx < (int)COLOR_COUNT && br >= 0 && br <= 255) {
      targetAlertActive = false;
      FastLED.setBrightness((uint8_t)br);
      showSolid(COLOR_PALETTE[idx]);
    }
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleLedTestSetFinal() {
  if (server.hasArg("minBr") && server.hasArg("maxBr") && server.hasArg("period")) {
    const int   minBrVal  = server.arg("minBr").toInt();
    const int   maxBrVal  = server.arg("maxBr").toInt();
    const float periodVal = server.arg("period").toFloat();
    if (minBrVal >= 0 && maxBrVal <= 255 && minBrVal < maxBrVal && periodVal > 0) {
      alertBreathMin = (uint8_t)minBrVal;
      alertBreathMax = (uint8_t)maxBrVal;
      const int steps = (int)(alertBreathMax - alertBreathMin) * 2;
      alertBreathStepMs = (unsigned long)((periodVal * 1000.0f) / (float)steps);
      if (alertBreathStepMs < 1) alertBreathStepMs = 1;
      targetAlertActive     = true;
      alertBreathBrightness = alertBreathMin;
      alertBreathGoingUp    = true;
      alertBreathLastStepMs = millis();
      showSolid(CRGB::Yellow);
    }
  }
  server.send(200, "application/json", "{\"ok\":true}");
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
  server.on("/led-test/set-color", HTTP_POST, handleLedTestSetColor);
  server.on("/led-test/set-final", HTTP_POST, handleLedTestSetFinal);
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