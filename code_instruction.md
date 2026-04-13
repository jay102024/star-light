# main.cpp 逐行解釋

以下依照 [src/main.cpp](src/main.cpp) 的行號進行說明。

## 1-57 行：引入函式庫、全域變數與常數

```cpp
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
	CRGB::Red,
	CRGB::Blue,
	CRGB::Green,
	CRGB::Purple,
	CRGB::Aqua,
	CRGB::Orange,
	CRGB::Pink,
	CRGB::White,
	CRGB::Cyan,
	CRGB::Magenta,
	CRGB::Gold,
	CRGB::DeepPink,
	CRGB::DarkTurquoise,
	CRGB::LawnGreen,
	CRGB::Coral,
	CRGB::HotPink,
	CRGB::DodgerBlue,
	CRGB::Tomato,
	CRGB::MediumPurple,
	CRGB::Chartreuse,
	CRGB::Turquoise
};

const size_t COLOR_COUNT = sizeof(COLOR_PALETTE) / sizeof(COLOR_PALETTE[0]);

const char INDEX_HTML[] PROGMEM = R"rawliteral(
```

- 第 1 行：引入 Arduino 核心 API。
- 第 2 行：引入 Wi-Fi 功能（ESP32 SoftAP 會用到）。
- 第 3 行：引入內建 HTTP 伺服器類別。
- 第 4 行：引入 FastLED 函式庫以控制 WS2812。
- 第 5 行：空行，分隔 include 與常數區。
- 第 6 行：定義感測器腳位為 GPIO 1。
- 第 7 行：定義 LED 資料腳位為 GPIO 3。
- 第 8 行：定義 LED 顆數為 24 顆。
- 第 9 行：空行。
- 第 10 行：註解，提醒呼吸燈與亮度調整位置。
- 第 11 行：建立 `leds` 陣列，儲存每顆 LED 的顏色。
- 第 12 行：空行。
- 第 13 行：開啟匿名命名空間，限制符號只在本檔案可見。
- 第 14 行：設定 AP SSID 名稱。
- 第 15 行：設定 AP 密碼。
- 第 16 行：空行。
- 第 17 行：建立 HTTP 伺服器，監聽 80 埠。
- 第 18 行：計數器變數，`volatile` 表示可能被不同執行脈絡讀寫。
- 第 19 行：目標人數，初始為 0。
- 第 20 行：記錄上一輪的感測器狀態，初始高電位。
- 第 21 行：空行。
- 第 22 行：是否啟動達標提醒效果。
- 第 23 行：呼吸燈目前亮度值。
- 第 24 行：呼吸方向（變亮中或變暗中）。
- 第 25 行：上次更新呼吸燈的時間戳。
- 第 26 行：空行。
- 第 27 行：每次亮度更新間隔 20ms。
- 第 28 行：呼吸燈最小亮度。
- 第 29 行：呼吸燈最大亮度。
- 第 30 行：空行。
- 第 31 行：宣告顏色調色盤陣列開始。
- 第 32 行：第 1 個顏色，紅色。
- 第 33 行：第 2 個顏色，藍色。
- 第 34 行：第 3 個顏色，綠色。
- 第 35 行：第 4 個顏色，紫色。
- 第 36 行：第 5 個顏色，青綠。
- 第 37 行：第 6 個顏色，橘色。
- 第 38 行：第 7 個顏色，粉紅。
- 第 39 行：第 8 個顏色，白色。
- 第 40 行：第 9 個顏色，青色。
- 第 41 行：第 10 個顏色，洋紅。
- 第 42 行：第 11 個顏色，金色。
- 第 43 行：第 12 個顏色，深粉紅。
- 第 44 行：第 13 個顏色，深綠松石。
- 第 45 行：第 14 個顏色，草綠。
- 第 46 行：第 15 個顏色，珊瑚。
- 第 47 行：第 16 個顏色，亮粉。
- 第 48 行：第 17 個顏色，道奇藍。
- 第 49 行：第 18 個顏色，番茄紅。
- 第 50 行：第 19 個顏色，中紫。
- 第 51 行：第 20 個顏色，黃綠。
- 第 52 行：第 21 個顏色，綠松石。
- 第 53 行：結束調色盤陣列。
- 第 54 行：空行。
- 第 55 行：計算調色盤元素數量。
- 第 56 行：空行。
- 第 57 行：開始宣告 HTML 頁面字串常數（存於程式記憶體）。

