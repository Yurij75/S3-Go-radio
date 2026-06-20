// ============================================
// HTML Sections for Web Interface
// ============================================

#ifndef WEB_INTERFACE_SECTIONS_H
#define WEB_INTERFACE_SECTIONS_H

#include <Audio.h>

#include "config.h"
#include "backlight_control.h"
#include "ir_remote.h"
#include "screens/screens.h"

extern Audio audio;
extern int currentVolume;
extern bool screensaverEnabled;
extern int screensaverIdleSeconds;

inline String buildNumberStepper(const String& inputId, long value, long minValue, long maxValue, long stepValue,
                                 const String& onInputJs, const String& onChangeJs = String(),
                                 const String& inputClass = String(), const String& wrapperClass = String()) {
  String html = "<div class='number-stepper";
  if (wrapperClass.length()) html += " " + wrapperClass;
  html += "'>";
  html += "<button type='button' class='stepper-btn' onclick=\"stepNumberInput('" + inputId + "',-" + String(stepValue) + ")\">-</button>";
  html += "<input type='number' id='" + inputId + "' min='" + String(minValue) + "' max='" + String(maxValue) + "' step='" + String(stepValue) + "' value='" + String(value) + "'";
  if (inputClass.length()) html += " class='" + inputClass + "'";
  html += " onblur='normalizeNumberInput(this)'";
  if (onInputJs.length()) html += " oninput=\"" + onInputJs + "\"";
  if (onChangeJs.length()) html += " onchange=\"" + onChangeJs + "\"";
  html += ">";
  html += "<button type='button' class='stepper-btn' onclick=\"stepNumberInput('" + inputId + "'," + String(stepValue) + ")\">+</button>";
  html += "</div>";
  return html;
}

inline String getTopBar() {
  return String();
}

inline String getPlayerSection() {
  String html = "<div id='player' class='content-section active player-full-height'>";
  
  // --- Player block (top, фиксированная высота под контент) ---
  html += "  <div class='player-wrapper'>";
  html += "    <div class='hero-card now-playing'>";
  html += "      <div class='player-topline'>";
  html += "        <div class='player-display'>";
  html += "          <div class='track-line' id='playlistStationLine'>" + currentStreamName + "</div>";
  html += "          <div class='station-name' id='stationName'><span id='stationNameText'>" + currentStreamName + "</span></div>";
  html += "          <div class='track-line' id='trackLine'>---</div>";
  html += "          <div class='tech-line' id='techLine'>---</div>";
  html += "        </div>";
  html += "        <div class='player-side-actions'>";
  html += "          <button class='section-nav-btn player-side-btn' type='button' data-section='display' onclick=\"showSection('display')\">Setting</button>";
  html += "          <button class='section-nav-btn player-side-btn' type='button' id='timerTopBtn' onclick='toggleTimerDropdown()'>Timer</button>";
  html += "          <div class='screen-split-nav player-side-btn' id='screenSplitNav'>";
  html += "            <button class='screen-split-btn screen-split-left' type='button' onclick='stepScreen(-1)' aria-label='Previous screen'>&lt;</button>";
  html += "            <button class='screen-split-btn screen-split-right' type='button' onclick='stepScreen(1)' aria-label='Next screen'>&gt;</button>";
  html += "          </div>";
  html += "        </div>";
  html += "      </div>";
  html += "      <div class='status-text' id='statusText'></div>";
  html += "      <div class='player-controls'>";
  html += "        <button class='player-btn utility' id='playlistQuickBtn' onclick='togglePlaylistDropdown()'>List</button>";
  html += "        <button class='player-btn' onclick='prevStation()'>Prev</button>";
  html += "        <button class='player-btn primary' id='playBtn' onclick='playPause()'>Play</button>";
  html += "        <button class='player-btn' onclick='nextStation()'>Next</button>";
  html += "        <button class='player-btn utility' id='eqQuickBtn' onclick='toggleEqDropdown()'>EQ</button>";
  html += "      </div>";
  html += "      <div class='quick-playlists' id='quickPlaylistsContainer'>";
  html += "        <div class='playlist-dropdown-list' id='playlistDropdownList'>";
  html += "          <div id='playlistDropdownContent'>Loading...</div>";
  html += "        </div>";
  html += "      </div>";
  html += "      <div class='quick-playlists quick-timer' id='quickTimerContainer'>";
  html += "        <div class='playlist-dropdown-list timer-dropdown-list' id='timerDropdownList'>";
  html += "          <div class='timer-dropdown-content'>";
  html += "            <div class='timer-dropdown-header'><span>Sleep timer</span><span class='sleep-timer-status' id='sleepTimerStatus'>OFF</span></div>";
  html += "            <div class='timer-options' id='sleepTimerOptions'>";
  html += "              <button type='button' class='timer-option' data-minutes='0' onclick='setSleepTimer(0)'>OFF</button>";
  html += "              <button type='button' class='timer-option' data-minutes='15' onclick='setSleepTimer(15)'>15 min</button>";
  html += "              <button type='button' class='timer-option' data-minutes='30' onclick='setSleepTimer(30)'>30 min</button>";
  html += "              <button type='button' class='timer-option' data-minutes='45' onclick='setSleepTimer(45)'>45 min</button>";
  html += "              <button type='button' class='timer-option' data-minutes='60' onclick='setSleepTimer(60)'>60 min</button>";
  html += "              <button type='button' class='timer-option' data-minutes='75' onclick='setSleepTimer(75)'>75 min</button>";
  html += "              <button type='button' class='timer-option' data-minutes='90' onclick='setSleepTimer(90)'>90 min</button>";
  html += "              <button type='button' class='timer-option' data-minutes='99' onclick='setSleepTimer(99)'>99 min</button>";
  html += "            </div>";
  html += "          </div>";
  html += "        </div>";
  html += "      </div>";
  html += "      <div class='quick-playlists quick-timer' id='quickEqContainer'>";
  html += "        <div class='playlist-dropdown-list timer-dropdown-list' id='eqDropdownList'>";
  html += "          <div class='timer-dropdown-content eq-dropdown-content'>";
  html += "            <div class='eq-sliders'>";
  html += "              <div class='eq-slider-row'><span>Balance <strong id='eqBalanceVal'>0</strong></span><div class='eq-range-wrap'><input id='eqBalance' type='range' min='-16' max='16' step='1' value='0' oninput=\"updateEqSliderLabel('Balance', this.value, 'eqBalanceVal'); scheduleEqUpdate()\"></div></div>";
  html += "              <div class='eq-slider-row'><span>ВЧ <strong id='eqTrebleVal'>0</strong></span><div class='eq-range-wrap'><input id='eqTreble' type='range' min='-12' max='12' step='1' value='0' oninput=\"updateEqSliderLabel('ВЧ', this.value, 'eqTrebleVal'); scheduleEqUpdate()\"></div></div>";
  html += "              <div class='eq-slider-row'><span>СЧ <strong id='eqMidVal'>0</strong></span><div class='eq-range-wrap'><input id='eqMid' type='range' min='-12' max='12' step='1' value='0' oninput=\"updateEqSliderLabel('СЧ', this.value, 'eqMidVal'); scheduleEqUpdate()\"></div></div>";
  html += "              <div class='eq-slider-row'><span>НЧ <strong id='eqBassVal'>0</strong></span><div class='eq-range-wrap'><input id='eqBass' type='range' min='-12' max='12' step='1' value='0' oninput=\"updateEqSliderLabel('НЧ', this.value, 'eqBassVal'); scheduleEqUpdate()\"></div></div>";
  html += "            </div>";
  html += "          </div>";
  html += "        </div>";
  html += "      </div>";
  html += "      <div class='volume-control'>";
  html += "        <label class='volume-label'><span>Volume</span><span class='volume-value' id='volDisplay'>" + String(currentVolume) + "</span></label>";
  html += "        <div class='volume-combo'>";
  html += "          <button class='volume-step-btn' type='button' onclick='stepVolume(-1)'>-</button>";
  html += "          <div class='volume-slider-wrap'><input id='volumeSlider' type='range' min='0' max='254' value='" + String(currentVolume) + "' oninput='setVolume(this.value)'></div>";
  html += "          <button class='volume-step-btn' type='button' onclick='stepVolume(1)'>+</button>";
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";
  html += "  </div>";

  // --- Playlist block (низ, растягивается на оставшуюся высоту) ---
  html += "  <div class='playlist-container playlist-fill'>";
  html += "    <div class='playlist-header compact'>";
  html += "      <div class='playlist-summary'>";
  html += "        <button class='small playlist-summary-edit playlist-summary-left' onclick='editCurrentPlaylist()'>Edit</button>";
  html += "        <div class='playlist-summary-main'>";
  html += "          <div class='playlist-summary-marquee'><span id='currentPlaylistName'>" + currentPlaylistFile + "</span></div>";
  html += "          <div class='playlist-summary-dot'>&bull;</div>";
  html += "          <div class='playlist-summary-count'><span id='streamCountChip'>" + String(streamCount) + "</span><span class='stream-count-full'> streams</span><span class='stream-count-short'> str.</span></div>";
  html += "        </div>";
  html += "        <div class='playlist-summary-actions'>";
  html += "          <button class='small playlist-summary-edit' onclick='openMyPlaylist()'>Фав</button>";
  html += "          <button class='small playlist-summary-edit' id='backPlaylistBtn' onclick='goBackToLastPlaylist()'>Back</button>";
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";
  html += "    <div class='streams-window' id='streamsWindow'><div id='streamsList'></div></div>";
  html += "  </div>";
  
  html += "</div>";
  return html;
}

