# Arsitektur GloveController

Dokumen ini menjelaskan isi proyek tanpa perlu membaca file sumber satu per satu. GloveController adalah remote sarung tangan berbasis ESP32-C3 SuperMini yang mengirim perintah ke drone F450 lewat ESP-NOW. Firmware dibuat modular, tanpa `.ino`, dengan loop utama non-blocking 40 Hz agar heartbeat ke drone tidak berhenti.

## Gambaran besar

GloveController bekerja sebagai pengirim paket kontrol yang terus aktif:

1. Boot, inisialisasi tombol, LED, MPU6050, gesture, dan ESP-NOW.
2. Setiap 25 ms membaca tombol, memperbarui gesture, memperbarui LED, lalu mengirim tepat satu paket.
3. Jika tidak ada perintah aktif, paket yang dikirim adalah `CMD_NONE` sebagai heartbeat.
4. Jika ada tombol atau gesture valid, modul komunikasi mengirim burst command beberapa tick, lalu memaksa minimal 2 tick `CMD_NONE` sebelum command berikutnya.
5. Status drone diterima lewat callback ESP-NOW broadcast 10 Hz. Status ini dipakai untuk LED dan untuk menentukan fungsi tombol ready.

Drone menolak paket dengan `seq` duplikat, sehingga setiap paket yang dikirim selalu menaikkan `seq`, termasuk heartbeat `CMD_NONE`.

## Struktur folder

```text
GloveController/
  platformio.ini
  README.md
  architecture/
    README.md
  include/
    config.h
    packets.h
    buttons.h
    glove_comm.h
    glove_imu.h
    gesture.h
    leds.h
  src/
    main.cpp
    comm/glove_comm.cpp
    imu/glove_imu.cpp
    input/buttons.cpp
    gesture/gesture.cpp
    ui/leds.cpp
```

## File konfigurasi proyek

### `platformio.ini`

Mengatur proyek PlatformIO untuk ESP32-C3 DevKitM-1 dengan framework Arduino:

- Platform: `espressif32 @ ~6.10.0`
- Board: `esp32-c3-devkitm-1`
- Serial monitor: `115200`
- Build flags:
  - `-I include`
  - `-DARDUINO_USB_MODE=1`
  - `-DARDUINO_USB_CDC_ON_BOOT=1`

File ini tidak berisi logika firmware, hanya konfigurasi build.

## Header konfigurasi dan protokol

### `include/config.h`

Ini file yang paling perlu diisi user sebelum upload ke board. Isinya:

- Placeholder pin:
  - `BTN_ARM_PIN`
  - `BTN_EMERGENCY_PIN`
  - `BTN_READY_PIN`
  - `LED_GREEN_PIN`
  - `LED_RED_PIN`
  - `SDA_PIN`
  - `SCL_PIN`
- Placeholder `DRONE_MAC_ADDR[6]`, default semua nol.
- Timing utama:
  - `CONTROL_TICK_MS = 25`
  - `STATUS_TIMEOUT_MS = 1000`
  - `COMMAND_MIN_NONE_TICKS = 2`
- Konfigurasi gesture:
  - `GESTURE_SAMPLE_MS = 250`
  - `GESTURE_ANGLE_THRESHOLD = 20.0f`
  - `GLOVE_PITCH_SIGN`
  - `GLOVE_ROLL_SIGN`
  - `GLOVE_SWAP_AXES`

Jika `DRONE_MAC_ADDR` masih semua nol, firmware tetap boot tetapi tidak mengirim paket. Ini disengaja agar tidak ada pengiriman ke alamat tidak jelas.

Komentar di file ini juga mengingatkan untuk menghindari pin strapping ESP32-C3 seperti GPIO 2, 8, dan 9 jika memungkinkan.

### `include/packets.h`

Mendefinisikan protokol biner yang harus sama persis dengan firmware drone.

Paket kontrol ke drone:

```cpp
struct __attribute__((packed)) DronePacketV2 {
  uint8_t seq;
  uint8_t command;
  uint16_t throttle;
};
```

Ukuran paket adalah 4 byte. `throttle` selalu `0` karena drone tidak lagi memakai throttle dari glove.

Nilai command:

- `CMD_NONE = 0`
- `CMD_MAJU = 1`
- `CMD_MUNDUR = 2`
- `CMD_KIRI = 3`
- `CMD_KANAN = 4`
- `CMD_BTN_ARM_LAND = 10`
- `CMD_BTN_EMERGENCY = 11`
- `CMD_BTN_READY = 12`

Paket status dari drone:

```cpp
struct __attribute__((packed)) DroneStatusPacket {
  uint8_t magic;
  uint8_t state;
  uint8_t seq;
};
```

