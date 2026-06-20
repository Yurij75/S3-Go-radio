#include "ir_remote.h"

#include <ArduinoJson.h>
#include <IRremote.hpp>
#include <LittleFS.h>

#include "config.h"

extern volatile bool pendingScreenChange;
extern volatile bool pendingStreamChange;
extern volatile bool pendingPrevStream;
extern bool pendingToggle;
extern bool showStationList;

extern void handleVolumeStep(int delta);
extern void moveStationListSelection(int delta);
extern bool selectNextPlaylist();

namespace {
constexpr const char* kConfigFilename = "/ir_remote_config.json";
constexpr const char* kConfigType = "ir-remote-config";

bool s_receiverActive = false;
IrRemoteConfig s_config;
IrRemoteLastCode s_lastCode = {};
uint32_t s_lastHandledCommand = 0;
unsigned long s_lastHandledAt = 0;

const char* const kActionKeys[IR_ACTION_COUNT] = {
  "volUp",
  "volDown",
  "playPause",
  "nextStream",
  "prevStream",
  "nextScreen",
  "nextPlaylist"
};

const char* const kActionLabels[IR_ACTION_COUNT] = {
  "Volume +",
  "Volume -",
  "Play / Pause",
  "Next station",
  "Previous station",
  "Next screen",
  "Next playlist"
};

uint32_t defaultCommandFor(IrRemoteAction action) {
  switch (action) {
    case IR_ACTION_VOL_UP: return IR_CMD_VOL_UP;
    case IR_ACTION_VOL_DOWN: return IR_CMD_VOL_DOWN;
    case IR_ACTION_PLAY_PAUSE: return IR_CMD_PLAY_PAUSE;
    case IR_ACTION_NEXT_STREAM: return IR_CMD_NEXT_STREAM;
    case IR_ACTION_PREV_STREAM: return IR_CMD_PREV_STREAM;
    case IR_ACTION_NEXT_SCREEN: return IR_CMD_NEXT_SCREEN;
    case IR_ACTION_NEXT_PLAYLIST: return IR_CMD_NEXT_PLAYLIST;
    default: return 0;
  }
}

void loadDefaults() {
  s_config.enabled = IR_REMOTE_PIN != 255;
  s_config.pin = IR_REMOTE_PIN;
  s_config.repeatDelayMs = 300;
  for (uint8_t i = 0; i < IR_ACTION_COUNT; ++i) {
    s_config.commands[i] = defaultCommandFor(static_cast<IrRemoteAction>(i));
  }
}

String jsonEscapeLocal(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

String hex32(uint32_t value) {
  char buf[11];
  snprintf(buf, sizeof(buf), "0x%lX", static_cast<unsigned long>(value));
  return String(buf);
}

String hex64(uint64_t value) {
  char buf[19];
  snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(value));
  return String(buf);
}

uint32_t effectiveCommand(const IRData& data) {
  if (data.command != 0) return static_cast<uint32_t>(data.command);
  return static_cast<uint32_t>(data.decodedRawData & 0xFFFFFFFFULL);
}

void applyAction(IrRemoteAction action) {
  switch (action) {
    case IR_ACTION_VOL_UP:
      if (showStationList) {
        moveStationListSelection(-1);
        break;
      }
      handleVolumeStep(1);
      break;
    case IR_ACTION_VOL_DOWN:
      if (showStationList) {
        moveStationListSelection(1);
        break;
      }
      handleVolumeStep(-1);
      break;
    case IR_ACTION_PLAY_PAUSE:
      pendingToggle = true;
      break;
    case IR_ACTION_NEXT_STREAM:
      pendingStreamChange = true;
      break;
    case IR_ACTION_PREV_STREAM:
      pendingPrevStream = true;
      break;
    case IR_ACTION_NEXT_SCREEN:
      pendingScreenChange = true;
      break;
    case IR_ACTION_NEXT_PLAYLIST:
      selectNextPlaylist();
      break;
    default:
      break;
  }
}

bool isVolumeAction(IrRemoteAction action) {
  return action == IR_ACTION_VOL_UP || action == IR_ACTION_VOL_DOWN;
}

bool findActionByCommand(uint32_t command, IrRemoteAction& action) {
  if (command == 0) return false;
  for (uint8_t i = 0; i < IR_ACTION_COUNT; ++i) {
    if (s_config.commands[i] == command) {
      action = static_cast<IrRemoteAction>(i);
      return true;
    }
  }
  return false;
}
} // namespace

