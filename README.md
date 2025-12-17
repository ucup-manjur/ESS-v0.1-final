📋 PANDUAN KERJA ESS (Engine Sound Simulator)
🎮 KONTROL FISIK
Tombol 1 (Button A)
Press: Ganti register (1→2→3→4→1)

LED: Menunjukkan register aktif

Register 1: LED 1 nyala

Register 2: LED 2 nyala

Register 3: LED 3 nyala

Register 4: Semua LED nyala

Tombol 2 (Button B)
Press: Play/Stop audio

Long Press (3s): Masuk/keluar Programming Mode

LED: Mati saat stop, nyala saat play

Tombol 3 (Button C)
Long Press (5s): Format LittleFS (hapus semua file)

Potentiometer
Fungsi: Kontrol RPM engine (1000-18000 RPM)

Range: Sample rate 8000-44100 Hz

🎵 SISTEM AUDIO
Struktur File
/Audio/engine.raw    ← Register 1
/Audio1/engine.raw   ← Register 2
/Audio2/engine.raw   ← Register 3
/Audio3/engine.raw   ← Register 4

Copy
Cara Kerja
Pilih register dengan Tombol 1

Tombol 2 untuk play/stop

Potentiometer untuk kontrol RPM

Audio otomatis loop sesuai RPM

📱 BLE CONTROL
Mode Normal
Kontrol audio via BLE

File transfer DISABLED

LED sesuai register

Mode Programming
Masuk: Tombol 2 long press

LED berkedip

File transfer ENABLED

Upload audio ke register aktif

BLE Commands
0xAA [CMD] [VAL] [CHECKSUM]

Copy
0x01: Gear Up

0x02: Gear Down

0x11: Volume/Mute

0x15: Set Audio Register

0x25: Request Status

🔧 UPLOAD AUDIO
Langkah Upload
Pilih register (Tombol 1)

Masuk programming mode (Tombol 2 long press)

Upload file via aplikasi BLE

Keluar programming mode (Tombol 2 long press)

File Requirements
Format: .raw (PCM 8-bit)

Ukuran: Maksimal 1MB

Auto-normalisasi: 19-237 range

⚠️ TROUBLESHOOTING
Audio Tidak Keluar
Cek register ada file engine.raw

Pastikan mode play (LED nyala)

Cek koneksi DAC pin

File Upload Gagal
Pastikan dalam programming mode

Cek koneksi BLE

Restart ESP32 jika perlu

Reset Total
Tombol 3 long press (5s) → Format LittleFS

Semua file terhapus, folder dibuat ulang

🎯 TIPS PENGGUNAAN
Backup audio di folder data/ sebelum upload firmware

Test register satu per satu setelah upload

Monitor serial untuk debug

Gunakan aplikasi BLE untuk kontrol remote

Format LittleFS jika ada masalah file

Status LED: Berkedip = Programming Mode, Solid = Normal Mode
