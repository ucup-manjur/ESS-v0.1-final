📋 PANDUAN KERJA ESS (Engine Sound Simulator)
🎮 KONTROL FISIK
Tombol 1 (Button A)
•	Press: Ganti register (1→2→3→4→1)
•	LED: Menunjukkan register aktif
o	Register 1: LED 1 nyala
o	Register 2: LED 2 nyala
o	Register 3: LED 3 nyala
o	Register 4: Semua LED nyala
Tombol 2 (Button B)
•	Press: Play/Stop audio
•	Long Press (3s): Masuk/keluar Programming Mode
•	LED: Mati saat stop, nyala saat play
Tombol 3 (Button C)
•	Long Press (5s): Format LittleFS (hapus semua file)
Potentiometer
•	Fungsi: Kontrol RPM engine (1000-18000 RPM)
•	Range: Sample rate 8000-44100 Hz
________________________________________
🎵 SISTEM AUDIO
Struktur File
/Audio/engine.raw    ← Register 1
/Audio1/engine.raw   ← Register 2
/Audio2/engine.raw   ← Register 3
/Audio3/engine.raw   ← Register 4
Copy
Cara Kerja
1.	Pilih register dengan Tombol 1
2.	Tombol 2 untuk play/stop
3.	Potentiometer untuk kontrol RPM
4.	Audio otomatis loop sesuai RPM
________________________________________
📱 BLE CONTROL
Mode Normal
•	Kontrol audio via BLE
•	File transfer DISABLED
•	LED sesuai register
Mode Programming
•	Masuk: Tombol 2 long press
•	LED berkedip
•	File transfer ENABLED
•	Upload audio ke register aktif
BLE Commands
0xAA [CMD] [VAL] [CHECKSUM]
Copy
•	0x01
: Gear Up
•	0x02
: Gear Down
•	0x11
: Volume/Mute
•	0x15
: Set Audio Register
•	0x25
: Request Status
________________________________________
🔧 UPLOAD AUDIO
Langkah Upload
1.	Pilih register (Tombol 1)
2.	Masuk programming mode (Tombol 2 long press)
3.	Upload file via aplikasi BLE
4.	Keluar programming mode (Tombol 2 long press)
File Requirements
•	Format: 
.raw
 (PCM 8-bit)
•	Ukuran: Maksimal 1MB
•	Auto-normalisasi: 19-237 range
________________________________________
⚠️ TROUBLESHOOTING
Audio Tidak Keluar
•	Cek register ada file 
engine.raw
•	Pastikan mode play (LED nyala)
•	Cek koneksi DAC pin
File Upload Gagal
•	Pastikan dalam programming mode
•	Cek koneksi BLE
•	Restart ESP32 jika perlu
Reset Total
•	Tombol 3 long press (5s) → Format LittleFS
•	Semua file terhapus, folder dibuat ulang
________________________________________
🎯 TIPS PENGGUNAAN
1.	Backup audio di folder 
data/
 sebelum upload firmware
2.	Test register satu per satu setelah upload
3.	Monitor serial untuk debug
4.	Gunakan aplikasi BLE untuk kontrol remote
5.	Format LittleFS jika ada masalah file
Status LED: Berkedip = Programming Mode, Solid = Normal Mode