const char* irRemoteConfigFilename() {
  return kConfigFilename;
}

const char* irRemoteActionKey(IrRemoteAction action) {
  return action < IR_ACTION_COUNT ? kActionKeys[action] : "";
}

const char* irRemoteActionLabel(IrRemoteAction action) {
  return action < IR_ACTION_COUNT ? kActionLabels[action] : "";
}

void irRemoteLoadConfig() {
  loadDefaults();

  File file = LittleFS.open(kConfigFilename, "r");
  if (!file) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) return;

  if (String(doc["type"] | "") != kConfigType) return;

  s_config.enabled = doc["enabled"] | s_config.enabled;
  s_config.pin = constrain(static_cast<int>(doc["pin"] | s_config.pin), 0, 255);
  s_config.repeatDelayMs = constrain(static_cast<int>(doc["repeatDelayMs"] | s_config.repeatDelayMs), 0, 2000);

  JsonObject commands = doc["commands"].as<JsonObject>();
  if (!commands.isNull()) {
    for (uint8_t i = 0; i < IR_ACTION_COUNT; ++i) {
      if (commands[kActionKeys[i]].is<uint32_t>()) {
        s_config.commands[i] = commands[kActionKeys[i]].as<uint32_t>();
      }
    }
  }
}

bool irRemoteSaveConfig(String* errorMessage) {
  JsonDocument doc;
  doc["type"] = kConfigType;
  doc["enabled"] = s_config.enabled;
  doc["pin"] = s_config.pin;
  doc["repeatDelayMs"] = s_config.repeatDelayMs;

  JsonObject commands = doc["commands"].to<JsonObject>();
  for (uint8_t i = 0; i < IR_ACTION_COUNT; ++i) {
    commands[kActionKeys[i]] = s_config.commands[i];
  }

  File file = LittleFS.open(kConfigFilename, "w", true);
  if (!file) {
    if (errorMessage) *errorMessage = "Failed to open IR config for writing";
    return false;
  }

  if (serializeJsonPretty(doc, file) == 0) {
    file.close();
    if (errorMessage) *errorMessage = "Failed to write IR config";
    return false;
  }

  file.close();
  return true;
}

void irRemoteBegin() {
  irRemoteEnd();
  if (!s_config.enabled || s_config.pin == 255) {
    Serial.println("[IR] receiver disabled");
    return;
  }

  IrReceiver.begin(s_config.pin, DISABLE_LED_FEEDBACK);
  s_receiverActive = true;
  Serial.printf("[IR] receiver initialized on GPIO %u\n", s_config.pin);
}

void irRemoteEnd() {
  if (!s_receiverActive) return;
  IrReceiver.end();
  s_receiverActive = false;
}

