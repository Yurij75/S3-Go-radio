#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <Arduino.h>

enum IrRemoteAction : uint8_t {
  IR_ACTION_VOL_UP = 0,
  IR_ACTION_VOL_DOWN,
  IR_ACTION_PLAY_PAUSE,
  IR_ACTION_NEXT_STREAM,
  IR_ACTION_PREV_STREAM,
  IR_ACTION_NEXT_SCREEN,
  IR_ACTION_NEXT_PLAYLIST,
  IR_ACTION_COUNT
};

struct IrRemoteConfig {
  bool enabled;
  uint8_t pin;
  uint16_t repeatDelayMs;
  uint32_t commands[IR_ACTION_COUNT];
};

struct IrRemoteLastCode {
  bool available;
  uint32_t command;
  uint32_t address;
  uint64_t value;
  int protocol;
  bool repeat;
  unsigned long receivedAt;
};

const char* irRemoteConfigFilename();
const char* irRemoteActionKey(IrRemoteAction action);
const char* irRemoteActionLabel(IrRemoteAction action);

void irRemoteLoadConfig();
bool irRemoteSaveConfig(String* errorMessage = nullptr);
void irRemoteBegin();
void irRemoteEnd();
void irRemoteLoop();

const IrRemoteConfig& irRemoteGetConfig();
IrRemoteLastCode irRemoteGetLastCode();
bool irRemoteSetEnabled(bool enabled);
bool irRemoteSetPin(uint8_t pin);
bool irRemoteSetRepeatDelay(uint16_t delayMs);
bool irRemoteSetCommand(IrRemoteAction action, uint32_t command);
bool irRemoteActionFromKey(const String& key, IrRemoteAction& action);
String irRemoteConfigJson();
String irRemoteLastCodeJson();

#endif // IR_REMOTE_H
