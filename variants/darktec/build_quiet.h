#pragma once

/*
 * Подключается через PLATFORMIO_BUILD_FLAGS -include при release/matrix/ondemand.
 *
 * PlatformIO/SCons переносит все -U* в конец argv gcc. Пара -UBATTERY… -DBATTERY…
 * в итоге оставляет макрос UNDEFINED (побеждает последний -U) — химия/защита
 * откатываются на #ifndef-дефолты в battery_chemistry.h.
 *
 * Поэтому батарею/радио/имя задаём через #undef/#define в -include (после
 * командных -D), а здесь только глушим отладочные флаги из platformio.ini.
 */

#ifdef MESH_DEBUG
#undef MESH_DEBUG
#endif

#ifdef BLE_DEBUG_LOGGING
#undef BLE_DEBUG_LOGGING
#endif
