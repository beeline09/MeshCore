# Задачи — Спека 0001, фаза 1

Формат: `[ ]` не начата, `[~]` в работе, `[x]` готово. Ссылки на FR/AC/D —
в spec.md. Правило: правки ядра = стоп и согласование (FR-30).

## Этап 0 — Таблицы SOC
- [x] T0.1 `tools/gen_darktec_soc_tables.py`: CSV → заголовочные таблицы (монотонность, шаг ≤ 25 мВ)
- [x] T0.2 Таблицы Li-ion из замеров; LiFePO4/LTO approximation из §8
- [x] T0.3 Native-тест контрольных точек (AC-9)

## Этап 1 — DarktecPrefs
- [x] T1.1 (решение: DarktecPrefs::begin() сам зовёт идемпотентный InternalFS.begin(), хук в DarktecBoard::begin) Найти и зафиксировать точку вызова после готовности InternalFS (D-7)
- [x] T1.2 `DarktecPrefs.h/.cpp`: формат DRTC + версия + CRC16 + атомарная запись
- [x] T1.3 Ленивая инициализация, ошибка ФС → дефолты без записи
- [x] T1.4 Поля: capacity/chem/cells/module/charge_ma (FR-14, FR-31)

## Этап 2 — SOC-модель
- [x] T2.1 `DarktecSoc.h`: чистая функция SOC (таблица + R-comp D-1)
- [x] T2.2 Кулонометр: интегрирование, покой D-2, SYSTEMOFF D-4, температура FR-24
- [ ] T2.3 Интеграция с текущим опросом батареи (без INA → табличный режим)
- [ ] T2.4 Переключить экран/значок на новый SOC, телеметрия — формат прежний
- [x] T2.5 Native-тесты SOC (AC-7/8/9)

## Этап 3 — Автомат заряда
- [x] T3.1 `DarktecChargeState`: idle/charging/full, гистерезис, EMA (FR-34)
- [ ] T3.2 Хуки: молния, «не гасить экран», направление кулонометра
- [x] T3.3 Native-тесты переходов (AC-13)

## Этап 4 — Экран настроек юга
- [~] T4.1 Разведка ЗАВЕРШЕНА: настройки = страничное меню внутри HomeScreen
      (ui-new/UITask.cpp, `enum class SettingsPage` на строке ~225: ROOT/CHAT/DISP/
      COMMS/SYSTEM/CODECS + рендер и handleInput в том же классе, файл ~2200 строк).
      Хук-оверлей невозможен без копирования всего класса — владелец разрешил
      правку ядра. План врезки: новый пункт(ы) в SettingsPage::ROOT-рендер и
      handleInput; значения из DarktecPrefs::get()/save (уже готовы).
      ПРОДОЛЖЕНИЕ В СЛЕДУЮЩЕЙ СЕССИИ со свежим контекстом.
- [x] T4.2 Выполнено: страница SettingsPage::DARKTEC_PAGE в ui-new/UITask.cpp
      (страж `#if defined(DARKTEC) && __has_include("DarktecPrefs.h")` →
      UI_DARKTEC_SETTINGS; в чужих вариантах код выпадает целиком).
      Пункт «[ Darktec ]» в ROOT (выход сдвинулся на 5-ю позицию, счёт 5→6);
      страница: Назад + «Ёмкость, mAh» (±100, 500–30000, wrap на краях),
      «Тип АКБ» (Li-ion/LiFePO4/LTO), «Ячеек» (1S/2S, 2S только для LTO —
      иначе заметка «2S: только LTO» и сброс при смене химии на не-LTO),
      «Радиомодуль» (HT-RA62/E22-30S/E22P-868/E22-33S), «Ток заряда, mA»
      (±100, 100–3000). Чтение/запись DarktecPrefs::get()/save (sanitize
      дублирует валидацию). Значения — из файла, первое — из флагов сборки.
- [x] T4.3 Подсказка "after reboot" (4 с, заметка на строке; D-8/FR-9..11);
      предупреждения "cells: 1 (2S=LTO)" / "2S: LTO only" (FR-10). Правка по
      замечанию владельца: строки пунктов — ТОЛЬКО английский (plain, без
      _TR), как весь остальной UI прошивки — русские варианты не добавлять.
      Автоперерисовка по таймеру заметки (аналог _pin_show_until). Сборки:
      Darktec_companion_radio_ble SUCCESS, Heltec_t114_companion_radio_ble
      SUCCESS (страж не задел чужие платы), native-тесты darktec 22/22
      зелёные; финальная версия с англ. строками перепрошита на устройство.

## Этап 5 — Флаги сборки и CI
- [x] T5.1: BATTERY_CAPACITY_MAH, DARKTEC_CHARGE_CURRENT_MA (battery_chemistry.h) + DARKTEC_RADIO_MODULE (enum 0..3, DarktecBoard.h; E22P: SX126X_RXEN не задаётся, EN P0.17 вручную HIGH из DarktecBoard::begin(); дефолт/RA-62/33S — прежняя разводка). Обе сборки (дефолт и -DDARKTEC_RADIO_MODULE=2) SUCCESS
- [x] T5.2 Все #error-валидации на месте: химия/защита/ячейки×химия/пакет≤5 В
      и ёмкость/ток (battery_chemistry.h, добавлены ранее) + новый диапазон
      DARKTEC_RADIO_MODULE 0..3 (DarktecBoard.h)
