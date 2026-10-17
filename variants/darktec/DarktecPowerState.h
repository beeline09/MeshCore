#pragma once
// Фасад над состоянием питания Darktec (реализация — DarktecBoard.cpp).
// Нужен, чтобы оверлей BatteryPack получал SOC модели без прямой связи
// с объектом платы (роль платы в main() отличается по примерам).

#include <stdint.h>

namespace DarktecPowerState {

// SOC модели (кулонометр + таблица + R-компенсация). До первого тика —
// табличное значение по переданному напряжению пакета (fallback).
uint8_t socPercent(uint16_t fallback_pack_mv);

// Автомат заряда не в Idle (Charging или Full) — «на зарядке».
bool chargeActive();

}  // namespace DarktecPowerState
