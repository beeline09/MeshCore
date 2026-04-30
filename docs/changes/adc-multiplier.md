# Runtime-множитель АЦП для калибровки напряжения батареи

## Проблема

Значение напряжения батареи рассчитывается как `analogReadMilliVolts() * ADC_MULTIPLIER`.
Множитель зависит от делителя напряжения на конкретной плате и ранее задавался только через
`#define ADC_MULTIPLIER` в заголовочном файле платы — без возможности изменить его без
перепрошивки.

## Решение

Добавлена runtime-настройка множителя через интерфейс `MainBoard`:

```cpp
virtual bool setAdcMultiplier(float multiplier) { return false; }
virtual float getAdcMultiplier() const { return 0.0f; }
```

Каждая поддерживаемая плата:
- возвращает текущий множитель через `getAdcMultiplier()`
- принимает новое значение через `setAdcMultiplier()` и применяет его немедленно
- использует compile-time дефолт из `ADC_MULTIPLIER` в `platformio.ini`

Платы без поддержки батареи возвращают `0.0f`.

## Применение

### Compile-time дефолт (platformio.ini)

```ini
build_flags =
  -D ADC_MULTIPLIER=5.42
```

### Runtime-переопределение

Значение можно изменить через CLI без перепрошивки:

```
set adc_multiplier 5.7
```

## Значения по умолчанию

| Плата | `ADC_MULTIPLIER` |
|-------|-----------------|
| Heltec V3 | 5.42 |
| Heltec V4 | 5.42 |
| Heltec E213 | 5.42 |
| Heltec E290 | 5.42 |
| Heltec T190 | 5.42 |
| HT-CT62 | 6.52 |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/helpers/ESP32Board.h` | Базовая реализация `setAdcMultiplier()` / `getAdcMultiplier()` |
| `variants/heltec_v3/HeltecV3Board.h/.cpp` | Переход на `getAdcMultiplier()` в `getBattMilliVolts()` |
| `variants/heltec_v4/HeltecV4Board.h/.cpp` | Аналогично |
| `variants/heltec_e213/HeltecE213Board.h/.cpp` | Аналогично |
| `variants/heltec_e290/HeltecE290Board.h/.cpp` | Аналогично |
| `variants/heltec_t190/HeltecT190Board.h/.cpp` | Аналогично |
| `variants/heltec_ct62/HT-CT62Board.h/.cpp` | Аналогично, дефолт 6.52 |
