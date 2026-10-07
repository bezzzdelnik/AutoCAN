#pragma once

#include <driver/gpio.h>

#ifndef CAN0_TX_GPIO
#define CAN0_TX_GPIO 4
#endif
#ifndef CAN0_RX_GPIO
#define CAN0_RX_GPIO 5
#endif
#ifndef CAN1_TX_GPIO
#define CAN1_TX_GPIO 6
#endif
#ifndef CAN1_RX_GPIO
#define CAN1_RX_GPIO 7
#endif

#ifndef CAN0_BITRATE
#define CAN0_BITRATE 500000
#endif
#ifndef CAN1_BITRATE
#define CAN1_BITRATE 500000
#endif

#ifndef CAN_LISTEN_ONLY
#define CAN_LISTEN_ONLY 1
#endif

// Serial line protocol for the Python logger (do not change casually).
// CAN <device_us> <ch> <id_hex> <ext> <rtr> <dlc> <data_hex_or_->
// ch: 0 = physical CAN0, 1 = physical CAN1, 2 = synthetic heartbeat (not on bus)
#define CAN_LOG_MAGIC "CAN"
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 921600
#endif

#ifndef HB_INTERVAL_MS
#define HB_INTERVAL_MS 2000
#endif
#ifndef HB_LOG_CH
#define HB_LOG_CH 2
#endif
#ifndef HB_CAN_ID
#define HB_CAN_ID 0x7FE
#endif