inline String getUploadSection() {
  return String()
    + "    <div class='section'>"
    + "      <div class='section-header'><div><div class='section-title'>Files, backgrounds & playlists</div><div class='section-note'>Upload assets, manage playlists and save display configs</div></div></div>"
    + "      <div class='section-grid files-grid'>"
    + "        <div class='stack'>"
    + "          <div class='setting-item'>"
    + "            <h3>Upload files</h3>"
    + "            <input type='file' id='uploadFiles' accept='.jpg,.jpeg,.png,.csv,.json,.tar,.m3u,.m3u8,.pls,.txt' multiple>"
    + "            <div class='actions-row'><button onclick='uploadSelectedFiles()'>Upload selected files</button></div>"
    + "            <div id='uploadProgress' class='upload-progress'>"
    + "              <div class='upload-progress-track'><div id='uploadProgressFill' class='upload-progress-fill'></div></div>"
    + "              <div id='uploadProgressText' class='upload-progress-text'>0%</div>"
    + "            </div>"
    + "          </div>"
    + "          <div class='setting-item'>"
    + "            <h3>Display config</h3>"
    + "            <div class='actions-row'>"
    + "              <button onclick='saveDisplayConfig()'>Save config</button>"
    + "              <button onclick='loadDisplayConfig()'>Load config</button>"
    + "              <button onclick=\"downloadFile('/display_config.json')\">Download JSON</button>"
    + "              <button onclick='downloadDisplayPackage()'>Export package</button>"
    + "              <button onclick='downloadProfileHeader()'>Download header</button>"
    + "            </div>"
    + "          </div>"
    + "        </div>"
    + "        <div class='stack'>"
    + "          <div class='section collapsible'>"
    + "            <button class='collapse-toggle' type='button' onclick='toggleCollapse(this)' aria-expanded='false'><span>Backgrounds</span><span class='collapse-icon'>+</span></button>"
    + "            <div class='collapse-content'><div class='section-note'>Select or delete local background images</div><div id='bgList'></div></div>"
    + "          </div>"
    + "          <div class='section collapsible'>"
    + "            <button class='collapse-toggle' type='button' onclick='toggleCollapse(this)' aria-expanded='false'><span>Playlists</span><span class='collapse-icon'>+</span></button>"
    + "            <div class='collapse-content'><div class='section-note'>Switch, download or open the editor</div><div id='plList'></div></div>"
    + "          </div>"
    + "        </div>"
    + "      </div>"
    + "    </div>";
}

inline String getPlaylistEditorSection() {
  return String()
    + "<div id='playlistEditor' class='content-section'>"
    + "  <div class='section'>"
    + "    <div class='section-header'><div><div class='section-title'>Playlist editor</div><div class='section-note'>Edit station names and stream URLs</div></div></div>"
    + "    <h2><span id='editorTitle'></span></h2>"
    + "    <div class='playlist-editor'>"
    + "      <table id='plEditorTable'><thead><tr><th>Station Name</th><th>Stream URL</th><th style='width:80px'></th></tr></thead><tbody></tbody></table>"
    + "    </div>"
    + "    <div class='actions-row'>"
    + "      <button onclick='addRow()'>Add row</button>"
    + "      <button onclick='moveUp()'>Move up</button>"
    + "      <button onclick='moveDown()'>Move down</button>"
    + "      <button onclick='savePlaylist()'>Save playlist</button>"
    + "      <button onclick=\"showSection('player')\">Close</button>"
    + "    </div>"
    + "  </div>"
    + "</div>";
}

