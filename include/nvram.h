#ifndef NVRAM_H
#define NVRAM_H

#include <Arduino.h>

// 全域身份變數（由 nvram.cpp 管理）
extern String teamId;
extern String deviceId;

// ========== 使用說明 ==========
// 在 nvram.cpp 最上面修改常數：
//
// 1. FORCE_RESET_NVRAM：強制清空舊資料
//    #define FORCE_RESET_NVRAM 0  → 保留舊資料（推薦）
//    #define FORCE_RESET_NVRAM 1  → 強制清空並重新寫入（用來覆蓋舊身份）
//    用完記得改回 0，否則每次開機都會重寫浪費 flash
//
// 2. FIXED_TABLE_NUMBER：指定燒錄的組別
//    #define FIXED_TABLE_NUMBER 0  → 自動生成（推薦新板子首次用）
//    #define FIXED_TABLE_NUMBER 5  → 強制燒 team-5 / esp32-table-5
//    #define FIXED_TABLE_NUMBER 20 → 強制燒 team-20 / esp32-table-20
//
// 例如要覆蓋舊的 team-17：
//   1. 改 FIXED_TABLE_NUMBER 為你想要的號碼（如 3）
//   2. 改 FORCE_RESET_NVRAM 為 1
//   3. 編譯燒錄
//   4. 燒成功後改 FORCE_RESET_NVRAM 回 0
//   5. 重新編譯燒錄（可選，確保下次開機不會重複清空）

// 初始化 NVS，載入或自動生成裝置身份
void initNvram();

// 重置身份（清空 NVS 中的身份資料，下次開機時重新生成）
void resetNvramIdentity();

#endif
