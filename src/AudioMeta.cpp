#include "AudioMeta.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#define AUDIO_HEADER_MAXLEN 512
AudioMetaManager audioMeta;

bool AudioMetaManager::load(const char *path, AudioMeta &meta) {
    if (!path || strlen(path) == 0) {
        Serial.println("⚠️ Path kosong");
        return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("⚠️ Gagal buka file %s\n", path);
        return false;
    }

    // ===== ✅ DEFAULT AMAN UNTUK AUDIOPLAYER BARU =====
    meta.title = String(path);
    meta.volume = 80;
    meta.gearCount = 4;
    meta.maxRPM = 15000;

    meta.playable_min = 500;     // default aman
    meta.playable_max = 15000;

    meta.sample_rate = 8000;     // default 8kHz
    meta.sample_engine_rpm = 15000;

    meta.dataOffset = 0;
    meta.dataLength = file.size();

    // ===== ✅ COBA BACA METADATA JSON =====
    if (file.available()) {
        String header = file.readStringUntil('\n');
        if (header.length() > AUDIO_HEADER_MAXLEN) {
            header = header.substring(0, AUDIO_HEADER_MAXLEN);
        }

        JsonDocument doc;

        DeserializationError err = deserializeJson(doc, header);

        if (!err) {
            meta.title     = doc["title"]     | meta.title;
            meta.volume    = doc["volume"]    | meta.volume;
            meta.gearCount = doc["gearCount"] | meta.gearCount;
            meta.maxRPM    = doc["maxRPM"]    | meta.maxRPM;

            // ✅ FIELD BARU UNTUK AUDIO DYNAMIC RPM
            meta.playable_min      = doc["playable_min"]      | meta.playable_min;
            meta.playable_max      = doc["playable_max"]      | meta.playable_max;
            meta.sample_rate       = doc["sample_rate"]       | meta.sample_rate;
            meta.sample_engine_rpm = doc["sample_engine_rpm"] | meta.sample_engine_rpm;

            meta.dataOffset = file.position();
            meta.dataLength = file.size() - meta.dataOffset;
        } else {
            // fallback mode raw full
            file.seek(0);
            meta.dataOffset = 0;
            meta.dataLength = file.size();
        }
    }

    file.close();
    return true;
}

void AudioMetaManager::print(const AudioMeta &meta) {
    Serial.printf("🎵 Judul: %s\n", meta.title.c_str());
    Serial.printf("🔊 Volume: %d\n", meta.volume);
    Serial.printf("⚙️ GearCount: %d\n", meta.gearCount);
    Serial.printf("🏁 MaxRPM: %lu\n", meta.maxRPM);

    Serial.printf("🌀 Playable RPM: %lu - %lu\n",
                  meta.playable_min, meta.playable_max);

    Serial.printf("🎚 Sample Rate: %lu Hz\n", meta.sample_rate);
    Serial.printf("🚗 Engine RPM Ref: %lu\n", meta.sample_engine_rpm);

    Serial.printf("📦 Data Offset: %lu | Data Length: %lu\n",
                  meta.dataOffset, meta.dataLength);
}


void AudioMetaManager::listAudioFiles() {
    File root = LittleFS.open("/");
    if (!root) {
        Serial.println("❌ Gagal buka root directory");
        root.close();
        return;
    }

    if (!root.isDirectory()) {
        root.close();
        Serial.println("❌ Root is not a directory - filesystem corrupt");
        Serial.println("💡 Try: LittleFS.format()");
        return;
    }

    Serial.println("📂 Daftar Audio di LittleFS:");
    Serial.println("------------------------------------");

    File file = root.openNextFile();
    int index = 1;

    while (file) {
        String name = file.name();

        if (name.endsWith(".raw")) {
            AudioMeta meta;

            Serial.printf("🎧 [%d] %s (%d bytes)\n",
                          index,
                          name.c_str(),
                          file.size());

            String fullPath = "/" + name;

            if (audioMeta.load(fullPath.c_str(), meta)) {
                Serial.printf("     🔊 Volume      : %d\n", meta.volume);
                Serial.printf("     ⚙️ GearCount   : %d\n", meta.gearCount);
                Serial.printf("     🌀 RPM Range   : %lu - %lu\n",
                              meta.playable_min,
                              meta.playable_max);
                Serial.printf("     🎚 Sample Rate : %lu Hz\n", meta.sample_rate);
                Serial.printf("     🚗 Engine RPM  : %lu\n", meta.sample_engine_rpm);
            } else {
                Serial.println("     ⚠️ Gagal baca metadata");
            }

            Serial.println("------------------------------------");
            index++;
        }

        file.close();
        file = root.openNextFile();
    }

    if (index == 1) {
        Serial.println("⚠️ Tidak ada file audio .raw ditemukan.");
    }
}

