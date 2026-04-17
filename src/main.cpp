#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <FastLED.h>

#define SENSOR_PIN 1  // KY-010 光遮斷傳感器的輸出腳位
#define LED_PIN 3   // WS2812 LED 的數據腳位
#define BUZZER_PIN 2  // 被動式蜂鳴器的訊號腳位
#define NUM_LEDS 24


CRGB leds[NUM_LEDS];

const char* WIFI_SSID = "counter";
const char* WIFI_PASSWORD = "88888888";
const char* SERVER_BASE_URL = "http://192.168.66.101:3000";
const char* TEAM_ID = "team-1";
const char* DEVICE_ID = "esp32-table-1";

WebServer server(80);
volatile unsigned long counter = 0; //目前計數值
int targetCount = 0; //目標值
int lastSensorState = HIGH; // 上次的感測器狀態，預設為 HIGH（未被遮擋）
unsigned long lastHeartbeatMs = 0; // 上次心跳的時間
unsigned long lastWifiRetryMs = 0; // 上次重試 WiFi 連線的時間
unsigned long lastRemoteSyncMs = 0; // 上次遠端同步的時間
unsigned long testLightEndMs = 0; // 測試燈結束的時間
long lastTestLightSeq = 0; // 上次測試燈序列
long lastTestBeepSeq = 0; // 上次蜂鳴測試序列
bool pendingCountSync = false; // 是否有待同步的計數
bool isScoringMode = false; // 是否處於計分模式
bool hasModeSync = false; // 是否已同步模式

constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 5000; // Wi-Fi 連線重試間隔
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 5000; // 心跳間隔
constexpr unsigned long REMOTE_SYNC_INTERVAL_MS = 1200; // 遠端同步間隔
constexpr unsigned long TEST_LIGHT_DURATION_MS = 1200; // 測試燈持續時間

bool targetAlertActive = false; // 是否正在目標達成警示中
uint8_t alertBreathBrightness = 180; // 呼吸燈亮度
bool alertBreathGoingUp = true; // 呼吸燈亮度增減方向
unsigned long alertBreathLastStepMs = 0; // 上次呼吸燈步進的時間

constexpr unsigned long ALERT_BREATH_STEP_MS = 10; // 呼吸燈步進間隔
constexpr uint8_t ALERT_BREATH_MIN = 10; // 呼吸燈最小亮度
constexpr uint8_t ALERT_BREATH_MAX = 255; // 呼吸燈最大亮度
constexpr uint8_t BUZZER_CHANNEL = 0; // ESP32 LEDC 聲音輸出通道
constexpr uint8_t BUZZER_RESOLUTION = 8; // LEDC PWM 解析度

const uint16_t SCORE_MELODY_NOTES[] = {988, 1319}; // 得分旋律頻率（Hz）
const unsigned long SCORE_MELODY_DURATIONS_MS[] = {100, 400}; // 各音符持續時間（ms）
const size_t SCORE_MELODY_LENGTH = sizeof(SCORE_MELODY_NOTES) / sizeof(SCORE_MELODY_NOTES[0]);
constexpr unsigned long SCORE_RAINBOW_DURATION_MS = 500; // 彩虹特效總時長（與旋律一致）

