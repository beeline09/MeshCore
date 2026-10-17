#pragma once
// Чистый формат файла настроек Darktec `/darktec.prefs` (spec 0001, FR-14..19).
// Без Arduino — подключается и в native-тестах. Структура NodePrefs ядра
// не затрагивается; ФС — та же InternalFS (LittleFS), что у ядра.

#include <stddef.h>
#include <stdint.h>

// Макросы BATTERY_CHEMISTRY/BATTERY_CELLS (дефолты из флагов сборки) —
// самодостаточный заголовок варианта (только макросы, годится и для native).
#include "battery_chemistry.h"

namespace DarktecPrefsFormat {

// Коды химии — совпадают с DarktecSoc::Chem (одна нумерация на вариант).
enum ChemCode : uint8_t { CHEM_LIION = 0, CHEM_LIFEPO4 = 1, CHEM_LTO = 2 };

// Коды радиомодулей (FR-2).
enum ModuleCode : uint8_t {
  MOD_HT_RA62 = 0,
  MOD_E22_900M30S = 1,
  MOD_E22P_868M30S = 2,
  MOD_E22_900M33S = 3,
};

struct Settings {
  uint16_t capacity_mah;  // ёмкость ЯЧЕЙКИ, mAh (FR-1)
  uint8_t chem;           // ChemCode (FR-9)
  uint8_t cells;          // 1..2; 2 — только LTO (FR-10)
  uint8_t module;         // ModuleCode (FR-2, FR-11)
  uint16_t charge_ma;     // номинал зарядного тока платы (FR-31)
};

constexpr uint16_t kCapacityMin = 500;
constexpr uint16_t kCapacityMax = 30000;
constexpr uint16_t kCapacityDefault = 4000;
constexpr uint16_t kChargeMin = 100;
constexpr uint16_t kChargeMax = 3000;
constexpr uint16_t kChargeDefault = 2000;

constexpr uint8_t kWireVersion = 1;
constexpr uint8_t kWireSize = 20;  // magic(4) + ver(1) + payload(13) + crc(2)
constexpr uint8_t kPayloadSize = 13;

// Починка недопустимых значений: диапазоны и правило «2S — только LTO».
// (объявлена до buildFlagDefaults, определение ниже по файлу)
inline void sanitize(Settings& s);

// Дефолт — из флагов сборки (FR-1/2/3, FR-31); макры химии/модуля приходят
// из battery_chemistry.h / platformio.ini. В native-тестах макров нет —
// берутся константы по умолчанию.
inline Settings buildFlagDefaults() {
  Settings s{};
#if defined(BATTERY_CAPACITY_MAH)
  s.capacity_mah = (uint16_t)BATTERY_CAPACITY_MAH;
#else
  s.capacity_mah = kCapacityDefault;
#endif
#if defined(BATTERY_CHEMISTRY)
  s.chem = (uint8_t)((int)BATTERY_CHEMISTRY - 1);  // BATTERY_CHEM_LIION=1 → 0
#else
  s.chem = CHEM_LIION;
#endif
#if defined(BATTERY_CELLS)
  s.cells = (uint8_t)BATTERY_CELLS;
#else
  s.cells = 1;
#endif
#if defined(DARKTEC_RADIO_MODULE)
  s.module = (uint8_t)DARKTEC_RADIO_MODULE;
#else
  s.module = MOD_E22_900M30S;
#endif
#if defined(DARKTEC_CHARGE_CURRENT_MA)
  s.charge_ma = (uint16_t)DARKTEC_CHARGE_CURRENT_MA;
#else
  s.charge_ma = kChargeDefault;
#endif
  sanitize(s);
  return s;
}

// Починка недопустимых значений: диапазоны и правило «2S — только LTO».
inline void sanitize(Settings& s) {
  if (s.capacity_mah < kCapacityMin || s.capacity_mah > kCapacityMax) {
    s.capacity_mah = kCapacityDefault;
  }
  if (s.charge_ma < kChargeMin || s.charge_ma > kChargeMax) {
    s.charge_ma = kChargeDefault;
  }
  if (s.chem > CHEM_LTO) s.chem = CHEM_LIION;
  if (s.module > MOD_E22_900M33S) s.module = MOD_E22_900M30S;
  if (s.cells < 1) s.cells = 1;
  if (s.cells > 2) s.cells = 2;
  if (s.chem != CHEM_LTO && s.cells > 1) s.cells = 1;  // US-6/FR-10
}

// CRC16/CCITT-FALSE (poly 0x1021, init 0xFFFF).
inline uint16_t crc16(const uint8_t* data, uint32_t len) {
  uint16_t crc = 0xFFFF;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

// Сериализация: "DRTC" + version + payload(capacity_le16, chem, cells,
// module, charge_le16, reserved 6×0x00) + crc16(version+payload). Возвращает
// kWireSize или 0, если буфер мал.
inline size_t serialize(const Settings& s, uint8_t* buf, size_t len) {
  if (len < kWireSize) return 0;
  Settings t = s;
  sanitize(t);
  buf[0] = 'D'; buf[1] = 'R'; buf[2] = 'T'; buf[3] = 'C';
  buf[4] = kWireVersion;
  buf[5] = (uint8_t)(t.capacity_mah & 0xFF);
  buf[6] = (uint8_t)(t.capacity_mah >> 8);
  buf[7] = t.chem;
  buf[8] = t.cells;
  buf[9] = t.module;
  buf[10] = (uint8_t)(t.charge_ma & 0xFF);
  buf[11] = (uint8_t)(t.charge_ma >> 8);
  for (int i = 0; i < 6; i++) buf[12 + i] = 0x00;  // резерв (FR-17)
  const uint16_t crc = crc16(buf + 4, 1 + kPayloadSize);
  buf[18] = (uint8_t)(crc & 0xFF);
  buf[19] = (uint8_t)(crc >> 8);
  return kWireSize;
}

// Обратное преобразование: false при битой магии/версии/CRC. Успешный
// результат проходит sanitize (старшие версии могут нести незнакомый резерв).
inline bool parse(const uint8_t* buf, size_t len, Settings& out) {
  if (len < kWireSize) return false;
  if (buf[0] != 'D' || buf[1] != 'R' || buf[2] != 'T' || buf[3] != 'C') return false;
  if (buf[4] != kWireVersion) return false;
  const uint16_t crc_stored = (uint16_t)(buf[18] | ((uint16_t)buf[19] << 8));
  if (crc16(buf + 4, 1 + kPayloadSize) != crc_stored) return false;
  Settings s{};
  s.capacity_mah = (uint16_t)(buf[5] | ((uint16_t)buf[6] << 8));
  s.chem = buf[7];
  s.cells = buf[8];
  s.module = buf[9];
  s.charge_ma = (uint16_t)(buf[10] | ((uint16_t)buf[11] << 8));
  sanitize(s);
  out = s;
  return true;
}

}  // namespace DarktecPrefsFormat
