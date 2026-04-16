# Counter 專案程式分段說明

---

## src/main.cpp（ESP32 韌體）

- L1-L76：引入函式庫、硬體腳位、Wi-Fi 與伺服器常數、全域狀態變數、燈色調色盤與狀態控制參數。
- L77-L294：內建控制網頁 `INDEX_HTML`（設定目標、顯示 count/target、按鈕操作、輪詢 `/state`）。
- L295-L355：函式前向宣告與基礎工具（像 `stateJson`、Wi-Fi 重連、簡易 JSON 數值解析）。
- L356-L458：計分模式（scoring mode）完整流程。
  - 遠端同步套用。
  - 本機 +1/-1/歸零/設目標。
  - 感測器觸發 +1。
  - 計分模式燈效（平時關燈，試亮白燈，+1 彩虹圈）。
- L459-L597：圓滿餐會模式（counting mode）完整流程。
  - 依 target 判斷達標。
  - 達標呼吸黃燈。
  - 感測器在達標後不再增加。
- L598-L658：`fetchRemoteState`，定期向後端抓 mode/count/target/testLightSeq，並做防回退保護。
- L659-L844：`sendHeartbeat`、LED 顯示工具、彩虹圈、顏色洗牌、試亮與呼吸燈刷新、ESP32 本地 HTTP 路由處理。
- L845-L894：`setup`，初始化 LED/感測器/Wi-Fi/路由，並在開機時做首次同步。
- L895-L906：`loop`，主循環執行（處理 web、維持連線、心跳、遠端同步、模式刷新）。

### main.cpp 變數用途總表

- `SENSOR_PIN`：光遮斷感測器接的 GPIO。
- `LED_PIN`：LED 資料腳位。
- `NUM_LEDS`：燈珠總數。
- `leds`：FastLED 實際輸出緩衝陣列。
- `WIFI_SSID`：Wi-Fi 名稱。
- `WIFI_PASSWORD`：Wi-Fi 密碼。
- `SERVER_BASE_URL`：中央後端 API 基底位址。
- `TEAM_ID`：這台 ESP32 所屬桌號 ID。
- `DEVICE_ID`：這台 ESP32 的裝置名稱。
- `server`：ESP32 本地 HTTP 伺服器。
- `counter`：目前計數值。
- `targetCount`：目標值。
- `lastSensorState`：上一輪感測器狀態，用來抓 HIGH->LOW 邊緣。
- `lastHeartbeatMs`：上次送 heartbeat 的時間戳。
- `lastWifiRetryMs`：上次重試 Wi-Fi 的時間戳。
- `lastRemoteSyncMs`：上次抓遠端狀態的時間戳。
- `testLightEndMs`：試亮結束時間。
- `lastTestLightSeq`：最後一次已處理的 test-light 序號。
- `pendingCountSync`：本地計數尚未被後端確認的旗標。
- `isScoringMode`：是否在計分模式。
- `hasModeSync`：是否曾同步過模式資訊。
- `WIFI_RETRY_INTERVAL_MS`：Wi-Fi 重連間隔。
- `HEARTBEAT_INTERVAL_MS`：heartbeat 送出間隔。
- `REMOTE_SYNC_INTERVAL_MS`：遠端狀態輪詢間隔。
- `TEST_LIGHT_DURATION_MS`：試亮持續毫秒數。
- `targetAlertActive`：達標呼吸燈是否啟動。
- `alertBreathBrightness`：呼吸燈目前亮度。
- `alertBreathGoingUp`：呼吸燈亮度遞增/遞減方向。
- `alertBreathLastStepMs`：上次呼吸亮度更新時間。
- `ALERT_BREATH_STEP_MS`：呼吸亮度更新步進間隔。
- `ALERT_BREATH_MIN`：呼吸最小亮度。
- `ALERT_BREATH_MAX`：呼吸最大亮度。
- `COLOR_PALETTE`：可抽樣顯示的顏色池。
- `COLOR_COUNT`：顏色池大小。
- `colorOrder`：打散後顏色索引順序。
- `colorOrderPos`：目前取用到打散序列的第幾個。
- `lastColorIdx`：上一個顏色索引，避免洗牌後首色重複。
- `currentDisplayColor`：目前底色。
- `INDEX_HTML`：燒在韌體裡的本地控制頁。

