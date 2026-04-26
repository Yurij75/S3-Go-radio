// web_interface.h
#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <WebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

// External declarations
extern WebServer server;
extern String currentStreamName;
extern String currentPlaylistFile;
extern String currentBackground;
extern int currentVolume;
extern int streamCount;
extern int currentStreamIndex;
extern bool radioStarted;
extern bool isPaused;
extern bool ledMusicEnabled;
extern int ledStripCount;
extern int ledEffectIndex;

extern int needlePosLX;
extern int needlePosLY;
extern int needlePosRX;
extern int needlePosRY;
extern int needleMinAngle;
extern int needleMaxAngle;
extern int needleUpSpeed;
extern int needleDownSpeed;
extern int needleThickness;
extern int needleLenMain;
extern int needleLenRed;
extern int needleCY;
extern int needleSpriteWidth;
extern int needleSpriteHeight;
extern bool debugBordersEnabled;
extern bool leftNeedleMirrored;
extern int needleRotation;

extern int stationNameX;
extern int stationNameY;
extern int streamTitle1X;
extern int streamTitle1Y;
extern int streamTitle2X;
extern int streamTitle2Y;
extern int stationScrollWidth;
extern int title1ScrollWidth;
extern int title2ScrollWidth;
extern int marqueePauseMs;
extern int bitrateX;
extern int bitrateY;
extern int clockX;
extern int clockY;
extern int volumeX;
extern int volumeY;
extern int ipX;
extern int ipY;
extern bool primaryMarqueeEnabled;
extern bool secondaryMarqueeEnabled;
extern bool quoteMarqueeEnabled;
extern int quoteX;
extern int quoteY;
extern int quoteW;
extern String quoteApiUrl;
extern int vuBarsHeight;
extern int vuBarsSegments;
extern int vuBarsOffsetX;
extern int vuBarsOffsetY;
extern int vuCardioX;
extern int vuCardioY;
extern int vuCardioW;
extern int vuCardioH;

// Function declarations
String getStreamName(int idx);

// WiFi setup functions
void handleWiFiSetupPage();
void handleWiFiScan();
void handleWiFiSave();

// Include other parts
#include "web_interface_api.h"
#include "web_interface_html.h"

#endif
