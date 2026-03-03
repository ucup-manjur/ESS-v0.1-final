# 🎛️ Slope Configuration via BLE

## 📋 Parameter yang Bisa Diatur:

### 1. **Low RPM Slope** (CMD 0x50)
- Default: 80
- Range: 50-200
- Fungsi: Kecepatan response di RPM rendah (0-25%)

### 2. **Mid RPM Slope** (CMD 0x51)
- Default: 250
- Range: 150-400
- Fungsi: Kecepatan response di RPM menengah (25-75%)

### 3. **High RPM Slope** (CMD 0x52)
- Default: 150
- Range: 100-300
- Fungsi: Kecepatan response di RPM tinggi (75-100%)

### 4. **Decel Multiplier** (CMD 0x53)
- Default: 1.8 (180%)
- Range: 1.0-3.0
- Fungsi: Multiplier untuk deceleration (engine brake)

### 5. **Aggressive Lag** (CMD 0x54)
- Default: 0.7 (70%)
- Range: 0.3-1.0
- Fungsi: Lag saat gas spontan (0.3=slow, 1.0=no lag)

### 6. **Aggressive Threshold** (CMD 0x55)
- Default: 1000 ADC units
- Range: 500-2000
- Fungsi: Threshold untuk deteksi gas spontan

## 📱 BLE Command Format:

```
0xAA [CMD] [VALUE_LOW] [VALUE_HIGH] [CHECKSUM]
```

### Contoh:

**Set Low RPM Slope = 100:**
```
0xAA 0x50 0x64 0x00 [CHK]
```

**Set Aggressive Lag = 0.5 (50%):**
```
0xAA 0x54 0x32 0x00 [CHK]  // 50 = 0.5 * 100
```

**Request Current Config:**
```
0xAA 0x56 0x00 0x00 [CHK]
```

## 🎯 Tuning Guide:

### Untuk Suara Lebih Responsif:
- Naikkan Low/Mid/High RPM Slope
- Naikkan Aggressive Lag (mendekati 1.0)

### Untuk Suara Lebih Realistik (Lag):
- Turunkan Low RPM Slope
- Turunkan Aggressive Lag (mendekati 0.3)
- Naikkan Decel Multiplier

### Untuk Engine Brake Lebih Kuat:
- Naikkan Decel Multiplier (>2.0)

---

**Note:** Handler BLE command perlu ditambahkan di SystemManager.cpp