bool scoreMelodyActive = false; // 得分旋律是否正在播放
size_t scoreMelodyIndex = 0; // 目前播放到第幾個音符
unsigned long scoreMelodyStepStartMs = 0; // 目前音符開始的時間
bool scoreRainbowActive = false; // 得分彩虹特效是否正在播放
unsigned long scoreRainbowStartMs = 0; // 得分彩虹特效開始時間

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

    .button.secondary {
      margin-top: 10px;
      width: 100%;
      background: linear-gradient(135deg, #f59e0b, #d97706);
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
    <button id="beepButton" class="button secondary" type="button">測試蜂鳴器</button>
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

    async function triggerBeep() {
      const button = document.getElementById('beepButton');
      button.disabled = true;
      try {
        await fetch('/beep', { method: 'POST' });
      } catch (error) {
        console.error(error);
      } finally {
        setTimeout(() => {
          button.disabled = false;
        }, 200);
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
    document.getElementById('beepButton').addEventListener('click', triggerBeep);

    fetchState();
  </script>
</body>
</html>
)rawliteral";

const char STAR_NIGHT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-Hant">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>星夜進度牆</title>
  <style>
    :root {
      color-scheme: dark;
      font-family: "Noto Sans TC", "Microsoft JhengHei", sans-serif;
    }

    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      min-height: 100vh;
      color: #e5efff;
      background:
        radial-gradient(circle at 20% 20%, rgba(83, 110, 255, 0.22), transparent 45%),
        radial-gradient(circle at 80% 12%, rgba(45, 87, 255, 0.18), transparent 42%),
        radial-gradient(circle at 50% 120%, rgba(17, 44, 133, 0.38), transparent 60%),
        linear-gradient(180deg, #030514 0%, #070b1f 48%, #040811 100%);
      overflow: hidden;
      display: grid;
      grid-template-rows: auto 1fr;
    }

    header {
      padding: 18px 18px 6px;
      text-align: center;
      position: relative;
      z-index: 5;
    }

    h1 {
      margin: 0;
      font-size: clamp(1.2rem, 2.2vw, 1.7rem);
      letter-spacing: 0.08em;
      color: #f4f8ff;
      text-shadow: 0 0 18px rgba(141, 181, 255, 0.45);
    }

    .subtitle {
      margin: 8px 0 0;
      color: rgba(223, 236, 255, 0.82);
      font-size: clamp(0.9rem, 1.6vw, 1rem);
      letter-spacing: 0.02em;
    }

    .sky {
      position: relative;
      width: 100%;
      height: 100%;
      overflow: hidden;
      isolation: isolate;
    }

    .cluster {
      position: absolute;
      width: 0;
      height: 0;
      transform: translate(-50%, -50%);
    }

    .big-star {
      --size: clamp(14px, 1.8vw, 22px);
      position: absolute;
      left: 0;
      top: 0;
      width: var(--size);
      height: var(--size);
      transform: translate(-50%, -50%) rotate(45deg) scale(0.95);
      border-radius: 4px;
      background: rgba(255, 255, 255, 0.04);
      box-shadow: 0 0 0 rgba(255, 245, 183, 0.0);
      transition: background 0.35s ease, box-shadow 0.35s ease, transform 0.35s ease;
      opacity: 0.36;
    }

    .big-star::before,
    .big-star::after {
      content: "";
      position: absolute;
      left: 50%;
      top: 50%;
      width: 100%;
      height: 100%;
      border-radius: 4px;
      transform: translate(-50%, -50%) rotate(45deg);
      background: inherit;
    }

    .big-star.on {
      background: linear-gradient(145deg, #fff4bf 0%, #ffd76b 100%);
      box-shadow:
        0 0 16px rgba(255, 227, 140, 0.85),
        0 0 34px rgba(255, 201, 76, 0.5);
      opacity: 1;
      transform: translate(-50%, -50%) rotate(45deg) scale(1);
    }

    .small-star {
      --size: clamp(5px, 0.75vw, 8px);
      position: absolute;
      left: 0;
      top: 0;
      width: var(--size);
      height: var(--size);
      transform: translate(-50%, -50%) rotate(45deg);
      border-radius: 2px;
      background: rgba(255, 255, 255, 0.045);
      opacity: 0.25;
      transition: background 0.25s ease, opacity 0.25s ease, box-shadow 0.25s ease;
    }

    .small-star.on {
      background: #d8e9ff;
      opacity: 1;
      box-shadow: 0 0 8px rgba(180, 217, 255, 0.72);
    }

    .status {
      position: absolute;
      right: 12px;
      bottom: 12px;
      z-index: 5;
      font-size: clamp(0.85rem, 1.2vw, 0.95rem);
      color: rgba(230, 241, 255, 0.86);
      padding: 8px 12px;
      border-radius: 999px;
      background: rgba(8, 14, 34, 0.65);
      border: 1px solid rgba(169, 196, 255, 0.22);
      backdrop-filter: blur(2px);
    }
  </style>
</head>
<body>
  <header>
    <h1>圓滿餐會星夜進度</h1>
    <p class="subtitle">每加入 1 人點亮 1 顆小星星；每滿 20 人點亮 1 顆大星星</p>
  </header>

  <main id="sky" class="sky"></main>
  <div id="status" class="status">0 / 400 人</div>

  <script>
    const BIG_STAR_COUNT = 20;
    const SMALL_PER_BIG = 20;
    const TOTAL_SMALL = BIG_STAR_COUNT * SMALL_PER_BIG;

    const skyEl = document.getElementById('sky');
    const statusEl = document.getElementById('status');

    // Simple deterministic random generator so star layout stays stable every reload.
    function createRng(seed) {
      let value = seed >>> 0;
      return function () {
        value = (1664525 * value + 1013904223) >>> 0;
        return value / 4294967296;
      };
    }

    const rng = createRng(0x7cafe001);
    const smallStarEls = [];
    const bigStarEls = [];

    function clamp(v, min, max) {
      return Math.min(max, Math.max(min, v));
    }

    function createCluster(cxPercent, cyPercent) {
      const cluster = document.createElement('section');
      cluster.className = 'cluster';
      cluster.style.left = `${cxPercent}%`;
      cluster.style.top = `${cyPercent}%`;

      const big = document.createElement('div');
      big.className = 'big-star';
      cluster.appendChild(big);
      bigStarEls.push(big);

      for (let i = 0; i < SMALL_PER_BIG; i++) {
        const s = document.createElement('div');
        s.className = 'small-star';

        const angle = rng() * Math.PI * 2;
        const radius = 5 + rng() * 9;
        const jitter = (rng() - 0.5) * 2.1;
        const sx = Math.cos(angle) * radius + jitter;
        const sy = Math.sin(angle) * radius + jitter;

        s.style.left = `${sx}vmin`;
        s.style.top = `${sy}vmin`;
        cluster.appendChild(s);
        smallStarEls.push(s);
      }

      skyEl.appendChild(cluster);
    }

    function buildSky() {
      const cols = 5;
      const rows = 4;
      let index = 0;
      for (let r = 0; r < rows; r++) {
        for (let c = 0; c < cols; c++) {
          const baseX = ((c + 0.5) / cols) * 100;
          const baseY = ((r + 0.5) / rows) * 82 + 12;
          const jitterX = (rng() - 0.5) * 6.5;
          const jitterY = (rng() - 0.5) * 8;
          const x = clamp(baseX + jitterX, 8, 92);
          const y = clamp(baseY + jitterY, 16, 93);
          createCluster(x, y);
          index++;
          if (index >= BIG_STAR_COUNT) {
            return;
          }
        }
      }
    }

    function applyProgress(count, target) {
      const safeCount = Math.max(0, Math.min(TOTAL_SMALL, Number.isFinite(count) ? count : 0));

      for (let i = 0; i < TOTAL_SMALL; i++) {
        smallStarEls[i].classList.toggle('on', i < safeCount);
      }

      const litBigStars = Math.floor(safeCount / SMALL_PER_BIG);
      for (let i = 0; i < BIG_STAR_COUNT; i++) {
        bigStarEls[i].classList.toggle('on', i < litBigStars);
      }

      if (target > 0) {
        statusEl.textContent = `${safeCount} / ${Math.min(target, TOTAL_SMALL)} 人`;
      } else {
        statusEl.textContent = `${safeCount} / ${TOTAL_SMALL} 人`;
      }
    }

    async function fetchState() {
      try {
        const response = await fetch('/state');
        const data = await response.json();
        applyProgress(Number(data.count) || 0, Number(data.target) || 0);
      } catch (error) {
        console.error(error);
      }
    }

    buildSky();
    applyProgress(0, 0);
    fetchState();
    setInterval(fetchState, 500);
  </script>
</body>
</html>
)rawliteral";

// ===== Forward declarations for functions defined below =====
void showSolid(const CRGB& color);
void sendHeartbeat(bool, bool);
void refreshTargetAlert();
void scoringMode_renderLedState();
void countingMode_renderLedState();
void scoringMode_applyCounterChange(unsigned long newValue);
void countingMode_applyCounterChange(unsigned long newValue);
void startTargetAlert();
void renderBaseColor();
CRGB nextRandomColor();
void triggerTestLight();
void refreshTestLight();
void runScoreRainbowLap();
void refreshScoreRainbow();
void startScoreMelody();
void stopScoreMelody();
void refreshScoreMelody();
void initBuzzerPwm();
void buzzerWriteTone(uint16_t frequency);

// 回傳目前 count / target 的 JSON 字串，供本機網頁輪詢 /state 使用
String stateJson() {
  return "{\"count\":" + String(counter) + ",\"target\":" + String(targetCount) + "}";
}

// 確保 Wi-Fi 保持連線；若斷線且距上次重試已夠久，就重新連線
void ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;  // 已連線，不需要做任何事
  }

  const unsigned long now = millis();
  if (now - lastWifiRetryMs < WIFI_RETRY_INTERVAL_MS) {
    return;  // 還沒到重試間隔，等待
  }

  lastWifiRetryMs = now;  // 記錄本次重試時間
  Serial.println("Wi-Fi disconnected, retrying...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// 手動從 JSON 字串解析指定欄位的整數值（避免引入重量級 JSON 函式庫）
// 找不到欄位、或解析失敗時回傳 defaultValue
long extractJsonLong(const String& json, const char* key, long defaultValue) {
  const String pattern = String("\"") + key + "\":";  // 目標欄位形式："key":
  const int start = json.indexOf(pattern);
  if (start < 0) {
    return defaultValue;  // 找不到欄位
  }

  int valueStart = start + pattern.length();  // 跳過 key 部分，指向數值開頭
  while (valueStart < static_cast<int>(json.length()) &&
         (json[valueStart] == ' ' || json[valueStart] == '\t')) {
    valueStart++;  // 跳過空白
  }

  int valueEnd = valueStart;
  while (valueEnd < static_cast<int>(json.length()) &&
         (json[valueEnd] == '-' || (json[valueEnd] >= '0' && json[valueEnd] <= '9'))) {
    valueEnd++;  // 找數值結束位置（支援負號）
  }

  if (valueEnd == valueStart) {
    return defaultValue;  // 沒有任何數字字元
  }

  return json.substring(valueStart, valueEnd).toInt();
}

// ===== SCORING MODE - INDEPENDENT FUNCTIONS =====

// 套用從伺服器同步回來的計分狀態（count + target）
// 若 count 有增加，觸發彩虹燈效
void scoringMode_applyRemoteState(unsigned long newCount, int newTarget) {
  const unsigned long previous = counter;
  counter = newCount;
  targetCount = newTarget < 0 ? 0 : newTarget;
  const bool scoringIncrement = newCount > previous;

  Serial.print("Scoring Mode - Remote sync -> Count: ");
  Serial.print(counter);
  Serial.print(" / Target: ");
  Serial.println(targetCount);

  currentDisplayColor = (counter == 0) ? CRGB::Black : nextRandomColor();
  scoringMode_renderLedState();

  if (scoringIncrement) {
    startScoreMelody();
    runScoreRainbowLap();
  }
}

// 處理本機計數變更（感測器或手動按鈕）並立即同步到伺服器
void scoringMode_applyCounterChange(unsigned long newValue) {
  const unsigned long previous = counter;
  counter = newValue;
  pendingCountSync = true;  // 標記為待同步，heartbeat 會帶上 count

  Serial.print("Scoring Mode - Sensor/Manual +1 -> Count: ");
  Serial.println(counter);

  currentDisplayColor = (counter == 0) ? CRGB::Black : nextRandomColor();
  scoringMode_renderLedState();

  if (newValue == previous + 1) {
    startScoreMelody();
    runScoreRainbowLap();
  }

  sendHeartbeat(true, true);
}

// 計分模式 LED 渲染：試亮中顯示白燈，其餘時間保持關燈
void scoringMode_renderLedState() {
  if (testLightEndMs > millis()) {
    FastLED.setBrightness(220);  // 試亮用高亮度
    showSolid(CRGB::White);
    return;
  }

  if (scoreRainbowActive) {
    return;  // 彩虹特效播放中時，不覆蓋畫面
  }

  // In scoring mode, keep LEDs off when no test light is active.
  currentDisplayColor = CRGB::Black;
  FastLED.setBrightness(180);
  showSolid(CRGB::Black);
}

// 讀取感測器；偵測到 HIGH->LOW 下降沿（有物體遮擋）時計分 +1
void scoringMode_handleSensorInput() {
  const int sensorState = digitalRead(SENSOR_PIN);

  if (lastSensorState == HIGH && sensorState == LOW) {  // 下降沿觸發
    scoringMode_applyCounterChange(counter + 1);
    delay(50);  // 簡易去彈跳
  }

  lastSensorState = sensorState;  // 更新狀態供下一輪比較
}

// 本機網頁「+1」按鈕觸發
void scoringMode_handleIncrement() {
  scoringMode_applyCounterChange(counter + 1);
}

// 本機網頁「-1」按鈕觸發；counter 為 0 時不減
void scoringMode_handleDecrement() {
  if (counter > 0) {
    counter--;
    pendingCountSync = true;  // 標記需要同步
    Serial.print("Scoring Mode - Manual -1 -> Count: ");
    Serial.println(counter);
    currentDisplayColor = (counter == 0) ? CRGB::Black : nextRandomColor();
    scoringMode_renderLedState();
    sendHeartbeat(true, true);  // 立即同步到伺服器
  }
}

// 本機網頁「歸零」按鈕觸發：計分清 0
void scoringMode_handleReset() {
  counter = 0;
  pendingCountSync = true;  // 標記需要同步
  Serial.println("Scoring Mode - Reset");
  currentDisplayColor = CRGB::Black;
  scoringMode_renderLedState();
  sendHeartbeat(true, true);  // 立即同步到伺服器
}

// 本機網頁「設定目標」觸發（計分模式下目標為參考值，不阻擋計數）
void scoringMode_handleSetTarget() {
  if (server.hasArg("target")) {
    const long value = server.arg("target").toInt();
    targetCount = static_cast<int>(value < 0 ? 0 : value);  // 不允許負值
    Serial.print("Scoring Mode - Target set: ");
    Serial.println(targetCount);
  }
}

// 計分模式主循環：每圈更新試亮、燈效、感測器
void scoringMode_refreshLoop() {
  refreshTestLight();             // 試亮計時到期就關閉
  scoringMode_renderLedState();   // 更新 LED 狀態
  refreshScoreRainbow();          // 更新得分彩虹特效
  scoringMode_handleSensorInput(); // 讀取感測器
}

// ===== COUNTING MODE - INDEPENDENT FUNCTIONS =====

// 套用從伺服器同步回來的餐會模式狀態（count + target）
// 若此次同步剛好践到目標，觸發達標提示燈效
void countingMode_applyRemoteState(unsigned long newCount, int newTarget) {
  const unsigned long previous = counter;
  counter = newCount;
  targetCount = newTarget < 0 ? 0 : newTarget;

  // 只有「第一次刺穿目標」才觸發提示（previous != counter 防止重複醒）
  const bool banquetTargetReached = targetCount > 0 && counter == static_cast<unsigned long>(targetCount) && previous != counter;

  Serial.print("Counting Mode - Remote sync -> Count: ");
  Serial.print(counter);
  Serial.print(" / Target: ");
  Serial.println(targetCount);

  if (banquetTargetReached) {
    startTargetAlert();
  } else {
    if (counter != static_cast<unsigned long>(targetCount)) {
      targetAlertActive = false;
      FastLED.setBrightness(180);
    }
    currentDisplayColor = (counter == 0) ? CRGB::Black : nextRandomColor();
    countingMode_renderLedState();
  }
}

// 處理本機計數變更（感測器或手動按鈕）並立即同步
void countingMode_applyCounterChange(unsigned long newValue) {
  const unsigned long previous = counter;
  counter = newValue;
  pendingCountSync = true;  // 標記待同步

  // 同樣判斷是否剛好達標
  const bool banquetTargetReached = targetCount > 0 && counter == static_cast<unsigned long>(targetCount) && previous != counter;

  Serial.print("Counting Mode - Sensor/Manual +1 -> Count: ");
  Serial.println(counter);

  if (banquetTargetReached) {
    startTargetAlert();
  } else {
    if (counter != static_cast<unsigned long>(targetCount)) {
      targetAlertActive = false;
      FastLED.setBrightness(180);
    }
    currentDisplayColor = (counter == 0) ? CRGB::Black : nextRandomColor();
    countingMode_renderLedState();
  }

  if (newValue == previous + 1) {
    startScoreMelody();
  }

  sendHeartbeat(true, true);
}

// 餐會模式 LED 渲染：優先順序 = 試亮 > 達標呼吸燈 > 常態底色
void countingMode_renderLedState() {
  if (testLightEndMs > millis()) {
    FastLED.setBrightness(220);  // 試亮用高亮度白燈
    showSolid(CRGB::White);
    return;
  }

  if (targetAlertActive) {
    FastLED.setBrightness(alertBreathBrightness);  // 亮度由呼吸燈動畫機控制
    showSolid(CRGB::Yellow);  // 達標 = 黃燈
    return;
  }

  FastLED.setBrightness(180);
  renderBaseColor();  // 顯示 currentDisplayColor
}

// 回傳目前是否已達標（計數模式中可阻擋感測器繼續觸發）
bool countingMode_hasReachedTarget() {
  return targetCount > 0 && counter >= static_cast<unsigned long>(targetCount);
}

// 讀取感測器；未達標時空缺 HIGH->LOW 觸發 +1
void countingMode_handleSensorInput() {
  const int sensorState = digitalRead(SENSOR_PIN);

  if (lastSensorState == HIGH && sensorState == LOW && !countingMode_hasReachedTarget()) {  // 達標後自動停止計數
    countingMode_applyCounterChange(counter + 1);
    delay(50);  // 簡易去彈跳
  }

  lastSensorState = sensorState;
}

// 本機網頁「+1」按鈕；達標後不動作
void countingMode_handleIncrement() {
  if (!countingMode_hasReachedTarget()) {
    countingMode_applyCounterChange(counter + 1);
  }
}

// 本機網頁「-1」按鈕；減少後若退出達標就關閉呼吸燈
void countingMode_handleDecrement() {
  if (counter > 0) {
    counter--;
    pendingCountSync = true;  // 標記需要同步
    Serial.print("Counting Mode - Manual -1 -> Count: ");
    Serial.println(counter);
    if (counter != static_cast<unsigned long>(targetCount)) {
      targetAlertActive = false;  // 退出達標就關閉呼吸燈
      FastLED.setBrightness(180);
    }
    currentDisplayColor = (counter == 0) ? CRGB::Black : nextRandomColor();
    countingMode_renderLedState();
    sendHeartbeat(true, true);  // 立即同步
  }
}

// 本機網頁「歸零」按鈕：計數清 0、關閉呼吸燈
void countingMode_handleReset() {
  counter = 0;
  pendingCountSync = true;  // 標記需要同步
  Serial.println("Counting Mode - Reset");
  targetAlertActive = false;  // 關閉達標提示
  FastLED.setBrightness(180);
  currentDisplayColor = CRGB::Black;
  countingMode_renderLedState();
  sendHeartbeat(true, true);  // 立即同步
}

// 本機網頁「設定目標」觸發；設定後若當前 count == target 就直接觸發提示
void countingMode_handleSetTarget() {
  if (server.hasArg("target")) {
    const long value = server.arg("target").toInt();
    targetCount = static_cast<int>(value < 0 ? 0 : value);  // 不允許負值
    Serial.print("Counting Mode - Target set: ");
    Serial.println(targetCount);
  }

  if (targetCount > 0 && counter == static_cast<unsigned long>(targetCount)) {
    startTargetAlert();  // 目標和 count 相等，立即啟動提示
  } else {
    targetAlertActive = false;
    FastLED.setBrightness(180);
    countingMode_renderLedState();
  }
}

// 餐會模式主循環：每圈更新試亮、呼吸燈、LED、感測器
void countingMode_refreshLoop() {
  refreshTestLight();              // 試亮計時到期就關閉
  refreshTargetAlert();            // 更新呼吸燈亮度
  countingMode_renderLedState();   // 更新 LED
  countingMode_handleSensorInput(); // 讀取感測器
}

// 定期從伺服器拉取該框 state（mode、count、target、testLightSeq、testBeepSeq）
// forceNow=true 時略過節流，立即執行
// 防跟回舊値：若本地待同步且遠端回傳較小的小吐，保留本地値
void fetchRemoteState(bool forceNow = false) {
  const unsigned long now = millis();
  if (!forceNow && now - lastRemoteSyncMs < REMOTE_SYNC_INTERVAL_MS) {
    return;  // 還沒到輪詢間隔
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;  // 進入斷線不發請求
  }

  lastRemoteSyncMs = now;  // 記錄本次同步時間

  HTTPClient http;
  const String url = String(SERVER_BASE_URL) + "/api/teams/" + TEAM_ID + "/state";
  http.begin(url);
  const int statusCode = http.GET();
  if (statusCode < 200 || statusCode >= 300) {
    http.end();
    return;  // HTTP 錯誤直接放棄
  }

  const String body = http.getString();
  http.end();

  if (body.indexOf("\"mode\":\"scoring\"") >= 0) {
    isScoringMode = true;   // 從回應確認模式為計分
    hasModeSync = true;
  } else if (body.indexOf("\"mode\":") >= 0) {
    isScoringMode = false;  // 其他模式（banquet）
    hasModeSync = true;
  }

  const long syncedCount = extractJsonLong(body, "count", static_cast<long>(counter));       // 從 JSON 提取 count
  const long syncedTarget = extractJsonLong(body, "target", static_cast<long>(targetCount)); // 從 JSON 提取 target
  const long syncedTestLightSeq = extractJsonLong(body, "testLightSeq", lastTestLightSeq);   // 從 JSON 提取試亮序號
  const long syncedTestBeepSeq = extractJsonLong(body, "testBeepSeq", lastTestBeepSeq);       // 從 JSON 提取蜂鳴測試序號

  if (syncedTestLightSeq > lastTestLightSeq) {
    lastTestLightSeq = syncedTestLightSeq;  // 更新已處理序號
    triggerTestLight();  // 新序號代表管理床按了試亮
  }

  if (syncedTestBeepSeq > lastTestBeepSeq) {
    lastTestBeepSeq = syncedTestBeepSeq;  // 更新已處理序號
    startScoreMelody();  // 新序號代表管理端按了蜂鳴測試
  }

  if (syncedCount < 0) {
    return;  // 異常値直接放棄
  }

  unsigned long mergedCount = static_cast<unsigned long>(syncedCount);
  if (pendingCountSync && mergedCount < counter) {
    // Local count changed recently and has not been confirmed by host yet.
    // Ignore stale lower remote count to avoid jumping back to 0 then +1.
    mergedCount = counter;  // 保留本地小偀，避免畫面抗回
  }

  if (mergedCount != counter || static_cast<int>(syncedTarget) != targetCount) {
    if (isScoringMode) {
      scoringMode_applyRemoteState(mergedCount, static_cast<int>(syncedTarget));
    } else {
      countingMode_applyRemoteState(mergedCount, static_cast<int>(syncedTarget));
    }
  }
}

// 定期對伺服器報平安，帶上裝置識別與可選的 count
// forceNow=true 略過間隔立即送
// includeCount=true 或 pendingCountSync=true 時將 count 一起上傳
void sendHeartbeat(bool forceNow = false, bool includeCount = false) {
  const unsigned long now = millis();
  if (!forceNow && now - lastHeartbeatMs < HEARTBEAT_INTERVAL_MS) {
    return;  // 還沒到間隔
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;  // 進入斷線不發請求
  }

  lastHeartbeatMs = now;  // 記錄本次報平安時間

  HTTPClient http;
  const String url = String(SERVER_BASE_URL) + "/api/devices/heartbeat";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  const bool shouldIncludeCount = includeCount || pendingCountSync;  // 只要有待同步就帶 count

  const String payload =
      String("{\"teamId\":\"") + TEAM_ID +
      "\",\"deviceId\":\"" + DEVICE_ID +
      (shouldIncludeCount ? String("\",\"count\":") + String(counter) : String()) +  // 有待同步才帶 count 欄位
      "}";

  const int statusCode = http.POST(payload);
  if (statusCode < 200 || statusCode >= 300) {
    Serial.print("Heartbeat failed, status: ");
    Serial.println(statusCode);
  } else if (shouldIncludeCount) {
    pendingCountSync = false;  // 伺服器已確認，清除待同步旗標
  }
  http.end();
}

// 把所有燈珠設為同一顏色並更新輸出
void showSolid(const CRGB& color) {
  fill_solid(leds, NUM_LEDS, color);  // 將所有燈珠填满指定顏色
  FastLED.show();                     // 實際輸出到燈拰
}

void runScoreRainbowLap() {
  scoreRainbowActive = true;
  scoreRainbowStartMs = millis();
}

void refreshScoreRainbow() {
  if (!scoreRainbowActive) {
    return;
  }

  const unsigned long now = millis();
  const unsigned long elapsed = now - scoreRainbowStartMs;
  if (elapsed >= SCORE_RAINBOW_DURATION_MS) {
    scoreRainbowActive = false;
    showSolid(CRGB::Black);
    return;
  }

  const uint8_t baseHue = static_cast<uint8_t>((elapsed * 256UL) / SCORE_RAINBOW_DURATION_MS);
  for (uint8_t i = 0; i < NUM_LEDS; ++i) {
    const uint8_t hue = static_cast<uint8_t>(baseHue + (i * 256 / NUM_LEDS));
    leds[i] = CHSV(hue, 255, 180);
  }
  FastLED.show();
}

// 初始化蜂鳴器 PWM，兼容 ESP32 Arduino Core 2.x/3.x
void initBuzzerPwm() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(BUZZER_PIN, 2000, BUZZER_RESOLUTION);
#else
  ledcSetup(BUZZER_CHANNEL, 2000, BUZZER_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
#endif
}

// 依 Core 版本以正確參數型式輸出指定頻率
void buzzerWriteTone(uint16_t frequency) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWriteTone(BUZZER_PIN, frequency);
#else
  ledcWriteTone(BUZZER_CHANNEL, frequency);
#endif
}

// 從第一個音開始播放得分旋律；若前一段尚未播完就直接重播
void startScoreMelody() {
  scoreMelodyActive = true;
  scoreMelodyIndex = 0;
  scoreMelodyStepStartMs = millis();
  buzzerWriteTone(SCORE_MELODY_NOTES[0]);
}

// 停止蜂鳴器輸出，並把旋律狀態重設回初始值
void stopScoreMelody() {
  buzzerWriteTone(0);
  scoreMelodyActive = false;
  scoreMelodyIndex = 0;
  scoreMelodyStepStartMs = 0;
}

// 以非阻塞方式推進旋律，不用 delay 卡住主循環
void refreshScoreMelody() {
  if (!scoreMelodyActive) {
    return;
  }

  const unsigned long now = millis();
  if (now - scoreMelodyStepStartMs < SCORE_MELODY_DURATIONS_MS[scoreMelodyIndex]) {
    return;
  }

  scoreMelodyIndex++;
  if (scoreMelodyIndex >= SCORE_MELODY_LENGTH) {
    stopScoreMelody();
    return;
  }

  scoreMelodyStepStartMs = now;
  buzzerWriteTone(SCORE_MELODY_NOTES[scoreMelodyIndex]);
}

// 重新洗牌顏色順序並調整避免首色跟上次相同
void shuffleColorOrder() {
  for (size_t i = 0; i < COLOR_COUNT; i++) colorOrder[i] = i;  // 初始化順序索引
  for (size_t i = COLOR_COUNT - 1; i > 0; i--) {
    const size_t j = random(i + 1);  // 隨機挑一個交換位置
    const size_t tmp = colorOrder[i];
    colorOrder[i] = colorOrder[j];   // 交換
    colorOrder[j] = tmp;
  }
  // 避免重洗後第一個顏色跟上一個相同
  if (lastColorIdx < COLOR_COUNT && colorOrder[0] == lastColorIdx && COLOR_COUNT > 1) {
    const size_t tmp = colorOrder[0];
    colorOrder[0] = colorOrder[1];  // 把重複的首色跟第二色對調
    colorOrder[1] = tmp;
  }
  colorOrderPos = 0;  // 重置取用位置
}

// 從洗牌後的顏色序列依序取出下一色；用完就重新洗牌
CRGB nextRandomColor() {
  if (colorOrderPos >= COLOR_COUNT) {
    shuffleColorOrder();  // 序列用完，重新洗牌
  }
  lastColorIdx = colorOrder[colorOrderPos++];  // 取出下一色索引並前進位置
  return COLOR_PALETTE[lastColorIdx];
}

// 啟動達標呼吸黃燈提示
void startTargetAlert() {
  targetAlertActive = true;
    alertBreathBrightness = ALERT_BREATH_MIN;  // 從最暗開始呼吸
  alertBreathGoingUp = true;
  alertBreathLastStepMs = millis();
  FastLED.setBrightness(alertBreathBrightness);
  showSolid(CRGB::Yellow);  // 黃燈代表達標
}

// 顯示 currentDisplayColor（常態底色）
void renderBaseColor() {
  showSolid(currentDisplayColor);
}

// 設定試亮結束時間点（現在 + 持續時間）
void triggerTestLight() {
  testLightEndMs = millis() + TEST_LIGHT_DURATION_MS;  // 設定到期時間
  Serial.println("Test light triggered");
}

// 每圈檢查試亮是否到期，到期就清零 testLightEndMs 讓燈效回覆常態
void refreshTestLight() {
  if (testLightEndMs == 0) {
    return;  // 結束時間未設定、表示沒有在試亮
  }

  const unsigned long now = millis();
  if (now < testLightEndMs) {
    return;  // 還在試亮期間
  }

  testLightEndMs = 0;  // 試亮結束，清零標記
}

// 每圈更新達標呼吸燈亮度（不斷小幅 +1/-1）
void refreshTargetAlert() {
  if (testLightEndMs > millis()) {
    return;  // 試亮優先，暂時停止呼吸燈
  }

  if (!targetAlertActive) {
    return;  // 呼吸燈未啟動
  }

  const unsigned long now = millis();

  if (now - alertBreathLastStepMs >= ALERT_BREATH_STEP_MS) {
    alertBreathLastStepMs = now;  // 更新上次步進時間
    if (alertBreathGoingUp) {
      if (alertBreathBrightness < ALERT_BREATH_MAX) {
        alertBreathBrightness++;  // 變亮
      } else {
        alertBreathGoingUp = false;  // 到頂，改為變暗
      }
    } else {
      if (alertBreathBrightness > ALERT_BREATH_MIN) {
        alertBreathBrightness--;  // 變暗
      } else {
        alertBreathGoingUp = true;  // 到底，改為變亮
      }
    }
    FastLED.setBrightness(alertBreathBrightness);
    FastLED.show();  // 立即更新 LED
  }
}

// HTTP 路由：回傳本機內建網頁
void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

// HTTP 路由：回傳星夜進度牆頁面
void handleStarNight() {
  server.send_P(200, "text/html; charset=utf-8", STAR_NIGHT_HTML);
}

// HTTP 路由：回傳目前 count / target JSON
void handleState() {
  server.send(200, "application/json", stateJson());
}

// HTTP 路由：手動測試蜂鳴器
void handleBeep() {
  stopScoreMelody();

  uint16_t hz = 2200;
  unsigned long durationMs = 400;

  if (server.hasArg("hz")) {
    const long value = server.arg("hz").toInt();
    if (value >= 100 && value <= 8000) {
      hz = static_cast<uint16_t>(value);
    }
  }

  if (server.hasArg("ms")) {
    const long value = server.arg("ms").toInt();
    if (value >= 50 && value <= 3000) {
      durationMs = static_cast<unsigned long>(value);
    }
  }

  buzzerWriteTone(hz);
  delay(durationMs);
  buzzerWriteTone(0);

  server.send(
      200,
      "application/json",
      String("{\"ok\":true,\"hz\":") + String(hz) +
          String(",\"ms\":") + String(durationMs) + String("}"));
}

// HTTP 路由： +1 操作
void handleIncrement() {
  if (isScoringMode) {
    scoringMode_handleIncrement();
  } else {
    countingMode_handleIncrement();
  }
  server.send(200, "application/json", stateJson());
}

// HTTP 路由： -1 操作
void handleDecrement() {
  if (isScoringMode) {
    scoringMode_handleDecrement();
  } else {
    countingMode_handleDecrement();
  }
  server.send(200, "application/json", stateJson());
}

// HTTP 路由： 歸零操作
void handleReset() {
  if (isScoringMode) {
    scoringMode_handleReset();
  } else {
    countingMode_handleReset();
  }
  server.send(200, "application/json", stateJson());
}

// HTTP 路由： 設定目標人數
void handleSetTarget() {
  if (isScoringMode) {
    scoringMode_handleSetTarget();
  } else {
    countingMode_handleSetTarget();
  }
  server.send(200, "application/json", stateJson());
}

// 開機初始化：LED、感測器、Wi-Fi、HTTP 路由
void setup() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);  // 註冊 WS2812 燈拰
  FastLED.setBrightness(180);
  showSolid(CRGB::Black);    // 開機先全部燈烅
  pendingCountSync = false;  // 開機先以遠端狀態為準，避免覆寫遠端計分

  pinMode(SENSOR_PIN, INPUT_PULLUP);  // 啟用內部上拉電阻
  pinMode(BUZZER_PIN, OUTPUT);  // 啟用蜂鳴器訊號腳位
  initBuzzerPwm();
  stopScoreMelody();
  Serial.begin(115200);
  delay(10);
  lastSensorState = digitalRead(SENSOR_PIN);  // 記錄開機時初始感測器狀態

  WiFi.mode(WIFI_STA);  // 站點模式
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  unsigned long wifiStartMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartMs < 15000) {  // 最多等 15 秒
    delay(300);
    Serial.print(".");
  }

  Serial.println();

  // 註冊 HTTP 路由
  server.on("/", handleRoot);
  server.on("/star_night", handleStarNight);
  server.on("/state", handleState);
  server.on("/beep", HTTP_GET, handleBeep);
  server.on("/beep", HTTP_POST, handleBeep);
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
    fetchRemoteState(true);      // 開機先同步遠端狀態
    sendHeartbeat(true, false);  // 再報平安（不主動上傳 count）
  } else {
    Serial.println("Wi-Fi connection failed");
    Serial.println("Please check SSID/password or router signal.");
  }
}

// 主循環：每圈處理本機請求、Wi-Fi、heartbeat、遠端同步、模式循環
void loop() {
  server.handleClient();        // 處理本機網頁連線
  ensureWifiConnected();        // 斷線時自動重連
  fetchRemoteState(false);      // 間隔拉取遠端狀態
  sendHeartbeat(false, false);  // 間隔報平安（預設不帶 count）
  refreshScoreMelody();         // 背景推進得分旋律

  if (isScoringMode) {
    scoringMode_refreshLoop();   // 計分模式每圈刷新
  } else {
    countingMode_refreshLoop();  // 餐會模式每圈刷新
  }
}