inline String getRemoteSection() {
  return String()
    + "<div id='remote' class='content-section'>"
    + "  <div class='section-shell'>"
    + "    <div class='section'>"
    + "      <div class='section-header'><div><div class='section-title'>IR remote</div><div class='section-note'>Receiver and button mapping saved in " + String(irRemoteConfigFilename()) + "</div></div><button class='section-nav-btn compact' type='button' onclick=\"showSection('player')\">Player</button></div>"
    + "      <div class='subsection-grid'>"
    + "        <div class='setting-item'><h3>Receiver</h3><label><input type='checkbox' id='remoteEnabled' onchange='saveRemoteBasics()'> Enabled</label><label>GPIO pin<input type='number' id='remotePin' min='0' max='255' value='255' onchange='saveRemoteBasics()'></label><label>Repeat delay, ms<input type='number' id='remoteRepeatDelay' min='0' max='2000' value='300' onchange='saveRemoteBasics()'></label><div class='section-note'>Use 255 to keep the receiver disabled.</div></div>"
    + "        <div class='setting-item'><h3>Last received code</h3><div class='info-line'><span class='info-label'>Command</span><span class='info-value' id='remoteLastCommand'>-</span></div><div class='info-line'><span class='info-label'>Address</span><span class='info-value' id='remoteLastAddress'>-</span></div><div class='info-line'><span class='info-label'>Value</span><span class='info-value' id='remoteLastValue'>-</span></div><div class='info-line'><span class='info-label'>Protocol</span><span class='info-value' id='remoteLastProtocol'>-</span></div><div class='actions-row'><button type='button' onclick='refreshRemoteLast()'>Refresh</button><button type='button' onclick=\"downloadFile('/ir_remote_config.json')\">Download JSON</button></div></div>"
    + "      </div>"
    + "    </div>"
    + "    <div class='section'>"
    + "      <div class='section-header'><div><div class='section-title'>Button mapping</div><div class='section-note'>Press a button on the remote, refresh the last code, then assign it to an action</div></div></div>"
    + "      <div class='remote-map' id='remoteMap'>Loading...</div>"
    + "    </div>"
    + "  </div>"
    + "</div>";
}

