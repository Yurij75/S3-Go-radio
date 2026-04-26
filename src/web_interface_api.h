// ============================================
// API Functions for Web Interface
// ============================================

#ifndef WEB_INTERFACE_API_H
#define WEB_INTERFACE_API_H

#include <ArduinoJson.h>

extern String currentPlayingPlaylistFile;

inline String getStreamsList() {
  String json = "[";
  for(int i = 0; i < streamCount; i++){
    if(i > 0) json += ",";
    json += "{\"idx\":" + String(i) + ",\"number\":" + String(i + 1) + ",\"name\":\"" + getStreamName(i) + "\",\"playing\":" + String((currentPlaylistFile == currentPlayingPlaylistFile && i == currentStreamIndex) ? "true" : "false") + "}";
  }
  json += "]";
  return json;
}

inline String getPlayerStatus() {
  String json = "{";
  json += "\"station\":\"" + currentStreamName + "\",";
  json += "\"playing\":" + String(radioStarted && !isPaused ? "true" : "false") + ",";
  json += "\"paused\":" + String(isPaused ? "true" : "false") + ",";
  json += "\"volume\":" + String(currentVolume);
  json += "}";
  return json;
}

inline String getNetworkInfo() {
  String json = "{";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"bg\":\"" + currentBackground + "\",";
  json += "\"playlist\":\"" + currentPlaylistFile + "\",";
  json += "\"streams\":" + String(streamCount) + ",";
  json += "\"status\":\"" + String(radioStarted ? (isPaused ? "PAUSED" : "PLAYING") : "STOPPED") + "\",";
  json += "\"heap\":" + String(ESP.getFreeHeap() / 1024) + ",";
  json += "\"psram\":" + String(ESP.getFreePsram() / 1024) + ",";
  json += "\"fsTotal\":" + String(LittleFS.totalBytes() / 1024) + ",";
  json += "\"fsUsed\":" + String(LittleFS.usedBytes() / 1024) + ",";
  json += "\"fsFree\":" + String((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024);
  json += "}";
  return json;
}

inline String getBackgroundsList() {
  String json = "[";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  bool first = true;
  
  while(file){
    String name = String(file.name());
    if(name.endsWith(".jpg") || name.endsWith(".jpeg")){
      if(!first) json += ",";
      json += "{\"name\":\"" + name + "\",\"selected\":" + String(name == currentBackground ? "true" : "false") + "}";
      first = false;
    }
    file = root.openNextFile();
  }
  json += "]";
  return json;
}

inline String getPlaylistsList() {
  String json = "[";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  bool first = true;
  
  while(file){
    String name = String(file.name());
    if(name.endsWith(".csv")){
      if(!first) json += ",";
      json += "{\"name\":\"" + name + "\",\"selected\":" + String(name == currentPlaylistFile ? "true" : "false") + "}";
      first = false;
    }
    file = root.openNextFile();
  }
  json += "]";
  return json;
}

#endif // WEB_INTERFACE_API_H
