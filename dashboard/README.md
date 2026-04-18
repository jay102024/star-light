# 星燈營 Dashboard

這是活動現場用的「中央計數網站」。

- `client` 頁面：給各桌長使用（加 1、減 1）。
- `admin` 頁面：給主控台使用（看全部桌次狀態、設定目標人數）。

---

## 1) 先準備好電腦環境

你只需要安裝 **Node.js**（建議 LTS 版本）。

安裝後，打開 PowerShell 輸入：

```powershell
node -v
npm -v
```

有看到版本號（例如 `v20.x.x`）就代表可以繼續。

---

## 2) 第一次啟動（照抄就好）

在專案根目錄執行：

```powershell
cd dashboard
npm install
npm start
```

看到這類訊息代表成功：

- `Dashboard server running at ...`
- `Hidden admin route: ...`

---

## 3) 網址怎麼打

伺服器預設使用 `3000` 連接埠。

- Client：`http://localhost:3000/client`
- Admin：`http://localhost:3000/admin-J2E13412`

如果是手機、其他電腦或 ESP32 要連入，改用這台主機目前的區網位址：

- Client：`http://192.168.31.242:3000/client`
- Admin：`http://192.168.31.242:3000/admin-J2E13412`

注意：`admin` 是隱藏路徑，請不要公開貼在群組。

---

## 4) 如何停止伺服器

在正在跑 `npm start` 的那個終端機按：

```text
Ctrl + C
```

---

## 5) 常見問題（新手最常遇到）

### Q1. `npm start` 失敗（Exit Code 1）

請依序檢查：

1. 你有先 `cd dashboard` 嗎？
2. 你有先跑過 `npm install` 嗎？
3. `3000` 埠是否被其他程式占用？

如果是埠被占用，可以改成 3001：

```powershell
$env:PORT = "3001"
npm start
```

然後用 `http://localhost:3001/client` 開網站。

### Q2. 我想改 admin 隱藏網址

啟動前先設定環境變數：

```powershell
$env:ADMIN_PATH_SEGMENT = "admin-你的密碼路徑"
npm start
```

例如設成 `admin-safe-2026`，那 admin 網址就會變：

`http://localhost:3000/admin-safe-2026`

若是從同網路其他裝置連入，則改成：

`http://192.168.31.242:3000/admin-safe-2026`

### Q3. 手機或其他電腦打不開

請確認：

1. 目標裝置和你的筆電在同一個 Wi-Fi。
2. 用你的筆電區網 IP 開啟（例如 `http://192.168.1.23:3000/client`）。
3. Windows 防火牆沒有擋住 Node.js。

---

## 6) 資料存在哪裡

網站狀態會存到：

`dashboard/data/state.json`

所以就算你重開伺服器，資料也不會立刻消失。

---

## 7) 給 ESP32 的串接方式（進階）

如果要讓 admin 看見裝置在線，ESP32 需要定期送 heartbeat：

- API：`POST /api/devices/heartbeat`
- Body 範例：

```json
{
  "teamId": "team-3",
  "deviceId": "esp32-table-3",
  "count": 8
}
```

只要固定回報，admin 畫面就會更新裝置在線狀態與人數。