# 星燈營 Counter 專案總說明（新手版）

這份文件是整個專案的總入口。
如果你是第一次接觸這個專案，照著下面步驟做就可以跑起來。

## 這個專案在做什麼

這個專案有 2 個部分：

1. `dashboard`：中央網站（給桌長與管理者操作）
2. `src/main.cpp`：ESP32 韌體（接感測器與燈條，並把資料回傳到網站）

你可以先只跑網站，再決定要不要接 ESP32。

## 目錄快速看

- `dashboard/`：Node.js 網站（client + admin）
- `src/main.cpp`：ESP32 主程式
- `platformio.ini`：ESP32 編譯與上傳設定

---

## 路線 A：只想先把網站開起來（建議先做）

### A-1. 先安裝 Node.js

到 Node.js 官網安裝 LTS 版本。

安裝完成後，開 PowerShell 檢查：

```powershell
node -v
npm -v
```

有出現版本號就代表成功。

### A-2. 啟動網站

在專案根目錄執行：

```powershell
cd dashboard
npm install
npm start
```

看到類似以下訊息表示成功：

- Dashboard server running at ...
- Hidden admin route: ...

### A-3. 打開網站

預設網址：

- Client: http://localhost:3000/client
- Admin: http://localhost:3000/admin-J2E13412

若要讓手機、其他電腦或 ESP32 連入，請改用這台主機目前的區網位址：

- Client: http://192.168.31.242:3000/client
- Admin: http://192.168.31.242:3000/admin-J2E13412

注意：admin 是隱藏路徑，建議不要公開。

### A-4. 停止網站

在執行 `npm start` 的終端按 `Ctrl + C`。

---

## 路線 B：要接 ESP32 一起跑

### B-1. 先準備工具

你需要：

- VS Code
- PlatformIO 擴充套件
- ESP32 開發板與 USB 線

### B-2. 設定 ESP32 連線參數

打開 `src/main.cpp`，先確認這幾個常數：

- `WIFI_SSID`：要連的 Wi-Fi 名稱
- `WIFI_PASSWORD`：Wi-Fi 密碼
- `SERVER_BASE_URL`：中央網站位址（目前設定為 `http://192.168.31.242:3000`）
- `TEAM_ID`：桌號（例如 `team-1`）
- `DEVICE_ID`：設備名稱（例如 `esp32-table-1`）

重點：

- 若 ESP32 要連到你電腦上的網站，`SERVER_BASE_URL` 要填「你電腦在區網的 IP」，不能填 `localhost`。
- 你的電腦和 ESP32 需要在同一個網路。

### B-3. 編譯與上傳

用 PlatformIO 執行 Build / Upload。

上傳後開 Serial Monitor（115200），觀察是否連上 Wi-Fi 與伺服器。

### B-4. 驗證是否成功

打開 admin 頁面：

- 本機：http://localhost:3000/admin-J2E13412
- 區網其他裝置：http://192.168.31.242:3000/admin-J2E13412

如果成功，該桌會顯示設備在線，並且人數會同步更新。

---

## 常見問題排除

### 1) `npm start` 失敗（Exit Code 1）

請照順序檢查：

1. 你是不是在 `dashboard` 資料夾內執行？
2. 有沒有先跑過 `npm install`？
3. 3000 埠是否被占用？

如果 3000 埠被占用，可改成 3001：

```powershell
$env:PORT = "3001"
npm start
```

然後改用 http://localhost:3001/client。

### 2) 手機或另一台電腦打不開網站

1. 確認都在同一個 Wi-Fi
2. 改用你電腦的區網 IP 開啟（例：`http://192.168.1.23:3000/client`）
3. 檢查 Windows 防火牆是否擋住 Node.js

### 3) 想改 admin 隱藏網址

在啟動前先設定：

```powershell
$env:ADMIN_PATH_SEGMENT = "admin-自訂路徑"
npm start
```

---

## 專案內部資料

網站狀態會存在：

- `dashboard/data/state.json`

所以網站重啟後，桌次狀態仍可保留。

---

## 一句話快速流程（活動當天）

1. 先開網站：`cd dashboard` -> `npm install`（首次）-> `npm start`
2. 打開 admin：本機用 `http://localhost:3000/admin-J2E13412`，其他裝置用 `http://192.168.31.242:3000/admin-J2E13412`
3. 啟動各桌 ESP32，確認在線
4. 各桌用 client 頁面操作計數