void irRemoteLoop() {
  if (!s_receiverActive || !s_config.enabled) return;

  if (!IrReceiver.decode()) return;

  const IRData& data = IrReceiver.decodedIRData;
  const uint32_t command = effectiveCommand(data);
  const unsigned long now = millis();
  const bool isRepeat = (data.flags & (IRDATA_FLAGS_IS_REPEAT | IRDATA_FLAGS_IS_AUTO_REPEAT)) != 0;

  s_lastCode.available = true;
  s_lastCode.command = command;
  s_lastCode.address = static_cast<uint32_t>(data.address);
  s_lastCode.value = static_cast<uint64_t>(data.decodedRawData);
  s_lastCode.protocol = static_cast<int>(data.protocol);
  s_lastCode.repeat = isRepeat;
  s_lastCode.receivedAt = now;

  IrRemoteAction action;
  if (findActionByCommand(command, action)) {
    const bool repeatedCommand = isRepeat && command == s_lastHandledCommand;
    if (repeatedCommand && (now - s_lastHandledAt) < s_config.repeatDelayMs) {
      IrReceiver.resume();
      return;
    }
    s_lastHandledCommand = command;
    s_lastHandledAt = now;
    applyAction(action);
  }

  IrReceiver.resume();
}

const IrRemoteConfig& irRemoteGetConfig() {
  return s_config;
}

IrRemoteLastCode irRemoteGetLastCode() {
  return s_lastCode;
}

bool irRemoteSetEnabled(bool enabled) {
  if (s_config.enabled == enabled) return false;
  s_config.enabled = enabled;
  return true;
}

bool irRemoteSetPin(uint8_t pin) {
  if (s_config.pin == pin) return false;
  s_config.pin = pin;
  return true;
}

bool irRemoteSetRepeatDelay(uint16_t delayMs) {
  delayMs = constrain(delayMs, static_cast<uint16_t>(0), static_cast<uint16_t>(2000));
  if (s_config.repeatDelayMs == delayMs) return false;
  s_config.repeatDelayMs = delayMs;
  return true;
}

bool irRemoteSetCommand(IrRemoteAction action, uint32_t command) {
  if (action >= IR_ACTION_COUNT) return false;
  if (s_config.commands[action] == command) return false;
  s_config.commands[action] = command;
  return true;
}

bool irRemoteActionFromKey(const String& key, IrRemoteAction& action) {
  for (uint8_t i = 0; i < IR_ACTION_COUNT; ++i) {
    if (key == kActionKeys[i]) {
      action = static_cast<IrRemoteAction>(i);
      return true;
    }
  }
  return false;
}

String irRemoteConfigJson() {
  String json = "{";
  json += "\"enabled\":" + String(s_config.enabled ? "true" : "false") + ",";
  json += "\"pin\":" + String(s_config.pin) + ",";
  json += "\"repeatDelayMs\":" + String(s_config.repeatDelayMs) + ",";
  json += "\"file\":\"" + jsonEscapeLocal(kConfigFilename) + "\",";
  json += "\"actions\":[";
  for (uint8_t i = 0; i < IR_ACTION_COUNT; ++i) {
    if (i > 0) json += ",";
    json += "{\"key\":\"" + jsonEscapeLocal(kActionKeys[i]) + "\",";
    json += "\"label\":\"" + jsonEscapeLocal(kActionLabels[i]) + "\",";
    json += "\"command\":" + String(s_config.commands[i]) + ",";
    json += "\"hex\":\"" + hex32(s_config.commands[i]) + "\"}";
  }
  json += "],\"last\":";
  json += irRemoteLastCodeJson();
  json += "}";
  return json;
}

String irRemoteLastCodeJson() {
  String json = "{";
  json += "\"available\":" + String(s_lastCode.available ? "true" : "false");
  if (s_lastCode.available) {
    json += ",\"command\":" + String(s_lastCode.command);
    json += ",\"commandHex\":\"" + hex32(s_lastCode.command) + "\"";
    json += ",\"address\":" + String(s_lastCode.address);
    json += ",\"addressHex\":\"" + hex32(s_lastCode.address) + "\"";
    json += ",\"value\":\"" + hex64(s_lastCode.value) + "\"";
    json += ",\"protocol\":" + String(s_lastCode.protocol);
    json += ",\"repeat\":" + String(s_lastCode.repeat ? "true" : "false");
    json += ",\"ageMs\":" + String(millis() - s_lastCode.receivedAt);
  }
  json += "}";
  return json;
}
