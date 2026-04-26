// ============================================
// Default Theme CSS (served as /thems.css)
// ============================================

#ifndef WEB_INTERFACE_THEMS_H
#define WEB_INTERFACE_THEMS_H

#include <Arduino.h>

static const char kThemsCssDefault[] PROGMEM = R"THEMSCSS(
/*
  Light theme – clean, readable, player block highlighted.
*/

:root {
  color-scheme: light;

  /* Base colors – мягкие, но контрастные */
  --bg: #dadde5;
  --bg-alt: #f4f5f7;
  --panel: #fefefe;
  --panel-soft: #f0f2f5;
  --panel-strong: #e2e5ea;

  /* Borders – заметные, сплошные */
  --line: #bcbfc7;
  --line-strong: #8f939e;

  /* Text – максимальный контраст */
  --text: #1a1c23;
  --muted: #4e525e;

  /* Accent – спокойный синий */
  --accent: #2c6e9e;
  --accent-strong: #1f537a;
  --good: #2e7d32;
  --warn: #b85c00;
  --danger: #b71c1c;

  --shadow: 0 2px 6px rgba(0, 0, 0, 0.08);
  --shadow-heavy: 0 8px 20px rgba(0, 0, 0, 0.12);

  /* Page background – светло-серый, без изображения */
  --page-bg: #cfd3dc;

  /* Top bar */
  --topbar-bg: #eef0f5;

  /* Default card background (для остальных секций) */
  --stickybar-bg: #ffffff;
  --card-bg: #ffffff;

  /* Scrollbars */
  --scrollbar-track: #cacdd4;
  --scrollbar-thumb: #8f94a0;
  --scrollbar-thumb-soft: #a8aeb8;

  /* Menu */
  --menu-hover-bg: #e2e6ed;
  --menu-active-bg: #d9e2ef;
  --menu-active-border: #2c6e9e;
  --menu-active-text: #1f537a;
  --menu-active-inset: #ffffff;

  /* Player display area */
  --player-display-bg: #ffffff;
  --player-display-border: #ccd0d8;
  --player-display-inset: #fefefe;
  --track-color: #1f537a;

  /* Buttons */
  --btn-bg: #e8ebf0;
  --btn-text: #1a1c23;
  --btn-shadow: 0 1px 2px rgba(0, 0, 0, 0.05);
  --btn-shadow-hover: 0 3px 8px rgba(0, 0, 0, 0.1);
  --btn-primary-text: #ffffff;

  --btn-utility-bg: #f2f4f8;
  --btn-utility-border: #c5c9d0;
  --btn-utility-inset: #ffffff;

  --btn-utility-active-bg: #e2ecf5;
  --btn-utility-active-border: #2c6e9e;
  --btn-utility-active-text: #1f537a;
  --btn-utility-active-shadow: 0 2px 6px rgba(44, 110, 158, 0.2);

  /* Dropdowns */
  --dropdown-bg: #fefefe;
  --option-bg: #f0f2f5;
  --option-active-bg: #e2ecf5;
  --option-active-border: #2c6e9e;

  /* Sub-panels */
  --panel-sub-bg: #f4f5f7;
  --panel-sub-border: #d0d4dc;

  /* Screen buttons */
  --screen-btn-bg: #e8ebf0;
  --screen-btn-active-bg: #e2ecf5;
  --screen-btn-active-border: #2c6e9e;

  /* Stations list */
  --station-bg: #fcfcfd;
  --station-hover-bg: #f0f2f5;
  --station-hover-border: #c5c9d0;
  --station-playing-bg: #e6f4ea;
  --station-playing-border: #2e7d32;

  --station-add-border: #c5c9d0;
  --station-add-bg: #f0f2f5;
  --station-add-text: #2c6e9e;
  --station-add-hover-bg: #e6f4ea;
  --station-add-hover-border: #2e7d32;

  --bullet-border: #9da3ae;

  /* Playlist summary */
  --playlist-summary-bg: #f4f5f7;
  --playlist-summary-border: #d0d4dc;

  /* Misc */
  --dropdown-header-bg: #f0f2f5;
  --collapse-bg: #e8ebf0;

  --file-row-bg: #fcfcfd;
  --file-row-border: #d0d4dc;
  --file-btn-bg: #f0f2f5;

  --file-selected-bg: #e6f4ea;
  --file-selected-text: #1b5e20;
  --file-selected-border: #2e7d32;

  --delete-bg: #fdeaea;
  --delete-text: #b71c1c;
  --delete-border: #f3b9b9;

  --small-bg: #e8ebf0;

  --danger-border: #f3b9b9;
  --danger-bg: #fef3f3;
  --danger-title: #b71c1c;

  --range-track-bg: #d0d4dc;
  --range-thumb-border: #ffffff;
  --range-thumb-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);

  --submit-bg: #e8ebf0;
  --submit-hover-bg: #e2ecf5;
  --submit-hover-border: #2c6e9e;

  --setting-item-bg: #fcfcfd;
  --setting-item-border: #d0d4dc;

  --led-effect-bg: #e8ebf0;
  --info-bg: #fcfcfd;
  --info-border: #d0d4dc;

  --playlist-editor-bg: #fefefe;
  --table-row-border: #d0d4dc;
  --table-row-hover: #f0f2f5;

  --status-bg: #2c3e44;
  --status-text: #f4f6f9;
  --status-border: #455a64;
  --status-error-border: #b71c1c;
  --status-error-text: #ffe3e3;

  --upload-track-bg: #d0d4dc;
  --upload-fill-bg: #2c6e9e;
}