Ukuran paket status adalah 3 byte. `magic` harus `0xA5`; selain itu paket diabaikan.

State drone:

- `DRONE_DISARMED = 0`
- `DRONE_ARMED = 1`
- `DRONE_FLYING = 2`
- `DRONE_LANDING = 3`
- `DRONE_EMERGENCY = 4`

## Loop utama

### `src/main.cpp`

Ini pusat orkestrasi firmware. File ini tidak menangani detail hardware secara langsung, tetapi memanggil modul lain.

Saat `setup()`:

1. Membuka serial 115200.
2. Memanggil `buttonsBegin()`.
3. Memanggil `ledsBegin()`.
4. Memanggil `imuBegin()`.
5. Memanggil `gestureBegin()`.
6. Memanggil `commBegin()`.

Saat `loop()`:

1. `handleSerial()` memproses perintah serial untuk IMU.
2. `gestureUpdate()` menjalankan sampling gesture jika sedang aktif.
3. `flushPendingGestureCommand()` mengirim command hasil gesture ketika pengirim tidak sibuk.
4. Jika belum waktunya tick 25 ms, loop langsung kembali.
5. Pada tick 25 ms:
   - `buttonsUpdate()`
   - cek edge tombol emergency
   - cek edge tombol arm/land
   - cek edge tombol ready
   - `ledsUpdate()`
   - `commTick()`

Hal penting: `commTick()` selalu dipanggil pada tick utama agar heartbeat tetap keluar. Gesture dan kalibrasi IMU dibuat non-blocking agar tidak menahan loop terlalu lama.

## Alur tombol

Tombol dibaca sebagai `INPUT_PULLUP`, aktif LOW, dan hanya edge tekan yang dihitung. Tidak ada auto-repeat.

### Tombol arm/land

Jika tombol arm ditekan:

- `main.cpp` memanggil `commStartCommand(CMD_BTN_ARM_LAND, 6)`.
- Modul komunikasi mengirim `CMD_BTN_ARM_LAND` selama 6 tick.
- Setelah burst selesai, komunikasi mengirim minimal 2 tick `CMD_NONE`.

Tombol ini berlaku di semua state drone. Arti akhirnya ditentukan firmware drone.

### Tombol emergency

Jika tombol emergency ditekan:

- `main.cpp` memanggil `commEmergencyCommand()`.
- Command emergency menyela command apa pun yang sedang berjalan.
- Modul komunikasi mengirim `CMD_BTN_EMERGENCY` selama 12 tick.

Emergency tetap boleh dikirim walaupun link status putus. Syaratnya hanya `DRONE_MAC_ADDR` sudah diisi.

### Tombol ready

Fungsi tombol ready bergantung pada status drone terakhir.

Jika link status putus:

- Tombol ready ditolak.
- LED merah berkedip cepat 3 kali sebagai tanda reject.

Jika state `ARMED`:

- Mengirim `CMD_BTN_READY` selama 6 tick.
- Ini dipakai sebagai takeoff.

Jika state `FLYING`:

- Memulai sesi sampling gesture.
- Setelah gesture valid, firmware mengirim dua burst berurutan:
  - `CMD_BTN_READY` selama 4 tick.
  - command arah hasil gesture selama 6 tick.
- Di antara kedua burst, modul komunikasi tetap memaksa minimal 2 tick `CMD_NONE`.

Jika state lain:

- Tombol ready ditolak.
- LED merah berkedip cepat 3 kali.

## Modul komunikasi

### `include/glove_comm.h`

Mengekspos API komunikasi:

- `commBegin()`
- `commTick()`
- `commDroneMacConfigured()`
- `commHasFreshStatus()`
- `commLastDroneState()`
- `commStatusAgeMs()`
- `commPrintDiagnostics()`
- `commStartCommand()`
- `commEmergencyCommand()`
- `commIsBusy()`

### `src/comm/glove_comm.cpp`

Modul ini bertanggung jawab atas ESP-NOW, paket keluar, dan status masuk.

Saat `commBegin()`:

1. Menginisialisasi WiFi station melalui ESP-IDF WiFi API.
2. Mengatur channel WiFi ke channel 1.
3. Mencetak MAC glove sendiri ke serial.
4. Mengecek apakah `DRONE_MAC_ADDR` masih nol.
5. Menginisialisasi ESP-NOW.
6. Mendaftarkan callback receive.
7. Menambahkan drone sebagai peer jika MAC sudah diisi.

Callback receive:

- Menerima paket status broadcast dari drone.
- Mengabaikan paket jika:
  - panjang bukan 3 byte
  - MAC pengirim bukan `DRONE_MAC_ADDR`
  - `magic != 0xA5`