## 58-272 行：內嵌網頁（HTML/CSS/JavaScript）

```html
<!DOCTYPE html>
<html lang="zh-Hant">
<head>
	<meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<title>ESP32 Counter</title>
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
		<h1>先設定今天的目標人數</h1>
		<p class="subtitle">輸入你這次要統計的人數目標，送出後就會進入計數畫面。</p>
		<div class="target-row">
			<input id="welcomeTargetInput" type="number" min="0" step="1" placeholder="例如：30">
			<button id="startButton" class="button" type="button">開始</button>
		</div>
	</section>

	<main id="appScreen" class="card hidden">
		<h1>目前計數</h1>
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
		<p class="hint">頁面每 500ms 會自動更新一次。</p>
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
```

- 第 58 行：HTML 文件型別宣告。
- 第 59 行：`html` 標籤開始，語系為繁體中文。
- 第 60 行：`head` 開始。
- 第 61 行：設定 UTF-8 編碼。
- 第 62 行：設定行動裝置 viewport。
- 第 63 行：頁面標題。
- 第 64 行：CSS 樣式區塊開始。
- 第 65 行：`:root` 規則開始。
- 第 66 行：宣告使用深色配色。
- 第 67 行：設定全域字型。
- 第 68 行：`:root` 規則結束。
- 第 69 行：空行。
- 第 70 行：`body` 樣式開始。
- 第 71 行：移除預設外距。
- 第 72 行：最小高度滿版。
- 第 73 行：使用 grid 排版。
- 第 74 行：內容水平垂直置中。
- 第 75 行：設定背景漸層。
- 第 76 行：設定文字顏色。
- 第 77 行：`body` 樣式結束。
- 第 78 行：空行。
- 第 79 行：卡片容器樣式開始。
- 第 80 行：設定卡片寬度。
- 第 81 行：設定內距。
- 第 82 行：設定圓角。
- 第 83 行：設定半透明背景。
- 第 84 行：設定陰影。
- 第 85 行：文字置中。
- 第 86 行：套用毛玻璃效果。
- 第 87 行：`.card` 樣式結束。
- 第 88 行：空行。
- 第 89 行：`.hidden` 樣式開始。
- 第 90 行：隱藏元素。
- 第 91 行：`.hidden` 結束。
- 第 92 行：空行。
- 第 93 行：歡迎區塊寬度樣式開始。
- 第 94 行：設定歡迎卡片最大寬度。
- 第 95 行：樣式結束。
- 第 96 行：空行。
- 第 97 行：`h1` 樣式開始。
- 第 98 行：標題外距。
- 第 99 行：標題字體大小。
- 第 100 行：`h1` 樣式結束。
- 第 101 行：空行。
- 第 102 行：副標樣式開始。
- 第 103 行：副標外距。
- 第 104 行：副標顏色。
- 第 105 行：副標行高。
- 第 106 行：副標樣式結束。
- 第 107 行：空行。
- 第 108 行：計數數字樣式開始。
- 第 109 行：外距設定。
- 第 110 行：大字體顯示。
- 第 111 行：粗體。
- 第 112 行：數字顏色。
- 第 113 行：樣式結束。
- 第 114 行：空行。
- 第 115 行：目標顯示樣式開始。
- 第 116 行：上方間距。
- 第 117 行：文字顏色。
- 第 118 行：字級設定。
- 第 119 行：樣式結束。
- 第 120 行：空行。
- 第 121 行：按鈕列樣式開始。
- 第 122 行：使用 grid。
- 第 123 行：三欄布局。
- 第 124 行：欄間距。
- 第 125 行：上外距。
- 第 126 行：樣式結束。
- 第 127 行：空行。
- 第 128 行：目標輸入列樣式開始。
- 第 129 行：使用 grid。
- 第 130 行：輸入框 + 按鈕欄位比例。
- 第 131 行：間距。
- 第 132 行：上外距。
- 第 133 行：樣式結束。
- 第 134 行：空行。
- 第 135 行：數字輸入框樣式開始。
- 第 136 行：寬度 100%。
- 第 137 行：包含邊框計算尺寸。
- 第 138 行：邊框樣式。
- 第 139 行：圓角。
- 第 140 行：背景色。
- 第 141 行：文字色。
- 第 142 行：內距。
- 第 143 行：字級。
- 第 144 行：樣式結束。
- 第 145 行：空行。
- 第 146 行：按鈕樣式開始。
- 第 147 行：取消預設邊框。
- 第 148 行：按鈕圓角。
- 第 149 行：內距。
- 第 150 行：字級。
- 第 151 行：字重。
- 第 152 行：字色。
- 第 153 行：按鈕背景漸層。
- 第 154 行：滑鼠游標改為手指。
- 第 155 行：過渡動畫設定。
- 第 156 行：按鈕樣式結束。
- 第 157 行：空行。
- 第 158 行：hover 狀態樣式開始。
- 第 159 行：懸停時微微上移。
- 第 160 行：懸停時透明度微調。
- 第 161 行：hover 樣式結束。
- 第 162 行：空行。
- 第 163 行：active 狀態樣式開始。
- 第 164 行：按下時回到原位。
- 第 165 行：active 樣式結束。
- 第 166 行：空行。
- 第 167 行：提示文字樣式開始。
- 第 168 行：外距。
- 第 169 行：文字顏色。
- 第 170 行：行高。
- 第 171 行：樣式結束。
- 第 172 行：`style` 區塊結束。
- 第 173 行：`head` 結束。
- 第 174 行：`body` 開始。
- 第 175 行：歡迎畫面區塊開始。
- 第 176 行：歡迎畫面標題。
- 第 177 行：歡迎畫面說明文字。
- 第 178 行：目標輸入列容器開始。
- 第 179 行：歡迎畫面的目標輸入框。
- 第 180 行：開始按鈕。
- 第 181 行：目標輸入列容器結束。
- 第 182 行：歡迎畫面區塊結束。
- 第 183 行：空行。
- 第 184 行：主要 app 畫面開始（預設隱藏）。
- 第 185 行：主畫面標題。
- 第 186 行：顯示目前計數值的區塊。
- 第 187 行：顯示目標值的區塊。
- 第 188 行：操作按鈕容器開始。
- 第 189 行：+1 按鈕。
- 第 190 行：-1 按鈕。
- 第 191 行：歸零按鈕。
- 第 192 行：操作按鈕容器結束。
- 第 193 行：目標設定列開始。
- 第 194 行：主畫面的目標輸入框。
- 第 195 行：設定目標按鈕。
- 第 196 行：目標設定列結束。
- 第 197 行：更新頻率提示文字。
- 第 198 行：主畫面結束。
- 第 199 行：空行。
- 第 200 行：JavaScript 區塊開始。
- 第 201 行：是否已進入主畫面的旗標。
- 第 202 行：輪詢計時器 ID，初始為空。
- 第 203 行：空行。
- 第 204 行：宣告 `updateUi`，用 API 回傳資料更新畫面。
- 第 205 行：更新畫面上的計數值。
- 第 206 行：更新目標顯示文字。
- 第 207 行：同步主畫面輸入框的目標值。
- 第 208 行：若尚未進入主畫面，才更新歡迎畫面輸入框。
- 第 209 行：歡迎畫面輸入框顯示 target（0 則顯示空字串）。
- 第 210 行：`if` 區塊結束。
- 第 211 行：`updateUi` 函式結束。
- 第 212 行：空行。
- 第 213 行：宣告 `fetchState`，從 `/state` 取得狀態。
- 第 214 行：`try` 區塊開始。
- 第 215 行：送出 GET 請求。
- 第 216 行：解析 JSON。
- 第 217 行：用回傳資料刷新 UI。
- 第 218 行：錯誤處理開始。
- 第 219 行：將錯誤輸出到瀏覽器 console。
- 第 220 行：錯誤處理結束。
- 第 221 行：`fetchState` 結束。
- 第 222 行：空行。
- 第 223 行：宣告 `enterApp`，切到主畫面。
- 第 224 行：標記已進入主畫面。
- 第 225 行：隱藏歡迎畫面。
- 第 226 行：顯示主畫面。
- 第 227 行：空行。
- 第 228 行：若尚未啟動輪詢才啟動。
- 第 229 行：每 500ms 執行一次 `fetchState`。
- 第 230 行：`if` 結束。
- 第 231 行：`enterApp` 結束。
- 第 232 行：空行。
- 第 233 行：宣告 `confirmTargetAndEnter`。
- 第 234 行：抓取歡迎畫面的輸入框元素。
- 第 235 行：讀取輸入值，空值時用 `0`。
- 第 236 行：POST 到 `/set-target` 並帶 `target` 參數。
- 第 237 行：請求完成後切換到主畫面。
- 第 238 行：函式結束。
- 第 239 行：空行。
- 第 240 行：宣告通用 POST 函式 `postAction`。
- 第 241 行：`try` 開始。
- 第 242 行：用 `fetch` 發送請求（可帶 body）。
- 第 243 行：HTTP 方法設為 POST。
- 第 244 行：有 body 才加上 `Content-Type`。
- 第 245 行：把 body 放進請求。
- 第 246 行：`fetch` 參數物件結束。
- 第 247 行：解析回應 JSON。
- 第 248 行：更新 UI。
- 第 249 行：錯誤處理開始。
- 第 250 行：錯誤輸出到 console。
- 第 251 行：錯誤處理結束。
- 第 252 行：`postAction` 結束。
- 第 253 行：空行。
- 第 254 行：綁定開始按鈕點擊事件。
- 第 255 行：綁定歡迎輸入框 keydown 事件。
- 第 256 行：判斷是否按下 Enter。
- 第 257 行：按 Enter 時觸發開始流程。
- 第 258 行：`if` 結束。
- 第 259 行：keydown 事件處理結束。
- 第 260 行：綁定 +1 按鈕事件。
- 第 261 行：綁定 -1 按鈕事件。
- 第 262 行：綁定歸零按鈕事件。
- 第 263 行：綁定設定目標按鈕事件。
- 第 264 行：讀取主畫面目標輸入值。
- 第 265 行：POST 設定目標。
- 第 266 行：事件函式結束。
- 第 267 行：空行。
- 第 268 行：頁面載入時先抓一次最新狀態。
- 第 269 行：`script` 結束。
- 第 270 行：`body` 結束。
- 第 271 行：`html` 結束。
- 第 272 行：C++ Raw String 結束。

