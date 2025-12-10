#include "AudioPlayer.h"
#include "config.h"

hw_timer_t *AudioPlayer::timer = nullptr;
uint8_t *AudioPlayer::audioBuffer = nullptr;
uint32_t AudioPlayer::audioLength = 0;
volatile uint32_t AudioPlayer::index = 0;
bool AudioPlayer::isMuted = false;

// Konstruktor AudioPlayer
AudioPlayer::AudioPlayer() {}

// Normalisasi data PCM 8-bit agar amplitudo merata dan centered di 127
// Mencari nilai min/max, lalu scale dan shift ke range 19-237
/*✅ Perbaikan Variable Names:
A → maxValue - Nilai maksimum dalam data
B → minValue - Nilai minimum dalam data
C → centerPoint - Titik tengah range
X → dynamicRange - Range dinamis audio
K → scaleFactor - Faktor skala normalisasi
T → offset - Offset untuk centering
Z → scaledCenter - Center point setelah scaling
val → normalizedValue - Nilai setelah normalisasi
*/
void AudioPlayer::normalizePCM8(uint8_t *data, size_t length) {
  if (!data || length == 0) return;

  uint8_t maxValue = 0;
  uint8_t minValue = 255;

  // Find min/max values in audio data
  for (size_t i = 0; i < length; i++) {
    if (data[i] > maxValue) maxValue = data[i];
    if (data[i] < minValue) minValue = data[i];
  }

  float centerPoint = (maxValue + minValue) * 0.5f;
  float dynamicRange = (float)(maxValue - minValue);
  float scaleFactor, offset;

  if (dynamicRange < 2.0f) {
    scaleFactor = 1.0f;
    offset = 127.0f - centerPoint;
  } else {
    scaleFactor = 218.0f / dynamicRange;
    float scaledCenter = centerPoint * scaleFactor;
    offset = 127.0f - scaledCenter;
  }

  // Apply normalization to keep values in range 19-237
  for (size_t i = 0; i < length; i++) {
    float normalizedValue = (data[i] * scaleFactor) + offset;
    if (normalizedValue < 19)  normalizedValue = 19;
    if (normalizedValue > 237) normalizedValue = 237;
    data[i] = (uint8_t)normalizedValue;
  }
}

// ISR timer untuk output audio ke DAC
// Dipanggil setiap interval sample rate (misal 16kHz = tiap 62.5μs)
void IRAM_ATTR AudioPlayer::onTimerISR() {
  if (isMuted || !audioBuffer || audioLength == 0) {
    dacWrite(AUDIO_DAC_PIN, 128);
    return;
  }

  if (index >= audioLength) index = 0;
  dacWrite(AUDIO_DAC_PIN, audioBuffer[index]);
  index++;
}

// Inisialisasi DAC dan timer hardware untuk playback audio
// Set sample rate default 16kHz
void AudioPlayer::begin() {
  dacWrite(AUDIO_DAC_PIN, 128);  // idle mid

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &AudioPlayer::onTimerISR, true);
  timerAlarmWrite(timer, 1000000 / 8000, true); // default 8kHz (idle)
  timerAlarmEnable(timer);
}

bool AudioPlayer::loadFile(const char *path) {
  const uint32_t MAX_FILE_SIZE = 1048576; // 1MB
  
  if (timer) timerAlarmDisable(timer);
  
  cleanupAudioBuffer();
  
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("❌ Gagal buka %s\n", path);
    return restoreTimer(false);
  }

  audioLength = f.size();
  if (!isValidFileSize(audioLength, MAX_FILE_SIZE)) {
    f.close();
    return restoreTimer(false);
  }
  
  if (!allocateBuffer() || !readFileData(f)) {
    f.close();
    return restoreTimer(false);
  }
  
  f.close();
  normalizePCM8(audioBuffer, audioLength);
  index = 0;
  
  Serial.printf("✅ Loaded + normalized: %s (%lu bytes)\n", path, audioLength);
  return restoreTimer(true);
}

// Mulai playback audio dari awal buffer
void AudioPlayer::startPlayback() {
  index = 0;
  isMuted = false;
}

// Stop playback dan set DAC ke posisi idle (128)
void AudioPlayer::stopPlayback() {
  isMuted = true;
  dacWrite(AUDIO_DAC_PIN, 128);
}

// Set sample rate audio (8kHz - 44.1kHz)
// Mengubah interval timer sesuai rate yang diinginkan
void AudioPlayer::setSampleRate(uint32_t rate) {
  if (!timer) return;

  if (rate < 8000) rate = 8000;
  if (rate > 44100) rate = 44100;

  timerAlarmDisable(timer);
  timerAlarmWrite(timer, 1000000 / rate, true);
  timerAlarmEnable(timer);
}

// Update sample rate berdasarkan nilai ADC (0-4095)
// Map ADC value ke range 8kHz - 44.1kHz
void AudioPlayer::updateSampleRateFromADC(int adcValue) {
  long rate = map(adcValue, 0, 4095, 8000, 44100);
  setSampleRate(rate);
}

// Set status mute audio
// true = mute, false = unmute
void AudioPlayer::mute(bool m) {
  isMuted = m;
}

// Toggle status mute (ON/OFF)
void AudioPlayer::toggleMute() {
  isMuted = !isMuted;
  Serial.printf("🔇 Mute: %s\n", isMuted ? "ON" : "OFF");
}

// Cek apakah audio sedang playing (tidak mute)
// Return true jika playing, false jika mute
bool AudioPlayer::isPlaying() {
  return !isMuted;
}

uint32_t AudioPlayer::getSampleRate() {
  if (!timer) return 16000;
  uint64_t alarmValue = timerAlarmRead(timer);
  return alarmValue > 0 ? 1000000 / alarmValue : 16000;
}

void AudioPlayer::cleanupAudioBuffer() {
  if (audioBuffer) {
    free(audioBuffer);
    audioBuffer = nullptr;
    audioLength = 0;
  }
}

bool AudioPlayer::isValidFileSize(uint32_t size, uint32_t maxSize) {
  if (size == 0 || size > maxSize) {
    Serial.printf("❌ Ukuran file invalid: %lu bytes\n", size);
    audioLength = 0;
    return false;
  }
  return true;
}

bool AudioPlayer::allocateBuffer() {
  audioBuffer = (uint8_t *)malloc(audioLength);
  if (!audioBuffer) {
    Serial.println("❌ RAM tidak cukup");
    audioLength = 0;
    return false;
  }
  return true;
}

bool AudioPlayer::readFileData(File& f) {
  size_t bytesRead = f.read(audioBuffer, audioLength);
  if (bytesRead != audioLength) {
    Serial.printf("❌ Read error: expected %lu, got %lu\n", audioLength, bytesRead);
    cleanupAudioBuffer();
    return false;
  }
  return true;
}

bool AudioPlayer::restoreTimer(bool success) {
  if (timer) timerAlarmEnable(timer);
  return success;
}

