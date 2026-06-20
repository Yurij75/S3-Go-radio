#ifndef VOLUME_CONTROL_H
#define VOLUME_CONTROL_H

#include <Arduino.h>
#include <Audio.h>

constexpr int kVolumePercentMin = 0;
constexpr int kVolumePercentMax = 254;

inline int getAudioVolumeNativeMax(Audio& audio) {
  const int steps = audio.getVolumeSteps();
  if (steps <= 0) return 255;
  return steps;
}

inline void configureAudioVolumeRange(Audio& audio) {
  audio.setVolumeSteps(255);
}

inline int volumePercentToNative(Audio& audio, int percent) {
  (void)audio;
  return constrain(percent, kVolumePercentMin, kVolumePercentMax);
}

inline int volumePercentToNative(Audio& audio, int percent, int stationOvol) {
  (void)audio;
  const int volume = constrain(percent, kVolumePercentMin, kVolumePercentMax);
  const int inputMax = 254 - stationOvol * 3;
  if (inputMax <= 0) return kVolumePercentMax;

  int nativeVolume = map(volume, 0, inputMax, 0, 254);
  if (nativeVolume > 254) nativeVolume = 254;
  if (nativeVolume < 0) nativeVolume = 0;
  return nativeVolume;
}

inline int volumeNativeToPercent(Audio& audio, int nativeVolume) {
  (void)audio;
  return constrain(nativeVolume, kVolumePercentMin, kVolumePercentMax);
}

inline void setAudioVolumePercent(Audio& audio, int percent) {
  audio.setVolume(volumePercentToNative(audio, percent));
}

inline void setAudioVolumePercent(Audio& audio, int percent, int stationOvol) {
  audio.setVolume(volumePercentToNative(audio, percent, stationOvol));
}

#endif