- [x] T5.3 On-demand цепь принимает новые флаги, CI-дефолты не тронуты:
      сайт (ondemand.js: валидаторы/slug/issue-блок + needsCustomBuild;
      index.html: «Радиомодуль», «Ёмкость ячейки, mAh», «Ток заряда, mA»;
      app.js: state/привязки/валидации) → workflow build-darktec-ondemand.yml
      (inputs + parse num() + env) → scripts/build-darktec-ondemand.sh
      (токены имени __cap/__mod/__chg + ADC_MULTIPLIER → DEFS_H -include).
      Имена сайт↔скрипт сверены на дефолте и не-дефолтах (совпадают байт в
      байт). Попутно закрыт старый пробел: adc_multiplier сайт слал, а
      workflow/скрипт игнорировали — теперь ADC_MULTIPLIER доезжает до
      DEFS_H и в имя файла. Стоковые сборки и их имена не изменены.

## Этап 6 — Сайт
- [x] T6.1 Поле «Ёмкость ячейки, mAh» (500–30000, шаг 100) → флаг
      BATTERY_CAPACITY_MAH + ключ кэша (__capN, только при не-дефолте)
- [x] T6.2 Селектор «Радиомодуль» (HT-RA62/E22-900M30S/E22P-868M30S/E22-900M33S)
      → DARKTEC_RADIO_MODULE + ключ кэша (__modN)
- [x] T6.3 Поле «Ток заряда, mA» (100–3000, шаг 100) + подсказка про перепайку
      делителя (текст из FR-5) → DARKTEC_CHARGE_CURRENT_MA + ключ кэша (__chgN)
- [x] T6.4 Клиентская валидация «Ячеек» уже была (CHEMS[].cells: 2S только LTO);
      новые: validateBatteryCapacity/validateChargeCurrent (диапазон + кратность
      100) в refreshOndemandFromCache и onBuildClick; needsCustomBuild() учитывает
      все три новых параметра. localStorage не добавлял — остальное на сайте
      тоже живёт в state сессии (консистентность с radio/adc).

## Этап 7 — Приёмка
- [x] T7.1 Юг: собрана и прошита 17.10.2026 (pio run -t upload,
      Darktec_companion_radio_ble). Владелец проверил на устройстве
      («вроде все работает»): меню [ Darktec ], правка значений,
      сохранение — AC-1/3/4/7/11.
- [~] T7.2 Зарядный цикл (AC-13): автомат проверен юнит-тестами; полный
      цикл на железе наблюдаем по ходу эксплуатации (молния/100% теперь
      по автомату, TPS54302 автотерминации не имеет).
- [ ] T7.3 On-demand сборки с сайта → AC-2 — после пуша сайта/workflow
      (первая сборка с новыми полями).
- [~] T7.4 Перепрошивки юг↔официалка → AC-5/6: файл /darktec.prefs
      переживает перепрошивку по построению (FR-16/17); полная проверка
      на фазе 1b (экран официалки).
- [x] T7.5 CI зелёные, дифф ядра → AC-10/11: Build Darktec Firmwares
      (south_edition, 19fb1d43) SUCCESS 13m25s 17.10.2026 — стоковые сборки
      для сайта пересобрались, имена не изменились (дефолты). Дифф ядра юга:
      только UITask.cpp (согласовано), остальное — variants/darktec, тесты,
      tools, env-native. Форк: main ef8debe9 (workflow), south_edition
      b4eccd46 (скрипт); сайт 380554c → Pages.
- [ ] T7.6 Отчёт владельцу: таблица AC → результат

## Прогресс (17 окт, сессия ZCode)

Готово: Этапы 0, 1, ядро 2–3 (DarktecSoc.h, DarktecSocModel.h, DarktecPrefs*,
DarktecPowerState.h, оверлей variants/darktec/BatteryPack.cpp, интеграция в
DarktecBoard::pollChargeSense), флаги capacity/charge_ma. 184/184 native-тестов
зелёные; сборка Darktec_companion_radio_ble SUCCESS.

ОСТАЛОСЬ (17 окт, доп.): Этап 4 (пункты настроек — владелец разрешил правку кода ядра юга, если хук не выйдет), Этап 6 (сайт), Этап 7 (приёмка). Прочее: (проверить вызов pollChargeSense при погашенном экране —
UITask зовёт isExternalPowered/getBattMilliVolts в отрисовке; при сна экрана
модель стоит, dt-кламп 1 ч ограничивает), T3.2 (молния уже через
isExternalPowered→автомат — проверить хвосты), Этап 4 целиком (экран настроек),
T5.1 DARKTEC_RADIO_MODULE, Этап 6 (сайт), Этап 7 (приёмка на железе).
Коммитов НЕТ — по указанию владельца всё коммитится после завершения.
