#include "web_ota.h"

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

extern WebServer server;

static String g_otaError;

static void setOTAError(const String& message) {
  g_otaError = message;
  Serial.println("[OTA] " + message);
}

static String getOTAPageHTML() {
  String html = "<!DOCTYPE html><html lang='ru'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Radio OTA Update</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Segoe UI', Arial, sans-serif; background: linear-gradient(135deg, #111 0%, #252525 100%); color: #fff; min-height: 100vh; padding: 24px; }";
  html += ".wrap { max-width: 760px; margin: 0 auto; }";
  html += ".card { background: rgba(42, 42, 42, 0.96); border: 2px solid #00ffff; border-radius: 14px; padding: 24px; box-shadow: 0 18px 40px rgba(0, 255, 255, 0.08); }";
  html += "h1 { color: #00ffff; font-size: 28px; margin-bottom: 10px; }";
  html += "p { color: #c9d3d3; line-height: 1.5; margin-bottom: 12px; }";
  html += ".hint { color: #8fe68f; font-size: 14px; }";
  html += ".warn { color: #ffb366; font-size: 14px; }";
  html += ".actions { display: flex; gap: 12px; flex-wrap: wrap; margin-top: 16px; margin-bottom: 18px; }";
  html += ".btn, button { display: inline-block; background: linear-gradient(135deg, #00ffff, #00cccc); color: #000; padding: 12px 18px; border: none; border-radius: 8px; font-size: 15px; font-weight: 700; cursor: pointer; text-decoration: none; }";
  html += ".btn.secondary { background: rgba(0,255,255,0.12); color: #00ffff; border: 1px solid #00ffff; }";
  html += "input[type='file'] { width: 100%; padding: 12px; border-radius: 8px; border: 2px solid #444; background: #1f1f1f; color: #fff; margin: 14px 0; }";
  html += ".progress { width: 100%; height: 18px; background: #1a1a1a; border: 1px solid #355; border-radius: 999px; overflow: hidden; margin: 16px 0 8px; }";
  html += ".bar { width: 0%; height: 100%; background: linear-gradient(90deg, #00ffff, #00ff88); transition: width 0.15s ease; }";
  html += ".status { min-height: 24px; color: #00ffff; font-size: 14px; margin-top: 10px; white-space: pre-wrap; }";
  html += ".meta { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 12px; margin: 18px 0; }";
  html += ".meta-item { background: rgba(0,0,0,0.22); border-radius: 10px; padding: 12px; border-left: 3px solid #00ffff; }";
  html += ".meta-label { color: #8ca7a7; font-size: 12px; margin-bottom: 6px; }";
  html += ".meta-value { color: #fff; font-size: 15px; word-break: break-word; }";
  html += "@media (max-width: 640px) { body { padding: 14px; } .card { padding: 18px; } h1 { font-size: 24px; } }";
  html += "</style>";
  html += "<script>";
  html += "function uploadFirmware(){";
  html += "  var fileInput = document.getElementById('firmware');";
  html += "  var status = document.getElementById('status');";
  html += "  var bar = document.getElementById('bar');";
  html += "  if(!fileInput.files.length){ status.innerText = 'Выберите BIN-файл прошивки.'; return false; }";
  html += "  var file = fileInput.files[0];";
  html += "  if(!file.name.toLowerCase().endsWith('.bin')){ status.innerText = 'Нужен файл прошивки с расширением .bin'; return false; }";
  html += "  var data = new FormData();";
  html += "  data.append('firmware', file);";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/update', true);";
  html += "  xhr.upload.onprogress = function(e){";
  html += "    if(e.lengthComputable){";
  html += "      var percent = Math.round((e.loaded / e.total) * 100);";
  html += "      bar.style.width = percent + '%';";
  html += "      status.innerText = 'Загрузка: ' + percent + '%';";
  html += "    }";
  html += "  };";
  html += "  xhr.onload = function(){";
  html += "    status.innerText = xhr.responseText || 'Готово';";
  html += "    if(xhr.status === 200){";
  html += "      bar.style.width = '100%';";
  html += "    }";
  html += "  };";
  html += "  xhr.onerror = function(){ status.innerText = 'Ошибка загрузки OTA'; };";
  html += "  status.innerText = 'Начинаю OTA-обновление...';";
  html += "  xhr.send(data);";
  html += "  return false;";
  html += "}";
  html += "</script>";
  html += "</head><body><div class='wrap'><div class='card'>";
  html += "<h1>OTA Firmware Update</h1>";
  html += "<p>Здесь можно обновить прошивку через веб без USB-подключения. Текущее управление радио, загрузка фонов и плейлистов остаются как раньше.</p>";
  html += "<div class='meta'>";
  html += "<div class='meta-item'><div class='meta-label'>IP адрес</div><div class='meta-value'>" + WiFi.localIP().toString() + "</div></div>";
  html += "<div class='meta-item'><div class='meta-label'>Имя устройства</div><div class='meta-value'>" + String(WiFi.getHostname()) + "</div></div>";
  html += "</div>";
  html += "<p class='hint'>Используйте файл `firmware.bin`, который собирает PlatformIO.</p>";
  html += "<p class='warn'>Если меняли `partitions.csv`, один раз прошейте устройство через USB: web OTA не обновляет таблицу разделов.</p>";
  html += "<p class='warn'>Во время обновления не отключайте питание. После успешной записи устройство автоматически перезагрузится.</p>";
  html += "<div class='actions'>";
  html += "<a class='btn secondary' href='/'>Назад к управлению</a>";
  html += "</div>";
  html += "<form onsubmit='return uploadFirmware();'>";
  html += "<input type='file' id='firmware' name='firmware' accept='.bin' required>";
  html += "<button type='submit'>Обновить прошивку</button>";
  html += "</form>";
  html += "<div class='progress'><div id='bar' class='bar'></div></div>";
  html += "<div id='status' class='status'></div>";
  html += "</div></div></body></html>";
  return html;
}

void handleOTAPage() {
  server.send(200, "text/html", getOTAPageHTML());
}

void handleOTAUpdateResult() {
  const bool success = !Update.hasError();
  server.send(success ? 200 : 500, "text/plain",
              success ? "OTA update complete. Device will reboot in a moment."
                      : (g_otaError.isEmpty() ? "OTA update failed. Check serial log." : g_otaError));

  if (success) {
    delay(500);
    ESP.restart();
  }
}

void handleOTAUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    g_otaError = "";
    const esp_partition_t* nextPartition = esp_ota_get_next_update_partition(nullptr);
    if (nextPartition == nullptr) {
      setOTAError("OTA failed: next OTA partition not found. Flash once via USB after changing partitions.");
      return;
    }

    Serial.printf("[OTA] Upload start: %s\n", upload.filename.c_str());
    Serial.printf("[OTA] Next partition: label=%s address=0x%08X size=%u bytes\n",
                  nextPartition->label,
                  static_cast<unsigned>(nextPartition->address),
                  static_cast<unsigned>(nextPartition->size));

    if (!Update.begin(nextPartition->size, U_FLASH)) {
      setOTAError("OTA failed: cannot start update in target partition.");
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.hasError()) {
      return;
    }

    const size_t written = Update.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      setOTAError("OTA failed while writing firmware to flash.");
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.hasError()) {
      return;
    }

    if (Update.end(true)) {
      Serial.printf("[OTA] Upload success: %u bytes\n", upload.totalSize);
    } else {
      setOTAError("OTA failed while finalizing firmware image.");
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    setOTAError("OTA upload was aborted.");
  }
}