### main.cpp 常見區域變數用途

- `now`：目前 `millis()`，統一用於節流與逾時判斷。
- `sensorState`：本輪讀到的感測器值。
- `previous`：變更前計數值，用來判斷是否剛好 +1 或剛好達標。
- `newValue`：本次欲套用的新計數值。
- `newCount`：遠端同步進來的 count。
- `newTarget`：遠端同步進來的 target。
- `banquetTargetReached`：餐會模式本次是否觸發達標邊界。
- `scoringIncrement`：計分模式本次是否發生遞增。
- `http`：HTTPClient 實例。
- `url`：本次 API 請求網址。
- `statusCode`：HTTP 回應碼。
- `body`：HTTP 回應字串。
- `syncedCount`：從回應解析出的 count。
- `syncedTarget`：從回應解析出的 target。
- `syncedTestLightSeq`：從回應解析出的 test-light 序號。
- `mergedCount`：融合本地與遠端後最終採用的 count。
- `shouldIncludeCount`：這次 heartbeat 是否要帶 count。
- `payload`：heartbeat 送出的 JSON 本體。
- `wifiStartMs`：開機連線起始時間，用於 15 秒逾時。
- `pattern`：JSON 關鍵欄位搜尋字串。
- `start`：欄位起始位置。
- `valueStart`：數值起始索引。
- `valueEnd`：數值結束索引。
- `baseHue`、`i`、`hue`：彩虹圈動畫運算變數。
- `j`、`tmp`：洗牌交換時的索引與暫存。

### INDEX_HTML 內 JavaScript 變數用途

- `hasEnteredApp`：是否已從歡迎畫面進入主控畫面。
- `pollingId`：輪詢 `/state` 的計時器 ID。
- `state`：更新 UI 用的狀態資料。
- `response`：fetch 回應物件。
- `data`：回應 JSON 內容。
- `input`：目標輸入框 DOM。
- `target`：要提交的 target 字串。
- `path`：要呼叫的 API 路徑。
- `body`：POST 內容。
- `error`：例外錯誤物件。
- `event`：鍵盤事件物件（Enter 送出）。

---

## dashboard/server.js（Node.js 後端）

- L1-L19：引入套件、環境變數、資料檔路徑與 timeout 常數。
- L20-L40：Express 基礎設定與靜態檔路由（`/`、`/client`、`/leaderboard`、隱藏 admin 路徑）。
- L42-L51：`/api/bootstrap`，提供前端初始資料。
- L53-L76：管理模式切換與每桌目標設定 API。
- L78-L152：每桌 count/score 更新、單桌歸零、試亮觸發、單桌 state 查詢 API。
- L154-L179：全桌歸零與隊長端 presence API。
- L181-L210：ESP32 heartbeat API 與健康檢查 API。
- L212-L221：Socket.IO 連線後推送 state。
- L223-L231：每 5 秒廣播一次 state、啟動 `boot`。
- L233-L249：`createDefaultState`，建立預設桌次資料。
- L251-L266：`loadState`，載入檔案或建立初始狀態。
- L268-L343：`normalizeState`、`findTeam`、`serializeTeam(s)`、`normalizeNumber` 等資料處理工具。
- L345-L360：`publishState` 與 `schedulePersist`（廣播 + 防抖寫檔）。
- L362-L365：`persistState`（實際寫入 `data/state.json`）。

---

## dashboard/public/admin.html（管理頁骨架）

- L1-L8：HTML head（meta、title、CSS）。
- L9-L29：模式選擇遮罩（計分模式 / 圓滿餐會）。
- L30-L42：排行榜遮罩。
- L44-L69：主頁內容（Hero、Summary、Team Grid、按鈕）。
- L71-L72：載入 Socket.IO 與 `assets/admin.js`。

---

## dashboard/public/assets/admin.js（管理頁邏輯）