/* ========== СТИЛИ ДЛЯ ПЛЕЕРА – КРЕМОВЫЙ БЛОК ========== */
.hero-card {
  background: #fff8ed !important;  /* тёплый кремовый */
  border: 1px solid #d4c9b5 !important;
  border-radius: 32px !important;
  box-shadow: 0 8px 20px rgba(0, 0, 0, 0.1) !important;
}

/* Чтобы остальные карточки не менялись – они используют var(--card-bg) */
.section, .playlist-container {
  background: var(--card-bg) !important;
  border: 1px solid var(--line) !important;
}

/* Тёмная тема – оставляем как было (у вас уже есть) */
html[data-theme='dark'] {
  color-scheme: dark;

  --bg: #14161b;
  --bg-alt: #1c2027;
  --panel: #20252d;
  --panel-soft: #262c35;
  --panel-strong: #2d3440;

  --line: rgba(255, 255, 255, 0.08);
  --line-strong: rgba(255, 255, 255, 0.16);

  --text: #eef2f7;
  --muted: #9aa6b6;

  --accent: #7dd3fc;
  --accent-strong: #38bdf8;
  --good: #86efac;
  --warn: #fbbf24;
  --danger: #f87171;

  --shadow: 0 22px 48px rgba(0, 0, 0, 0.28);

  --page-bg: #404040;

  --topbar-bg: rgba(17, 19, 24, 0.88);

  --stickybar-bg: linear-gradient(180deg, rgba(36, 41, 50, 0.96), rgba(29, 33, 40, 0.96));
  --card-bg: linear-gradient(180deg, rgba(36, 41, 50, 0.96), rgba(29, 33, 40, 0.96));

  --scrollbar-track: rgba(0, 0, 0, 0.20);
  --scrollbar-thumb: rgba(125, 211, 252, 0.45);
  --scrollbar-thumb-soft: rgba(125, 211, 252, 0.35);

  --menu-hover-bg: rgba(255, 255, 255, 0.04);
  --menu-active-bg: rgba(125, 211, 252, 0.12);
  --menu-active-border: rgba(125, 211, 252, 0.32);
  --menu-active-text: #dff5ff;
  --menu-active-inset: rgba(125, 211, 252, 0.14);

  --player-display-bg: rgba(255, 255, 255, 0.06);
  --player-display-border: rgba(125, 211, 252, 0.12);
  --player-display-inset: rgba(255, 255, 255, 0.03);
  --track-color: rgba(216, 245, 255, 0.95);

  --btn-bg: linear-gradient(180deg, rgba(255, 255, 255, 0.10), rgba(255, 255, 255, 0.04));
  --btn-text: var(--text);
  --btn-shadow: 0 10px 25px rgba(0, 0, 0, 0.30);
  --btn-shadow-hover: 0 14px 32px rgba(0, 0, 0, 0.36);
  --btn-primary-text: #062433;

  --btn-utility-bg: linear-gradient(180deg, rgba(255, 255, 255, 0.08), rgba(255, 255, 255, 0.04));
  --btn-utility-border: rgba(255, 255, 255, 0.10);
  --btn-utility-inset: rgba(255, 255, 255, 0.03);

  --btn-utility-active-bg: linear-gradient(180deg, rgba(125, 211, 252, 0.28), rgba(56, 189, 248, 0.16));
  --btn-utility-active-border: rgba(125, 211, 252, 0.42);
  --btn-utility-active-text: #effbff;
  --btn-utility-active-shadow: 0 12px 30px rgba(56, 189, 248, 0.20);

  --dropdown-bg: rgba(17, 19, 24, 0.92);
  --option-bg: rgba(255, 255, 255, 0.04);
  --option-active-bg: rgba(125, 211, 252, 0.12);
  --option-active-border: rgba(125, 211, 252, 0.35);

  --panel-sub-bg: rgba(255, 255, 255, 0.035);
  --panel-sub-border: rgba(255, 255, 255, 0.06);

  --screen-btn-bg: rgba(255, 255, 255, 0.05);
  --screen-btn-active-bg: rgba(125, 211, 252, 0.12);
  --screen-btn-active-border: rgba(125, 211, 252, 0.35);

  --station-bg: rgba(255, 255, 255, 0.035);
  --station-hover-bg: rgba(125, 211, 252, 0.08);
  --station-hover-border: rgba(125, 211, 252, 0.18);
  --station-playing-bg: rgba(134, 239, 172, 0.10);
  --station-playing-border: rgba(134, 239, 172, 0.26);

  --station-add-border: rgba(125, 211, 252, 0.28);
  --station-add-bg: rgba(125, 211, 252, 0.10);
  --station-add-text: #eaf8ff;
  --station-add-hover-bg: rgba(134, 239, 172, 0.16);
  --station-add-hover-border: rgba(134, 239, 172, 0.36);
  --bullet-border: rgba(125, 211, 252, 0.60);

  --playlist-summary-bg: rgba(255, 255, 255, 0.035);
  --playlist-summary-border: rgba(255, 255, 255, 0.05);
  --dropdown-header-bg: rgba(255, 255, 255, 0.04);
  --collapse-bg: rgba(255, 255, 255, 0.04);

  --file-row-bg: rgba(255, 255, 255, 0.025);
  --file-row-border: rgba(255, 255, 255, 0.04);
  --file-btn-bg: rgba(255, 255, 255, 0.04);

  --file-selected-bg: rgba(134, 239, 172, 0.12);
  --file-selected-text: #eafff1;
  --file-selected-border: rgba(134, 239, 172, 0.32);

  --delete-bg: rgba(248, 113, 113, 0.18);
  --delete-text: #ffe2e2;
  --delete-border: rgba(248, 113, 113, 0.32);

  --small-bg: rgba(255, 255, 255, 0.045);

  --danger-border: rgba(248, 113, 113, 0.24);
  --danger-bg: linear-gradient(180deg, rgba(70, 26, 32, 0.35), rgba(41, 24, 28, 0.50));
  --danger-title: #ffe1e1;

  --range-track-bg: rgba(255, 255, 255, 0.10);
  --range-thumb-border: rgba(223, 245, 255, 0.95);
  --range-thumb-shadow: 0 4px 14px rgba(56, 189, 248, 0.35);

  --submit-bg: rgba(255, 255, 255, 0.05);
  --submit-hover-bg: rgba(125, 211, 252, 0.10);
  --submit-hover-border: rgba(125, 211, 252, 0.28);

  --setting-item-bg: rgba(255, 255, 255, 0.035);
  --setting-item-border: rgba(255, 255, 255, 0.05);
  --led-effect-bg: rgba(255, 255, 255, 0.04);
  --info-bg: rgba(255, 255, 255, 0.035);
  --info-border: rgba(255, 255, 255, 0.04);

  --playlist-editor-bg: #171b21;
  --table-row-border: rgba(255, 255, 255, 0.05);
  --table-row-hover: rgba(125, 211, 252, 0.08);

  --status-bg: rgba(17, 19, 24, 0.92);
  --status-text: #eef8ff;
  --status-border: rgba(125, 211, 252, 0.35);
  --status-error-border: rgba(248, 113, 113, 0.65);
  --status-error-text: #ffd0d0;

  --upload-track-bg: rgba(255, 255, 255, 0.12);
  --upload-fill-bg: linear-gradient(90deg, var(--accent), var(--good));
}

/* В тёмной теме тоже можно переопределить плеер, но оставим как есть */
html[data-theme='dark'] .hero-card {
  background: #23262e !important;
  border-color: #3d4552 !important;
}
)THEMSCSS";

inline const char* getThemsCssDefault() { return kThemsCssDefault; }

#endif // WEB_INTERFACE_THEMS_H