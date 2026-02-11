/*
 * ============================================================================
 * AudioMeta.h - Audio File Metadata Manager
 * ============================================================================
 * 
 * DESKRIPSI:
 * Class untuk membaca dan mengelola metadata audio file. Mendukung JSON
 * header di awal file untuk informasi tambahan seperti title, volume,
 * gear count, dan RPM range.
 * 
 * FITUR UTAMA:
 * 1. JSON Metadata:
 *    - Title: Nama audio
 *    - Volume: Default volume (0-100)
 *    - Gear Count: Jumlah gear (untuk auto-shift)
 *    - Max RPM: RPM maksimum
 * 
 * 2. Dynamic RPM:
 *    - Playable Min: RPM minimum untuk playback
 *    - Playable Max: RPM maksimum untuk playback
 *    - Sample Rate: Sample rate audio file
 *    - Sample Engine RPM: RPM referensi saat recording
 * 
 * 3. Data Offset:
 *    - Data Offset: Byte offset ke audio data
 *    - Data Length: Panjang audio data (bytes)
 *    - Skip JSON header saat playback
 * 
 * 4. File Listing:
 *    - List semua .raw files di LittleFS
 *    - Show metadata untuk setiap file
 *    - Organized by folder
 * 
 * CARA MENGGUNAKAN:
 * 
 * 1. Load Metadata:
 *    ```cpp
 *    AudioMeta meta;
 *    if (audioMeta.load("/Audio/engine.raw", meta)) {
 *        Serial.println(meta.title);
 *        Serial.println(meta.volume);
 *    }
 *    ```
 * 
 * 2. Print Metadata:
 *    ```cpp
 *    audioMeta.print(meta);
 *    // Output:
 *    // 🎵 Judul: V8 Engine
 *    // 🔊 Volume: 80
 *    // ⚙️ GearCount: 6
 *    // 🏁 MaxRPM: 8000
 *    ```
 * 
 * 3. List All Files:
 *    ```cpp
 *    audioMeta.listAudioFiles();
 *    // Output:
 *    // 📂 Daftar Audio di LittleFS:
 *    // 🎧 [1] engine.raw (1024000 bytes)
 *    //      🔊 Volume: 80
 *    //      ⚙️ GearCount: 6
 *    ```
 * 
 * JSON FORMAT:
 * File audio dapat dimulai dengan JSON header (optional):
 * ```json
 * {"title":"V8 Engine","volume":80,"gearCount":6,"maxRPM":8000,
 *  "playable_min":1000,"playable_max":8000,"sample_rate":44100,
 *  "sample_engine_rpm":3000}\n
 * [RAW PCM DATA...]
 * ```
 * 
 * DEFAULT VALUES (jika tidak ada JSON):
 * - Title: Filename
 * - Volume: 80
 * - Gear Count: 4
 * - Max RPM: 15000
 * - Playable Min: 500
 * - Playable Max: 15000
 * - Sample Rate: 8000 Hz
 * - Sample Engine RPM: 15000
 * - Data Offset: 0 (no header)
 * - Data Length: File size
 * 
 * STRUCT AudioMeta:
 * ```cpp
 * struct AudioMeta {
 *     String title;              // Audio title
 *     uint8_t volume;            // Default volume (0-100)
 *     uint8_t gearCount;         // Number of gears
 *     uint32_t maxRPM;           // Max RPM
 *     uint32_t playable_min;     // Min playable RPM
 *     uint32_t playable_max;     // Max playable RPM
 *     uint32_t sample_rate;      // Audio sample rate
 *     uint32_t sample_engine_rpm;// Reference RPM
 *     uint32_t dataOffset;       // Byte offset to audio data
 *     uint32_t dataLength;       // Audio data length
 * };
 * ```
 * 
 * CATATAN:
 * - JSON header maksimal 512 bytes (AUDIO_HEADER_MAXLEN)
 * - JSON harus di baris pertama, diakhiri \n
 * - Jika tidak ada JSON, semua file dianggap raw PCM
 * - Metadata tidak wajib, system akan gunakan default values
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#ifndef AUDIO_META_H
#define AUDIO_META_H

#include <Arduino.h>

struct AudioMeta {
    String title;
    uint8_t volume;
    uint8_t gearCount;
    uint32_t maxRPM;

    uint32_t playable_min;
    uint32_t playable_max;

    uint32_t sample_rate;
    uint32_t sample_engine_rpm;

    uint32_t dataOffset;
    uint32_t dataLength;
};

class AudioMetaManager {
public:
    bool load(const char *path, AudioMeta &meta);
    void print(const AudioMeta &meta);
    void listAudioFiles();
};

// ✅ INI YANG WAJIB ADA
extern AudioMetaManager audioMeta;

#endif