- Jika valid, hanya menyimpan:
  - state drone
  - seq status
  - waktu status terakhir
- Tidak ada `Serial.print()` di callback.

Scheduler command:

- Menyimpan command aktif dan sisa pengulangan.
- Jika command aktif ada, `commTick()` mengirim command itu.
- Setelah command habis, scheduler mengirim minimal 2 tick `CMD_NONE`.
- Command baru diabaikan jika scheduler sibuk, kecuali emergency.
- Emergency selalu menimpa command aktif.

Pengiriman paket:

- Setiap `commTick()` membuat satu `DronePacketV2`.
- `seq` selalu naik, termasuk untuk `CMD_NONE`.
- `throttle` selalu `0`.
- Jika MAC drone belum diisi, paket tidak dikirim dan serial memberi peringatan.

## Modul input tombol

### `include/buttons.h`

Mendefinisikan tombol:

- `BUTTON_ARM`
- `BUTTON_EMERGENCY`
- `BUTTON_READY`

API:

- `buttonsBegin()`
- `buttonsUpdate()`
- `buttonPressedEdge(ButtonId button)`

### `src/input/buttons.cpp`

Modul ini melakukan debounce 30 ms.

Setiap tombol punya state:

- pin
- reading terakhir
- stable pressed
- edge tekan
- waktu perubahan terakhir

`buttonPressedEdge()` hanya bernilai true satu tick saat tombol berubah dari tidak ditekan menjadi ditekan secara stabil. Tombol ditahan tidak menghasilkan repeat.

Jika pin masih `-1`, modul mencetak peringatan dan tombol tersebut diabaikan.

## Modul IMU

### `include/glove_imu.h`

Mendefinisikan:

- `ImuRawSample`: data akselerometer dan gyro mentah.
- `ImuAngles`: roll dan pitch dalam derajat.

API:

- `imuBegin()`
- `imuReady()`
- `imuReadRaw()`
- `imuReadAngles()`
- `imuHandleSerial()`
- `imuPrintDiagnostics()`
- `imuHasError()`

### `src/imu/glove_imu.cpp`

Modul ini membaca MPU6050 GY-521 langsung dari register I2C memakai `Wire.h`. Tidak ada library MPU pihak ketiga.

Alamat I2C:

- Utama: `0x68`
- Fallback: `0x69`

Register yang dikonfigurasi:

- `PWR_MGMT_1 = 0x00`, wake up.
- `CONFIG = 0x03`, DLPF.
- `GYRO_CONFIG = 0x08`, gyro ±500 derajat/detik.
- `ACCEL_CONFIG = 0x10`, accel ±8g.

Skala:

- Accel: `4096 LSB/g`.
- Gyro: `65.5 LSB/(derajat/detik)`.

Pembacaan raw:

- Membaca 14 byte mulai `ACCEL_XOUT_H`.
- Data temperatur dibaca dari stream tetapi tidak dipakai.
- Jika pembacaan gagal 5 kali berturut-turut, flag IMU error aktif.

Perhitungan sudut:

- Roll:
  - `atan(accY / sqrt(accX^2 + accZ^2))`
- Pitch:
  - `-atan(accX / sqrt(accY^2 + accZ^2))`
- Hasil dikonversi ke derajat memakai `M_PI`.
- Offset kalibrasi dan sudut netral dikurangi dari hasil akhir.

NVS:

- Namespace: `"glove"`
- Menyimpan offset accel, offset gyro, dan sudut netral.
- Dimuat otomatis saat boot.

Perintah serial:

- `c`: mulai kalibrasi datar 1000 sampel.
- `n`: mulai tangkap sudut netral 500 sampel.
- `p`: cetak roll/pitch live setiap 100 ms selama 5 detik.
- `d`: cetak diagnostik IMU.

Kalibrasi `c` dan `n` berjalan non-blocking. Artinya firmware tetap bisa mengirim heartbeat selama proses sampling.

## Modul gesture

### `include/gesture.h`

Mendefinisikan hasil gesture:

- `GESTURE_NONE`
- `GESTURE_MAJU`
- `GESTURE_MUNDUR`
- `GESTURE_KIRI`
- `GESTURE_KANAN`

API:

- `gestureBegin()`
- `gestureStart(callback)`
- `gestureUpdate()`
- `gestureIsActive()`

### `src/gesture/gesture.cpp`

Gesture dimulai hanya saat tombol ready ditekan ketika drone `FLYING`.

Saat aktif:

