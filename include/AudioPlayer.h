#pragma once
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

class AudioPlayer {
public:
  AudioPlayer();

  void begin();
  bool loadFile(const char *path);
  void startPlayback();
  void stopPlayback();

  void setSampleRate(uint32_t rate);
  void updateSampleRateFromADC(int adcValue);

  void mute(bool m);
  void toggleMute();
  bool isPlaying();
  uint32_t getSampleRate();

  static void IRAM_ATTR onTimerISR();

private:
  void normalizePCM8(uint8_t *data, size_t length);
  void cleanupAudioBuffer();
  bool isValidFileSize(uint32_t size, uint32_t maxSize);
  bool allocateBuffer();
  bool readFileData(File& audioFile);
  bool restoreTimer(bool success);

  static hw_timer_t *timer;
  static uint8_t *audioBuffer;
  static uint32_t audioLength;
  static volatile uint32_t index;
  static bool isMuted;
};
