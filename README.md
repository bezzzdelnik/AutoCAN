# AutoCAN

Двухканальный логгер CAN для **ESP32-C6** + **2× SN65HVD230** (PlatformIO + [Arduino-ESP32](https://github.com/espressif/arduino-esp32) 3.x) и Python-скрипт записи/воспроизведения лога.

> Официальный `platformio/espressif32@6` тащит Arduino **2.x** без C6. Для C6 используется [pioarduino](https://github.com/pioarduino/platform-espressif32) со стабильным Arduino-ESP32 3.x.

## Состав

| Путь | Назначение |
|------|------------|
| `platformio.ini` | Плата ESP32-C6, пины, bitrate, listen-only |
| `src/main.cpp` | Dual TWAI → Serial |
| `tools/can_logger.py` | Запись CSV с таймштампами |
| `tools/can_playback.py` | Воспроизведение лога с исходными интервалами |
| `docs/wiring.md` | Схема ESP32 ↔ SN65HVD230 ↔ CAN авто |

## Прошивка

```bash
pio run -e esp32-c6
pio run -e esp32-c6 -t upload
pio device monitor -b 921600
```

Формат строки Serial:

```text
CAN <device_us> <ch> <id_hex> <ext> <rtr> <dlc> <data_hex|->
```

Пример:

```text
CAN 1234567 0 000001A0 0 0 8 1122334455667788
```

Строки с `#` — служебные (конфиг/ошибки).

### Настройки в `platformio.ini`

- `CAN0_TX_GPIO` / `CAN0_RX_GPIO`, `CAN1_*` — пины
- `CAN0_BITRATE` / `CAN1_BITRATE` — 125000 / 250000 / 500000 / …
- `CAN_LISTEN_ONLY=1` — только приём (рекомендуется на авто)

Пины по умолчанию: **CAN0** GPIO4/5, **CAN1** GPIO6/7.

## Логгер (Python)

```bash
pip install -r tools/requirements.txt
python tools/can_logger.py -l          # список COM-портов
python tools/can_logger.py -p COM5     # запись в logs/can_*.csv
```

CSV-поля: `host_iso`, `host_mono_s`, `device_us`, `ch`, `id`, `ext`, `rtr`, `dlc`, `data`.

Воспроизведение:

```bash
python tools/can_playback.py logs/can_YYYYMMDD_HHMMSS.csv
python tools/can_playback.py logs/can_....csv -s 2    # 2× быстрее
```

`host_mono_s` — монотонные секунды хоста (точные интервалы для replay).  
`device_us` — микросекунды на ESP32 (относительное время на устройстве).

## Схема

См. [docs/wiring.md](docs/wiring.md). Кратко: каждый SN65HVD230 питается от **3.3 V**, TXD←GPIO TX, RXD→GPIO RX, CANH/CANL — в шину авто; на живой машине терминатор 120 Ω обычно **не** ставить.

## PlatformIO + системный Python

В `.vscode/settings.json` отключён встроенный PIO Core — используется системный Python 3.12 и `pio` из его Scripts.