1. Mengambil sampel roll/pitch tiap 10 ms.
2. Durasi default sampling: 250 ms.
3. Merata-ratakan semua sampel valid.
4. Menerapkan `GLOVE_SWAP_AXES`, `GLOVE_PITCH_SIGN`, dan `GLOVE_ROLL_SIGN`.
5. Membandingkan pitch dan roll terhadap threshold default 20 derajat.
6. Memilih sumbu dominan sebagai arah.

Keputusan arah:

- Pitch positif melewati threshold: `MAJU`.
- Pitch negatif melewati threshold: `MUNDUR`.
- Roll positif melewati threshold: `KANAN`.
- Roll negatif melewati threshold: `KIRI`.

Reject gesture:

- Jika tidak ada sumbu melewati threshold.
- Jika pitch dan roll sama-sama melewati threshold dan sumbu kedua lebih dari 0.7 kali sumbu dominan.
- Jika IMU error.

Jika gesture valid, callback mengembalikan command arah ke `main.cpp`. Jika tidak valid, LED merah blink reject 2 kali dan tidak ada command yang dikirim.

## Modul LED

### `include/leds.h`

API:

- `ledsBegin()`
- `ledsUpdate()`
- `ledsBlinkReject(count)`

### `src/ui/leds.cpp`

Modul ini menjaga hanya satu warna LED menyala pada satu waktu.

Prioritas LED:

1. Reject blink sementara dari tombol/gesture.
2. IMU error, link putus, atau drone emergency: merah kedip cepat 150 ms.
3. Belum pernah menerima status valid: merah solid.
4. Drone `ARMED` atau `FLYING`: hijau solid.
5. Drone `LANDING`: hijau kedip 500 ms.
6. State lain, termasuk `DISARMED`: merah solid.

Reject blink:

- `ledsBlinkReject(2)` untuk gesture invalid.
- `ledsBlinkReject(3)` untuk tombol ready ditolak.

## Alur status link

Status valid hanya diterima jika:

- panjang paket 3 byte
- magic `0xA5`
- MAC pengirim sama dengan `DRONE_MAC_ADDR`

Jika belum ada status valid sejak boot:

- LED merah solid.
- Tombol ready ditolak.

Jika pernah ada status valid tetapi umur status lebih dari 1000 ms:

- Link dianggap putus.
- LED merah kedip cepat.
- Tombol ready ditolak.
- Tombol emergency tetap boleh mengirim command.

## Alur gesture saat FLYING

Urutan lengkap saat drone `FLYING` dan tombol ready ditekan:

1. `main.cpp` memanggil `gestureStart()`.
2. `gesture.cpp` sampling roll/pitch 250 ms tanpa blocking.
3. Heartbeat tetap dikirim oleh `commTick()` setiap 25 ms.
4. Jika gesture invalid:
   - tidak ada command dikirim
   - LED merah blink 2 kali
5. Jika gesture valid:
   - `main.cpp` menyimpan arah sebagai pending command
   - saat komunikasi idle, kirim `CMD_BTN_READY` 4 tick
   - komunikasi otomatis memberi minimal 2 tick `CMD_NONE`
   - setelah idle lagi, kirim command arah 6 tick

Alur ini dibuat agar firmware drone yang hanya mengeksekusi command pada rising edge tetap melihat pemisah `CMD_NONE`.

## Diagnostik serial

Serial berjalan di 115200.

Perintah:

- `c`: kalibrasi datar.
- `n`: tangkap netral tangan.
- `p`: print roll/pitch live untuk cek tanda sumbu.
- `d`: diagnostik gabungan dari komunikasi dan IMU.

Saat boot, serial juga mencetak MAC glove sendiri. MAC ini perlu disalin ke firmware drone sebagai `GLOVE_MAC_ADDR`.

## Catatan keselamatan

- `DRONE_MAC_ADDR` default nol agar firmware tidak mengirim ke alamat tidak valid.
- Command emergency bisa menyela command lain.
- Command non-emergency diabaikan saat scheduler sedang sibuk.
- Heartbeat `CMD_NONE` dikirim terus selama `commTick()` berjalan.
- Kalibrasi dan gesture non-blocking supaya heartbeat tidak berhenti lama.
- Tombol ready ditolak saat link status tidak valid atau IMU error.
- Paket status tidak dipercaya kecuali ukuran, magic, dan MAC cocok.

## File mana yang perlu diedit user

Biasanya user hanya perlu mengedit:

- `include/config.h`

Isi pin, MAC drone, dan jika perlu tuning orientasi gesture:

- `GLOVE_PITCH_SIGN`
- `GLOVE_ROLL_SIGN`
- `GLOVE_SWAP_AXES`
- `GESTURE_ANGLE_THRESHOLD`

File lain sebaiknya tidak diubah kecuali ingin mengubah perilaku firmware.
