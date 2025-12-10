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
