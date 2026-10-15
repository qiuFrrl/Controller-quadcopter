#pragma once

#include <stdint.h>

enum DroneCommand : uint8_t {
  CMD_NONE = 0,
  CMD_MAJU = 1,
  CMD_MUNDUR = 2,
  CMD_KIRI = 3,
  CMD_KANAN = 4,
  CMD_BTN_ARM_LAND = 10,
  CMD_BTN_EMERGENCY = 11,
  CMD_BTN_READY = 12
};

struct __attribute__((packed)) DronePacketV2 {
  uint8_t seq;
  uint8_t command;
  uint16_t throttle;
};

struct __attribute__((packed)) DroneStatusPacket {
  uint8_t magic;
  uint8_t state;
  uint8_t seq;
};

enum DroneState : uint8_t {
  DRONE_DISARMED = 0,
  DRONE_ARMED = 1,
  DRONE_FLYING = 2,
  DRONE_LANDING = 3,
  DRONE_EMERGENCY = 4
};
