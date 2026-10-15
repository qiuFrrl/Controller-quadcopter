#pragma once

#include <stdint.h>

// Isi pin sesuai wiring glove. Jika memungkinkan hindari pin strapping ESP32-C3:
// GPIO 2, GPIO 8, dan GPIO 9 dapat mempengaruhi mode boot saat diberi level eksternal.
static const int BTN_ARM_PIN = 1;
static const int BTN_EMERGENCY_PIN = 4;
static const int BTN_READY_PIN = 10;
static const int LED_GREEN_PIN = 5;
static const int LED_RED_PIN = 3;
static const int SDA_PIN = 6;
static const int SCL_PIN = 7;

// Isi dari serial drone saat boot, baris: "[ESP-NOW] MAC Address Drone".
// Selama masih semua nol, glove akan menolak mengirim paket.
static const uint8_t DRONE_MAC_ADDR[6] = {0, 0, 0, 0, 0, 0};

static const uint8_t WIFI_CHANNEL = 1;
static const uint32_t CONTROL_TICK_MS = 25;
static const uint32_t STATUS_TIMEOUT_MS = 1000;
static const uint32_t COMMAND_MIN_NONE_TICKS = 2;

static const uint32_t GESTURE_SAMPLE_MS = 250;
static const float GESTURE_ANGLE_THRESHOLD = 20.0f;

// Ubah tanda atau tukar sumbu jika arah MPU terbalik pada glove:
// 1. Jalankan 'p' di serial.
// 2. Miringkan tangan maju, mundur, kanan, kiri.
// 3. Jika pitch/roll terbalik, ubah sign ke -1. Jika pitch dan roll tertukar, set swap ke 1.
#define GLOVE_PITCH_SIGN 1
#define GLOVE_ROLL_SIGN 1
#define GLOVE_SWAP_AXES 0
