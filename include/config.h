#pragma once

#include <stdint.h>

// Isi pin sesuai wiring glove. Jika memungkinkan hindari pin strapping ESP32-C3:
// GPIO 2, GPIO 8, dan GPIO 9 dapat mempengaruhi mode boot saat diberi level eksternal.
static const int BTN_ARM_PIN = 1;
static const int BTN_EMERGENCY_PIN = 4;
static const int BTN_READY_PIN = 10;
static const int LED_GREEN_PIN = 5;
static const int LED_RED_PIN = 3;
static const int SDA_PIN = 8;
static const int SCL_PIN = 9;

// MAC Address Broadcast untuk transmisi ESP-NOW langsung ke drone
static const uint8_t DRONE_MAC_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const uint8_t WIFI_CHANNEL = 1;
static const uint32_t CONTROL_TICK_MS = 10;
static const uint32_t STATUS_TIMEOUT_MS = 1000;
static const uint32_t COMMAND_MIN_NONE_TICKS = 2;

static const uint32_t GESTURE_SAMPLE_MS = 250;
static const float GESTURE_ANGLE_THRESHOLD = 20.0f;

// Ubah tanda atau tukar sumbu jika arah MPU terbalik pada glove:
// 1. Jalankan 'p' di serial.
// 2. Miringkan tangan maju, mundur, kanan, kiri.
// 3. Jika pitch/roll terbalik, ubah sign ke -1. Jika pitch dan roll tertukar, set swap ke 1.
#define GLOVE_PITCH_SIGN -1
#define GLOVE_ROLL_SIGN -1
#define GLOVE_SWAP_AXES 0

// Nilai kalibrasi MPU6050 glove
// Set true untuk mengunci kalibrasi menggunakan nilai di bawah ini.
#define GLOVE_USE_CONFIG_CALIBRATION true
static const float IMU_ACC_OFFSET_X = -0.5058f;
static const float IMU_ACC_OFFSET_Y = -0.4324f;
static const float IMU_ACC_OFFSET_Z = -0.2060f;
static const float IMU_GYRO_OFFSET_X = 43.4359f; // RateCalibrationRoll
static const float IMU_GYRO_OFFSET_Y = 0.4136f;  // RateCalibrationPitch
static const float IMU_GYRO_OFFSET_Z = -4.6446f; // RateCalibrationYaw

