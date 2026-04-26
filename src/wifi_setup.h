// ============================================
// WiFi Setup Web Interface
// wifi_setup.h
// ============================================

#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <WebServer.h>
#include <WiFi.h>
#include <Preferences.h>

extern WebServer server;
extern Preferences prefs;

inline String getWiFiSetupHTML() {
  String html = "<!DOCTYPE html><html lang='ru'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>S3-Go!-light - WiFi Setup</title>";
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "html, body { height: 100%; width: 100%; }";
  html += "body { font-family: 'Segoe UI', Arial, sans-serif; background: linear-gradient(135deg, #1a1a1a 0%, #2d2d2d 100%); color: #fff; display: flex; align-items: center; justify-content: center; min-height: 100vh; margin: 0; }";
  html += ".container { background: rgba(42, 42, 42, 0.95); padding: 40px; border-radius: 15px; border: 2px solid #00ffff; box-shadow: 0 20px 60px rgba(0, 255, 255, 0.1); max-width: 450px; width: 90%; }";
  html += ".title { color: #00ffff; font-size: 28px; font-weight: 700; margin-bottom: 10px; text-align: center; }";
  html += ".subtitle { color: #888; font-size: 14px; margin-bottom: 30px; text-align: center; }";
  html += ".form-group { margin-bottom: 20px; }";
  html += ".password-group { position: relative; }";
  html += "label { display: block; color: #00ffff; font-weight: 600; margin-bottom: 8px; font-size: 14px; }";
  html += "input { width: 100%; padding: 12px 14px; border: 2px solid #444; border-radius: 8px; background: #222; color: #fff; font-size: 16px; transition: all 0.25s ease; }";
  html += ".password-group input { padding-right: 45px; }";
  html += "input:focus { outline: none; border-color: #00ffff; box-shadow: 0 0 10px rgba(0, 255, 255, 0.2); }";
  html += ".toggle-password { position: absolute; right: 12px; top: 38px; cursor: pointer; user-select: none; font-size: 20px; color: #888; transition: color 0.2s; }";
  html += ".toggle-password:hover { color: #00ffff; }";
  html += "button { width: 100%; padding: 14px; background: linear-gradient(135deg, #00ffff, #00cccc); color: #000; border: none; border-radius: 8px; font-size: 16px; font-weight: 700; cursor: pointer; transition: all 0.25s ease; margin-top: 10px; }";
  html += "button:hover { transform: translateY(-2px); box-shadow: 0 8px 25px rgba(0, 255, 255, 0.3); }";
  html += "button:active { transform: translateY(0); }";
  html += ".networks { margin-top: 30px; }";
  html += ".networks-title { color: #00ff00; font-size: 14px; font-weight: 600; margin-bottom: 12px; }";
  html += ".network-list { background: rgba(0, 0, 0, 0.3); border-radius: 8px; max-height: 200px; overflow-y: auto; border: 1px solid #333; }";
  html += ".network-item { padding: 10px 12px; cursor: pointer; transition: all 0.15s ease; border-bottom: 1px solid #333; display: flex; justify-content: space-between; align-items: center; }";
  html += ".network-item:last-child { border-bottom: none; }";
  html += ".network-item:hover { background: rgba(0, 255, 255, 0.05); padding-left: 14px; }";
  html += ".network-name { color: #00ffff; font-weight: 600; }";
  html += ".network-signal { font-size: 12px; color: #888; }";
  html += ".loading { display: none; text-align: center; color: #00ff00; margin: 15px 0; }";
  html += ".spinner { display: inline-block; width: 16px; height: 16px; border: 2px solid #444; border-top-color: #00ffff; border-radius: 50%; animation: spin 0.8s linear infinite; }";
  html += "@keyframes spin { to { transform: rotate(360deg); } }";
  html += ".error { background: rgba(255, 68, 68, 0.1); color: #ff6666; padding: 12px; border-radius: 6px; margin-bottom: 15px; border-left: 4px solid #ff4444; display: none; }";
  html += ".success { background: rgba(0, 255, 0, 0.1); color: #66ff66; padding: 12px; border-radius: 6px; margin-bottom: 15px; border-left: 4px solid #00ff00; display: none; }";
  html += "</style>";
  
  html += "<script>";
  html += "let wifiList = [];";
  html += "function togglePassword() {";
  html += "  const pwdInput = document.getElementById('password');";
  html += "  const eyeIcon = document.getElementById('eyeIcon');";
  html += "  if (pwdInput.type === 'password') {";
  html += "    pwdInput.type = 'text';";
  html += "    eyeIcon.textContent = '🙈';";
  html += "  } else {";
  html += "    pwdInput.type = 'password';";
  html += "    eyeIcon.textContent = '👁️';";
  html += "  }";
  html += "}";
  html += "function scanNetworks() {";
  html += "  document.querySelector('.loading').style.display = 'block';";
  html += "  fetch('/scan').then(r => r.json()).then(data => {";
  html += "    wifiList = data;";
  html += "    renderNetworks();";
  html += "    document.querySelector('.loading').style.display = 'none';";
  html += "  }).catch(e => {";
  html += "    document.querySelector('.loading').style.display = 'none';";
  html += "    showError('Scan failed');";
  html += "  });";
  html += "}";
  html += "function renderNetworks() {";
  html += "  let html = '';";
  html += "  wifiList.forEach(net => {";
  html += "    html += '<div class=\"network-item\" onclick=\"selectNetwork(\\''+net.ssid.replace(/'/g, \"\\\\'\")+'\\')\">'+";
  html += "      '<span class=\"network-name\">'+net.ssid+'</span>'+";
  html += "      '<span class=\"network-signal\">'+net.rssi+' dBm</span>'+";
  html += "    '</div>';";
  html += "  });";
  html += "  document.querySelector('.network-list').innerHTML = html || '<div style=\"padding:10px;color:#888\">No networks found</div>';";
  html += "}";
  html += "function selectNetwork(ssid) {";
  html += "  document.getElementById('ssid').value = ssid;";
  html += "  document.getElementById('password').focus();";
  html += "}";
  html += "function saveWiFi() {";
  html += "  const ssid = document.getElementById('ssid').value.trim();";
  html += "  const pass = document.getElementById('password').value.trim();";
  html += "  if(!ssid) { showError('SSID required'); return; }";
  html += "  if(!pass) { showError('Password required'); return; }";
  html += "  document.querySelector('.loading').style.display = 'block';";
  html += "  fetch('/savewifi?ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)).then(r => r.text()).then(() => {";
  html += "    showSuccess('WiFi saved! Rebooting...');";
  html += "    setTimeout(() => location.reload(), 2000);";
  html += "  }).catch(() => {";
  html += "    document.querySelector('.loading').style.display = 'none';";
  html += "    showError('Save failed');";
  html += "  });";
  html += "}";
  html += "function showError(msg) { const el = document.querySelector('.error'); el.innerText = '❌ '+msg; el.style.display = 'block'; }";
  html += "function showSuccess(msg) { const el = document.querySelector('.success'); el.innerText = '✅ '+msg; el.style.display = 'block'; }";
  html += "window.addEventListener('load', () => { scanNetworks(); });";
  html += "document.addEventListener('keypress', (e) => { if(e.key === 'Enter' && e.ctrlKey) saveWiFi(); });";
  html += "</script>";
  
  html += "</head><body>";
  html += "<div class='container'>";
  html += "  <div class='title'>📡 WiFi Setup</div>";
  html += "  <div class='subtitle'>ESP32 Radio - Configure Network</div>";
  html += "  <div class='error'></div>";
  html += "  <div class='success'></div>";
  
  html += "  <div class='form-group'>";
  html += "    <label>Network (SSID)</label>";
  html += "    <input type='text' id='ssid' placeholder='Network name' autocomplete='off'>";
  html += "  </div>";
  
  html += "  <div class='form-group password-group'>";
  html += "    <label>Password</label>";
  html += "    <input type='password' id='password' placeholder='Network password' autocomplete='off'>";
  html += "    <span class='toggle-password' onclick='togglePassword()' id='eyeIcon'>👁️</span>";
  html += "  </div>";
  
  html += "  <button onclick='saveWiFi()'>💾 Save WiFi & Connect</button>";
  
  html += "  <div class='networks'>";
  html += "    <div class='networks-title'>Available Networks (tap to select)</div>";
  html += "    <div class='network-list'></div>";
  html += "    <button onclick='scanNetworks()' style='margin-top:12px;background:rgba(0,255,255,0.2);color:#00ffff'>🔄 Rescan</button>";
  html += "  </div>";
  
  html += "  <div class='loading'>";
  html += "    <div class='spinner'></div>";
  html += "    <div style='margin-top:10px'>Scanning...</div>";
  html += "  </div>";
  
  html += "</div>";
  html += "</body></html>";
  
  return html;
}

inline void handleWiFiSetupPage() {
  server.send(200, "text/html", getWiFiSetupHTML());
}

inline void handleWiFiScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  
  server.send(200, "application/json", json);
}

inline void handleWiFiSave() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    
    // Save to preferences
    Preferences prefs;
    prefs.begin("radio", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);
    prefs.end();
    
    //Serial.printf("WiFi saved - SSID: %s\n", ssid.c_str());
    
    server.send(200, "text/plain", "OK");
    
    // Reboot after short delay
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

#endif // WIFI_SETUP_H