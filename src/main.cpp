/**
 * AutoCAN — ESP32-C6 dual TWAI reader via 2× SN65HVD230
 * Framework: Arduino-ESP32 3.x (https://github.com/espressif/arduino-esp32)
 *
 * Serial line format (ASCII, one frame per line):
 *   CAN <device_us> <ch> <id_hex> <ext> <rtr> <dlc> <data_hex|->
 *
 * ch=0 → physical CAN0, ch=1 → physical CAN1,
 * ch=2 → synthetic heartbeat (Serial only, listen-only bus не трогаем).
 */

#include <Arduino.h>
#include <driver/twai.h>
#include <esp_timer.h>

#include "can_config.h"

static twai_handle_t g_twai[2] = {nullptr, nullptr};

static twai_timing_config_t timing_for_bitrate(uint32_t bitrate) {
  switch (bitrate) {
    case 1000000:
      return TWAI_TIMING_CONFIG_1MBITS();
    case 800000:
      return TWAI_TIMING_CONFIG_800KBITS();
    case 500000:
      return TWAI_TIMING_CONFIG_500KBITS();
    case 250000:
      return TWAI_TIMING_CONFIG_250KBITS();
    case 125000:
      return TWAI_TIMING_CONFIG_125KBITS();
    case 100000:
      return TWAI_TIMING_CONFIG_100KBITS();
    case 50000:
      return TWAI_TIMING_CONFIG_50KBITS();
    case 25000:
      return TWAI_TIMING_CONFIG_25KBITS();
    default:
      Serial.printf("# WARN unknown bitrate %lu, fallback 500k\n",
                    (unsigned long)bitrate);
      return TWAI_TIMING_CONFIG_500KBITS();
  }
}

static bool install_channel(uint8_t ch, gpio_num_t tx, gpio_num_t rx,
                            uint32_t bitrate) {
  const twai_mode_t mode =
      CAN_LISTEN_ONLY ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL;

  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, mode);
  g_config.controller_id = ch;
  g_config.rx_queue_len = 32;
  g_config.tx_queue_len = 8;

  twai_timing_config_t t_config = timing_for_bitrate(bitrate);
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t err =
      twai_driver_install_v2(&g_config, &t_config, &f_config, &g_twai[ch]);
  if (err != ESP_OK) {
    Serial.printf("# ERR ch%u install: %s\n", ch, esp_err_to_name(err));
    return false;
  }

  err = twai_start_v2(g_twai[ch]);
  if (err != ESP_OK) {
    Serial.printf("# ERR ch%u start: %s\n", ch, esp_err_to_name(err));
    twai_driver_uninstall_v2(g_twai[ch]);
    g_twai[ch] = nullptr;
    return false;
  }

  Serial.printf("# OK ch%u TX=GPIO%u RX=GPIO%u bitrate=%lu mode=%s\n", ch,
                (unsigned)tx, (unsigned)rx, (unsigned long)bitrate,
                CAN_LISTEN_ONLY ? "listen-only" : "normal");
  return true;
}

static void emit_frame(uint8_t ch, const twai_message_t &msg) {
  const uint64_t us = (uint64_t)esp_timer_get_time();

  Serial.printf("%s %llu %u %08lX %u %u %u ", CAN_LOG_MAGIC,
                (unsigned long long)us, ch, (unsigned long)msg.identifier,
                msg.extd ? 1u : 0u, msg.rtr ? 1u : 0u, msg.data_length_code);

  if (msg.rtr || msg.data_length_code == 0) {
    Serial.print("-");
  } else {
    for (int i = 0; i < msg.data_length_code; ++i) {
      Serial.printf("%02X", msg.data[i]);
    }
  }
  Serial.print('\n');
}

static void poll_channel(uint8_t ch) {
  if (!g_twai[ch]) {
    return;
  }

  twai_message_t msg;
  while (twai_receive_v2(g_twai[ch], &msg, 0) == ESP_OK) {
    emit_frame(ch, msg);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);  // HWCDC → native USB COM6 (USB-Serial/JTAG)
  // Ждём открытия монитора хостом (после reset USB перечисляется заново)
  const uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 3000) {
    delay(10);
  }
  delay(200);

  Serial.println();
  Serial.println("# AutoCAN ESP32-C6 dual TWAI logger (Arduino-ESP32)");
  Serial.println("# USB: native Serial/JTAG (COM6)");
  Serial.println("# format: CAN <device_us> <ch> <id> <ext> <rtr> <dlc> <data>");
  Serial.println("# ch0=CAN0 ch1=CAN1 ch2=heartbeat(synth id=0x7FE)");
  Serial.printf("# build %s %s\n", __DATE__, __TIME__);
  Serial.flush();

  const bool ok0 = install_channel(0, (gpio_num_t)CAN0_TX_GPIO,
                                   (gpio_num_t)CAN0_RX_GPIO, CAN0_BITRATE);
  const bool ok1 = install_channel(1, (gpio_num_t)CAN1_TX_GPIO,
                                   (gpio_num_t)CAN1_RX_GPIO, CAN1_BITRATE);

  if (!ok0 && !ok1) {
    Serial.println("# FATAL: no CAN channels started");
  } else {
    Serial.println("# READY");
  }
  Serial.flush();
}

static void emit_heartbeat() {
  // Только в лог Serial — в listen-only на шину не передаём.
  const uint64_t us = (uint64_t)esp_timer_get_time();
  const uint32_t ms = millis();
  const uint8_t alive0 = g_twai[0] ? 1u : 0u;
  const uint8_t alive1 = g_twai[1] ? 1u : 0u;

  // data[0..3]=uptime_ms BE, data[4]=ch0_ok, data[5]=ch1_ok, data[6..7]=0
  Serial.printf("%s %llu %u %08lX 0 0 8 %08lX%02X%02X0000\n", CAN_LOG_MAGIC,
                (unsigned long long)us, (unsigned)HB_LOG_CH,
                (unsigned long)HB_CAN_ID, (unsigned long)ms, alive0, alive1);
  Serial.flush();
}

void loop() {
  poll_channel(0);  // CAN0 → строки с ch=0
  poll_channel(1);  // CAN1 → строки с ch=1

  static uint32_t last_hb_ms = 0;
  const uint32_t now = millis();
  if (now - last_hb_ms >= HB_INTERVAL_MS) {
    last_hb_ms = now;
    emit_heartbeat();
  }
}
