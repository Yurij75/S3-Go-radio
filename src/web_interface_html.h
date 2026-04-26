// ============================================
// HTML Generator for Web Interface
// ============================================

#ifndef WEB_INTERFACE_HTML_H
#define WEB_INTERFACE_HTML_H

#include "web_interface_sections.h"
#include "web_interface_styles.h"
#include "web_interface_scripts.h"

inline String getWebInterfaceHTML() {
  String html = "<!DOCTYPE html><html lang='ru'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>S3 Radio Control</title>";
  html += "<link rel='stylesheet' href='/thems.css'>";
  html += "<style>";
  html += getStyles();
  html += "</style>";
  html += "<script>";
  html += getScripts();
  html += "</script>";
  html += "</head><body>";

  html += getTopBar();
  html += "<div class='main-content'>";
  html += getPlayerSection();
  html += getPlaylistEditorSection();
  html += getNeedlesSection();
  html += getTextPosSection();
  html += getNetworkSection();
  html += "</div>";
  html += "<div id='statusBar' class='status-bar'></div>";

  html += "<script>";
  html += getInitScript();
  html += "</script>";
  html += "</body></html>";

  return html;
}

#endif // WEB_INTERFACE_HTML_H
