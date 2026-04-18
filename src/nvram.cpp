#include "nvram.h"
#include <Preferences.h>

// 手動指定要燒錄的組別 (1-20)。設定後會固定用這組，未設定時自動生成
// 例如改成 5 就會燒成 team-5 / esp32-table-5
// 0 表示自動生成（推薦首次開發用）
#define FIXED_TABLE_NUMBER 10

// 強制清空舊 NVS 資料並重新寫入（覆蓋舊身份）
// 設定為 1 時，每次開機都會清空舊的 teamId/deviceId 然後重新寫入
// 用完記得改回 0，否則每次重啟都會重寫（浪費 flash）
#define FORCE_RESET_NVRAM 0

// 全域身份變數
String teamId;
String deviceId;

// 初始化 NVS，載入或自動生成裝置身份
void initNvram() {
  Preferences prefs;
  prefs.begin("identity", false);

  #if FORCE_RESET_NVRAM
    // 強制清空舊資料
    Serial.println("FORCE_RESET_NVRAM enabled - clearing old identity...");
    prefs.clear();
    teamId = "";
    deviceId = "";
  #else
    teamId = prefs.getString("teamId", "");
    deviceId = prefs.getString("deviceId", "");
  #endif

  int tableNumber = 0;
  
  #if FIXED_TABLE_NUMBER > 0 && FIXED_TABLE_NUMBER <= 20
    // 編譯時指定了固定組別
    tableNumber = FIXED_TABLE_NUMBER;
    Serial.print("Using fixed table number: ");
    Serial.println(tableNumber);
  #else
    // 自動生成：基於 MAC 地址
    const uint64_t mac = ESP.getEfuseMac();
    const uint32_t macLow = static_cast<uint32_t>(mac & 0xFFFFFFFFULL);
    tableNumber = static_cast<int>(macLow % 20UL) + 1;  // 1..20
    Serial.print("Auto-generated table number from MAC: ");
    Serial.println(tableNumber);
  #endif

  if (deviceId.length() == 0) {
    deviceId = String("esp32-table-") + String(tableNumber);
    prefs.putString("deviceId", deviceId);
    Serial.print("Set Device ID: ");
    Serial.println(deviceId);
  }

  if (teamId.length() == 0) {
    teamId = String("team-") + String(tableNumber);
    prefs.putString("teamId", teamId);
    Serial.print("Set Team ID: ");
    Serial.println(teamId);
  }

  prefs.end();

  Serial.print("Loaded Team ID: ");
  Serial.println(teamId);
  Serial.print("Loaded Device ID: ");
  Serial.println(deviceId);
}

// 重置身份（清空 NVS 中的身份資料，下次開機時重新生成）
void resetNvramIdentity() {
  Preferences prefs;
  prefs.begin("identity", false);
  prefs.remove("teamId");
  prefs.remove("deviceId");
  prefs.end();
  
  Serial.println("NVRAM identity reset. Restart device to regenerate.");
  
  teamId = "";
  deviceId = "";
}