- L1-L37：DOM 綁定、全域狀態、按鈕事件註冊、啟動 `bootstrap()`。
- L38-L57：`bootstrap`，拉初始資料、接上 Socket.IO 即時更新。
- L58-L66：`setMode`，切換活動模式 API。
- L68-L86：模式/排行榜 overlay 顯示與關閉。
- L88-L111：`render`，依模式更新文案並驅動畫面。
- L114-L155：`renderSummary`，渲染上方統計卡片。
- L157-L171：`renderTeamGrid`，分流到 scoring 或 banquet 卡片。
- L173-L232：`renderScoringGrid`（計分版卡片，含加減/歸零/設定分數）。
- L234-L278：`attachScoreFormListeners`（含 `/score` 失敗時回退 `/count` 的相容邏輯）。
- L280-L354：`renderBanquetGrid`（餐會版卡片，含目標設定、試亮、加減、歸零）。
- L356-L376：`attachResetListeners`（單桌歸零確認）。
- L378-L396：`attachDeltaListeners`（加減 count/score）。
- L398-L416：編輯狀態與 draft 輔助函式。
- L418-L438：`resetAllCountsAndReload`（全桌歸零）。
- L440-L447：`escapeHtml`（防 XSS）。
- L449-L466：排序工具（固定桌序與排行榜排序）。

---

## dashboard/public/client.html（隊長端骨架）

- L1-L8：HTML head。
- L10-L17：Hero 說明區。
- L19-L26：桌次選擇區（Team Picker）。
- L28-L51：計數器區（count/target、狀態文案、+1/-1、切換桌次）。
- L53-L54：載入 Socket.IO 與 `assets/client.js`。

---

## dashboard/public/assets/client.js（隊長端邏輯）

- L1-L20：DOM、localStorage key、全域狀態、啟動 `bootstrap()`。
- L21-L37：`bootstrap`，取初始 teams、恢復已選桌、訂閱 Socket.IO。
- L39-L48：按鈕事件（+1/-1、切換桌次）。
- L50-L95：`render`，桌次清單與計數面板切換、狀態文案更新。
- L97-L107：`updateCount`，送出加減請求。
- L109-L117：`sendPresence`，回報隊長頁在線。
- L119-L130：`startPresencePing` / `stopPresencePing`（每 15 秒 ping）。
- L132-L139：`escapeHtml`（防 XSS）。

---

## dashboard/public/leaderboard.html（排行榜骨架）

- L1-L8：HTML head。
- L10-L18：排行榜標題與內容容器。
- L20-L24：載入 Socket.IO 與 `assets/leaderboard.js`。

---

## dashboard/public/assets/leaderboard.js（排行榜邏輯）

- L1-L9：DOM 綁定與初始狀態。
- L10-L23：`bootstrap`，初始化 + Socket.IO 更新。
- L25-L37：`render`，依 mode 分流顯示。
- L39-L57：`renderScoring`，計分模式排序與前三名樣式。
- L59-L82：`renderBanquet`，餐會模式進度列與達標排序。
- L84-L87：`getTeamOrder`，解析桌號排序。
- L89-L96：`escapeHtml`。

---

## dashboard/public/assets/styles.css（樣式）

- L1-L26：全域主題變數、基本 box model、body 背景。
- L27-L114：共用排版容器（shell、hero、summary、標題字體與文案）。
- L115-L214：面板、卡片、按鈕、狀態 pill、表單與計數器基本樣式。
- L215-L352：Admin 專用格線與卡片排版（多欄桌卡、操作列、輸入樣式）。
- L353-L426：模式選擇 overlay 樣式。
- L427-L447：Hero 右上按鈕區（切換模式/排行榜）樣式。
- L448-L529：管理頁內排行榜 overlay 樣式。
- L530-L555：計分卡片補強與中尺寸 RWD。
- L556-L595：手機版 RWD（欄位收斂、按鈕滿版、單欄）。
- L596-L734：獨立 `/leaderboard` 全頁視覺（排名列、前三名高亮、進度條）。
- L735-L744：排行榜手機尺寸微調。

---

## 檔案之間的資料流（快速對照）

- 後端資料中心：`dashboard/server.js`。
- 三個前端頁面：都先打 `/api/bootstrap`，再吃 Socket.IO `state` 即時更新。
- ESP32：
  - 定期打 `/api/devices/heartbeat`。
  - 定期抓 `/api/teams/:teamId/state`。
  - 本地感測器和按鈕變更會回寫到後端，並套用燈效邏輯。