inline String getNeedlesSection() {
  String html = "<div id='display' class='content-section'>";
  html += "  <div class='section-shell'>";
  html += "    <div class='display-sticky-bar'>";
  html += "      <div class='display-quick-grid'>";
  html += "        <div class='screen-btns screen-btns-sticky' id='screenButtonsDisplay'></div>";
  html += "        <div class='display-action-stack'>";
  html += "          <button type='button' class='section-nav-btn compact top-action-btn led-top-btn' id='ledTopToggleBtn' onclick='toggleLed()'>LED</button>";
  html += "          <button type='button' class='screen-btn' id='flacTopBtn' onclick=\"selectScreen('flac')\">FLAC</button>";
  html += "          <button type='button' class='section-nav-btn compact top-action-btn player-return-btn' onclick=\"showSection('player')\">Player</button>";
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";
  // Put frequent tasks first: file upload + backgrounds/playlists.
  html += getUploadSection();
  html += "    <div class='section'>";
  html += "      <div class='section-header'><div><div class='section-title'>Display basics</div><div class='section-note'>Quick controls for rotation and screen selection</div></div></div>";
  html += "      <div class='subsection-grid'>";
  html += "        <div class='setting-item'><h3>Rotation</h3><div class='screen-btns'><button class='screen-btn' onclick='setRotation(0)'>0°</button><button class='screen-btn' onclick='setRotation(1)'>90°</button><button class='screen-btn' onclick='setRotation(2)'>180°</button><button class='screen-btn' onclick='setRotation(3)'>270°</button></div></div>";
  html += "        <div class='setting-item'><h3>Web theme</h3><div class='screen-btns'><button type='button' class='screen-btn' id='themeLightBtn' onclick=\"setWebTheme('light')\">Light</button><button type='button' class='screen-btn' id='themeDarkBtn' onclick=\"setWebTheme('dark')\">Dark</button></div><div class='section-note'>Saved in this browser</div></div>";
  html += "        <div class='setting-item'><h3>Активные окна</h3><div class='screen-active-list' id='screenActiveList'></div><div class='section-note'>В круге остаются только отмеченные окна</div></div>";
  html += "        <div class='setting-item'><h3>Brightness</h3><label>Level: <span id='tftBrightnessVal'>" + String(getTftBrightness()) + "</span></label>" + buildNumberStepper("tftBrightness", getTftBrightness(), 0, 255, 1, "document.getElementById('tftBrightnessVal').innerText=this.value", "safeFetch('/setbrightness?b='+this.value)") + "<div class='section-note'>0 = off</div></div>";
  html += "        <div class='setting-item'><h3>Инверсия дисплея</h3><div class='actions-row'><button id='invertDisplayBtn' onclick='toggleDisplayInvert()'>Loading...</button></div><div class='section-note'>Негатив / Позитив всего экрана</div></div>";
  html += "        <div class='setting-item'><h3>VU meter</h3><div class='actions-row'><button id='vuMeterToggleBtn' onclick='toggleVuMeter()'>Loading...</button></div><div class='section-note' id='vuMeterStatus'>Loading...</div></div>";
  html += "        <div class='setting-item'><h3>Имя станции</h3><div class='actions-row'><button id='marquee1ToggleBtn' onclick='toggleMarquee1()'>Loading...</button></div><div class='section-note' id='marquee1Status'>Per screen setting</div></div>";
  html += "        <div class='setting-item'><h3>Исполнитель</h3><div class='actions-row'><button id='marquee2ToggleBtn' onclick='toggleMarquee2()'>Loading...</button></div><div class='section-note' id='marquee2Status'>Per screen setting</div></div>";
  html += "        <div class='setting-item'><h3>Размер строк станции</h3><div class='actions-row'><button type='button' id='stationFontSmallBtn' onclick=\"setStationFontSize('small')\">A-</button><button type='button' id='stationFontNormalBtn' onclick=\"setStationFontSize('normal')\">A</button><button type='button' id='stationFontLargeBtn' onclick=\"setStationFontSize('large')\">A+</button></div><div class='section-note' id='stationFontStatus'>Loading...</div></div>";
  html += "      </div>";
  html += "    </div>";
  // html += "    <div class='section collapsible'>";
  // html += "      <button class='collapse-toggle' type='button' onclick='toggleCollapse(this)' aria-expanded='false'><span>VU Meter 1</span><span class='collapse-icon'>+</span></button>";
  // html += "      <div class='collapse-content'>";
  // html += "      <div class='settings-grid'>";
  // html += "      <div class='setting-item'><label>Bar Height: <span id='vuBarHeightVal'>" + String(vuBarsHeight) + "</span></label>" + buildNumberStepper("vuBarHeight", vuBarsHeight, 6, 120, 1, "document.getElementById('vuBarHeightVal').innerText=this.value; safeFetch('/setvubars?h='+this.value)") + "</div>";
  // html += "      <div class='setting-item'><label>Segments: <span id='vuBarSegVal'>" + String(vuBarsSegments) + "</span></label>" + buildNumberStepper("vuBarSeg", vuBarsSegments, 4, 40, 1, "document.getElementById('vuBarSegVal').innerText=this.value; safeFetch('/setvubars?seg='+this.value)") + "</div>";
  // html += "      <div class='setting-item'><label>Position X: <span id='vuBarXVal'>" + String(vuBarsOffsetX) + "</span></label>" + buildNumberStepper("vuBarX", vuBarsOffsetX, -200, 200, 1, "document.getElementById('vuBarXVal').innerText=this.value; safeFetch('/setvubars?x='+this.value)") + "</div>";
  // html += "      <div class='setting-item'><label>Position Y: <span id='vuBarYVal'>" + String(vuBarsOffsetY) + "</span></label>" + buildNumberStepper("vuBarY", vuBarsOffsetY, -200, 200, 1, "document.getElementById('vuBarYVal').innerText=this.value; safeFetch('/setvubars?y='+this.value)") + "</div>";
  // html += "      <div class='setting-item'><h3>Bar Gradient</h3><label>Bottom: <input type='color' id='vuBarColorBottom' onchange=\"safeFetch('/setvubars?bottom='+hexToRgb565(this.value).toString(16))\"></label><label>Top: <input type='color' id='vuBarColorTop' onchange=\"safeFetch('/setvubars?top='+hexToRgb565(this.value).toString(16))\"></label></div>";
  // html += "      </div>";
  // html += "    </div>";
  // html += "      </div>";
  html += "    <div class='section collapsible'>";
  html += "      <button class='collapse-toggle' type='button' onclick='toggleCollapse(this)' aria-expanded='false'><span>FFT</span><span class='collapse-icon'>+</span></button>";
  html += "      <div class='collapse-content'>";
  html += "      <div class='settings-grid'>";
  html += "      <div class='setting-item'><label>Window X: <span id='fftXVal'>0</span></label>" + buildNumberStepper("fftX", 0, 0, 480, 1, "document.getElementById('fftXVal').innerText=this.value; safeFetch('/setfft?x='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Window Y: <span id='fftYVal'>0</span></label>" + buildNumberStepper("fftY", 0, 0, 320, 1, "document.getElementById('fftYVal').innerText=this.value; safeFetch('/setfft?y='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Width: <span id='fftWVal'>320</span></label>" + buildNumberStepper("fftW", 320, 40, 480, 1, "document.getElementById('fftWVal').innerText=this.value; safeFetch('/setfft?w='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Height: <span id='fftHVal'>240</span></label>" + buildNumberStepper("fftH", 240, 40, 320, 1, "document.getElementById('fftHVal').innerText=this.value; safeFetch('/setfft?h='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Bands: <span id='fftBandsVal'>16</span></label>" + buildNumberStepper("fftBands", 16, 1, FFT_BANDS, 1, "document.getElementById('fftBandsVal').innerText=this.value; safeFetch('/setfft?bands='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Segments: <span id='fftSegmentsVal'>20</span></label>" + buildNumberStepper("fftSegments", 20, 1, 80, 1, "document.getElementById('fftSegmentsVal').innerText=this.value; safeFetch('/setfft?segments='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Min bin: <span id='fftMinBinVal'>0</span></label>" + buildNumberStepper("fftMinBin", 0, 0, FFT_BANDS - 1, 1, "document.getElementById('fftMinBinVal').innerText=this.value; safeFetch('/setfft?minBin='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Max bin: <span id='fftMaxBinVal'>" + String(FFT_BANDS - 1) + "</span></label>" + buildNumberStepper("fftMaxBin", FFT_BANDS - 1, 0, FFT_BANDS - 1, 1, "document.getElementById('fftMaxBinVal').innerText=this.value; safeFetch('/setfft?maxBin='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>FFT profile</label><select id='fftProfile' onchange=\"safeFetch('/setfft?profile='+this.value)\"><option value='0'>Balanced RMS</option><option value='1'>Classic 32</option><option value='2'>Smooth Mel</option></select></div>";
  html += "      <div class='setting-item'><label>Aggregation</label><select id='fftAggregation' onchange=\"safeFetch('/setfft?aggregation='+this.value)\"><option value='0'>Peak</option><option value='1'>Average</option><option value='2'>RMS</option></select></div>";
  html += "      <div class='setting-item'><label>Curve</label><select id='fftCurve' onchange=\"safeFetch('/setfft?curve='+this.value)\"><option value='0'>Linear</option><option value='1'>Log</option><option value='2'>Power/Gamma</option></select></div>";
  html += "      <div class='setting-item'><label>Gain: <span id='fftGainVal'>1.8</span></label><input id='fftGain' type='range' min='0' max='8' step='0.05' oninput=\"document.getElementById('fftGainVal').innerText=this.value\" onchange=\"safeFetch('/setfft?gain='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Pre gain: <span id='fftPreGainVal'>1</span></label><input id='fftPreGain' type='range' min='0' max='8' step='0.05' oninput=\"document.getElementById('fftPreGainVal').innerText=this.value\" onchange=\"safeFetch('/setfft?preGain='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Noise gate: <span id='fftNoiseGateVal'>6</span></label><input id='fftNoiseGate' type='range' min='0' max='120' step='1' oninput=\"document.getElementById('fftNoiseGateVal').innerText=this.value\" onchange=\"safeFetch('/setfft?noiseGate='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Noise floor: <span id='fftNoiseFloorVal'>0</span></label><input id='fftNoiseFloor' type='range' min='0' max='160' step='1' oninput=\"document.getElementById('fftNoiseFloorVal').innerText=this.value\" onchange=\"safeFetch('/setfft?noiseFloor='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Sensitivity: <span id='fftSensitivityVal'>1</span></label><input id='fftSensitivity' type='range' min='0' max='4' step='0.05' oninput=\"document.getElementById('fftSensitivityVal').innerText=this.value\" onchange=\"safeFetch('/setfft?sensitivity='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Gamma: <span id='fftGammaVal'>0.72</span></label><input id='fftGamma' type='range' min='0.1' max='3' step='0.01' oninput=\"document.getElementById('fftGammaVal').innerText=this.value\" onchange=\"safeFetch('/setfft?gamma='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Log strength: <span id='fftLogStrengthVal'>4</span></label><input id='fftLogStrength' type='range' min='0.1' max='12' step='0.1' oninput=\"document.getElementById('fftLogStrengthVal').innerText=this.value\" onchange=\"safeFetch('/setfft?logStrength='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Attack: <span id='fftAttackVal'>1</span></label><input id='fftAttack' type='range' min='0' max='1' step='0.01' oninput=\"document.getElementById('fftAttackVal').innerText=this.value\" onchange=\"safeFetch('/setfft?attack='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Release: <span id='fftReleaseVal'>0.45</span></label><input id='fftRelease' type='range' min='0' max='1' step='0.01' oninput=\"document.getElementById('fftReleaseVal').innerText=this.value\" onchange=\"safeFetch('/setfft?release='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Low boost: <span id='fftLowBoostVal'>1.15</span></label><input id='fftLowBoost' type='range' min='0' max='4' step='0.05' oninput=\"document.getElementById('fftLowBoostVal').innerText=this.value\" onchange=\"safeFetch('/setfft?lowBoost='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Mid boost: <span id='fftMidBoostVal'>1</span></label><input id='fftMidBoost' type='range' min='0' max='4' step='0.05' oninput=\"document.getElementById('fftMidBoostVal').innerText=this.value\" onchange=\"safeFetch('/setfft?midBoost='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>High boost: <span id='fftHighBoostVal'>0.95</span></label><input id='fftHighBoost' type='range' min='0' max='4' step='0.05' oninput=\"document.getElementById('fftHighBoostVal').innerText=this.value\" onchange=\"safeFetch('/setfft?highBoost='+this.value)\"></div>";
  html += "      <div class='setting-item'><label>Bar gap: <span id='fftBarGapVal'>2</span></label>" + buildNumberStepper("fftBarGap", 2, 0, 12, 1, "document.getElementById('fftBarGapVal').innerText=this.value; safeFetch('/setfft?barGap='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Side margin: <span id='fftSideMarginVal'>4</span></label>" + buildNumberStepper("fftSideMargin", 4, 0, 80, 1, "document.getElementById('fftSideMarginVal').innerText=this.value; safeFetch('/setfft?sideMargin='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Top margin: <span id='fftTopMarginVal'>8</span></label>" + buildNumberStepper("fftTopMargin", 8, 0, 100, 1, "document.getElementById('fftTopMarginVal').innerText=this.value; safeFetch('/setfft?topMargin='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Bottom margin: <span id='fftBottomMarginVal'>12</span></label>" + buildNumberStepper("fftBottomMargin", 12, 0, 100, 1, "document.getElementById('fftBottomMarginVal').innerText=this.value; safeFetch('/setfft?bottomMargin='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Min lit: <span id='fftMinLitSegmentsVal'>0</span></label>" + buildNumberStepper("fftMinLitSegments", 0, 0, 10, 1, "document.getElementById('fftMinLitSegmentsVal').innerText=this.value; safeFetch('/setfft?minLitSegments='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Peak fall: <span id='fftPeakFallVal'>2</span></label>" + buildNumberStepper("fftPeakFall", 2, 0, 20, 1, "document.getElementById('fftPeakFallVal').innerText=this.value; safeFetch('/setfft?peakFall='+this.value)") + "</div>";
  html += "      <div class='setting-item'><h3>Switches</h3><label><input type='checkbox' id='fftPeakHold' onchange=\"safeFetch('/setfft?peakHold='+(this.checked?1:0))\"> Peak hold</label><label><input type='checkbox' id='fftMirror' onchange=\"safeFetch('/setfft?mirror='+(this.checked?1:0))\"> Mirror</label><label><input type='checkbox' id='fftInvert' onchange=\"safeFetch('/setfft?invert='+(this.checked?1:0))\"> Invert level</label></div>";
  html += "      <div class='setting-item'><h3>FFT Gradient</h3><label>Bottom: <input type='color' id='fftColorBottom' onchange=\"safeFetch('/setfft?bottom='+hexToRgb565(this.value).toString(16))\"></label><label>Top: <input type='color' id='fftColorTop' onchange=\"safeFetch('/setfft?top='+hexToRgb565(this.value).toString(16))\"></label><label>Peak: <input type='color' id='fftColorPeak' onchange=\"safeFetch('/setfft?peak='+hexToRgb565(this.value).toString(16))\"></label><label>Off: <input type='color' id='fftColorOff' onchange=\"safeFetch('/setfft?off='+hexToRgb565(this.value).toString(16))\"></label></div>";
  html += "      </div>";
  html += "      </div>";
  html += "    </div>";
  html += "    <div class='section collapsible'>";
  html += "      <button class='collapse-toggle' type='button' onclick='toggleCollapse(this)' aria-expanded='false'><span>VU Meter 2</span><span class='collapse-icon'>+</span></button>";
  html += "      <div class='collapse-content'>";
  html += "      <div class='settings-grid'>";
  html += "      <div class='setting-item'><label>Left X: <span id='lxVal'>" + String(needlePosLX) + "</span></label>" + buildNumberStepper("lx", needlePosLX, 0, 480, 1, "document.getElementById('lxVal').innerText=this.value; safeFetch('/setneedle?lx='+this.value+'&ly='+document.getElementById('ly').value+'&rx='+document.getElementById('rx').value+'&ry='+document.getElementById('ry').value)") + "</div>";
  html += "      <div class='setting-item'><label>Left Y: <span id='lyVal'>" + String(needlePosLY) + "</span></label>" + buildNumberStepper("ly", needlePosLY, 0, 480, 1, "document.getElementById('lyVal').innerText=this.value; safeFetch('/setneedle?lx='+document.getElementById('lx').value+'&ly='+this.value+'&rx='+document.getElementById('rx').value+'&ry='+document.getElementById('ry').value)") + "</div>";
  html += "      <div class='setting-item'><label>Right X: <span id='rxVal'>" + String(needlePosRX) + "</span></label>" + buildNumberStepper("rx", needlePosRX, 0, 480, 1, "document.getElementById('rxVal').innerText=this.value; safeFetch('/setneedle?lx='+document.getElementById('lx').value+'&ly='+document.getElementById('ly').value+'&rx='+this.value+'&ry='+document.getElementById('ry').value)") + "</div>";
  html += "      <div class='setting-item'><label>Right Y: <span id='ryVal'>" + String(needlePosRY) + "</span></label>" + buildNumberStepper("ry", needlePosRY, 0, 480, 1, "document.getElementById('ryVal').innerText=this.value; safeFetch('/setneedle?lx='+document.getElementById('lx').value+'&ly='+document.getElementById('ly').value+'&rx='+document.getElementById('rx').value+'&ry='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Min Angle: <span id='minAngleVal'>" + String(needleMinAngle) + "°</span></label>" + buildNumberStepper("minAngle", needleMinAngle, -360, 360, 1, "document.getElementById('minAngleVal').innerText=this.value+'°'; safeFetch('/setneedleangles?minAngle='+this.value+'&maxAngle='+document.getElementById('maxAngle').value)") + "</div>";
  html += "      <div class='setting-item'><label>Max Angle: <span id='maxAngleVal'>" + String(needleMaxAngle) + "°</span></label>" + buildNumberStepper("maxAngle", needleMaxAngle, -360, 360, 1, "document.getElementById('maxAngleVal').innerText=this.value+'°'; safeFetch('/setneedleangles?minAngle='+document.getElementById('minAngle').value+'&maxAngle='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Up Speed: <span id='upSpeedVal'>" + String(needleUpSpeed) + "</span></label>" + buildNumberStepper("upSpeed", needleUpSpeed, 1, 20, 1, "document.getElementById('upSpeedVal').innerText=this.value; safeFetch('/setneedlespeeds?upSpeed='+this.value+'&downSpeed='+document.getElementById('downSpeed').value)") + "</div>";
  html += "      <div class='setting-item'><label>Down Speed: <span id='downSpeedVal'>" + String(needleDownSpeed) + "</span></label>" + buildNumberStepper("downSpeed", needleDownSpeed, 1, 10, 1, "document.getElementById('downSpeedVal').innerText=this.value; safeFetch('/setneedlespeeds?upSpeed='+document.getElementById('upSpeed').value+'&downSpeed='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Thickness: <span id='thkVal'>" + String(needleThickness) + "</span></label>" + buildNumberStepper("thk", needleThickness, 1, 9, 1, "document.getElementById('thkVal').innerText=this.value; updateNeedleAppearance()") + "</div>";
  html += "      <div class='setting-item'><label>Main Length: <span id='mainVal'>" + String(needleLenMain) + "</span></label>" + buildNumberStepper("main", needleLenMain, 20, 180, 1, "document.getElementById('mainVal').innerText=this.value; updateNeedleAppearance()") + "</div>";
  html += "      <div class='setting-item'><label>Red Length: <span id='redVal'>" + String(needleLenRed) + "</span></label>" + buildNumberStepper("red", needleLenRed, 5, 40, 1, "document.getElementById('redVal').innerText=this.value; updateNeedleAppearance()") + "</div>";
  html += "      <div class='setting-item'><label>Center Y: <span id='cyVal'>" + String(needleCY) + "</span></label>" + buildNumberStepper("cy", needleCY, 0, 260, 1, "document.getElementById('cyVal').innerText=this.value; updateNeedleAppearance()") + "</div>";
  html += "      <div class='setting-item'><label>Sprite Width: <span id='swVal'>" + String(needleSpriteWidth) + "</span></label>" + buildNumberStepper("sw", needleSpriteWidth, 16, 480, 1, "document.getElementById('swVal').innerText=this.value; updateNeedleAppearance()") + "</div>";
  html += "      <div class='setting-item'><label>Sprite Height: <span id='shVal'>" + String(needleSpriteHeight) + "</span></label>" + buildNumberStepper("sh", needleSpriteHeight, 16, 320, 1, "document.getElementById('shVal').innerText=this.value; updateNeedleAppearance()") + "</div>";
  html += "      <div class='setting-item'><label>Debug Borders: <input type='checkbox' id='dbg' " + String(debugBordersEnabled ? "checked" : "") + " onchange='updateNeedleAppearance()'></label></div>";
  html += "      <div class='setting-item'><label>Left Mirror: <input type='checkbox' id='mirrorL' " + String(leftNeedleMirrored ? "checked" : "") + " onchange='updateNeedleAppearance()'></label></div>";
  html += "      <div class='setting-item'><label>Rotation: <button id='needleRotBtn' onclick='cycleNeedleRotation()'>" + String(needleRotation) + "°</button><input type='hidden' id='needleRot' value='" + String(needleRotation) + "'></label></div>";
  html += "      <div class='setting-item'><h3>Needle Colors</h3><label>Main: <input type='color' id='colorNeedleMain' onchange=\"safeFetch('/setcolors?needleMain='+hexToRgb565(this.value).toString(16))\"></label><label>Red: <input type='color' id='colorNeedleRed' onchange=\"safeFetch('/setcolors?needleRed='+hexToRgb565(this.value).toString(16))\"></label></div>";
  html += "      </div>";
  html += "    </div>";
  html += "      </div>";
  html += "    <div class='section collapsible'>";
  html += "      <button class='collapse-toggle' type='button' onclick='toggleCollapse(this)' aria-expanded='false'><span>VU Meter 3</span><span class='collapse-icon'>+</span></button>";
  html += "      <div class='collapse-content'>";
  html += "      <div class='settings-grid'>";
  html += "      <div class='setting-item'><label>Window X: <span id='vuCardioXVal'>" + String(vuCardioX) + "</span></label>" + buildNumberStepper("vuCardioX", vuCardioX, 0, 480, 1, "document.getElementById('vuCardioXVal').innerText=this.value; safeFetch('/setvucardio?x='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Window Y: <span id='vuCardioYVal'>" + String(vuCardioY) + "</span></label>" + buildNumberStepper("vuCardioY", vuCardioY, 0, 320, 1, "document.getElementById('vuCardioYVal').innerText=this.value; safeFetch('/setvucardio?y='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Window Width: <span id='vuCardioWVal'>" + String(vuCardioW) + "</span></label>" + buildNumberStepper("vuCardioW", vuCardioW, 80, 480, 1, "document.getElementById('vuCardioWVal').innerText=this.value; safeFetch('/setvucardio?w='+this.value)") + "</div>";
  html += "      <div class='setting-item'><label>Window Height: <span id='vuCardioHVal'>" + String(vuCardioH) + "</span></label>" + buildNumberStepper("vuCardioH", vuCardioH, 40, 320, 1, "document.getElementById('vuCardioHVal').innerText=this.value; safeFetch('/setvucardio?h='+this.value)") + "</div>";
  html += "      </div>";
  html += "    </div>";
  html += "      </div>";
  html += "    <div class='section collapsible'>";
  html += "      <button class='collapse-toggle' type='button' onclick='toggleCollapse(this)' aria-expanded='false'><span>Настройки текста</span><span class='collapse-icon'>+</span></button>";
  html += "      <div class='collapse-content'>";
  html += "      <div class='inline-settings'>";
  html += "        <div class='inline-setting-row'><div class='inline-setting-title'>Station Name</div><div class='inline-setting-controls'><div class='inline-stepper'><label class='inline-slider-label'>X <span id='stationXVal'>" + String(stationNameX) + "</span></label>" + buildNumberStepper("stationX", stationNameX, 0, 480, 1, "document.getElementById('stationXVal').innerText=this.value; safeFetch('/settextpos?stationX='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Y <span id='stationYVal'>" + String(stationNameY) + "</span></label>" + buildNumberStepper("stationY", stationNameY, 0, 480, 1, "document.getElementById('stationYVal').innerText=this.value; safeFetch('/settextpos?stationY='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Width <span id='stationWVal'>" + String(stationScrollWidth > 0 ? stationScrollWidth : 0) + "</span></label>" + buildNumberStepper("stationW", stationScrollWidth > 0 ? stationScrollWidth : 40, 40, 480, 1, "document.getElementById('stationWVal').innerText=this.value; safeFetch('/settextpos?stationW='+this.value)", String(), String(), "inline-stepper-control") + "</div><label class='inline-color'>Color <input type='color' id='colorStation' onchange=\"safeFetch('/setcolors?station='+hexToRgb565(this.value).toString(16))\"></label></div></div>";
  html += "        <div class='inline-setting-row'><div class='inline-setting-title'>Исполнитель</div><div class='inline-setting-controls'><div class='inline-stepper'><label class='inline-slider-label'>X <span id='title1XVal'>" + String(streamTitle1X) + "</span></label>" + buildNumberStepper("title1X", streamTitle1X, 0, 480, 1, "document.getElementById('title1XVal').innerText=this.value; safeFetch('/settextpos?title1X='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Y <span id='title1YVal'>" + String(streamTitle1Y) + "</span></label>" + buildNumberStepper("title1Y", streamTitle1Y, 0, 480, 1, "document.getElementById('title1YVal').innerText=this.value; safeFetch('/settextpos?title1Y='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Width <span id='title1WVal'>" + String(title1ScrollWidth > 0 ? title1ScrollWidth : 0) + "</span></label>" + buildNumberStepper("title1W", title1ScrollWidth > 0 ? title1ScrollWidth : 40, 40, 480, 1, "document.getElementById('title1WVal').innerText=this.value; safeFetch('/settextpos?title1W='+this.value)", String(), String(), "inline-stepper-control") + "</div><label class='inline-color'>Color <input type='color' id='colorTitle' onchange=\"safeFetch('/setcolors?title='+hexToRgb565(this.value).toString(16))\"></label></div></div>";
  html += "        <div class='inline-setting-row'><div class='inline-setting-title'>Трек</div><div class='inline-setting-controls'><div class='inline-stepper'><label class='inline-slider-label'>X <span id='title2XVal'>" + String(streamTitle2X) + "</span></label>" + buildNumberStepper("title2X", streamTitle2X, 0, 480, 1, "document.getElementById('title2XVal').innerText=this.value; safeFetch('/settextpos?title2X='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Y <span id='title2YVal'>" + String(streamTitle2Y) + "</span></label>" + buildNumberStepper("title2Y", streamTitle2Y, 0, 480, 1, "document.getElementById('title2YVal').innerText=this.value; safeFetch('/settextpos?title2Y='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Width <span id='title2WVal'>" + String(title2ScrollWidth > 0 ? title2ScrollWidth : 0) + "</span></label>" + buildNumberStepper("title2W", title2ScrollWidth > 0 ? title2ScrollWidth : 40, 40, 480, 1, "document.getElementById('title2WVal').innerText=this.value; safeFetch('/settextpos?title2W='+this.value)", String(), String(), "inline-stepper-control") + "</div><label class='inline-color'>Color <input type='color' id='colorTitle2' onchange=\"safeFetch('/setcolors?title='+hexToRgb565(this.value).toString(16))\"></label></div></div>";
  html += "        <div class='inline-setting-row'><div class='inline-setting-title'>Marquee</div><div class='inline-setting-controls'><button id='marquee1ToggleBtn' class='inline-btn' onclick='toggleMarquee1()'>" + String(primaryMarqueeEnabled ? "Turn Marquee Off" : "Turn Marquee On") + "</button><div class='section-note' id='marquee1Status'>" + String(primaryMarqueeEnabled ? "Enabled for current screen" : "Disabled for current screen") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Pause <span id='marqueePauseVal'>" + String(marqueePauseMs) + "</span></label>" + buildNumberStepper("marqueePauseInput", marqueePauseMs, 0, 5000, 100, "document.getElementById('marqueePauseVal').innerText=this.value; safeFetch('/settextpos?marqueePauseMs='+this.value)", String(), String(), "inline-stepper-control") + "</div></div></div>";
  html += "        <div class='inline-setting-row'><div class='inline-setting-title'>Bitrate</div><div class='inline-setting-controls'><div class='inline-stepper'><label class='inline-slider-label'>X <span id='bitrateXVal'>" + String(bitrateX) + "</span></label>" + buildNumberStepper("bitrateX", bitrateX, 0, 480, 1, "document.getElementById('bitrateXVal').innerText=this.value; safeFetch('/settextpos?bitrateX='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Y <span id='bitrateYVal'>" + String(bitrateY) + "</span></label>" + buildNumberStepper("bitrateY", bitrateY, 0, 480, 1, "document.getElementById('bitrateYVal').innerText=this.value; safeFetch('/settextpos?bitrateY='+this.value)", String(), String(), "inline-stepper-control") + "</div><label class='inline-color'>Color <input type='color' id='colorBitrate' onchange=\"safeFetch('/setcolors?bitrate='+hexToRgb565(this.value).toString(16))\"></label></div></div>";
  html += "        <div class='inline-setting-row'><div class='inline-setting-title'>Clock</div><div class='inline-setting-controls'><div class='inline-stepper'><label class='inline-slider-label'>X <span id='clockXVal'>" + String(clockX) + "</span></label>" + buildNumberStepper("clockX", clockX, 0, 480, 1, "document.getElementById('clockXVal').innerText=this.value; safeFetch('/settextpos?clockX='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Y <span id='clockYVal'>" + String(clockY) + "</span></label>" + buildNumberStepper("clockY", clockY, 0, 480, 1, "document.getElementById('clockYVal').innerText=this.value; safeFetch('/settextpos?clockY='+this.value)", String(), String(), "inline-stepper-control") + "</div><label class='inline-color'>Color <input type='color' id='colorClock' onchange=\"safeFetch('/setcolors?clock='+hexToRgb565(this.value).toString(16))\"></label></div></div>";
  html += "        <div class='inline-setting-row'><div class='inline-setting-title'>Volume</div><div class='inline-setting-controls'><div class='inline-stepper'><label class='inline-slider-label'>X <span id='volumeXVal'>" + String(volumeX) + "</span></label>" + buildNumberStepper("volumeX", volumeX, 0, 480, 1, "document.getElementById('volumeXVal').innerText=this.value; safeFetch('/settextpos?volumeX='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Y <span id='volumeYVal'>" + String(volumeY) + "</span></label>" + buildNumberStepper("volumeY", volumeY, 0, 480, 1, "document.getElementById('volumeYVal').innerText=this.value; safeFetch('/settextpos?volumeY='+this.value)", String(), String(), "inline-stepper-control") + "</div><label class='inline-color'>Color <input type='color' id='colorVolume' onchange=\"safeFetch('/setcolors?volume='+hexToRgb565(this.value).toString(16))\"></label></div></div>";
  html += "        <div class='inline-setting-row'><div class='inline-setting-title'>IP Address</div><div class='inline-setting-controls'><div class='inline-stepper'><label class='inline-slider-label'>X <span id='ipXVal'>" + String(ipX) + "</span></label>" + buildNumberStepper("ipX", ipX, 0, 480, 1, "document.getElementById('ipXVal').innerText=this.value; safeFetch('/settextpos?ipX='+this.value)", String(), String(), "inline-stepper-control") + "</div><div class='inline-stepper'><label class='inline-slider-label'>Y <span id='ipYVal'>" + String(ipY) + "</span></label>" + buildNumberStepper("ipY", ipY, 0, 480, 1, "document.getElementById('ipYVal').innerText=this.value; safeFetch('/settextpos?ipY='+this.value)", String(), String(), "inline-stepper-control") + "</div><label class='inline-color'>Color <input type='color' id='colorIP' onchange=\"safeFetch('/setcolors?ip='+hexToRgb565(this.value).toString(16))\"></label></div></div>";
  html += "      </div>";
  html += "      </div>";
  html += "    </div>";
  html += "    <div class='section'><div class='section-header'><div><div class='section-title'>Service actions</div><div class='section-note'>Less frequent controls live here, not on the player page</div></div></div>";
  html += "      <div class='subsection-grid'>";
  html += "        <div class='setting-item'><h3>Info</h3><div class='section-note'>Live device and storage status</div><div class='info-line'><span class='info-label'>IP Address:</span><span class='info-value' id='netIP'>" + WiFi.localIP().toString() + "</span></div><div class='info-line'><span class='info-label'>WiFi Signal:</span><span class='info-value' id='netRSSI'>" + String(WiFi.RSSI()) + " dBm</span></div><div class='info-line'><span class='info-label'>Current Background:</span><span class='info-value' id='netBg'>" + currentBackground + "</span></div><div class='info-line'><span class='info-label'>Current Playlist:</span><span class='info-value' id='netPlaylist'>" + currentPlaylistFile + "</span></div><div class='info-line'><span class='info-label'>Streams Count:</span><span class='info-value' id='netStreams'>" + String(streamCount) + "</span></div><div class='info-line'><span class='info-label'>Radio Status:</span><span class='info-value' id='netStatus'></span></div><div class='info-line'><span class='info-label'>Free Heap:</span><span class='info-value' id='netHeap'>" + String(ESP.getFreeHeap() / 1024) + " KB</span></div><div class='info-line'><span class='info-label'>Free PSRAM:</span><span class='info-value' id='netPSRAM'>" + String(ESP.getFreePsram() / 1024) + " KB</span></div><div class='info-line'><span class='info-label'>LittleFS Total:</span><span class='info-value' id='netFSTotal'>" + String(LittleFS.totalBytes() / 1024) + " KB</span></div><div class='info-line'><span class='info-label'>LittleFS Used:</span><span class='info-value' id='netFSUsed'>" + String(LittleFS.usedBytes() / 1024) + " KB</span></div><div class='info-line'><span class='info-label'>LittleFS Free:</span><span class='info-value' id='netFSFree'>" + String((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024) + " KB</span></div></div>";
  html += "        <div class='setting-item'><h3>LED</h3><div class='actions-row'><button id='ledToggleBtn' onclick='toggleLed()'>" + String(ledMusicEnabled ? "Turn LED Off" : "Turn LED On") + "</button></div><div class='section-note' id='ledStatus'>" + String(ledMusicEnabled ? "Enabled" : "Disabled") + "</div><div class='led-panel'><label>LED count<input type='number' id='ledCountInput' min='1' max='300' value='" + String(ledStripCount) + "' onchange='setLedCount(this.value)'></label><label>Color order<select id='ledColorOrderSelect' onchange='setLedColorOrder(this.value)'><option value='RGB'>RGB</option><option value='RBG'>RBG</option><option value='GRB'>GRB</option><option value='GBR'>GBR</option><option value='BRG'>BRG</option><option value='BGR'>BGR</option></select></label><div class='led-effect-row'><button class='small' type='button' onclick=\"changeLedEffect('prev')\">Prev effect</button><div class='led-effect-name' id='ledEffectName'>Loading...</div><button class='small' type='button' onclick=\"changeLedEffect('next')\">Next effect</button></div></div></div>";
  html += "        <div class='setting-item'><h3>Заставка</h3><div class='actions-row'><button id='screensaverToggleBtn' onclick='toggleScreensaver()'>" + String(screensaverEnabled ? "Заставка выкл." : "Заставка вкл.") + "</button></div><div class='section-note' id='screensaverStatus'>" + String(screensaverEnabled ? "Включится через " + String(screensaverIdleSeconds) + " сек. простоя" : "Отключена") + "</div><label class='inline-slider-label'>Через секунд <span id='screensaverIdleInputVal'>" + String(screensaverIdleSeconds) + "</span></label>" + buildNumberStepper("screensaverIdleInput", screensaverIdleSeconds, 5, 3600, 5, "document.getElementById('screensaverIdleInputVal').innerText=this.value", "setScreensaverIdle(this.value)", String(), "inline-stepper-control") + "<div class='section-note'>Для своих слов загрузите файл screensaver.txt, по одной строке на слово.</div></div>";
  html += "        <div class='setting-item'><h3>WiFi networks</h3><div class='wifi-slots' id='wifiSlots'></div><div class='actions-row'><button type='button' onclick='saveWifiSlots()'>Save WiFi list</button></div><div class='section-note'>At boot the radio tries these networks from top to bottom</div></div>";
  html += "        <div class='setting-item'><h3>Firmware</h3><div class='actions-row'><button onclick=\"window.location='/ota'\">Open OTA update</button><button onclick=\"showSection('remote')\">Remote settings</button><button onclick=\"safeFetch('/updatetft', function(){ showStatus('TFT updated'); });\">Update TFT</button></div></div>";
  html += "      </div>";
  html += "    </div>";
  html += "    <div class='section danger-zone'><div class='section-header'><div><div class='section-title'>Danger zone</div><div class='section-note'>Use only when you really want to reset visual settings</div></div></div><div class='actions-row'><button class='delete-btn' onclick='fetch(\"/visualreset\").then(()=>showStatus(\"Visual settings reset\"));'>Reset visual settings</button></div></div>";
  html += "  </div>";
  html += "</div>";
  return html;
}

inline String getTextPosSection() {
  return String();
}

inline String getNetworkSection() {
  return String();
}

#endif // WEB_INTERFACE_SECTIONS_H