## 273-444 行：C++ 功能函式、HTTP 路由、setup/loop

```cpp
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
		alertBreathBrightness = ALERT_BREATH_MIN;
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
```

- 第 273 行：結束匿名命名空間。
- 第 274 行：空行。
- 第 275 行：宣告 `stateJson()`。
- 第 276 行：組合目前 `count` 與 `target` 的 JSON 字串。
- 第 277 行：函式結束。
- 第 278 行：空行。
- 第 279 行：宣告 `showSolid()`。
- 第 280 行：將所有 LED 填滿同一個顏色。
- 第 281 行：把顏色資料送出到 LED 燈條。
- 第 282 行：函式結束。
- 第 283 行：空行。
- 第 284 行：宣告 `colorForCounter()`，依計數值選顏色。
- 第 285 行：若值為 0，進入判斷。
- 第 286 行：回傳黑色（關燈）。
- 第 287 行：`if` 結束。
- 第 288 行：空行。
- 第 289 行：用取餘數循環索引到調色盤。
- 第 290 行：回傳對應顏色。
- 第 291 行：函式結束。
- 第 292 行：空行。
- 第 293 行：宣告 `startTargetAlert()`，啟動達標提醒。
- 第 294 行：提醒狀態開啟。
- 第 295 行：呼吸亮度設為最小值。
- 第 296 行：先設定為往上變亮。
- 第 297 行：記錄開始時間。
- 第 298 行：套用目前亮度。
- 第 299 行：先顯示黃色作為提醒基底色。
- 第 300 行：函式結束。
- 第 301 行：空行。
- 第 302 行：宣告 `renderBaseColor()`。
- 第 303 行：依目前計數值重畫 LED 顏色。
- 第 304 行：函式結束。
- 第 305 行：空行。
- 第 306 行：宣告 `hasReachedTarget()`。
- 第 307 行：判斷目標有效且計數已達或超過目標。
- 第 308 行：函式結束。
- 第 309 行：空行。
- 第 310 行：宣告 `refreshTargetAlert()`，更新呼吸效果。
- 第 311 行：若未啟用提醒就直接返回。
- 第 312 行：提前返回。
- 第 313 行：`if` 結束。
- 第 314 行：空行。
- 第 315 行：讀取目前時間。
- 第 316 行：空行。
- 第 317 行：到達更新間隔才進行亮度變化。
- 第 318 行：更新上次刷新時間。
- 第 319 行：若目前是往上變亮。
- 第 320 行：若還沒到最大亮度。
- 第 321 行：亮度 +1。
- 第 322 行：否則（到頂）。
- 第 323 行：改成往下變暗。
- 第 324 行：內層 `if` 結束。
- 第 325 行：`if (alertBreathGoingUp)` 的 `else`。
- 第 326 行：若還高於最小亮度。
- 第 327 行：亮度 -1。
- 第 328 行：否則（到底）。
- 第 329 行：改成往上變亮。
- 第 330 行：內層 `if` 結束。
- 第 331 行：方向判斷結束。
- 第 332 行：把新亮度寫入 FastLED。
- 第 333 行：立即更新燈條。
- 第 334 行：時間判斷 `if` 結束。
- 第 335 行：函式結束。
- 第 336 行：空行。
- 第 337 行：宣告 `applyCounterChange()`，集中處理計數變動。
- 第 338 行：保存變動前計數。
- 第 339 行：寫入新計數。
- 第 340 行：空行。
- 第 341 行：若是「首次剛好達標」則進入提醒流程。
- 第 342 行：啟動達標提醒。
- 第 343 行：否則走一般流程。
- 第 344 行：若目前不等於目標值。
- 第 345 行：關閉提醒狀態。
- 第 346 行：恢復一般亮度 80。
- 第 347 行：內層 `if` 結束。
- 第 348 行：重新渲染一般顏色。
- 第 349 行：外層 `if/else` 結束。
- 第 350 行：空行。
- 第 351 行：序列埠輸出變動原因。
- 第 352 行：序列埠輸出固定字串。
- 第 353 行：序列埠輸出目前計數並換行。
- 第 354 行：函式結束。
- 第 355 行：空行。
- 第 356 行：宣告 `handleRoot()`。
- 第 357 行：回傳首頁 HTML（從 PROGMEM 送出）。
- 第 358 行：函式結束。
- 第 359 行：空行。
- 第 360 行：宣告 `handleState()`。
- 第 361 行：回傳 JSON 狀態。
- 第 362 行：函式結束。
- 第 363 行：空行。
- 第 364 行：宣告 `handleIncrement()`。
- 第 365 行：只有未達標時才允許 +1。
- 第 366 行：套用 +1，原因標記為手動。
- 第 367 行：`if` 結束。
- 第 368 行：回傳最新 JSON。
- 第 369 行：函式結束。
- 第 370 行：空行。
- 第 371 行：宣告 `handleDecrement()`。
- 第 372 行：只有大於 0 才允許 -1。
- 第 373 行：套用 -1，原因標記為手動。
- 第 374 行：`if` 結束。
- 第 375 行：回傳最新 JSON。
- 第 376 行：函式結束。
- 第 377 行：空行。
- 第 378 行：宣告 `handleReset()`。
- 第 379 行：計數歸零。
- 第 380 行：回傳最新 JSON。
- 第 381 行：函式結束。
- 第 382 行：空行。
- 第 383 行：宣告 `handleSetTarget()`。
- 第 384 行：若請求帶有 `target` 參數。
- 第 385 行：將字串目標轉成整數。
- 第 386 行：若為負值則修正為 0，並存入 `targetCount`。
- 第 387 行：`if (hasArg)` 結束。
- 第 388 行：空行。
- 第 389 行：若目前計數剛好等於新目標且目標有效。
- 第 390 行：立即啟動達標提醒。
- 第 391 行：否則走一般狀態。
- 第 392 行：關閉提醒。
- 第 393 行：恢復一般亮度。
- 第 394 行：重畫一般顏色。
- 第 395 行：`if/else` 結束。
- 第 396 行：空行。
- 第 397 行：序列埠輸出「目標已設定」。
- 第 398 行：序列埠輸出目標值。
- 第 399 行：回傳最新 JSON。
- 第 400 行：函式結束。
- 第 401 行：空行。
- 第 402 行：`setup()` 開始，啟動時執行一次。
- 第 403 行：初始化 WS2812 燈條型號、腳位、顆數。
- 第 404 行：設定初始亮度 150。
- 第 405 行：先全黑（關燈）。
- 第 406 行：空行。
- 第 407 行：感測器腳位設為上拉輸入。
- 第 408 行：啟動序列埠，鮑率 115200。
- 第 409 行：短暫延遲，等待硬體穩定。
- 第 410 行：讀取目前感測器狀態作為初值。
- 第 411 行：空行。
- 第 412 行：啟動 SoftAP。
- 第 413 行：空行。
- 第 414 行：註冊首頁路由。
- 第 415 行：註冊狀態查詢路由。
- 第 416 行：註冊 `increment` POST 路由。
- 第 417 行：註冊 `decrement` POST 路由。
- 第 418 行：註冊 `reset` POST 路由。
- 第 419 行：註冊 `set-target` POST 路由。
- 第 420 行：啟動伺服器。
- 第 421 行：空行。
- 第 422 行：序列埠印空行。
- 第 423 行：輸出 AP 啟動訊息。
- 第 424 行：輸出 SSID 提示字。
- 第 425 行：輸出 SSID 內容。
- 第 426 行：輸出密碼提示字。
- 第 427 行：輸出密碼內容。
- 第 428 行：輸出網址前綴。
- 第 429 行：輸出 AP IP 位址。
- 第 430 行：`setup()` 結束。
- 第 431 行：空行。
- 第 432 行：`loop()` 開始，主迴圈反覆執行。
- 第 433 行：處理 HTTP 用戶端請求。
- 第 434 行：更新達標呼吸燈效果。
- 第 435 行：空行。
- 第 436 行：讀取目前感測器狀態。
- 第 437 行：空行。
- 第 438 行：偵測高到低沿（遮斷觸發）且未達標時。
- 第 439 行：感測器觸發計數 +1。
- 第 440 行：延遲 50ms 做簡單防彈跳。
- 第 441 行：`if` 結束。
- 第 442 行：空行。
- 第 443 行：更新上一輪感測器狀態。
- 第 444 行：`loop()` 結束。

## 補充說明

- 這份程式把前端頁面直接內嵌在 C++ 字串中，ESP32 當 AP 與 Web Server，手機連上 AP 就能操作。
- 計數來源有兩種：網頁按鈕手動 +1/-1，以及 KY-010 感測器觸發自動 +1。
- 到達目標時會啟動黃光呼吸提醒；離開目標值時會回到一般顏色顯示。
