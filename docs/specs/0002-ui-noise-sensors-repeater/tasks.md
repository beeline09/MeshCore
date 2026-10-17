# 0002 — Задачи

Формат: [ ] не начато, [~] в работе, [x] готово.

## Этап 1 — Юг-клиент + сайт

- [x] T1.1 Prefs: keep_display_chg (байт 0 резерва) + noise_speed_idx
      (байт 1) в Settings/serialize/parse/sanitize/buildFlagDefaults
      (флаг DARKTEC_KEEP_DISPLAY_CHG), native-тесты на новые байты.
- [x] T1.2 DarktecDisplay: гэйт turnOff/isOn по зарядке + keep_display_chg.
- [x] T1.3 Юг UI: пункт «Screen on chg» (On/Off) на странице Darktec.
- [x] T1.4 Юг UI: страница NOISE — график + подписи + инверсия + долгое
      (NF↔SNR) + тройной (скорость) + удержание подсветки.
- [x] T1.5 Юг UI: страница INA — таблица DC+Bat без SP (V/A/P, рамки,
      точки карусели видны, строки покрупнее) + ETA (треугольник, FR-14).
- [x] T1.6 Сайт: чекбокс «Держать экран всегда включенным при зарядке» →
      флаг/issue/токен __kdc0/needsCustomBuild.
- [x] T1.7 Workflow main + оба скрипта: keep_display_chg → DEFS_H/токен;
      сверка имён сайт↔скрипты.
- [x] T1.8 Сборки (Darktec + Heltec-страж), native-тесты, прошивка ноды.
      ГОТОВО 17.10: native 26/26; Darktec_companion_radio_ble SUCCESS;
      Heltec_t114 SUCCESS (страж); имена сайт↔скрипты сверены (с __kdc0 и
      без); workflow main 403b703d (keep_display_chg); нода прошита
      (юг, -DDARKTEC_RADIO_MODULE=3, чистая сборка). Ждёт проверки
      владельцем на устройстве (экраны Noise/INA, Screen on chg).

## Этап 2 — Юг-репитер

- [x] T2.1 Оверлей variants/darktec/repeater_ui/UITask.cpp + env'ы
      (подмена UITask, DISPLAY_CLASS=DarktecDisplay).
- [x] T2.2 Каркас карусели 5 страниц + страница «Радио» (перенос).
- [x] T2.3 Страница «Соседи»: трекер + сортировка + долгое (запрос) +
      тройной (очистить и заново).
- [x] T2.4 Страницы «Шум/SNR» и «INA+ETA» (общие хелперы DarktecScreens.h;
      в таблице репитера SP/DC/Bat, точки скрыты).
- [x] T2.5 Страница «Настройки»: [ Advert ] + [ Darktec ] + [ Exit ].
- [x] T2.6 Сборка Darktec_repeater(+bridge) SUCCESS; приёмка — на железе
      репитера владельца (прошивка по готовности платы).
      ИТОГИ ЭТАПА 2 (17.10): правка ядра — только доступ к соседям
      (simple_repeater/MyMesh.h/.cpp: getNeighboursSorted/clearNeighbours,
      страж DARKTEC); оверлей repeater_ui/UITask.cpp реализует класс UITask
      1:1 (заголовок не форкался). Решения по ходу: долгое на странице
      «Радио» — ВЫКЛЮЧЕНИЕ (как было), на остальных — действия страницы;
      настройки = страница карусели «[ Settings ]» (долгое — открыть),
      внутри папка [ Advert ] / [ Darktec ] (полный редактор из 0001) /
      [ Back ]; сортировка соседей по SNR, hex-имя (имена в NeighbourInfo
      отсутствуют); env'ы репитера переведены на DarktecDisplay; libdeps
      репитера чинились копией из companion (сеть рвала git-клон RadioLib).

## Этап 3 — Официалка-клиент

- [x] T3.1 companion_ui: страницы NOISE/INA + пункт Screen on chg
      (файлы варианта общие — DarktecPrefs/DarktecDisplay/DarktecScreens
      копируются идентично).
- [x] T3.2 Сборки ble/usb (dev), прошивка ноды, проверка общего файла.

## Этап 4 — Официалка-репитер

- [x] T4.1 Оверлей repeater_ui в dev-darktec (сверить MyMesh репитера).
- [x] T4.2 Сборка Darktec_repeater (dev), приёмка.
      ИТОГИ ЭТАПОВ 3-4 (17.10): Этап 3 принят владельцем после 2 правок
      (клиентская INA-таблица без SP с видимыми точками; папка настроек
      y=14/шаг 7 — 7 строк без скролла; рендер таблицы в официалке заменён
      на южную посимвольно). Этап 4: тот же оверлей repeater_ui скопирован
      в dev (MyMesh репитера официалки уже нёс соседей/discovery — добавлены
      только getNeighboursSorted/clearNeighbours под стражем DARKTEC),
      сборки Darktec_repeater/bridge/companion SUCCESS; нода прошита
      официальным репитером (флаг модуля 3) на приёмку.
      ПОСЛЕ ПРИЁМКИ: вернуть ноду на южную клиента, пуш всего 0002.

## Прогресс

- 17.10.2026: спека рев.3 СОГЛАСОВАНА владельцем. План — на согласовании. Код не начинался.
