#ifndef DISPLAY_CONFIG_STORE_H
#define DISPLAY_CONFIG_STORE_H

#include <vector>
#include <WString.h>

const char* getDefaultDisplayConfigFilename();
bool saveDisplayConfigToFile(const String& filename, String* errorMessage = nullptr);
bool loadDisplayConfigFromFile(const String& filename, String* errorMessage = nullptr);
bool collectDisplayConfigAssetsFromFile(const String& filename, std::vector<String>& assetFilenames, String* errorMessage = nullptr);
String buildCurrentProfileHeader();

#endif // DISPLAY_CONFIG_STORE_H
