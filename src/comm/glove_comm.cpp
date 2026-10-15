#include <Arduino.h>
#include <math.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <string.h>
#include "config.h"
#include "glove_comm.h"

static volatile uint8_t s_statusState = DRONE_DISARMED;
static volatile uint8_t s_statusSeq = 0;
static volatile uint32_t s_statusMs = 0;
static uint8_t s_txSeq = 0;
static bool s_readyToSend = false;
static bool s_warnedNoMac = false;

static uint8_t s_activeCommand = CMD_NONE;
static uint8_t s_activeRemaining = 0;
static uint8_t s_noneGapRemaining = 0;

static bool macEquals(const uint8_t *left, const uint8_t *right) {
  for (uint8_t i = 0; i < 6; i++) {
    if (left[i] != right[i]) {
      return false;
    }
  }
  return true;
}

bool commDroneMacConfigured() {
  const uint8_t zeroMac[6] = {0, 0, 0, 0, 0, 0};
  return !macEquals(DRONE_MAC_ADDR, zeroMac);
}

static void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (i > 0) {
      Serial.print(":");
    }
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
  }
}

static bool isBroadcastMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) {
      return false;
    }
  }
  return true;
}

static void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != static_cast<int>(sizeof(DroneStatusPacket))) {
    return;
  }
  if (!commDroneMacConfigured()) {
    return;
  }
  // Jika DRONE_MAC_ADDR di-set broadcast (FF:FF:FF:FF:FF:FF), terima status dari drone manapun
  if (!isBroadcastMac(DRONE_MAC_ADDR) && !macEquals(mac, DRONE_MAC_ADDR)) {
    return;
  }

  DroneStatusPacket packet;
  memcpy(&packet, data, sizeof(packet));
  if (packet.magic != 0xA5) {
    return;
  }

  s_statusState = packet.state;
  s_statusSeq = packet.seq;
  s_statusMs = millis();
}

void commBegin() {
  wifi_init_config_t wifiConfig = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifiConfig);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t ownMac[6] = {0, 0, 0, 0, 0, 0};
  esp_read_mac(ownMac, ESP_MAC_WIFI_STA);
  Serial.print("[ESP-NOW] MAC Address Glove: ");
  printMac(ownMac);
  Serial.println();

  if (!commDroneMacConfigured()) {
    Serial.println("[ESP-NOW] PERINGATAN KERAS: DRONE_MAC_ADDR masih 00:00:00:00:00:00. Paket tidak akan dikirim.");
  }

  esp_err_t initResult = esp_now_init();
  if (initResult != ESP_OK) {
    Serial.print("[ESP-NOW] init gagal: ");
    Serial.println(initResult);
    s_readyToSend = false;
    return;
  }

  esp_now_register_recv_cb(onReceive);

  if (commDroneMacConfigured()) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, DRONE_MAC_ADDR, 6);
    peer.channel = WIFI_CHANNEL;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;
    esp_err_t addResult = esp_now_add_peer(&peer);
    if (addResult != ESP_OK && addResult != ESP_ERR_ESPNOW_EXIST) {
      Serial.print("[ESP-NOW] tambah peer gagal: ");
      Serial.println(addResult);
      s_readyToSend = false;
      return;
    }
    s_readyToSend = true;
  }
}

bool commHasFreshStatus() {
  uint32_t lastMs = s_statusMs;
  return lastMs > 0 && (millis() - lastMs) <= STATUS_TIMEOUT_MS;
}

uint8_t commLastDroneState() {
  return s_statusState;
}

uint32_t commStatusAgeMs() {
  uint32_t lastMs = s_statusMs;
  if (lastMs == 0) {
    return 0xFFFFFFFFUL;
  }
  return millis() - lastMs;
}

bool commIsBusy() {
  return s_activeRemaining > 0 || s_noneGapRemaining > 0;
}

void commSendImmediate(uint8_t command, uint16_t throttle) {
  if (!commDroneMacConfigured() || !s_readyToSend) {
    return;
  }
  DronePacketV2 packet = {};
  packet.seq = s_txSeq++;
  packet.command = command;
  packet.throttle = throttle;
  esp_now_send(DRONE_MAC_ADDR, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

bool commStartCommand(uint8_t command, uint8_t repeatTicks) {
  if (command == CMD_BTN_EMERGENCY) {
    commEmergencyCommand();
    return true;
  }
  s_activeCommand = command;
  s_activeRemaining = repeatTicks;
  s_noneGapRemaining = 0;
  // Langsung kirim instan paket pertama seketika (< 1 ms latency)
  commSendImmediate(command, 0);
  return true;
}

void commEmergencyCommand() {
  s_activeCommand = CMD_BTN_EMERGENCY;
  s_activeRemaining = 4;
  s_noneGapRemaining = 0;
  commSendImmediate(CMD_BTN_EMERGENCY, 0);
}

void commTick() {
  uint8_t commandToSend = CMD_NONE;

  if (s_activeRemaining > 0) {
    commandToSend = s_activeCommand;
    s_activeRemaining--;
    if (s_activeRemaining == 0) {
      s_activeCommand = CMD_NONE;
      s_noneGapRemaining = COMMAND_MIN_NONE_TICKS;
    }
  } else if (s_noneGapRemaining > 0) {
    s_noneGapRemaining--;
  }

  DronePacketV2 packet = {};
  packet.seq = s_txSeq++;
  packet.command = commandToSend;
  packet.throttle = 0;

  if (!commDroneMacConfigured()) {
    if (!s_warnedNoMac) {
      Serial.println("[ESP-NOW] Kirim diblokir: isi DRONE_MAC_ADDR di include/config.h.");
      s_warnedNoMac = true;
    }
    return;
  }
  if (!s_readyToSend) {
    return;
  }
  esp_now_send(DRONE_MAC_ADDR, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
}

void commPrintDiagnostics() {
  Serial.print("[DIAG] MAC drone: ");
  printMac(DRONE_MAC_ADDR);
  Serial.print(" configured=");
  Serial.print(commDroneMacConfigured() ? "ya" : "tidak");
  Serial.print(" state=");
  Serial.print(static_cast<uint8_t>(s_statusState));
  Serial.print(" statusAgeMs=");
  Serial.print(commStatusAgeMs());
  Serial.print(" seqStatus=");
  Serial.print(static_cast<uint8_t>(s_statusSeq));
  Serial.print(" txSeq=");
  Serial.println(s_txSeq);
}
