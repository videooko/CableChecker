#include "DisplayManager.h"
#include <Wire.h>

DisplayManager::DisplayManager()
    : _oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1) {
    memset(_lastRenderedNet, 0, sizeof(_lastRenderedNet));
    _toastLine1[0] = '\0';
    _toastLine2[0] = '\0';
}

bool DisplayManager::begin() {
    Wire.beginTransmission(OLED_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        _oledReady = false;
        return false;
    }
    _oledReady = _oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
    if (_oledReady) {
        _oled.clearDisplay();
        _oled.setTextColor(SSD1306_WHITE);
        _oled.display();
    }
    return _oledReady;
}

void DisplayManager::showStartupScreen(bool mcpOk) {
    if (!_oledReady) return;
    _oled.clearDisplay();
    _oled.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(14, 8);
    _oled.print("CABLE CHECKER v3");
    _oled.drawLine(6, 19, 121, 19, SSD1306_WHITE);

    _oled.setCursor(8, 24);
    _oled.print("MCP23017: ");
    _oled.print(mcpOk ? "OK (0x20)" : "ERROR!");

    _oled.setCursor(8, 36);
    _oled.print("Turn: Cable / Scroll");
    _oled.setCursor(8, 46);
    _oled.print("Click: Map Hold: Menu");
    _oled.display();

    snprintf(_toastLine1, sizeof(_toastLine1), "CABLE CHECKER v3.1");
    snprintf(_toastLine2, sizeof(_toastLine2), mcpOk ? "Encoder Ready" : "Check MCP23017!");
    _toastStartMs = millis();
    _toastDurationMs = 950;
    _toastActive = true;
}

void DisplayManager::showToast(const char* title, const char* subtitle, uint16_t durationMs) {
    strncpy(_toastLine1, title, sizeof(_toastLine1) - 1);
    _toastLine1[sizeof(_toastLine1) - 1] = '\0';
    strncpy(_toastLine2, subtitle, sizeof(_toastLine2) - 1);
    _toastLine2[sizeof(_toastLine2) - 1] = '\0';
    _toastStartMs = millis();
    _toastDurationMs = durationMs;
    _toastActive = true;
    _dirty = true;
}

void DisplayManager::setScreenState(UiScreenState st) {
    if (_screenState != st) {
        _screenState = st;
        _dirty = true;
    }
}

void DisplayManager::toggleViewMode() {
    if (_viewMode == VIEW_MATRIX) {
        _viewMode = VIEW_PIN_DETAILS;
        _detailPage = 0;
    } else {
        _viewMode = VIEW_MATRIX;
    }
    _dirty = true;
}

void DisplayManager::nextDetailPage() {
    _detailPage = (_detailPage + 1) % 2;
    _dirty = true;
}

void DisplayManager::moveMenuCursor(int8_t delta, uint8_t totalItems) {
    if (totalItems == 0) return;
    int16_t next = static_cast<int16_t>(_menuCursor) + delta;
    while (next < 0) next += totalItems;
    _menuCursor = static_cast<uint8_t>(next % totalItems);
    _dirty = true;
}

void DisplayManager::resetMenuCursor(uint8_t initial) {
    _menuCursor = initial;
    _dirty = true;
}

void DisplayManager::setWifiClientCount(uint8_t clients) {
    if (_wifiClients != clients) {
        _wifiClients = clients;
        _dirty = true;
    }
}

void DisplayManager::renderTopBar(const TestReport& report, const CableProfile& profile) {
    _oled.setCursor(0, 0);
    char shortCab[10];
    strncpy(shortCab, profile.name, 9);
    shortCab[9] = '\0';
    _oled.print(shortCab);

    _oled.setCursor(58, 0);
    switch (report.verdict) {
        case VERDICT_NO_CABLE:
            _oled.print("[NO CABLE]");
            break;
        case VERDICT_PASS:
            _oled.print("[PASS  OK]");
            break;
        case VERDICT_FAIL:
            _oled.print("[FAIL  !!]");
            break;
        case VERDICT_PROBE_ACTIVE:
            _oled.printf("[PRB: %uP]", report.activeWiresCount);
            break;
        case VERDICT_HW_ERROR:
            _oled.print("[HW FAULT]");
            break;
    }
    _oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void DisplayManager::renderMatrixView(const TestReport& report, const ProjectDef& proj, const CableProfile& profile) {
    renderTopBar(report, profile);

    // Рядок пінів сторони А (відображаємо 1..pinsA, або і більше якщо там виявлено сигнал)
    _oled.setCursor(0, 15);
    _oled.print("A:");
    for (uint8_t i = 0; i < PINS_PER_SIDE; i++) {
        _oled.setCursor(16 + i * 14, 15);
        if (i < profile.pinsA || report.rawNet[i] != 0) {
            _oled.print(i + 1);
        } else {
            _oled.print(" ");
        }
    }

    // Середній рядок статусів та рядок сторони Б
    _oled.setCursor(0, 37);
    _oled.print("B:");

    for (uint8_t i = 0; i < PINS_PER_SIDE; i++) {
        uint8_t x = 16 + i * 14;
        PinStatus st = report.pinStatusA[i];
        uint8_t maskB = report.targetMaskB[i];
        uint8_t maskA = report.localShortMaskA[i];

        if (i >= profile.pinsA && st == PIN_STATUS_UNUSED && report.rawNet[i] == 0) {
            continue; // Пін поза межами роз'єму А і порожній
        }

        // Символ лінії зв'язку (y = 26)
        _oled.setCursor(x, 26);
        if (st == PIN_STATUS_OK) {
            _oled.print("|");
        } else if (st == PIN_STATUS_UNUSED) {
            _oled.print((report.verdict == VERDICT_NO_CABLE && (profile.expectedNet[i] != 0)) ? "." : " ");
        } else if (st == PIN_STATUS_OPEN) {
            _oled.print("x");
        } else {
            _oled.print("X");
        }

        // Куди прийшов сигнал на сторону Б (y = 37)
        _oled.setCursor(x, 37);
        if (maskB == 0) {
            if (maskA != 0) {
                // Якщо перемичка A-A передбачена еталоном -> 'J' (Jumper), інакше -> 'S' (Short)
                _oled.print((st == PIN_STATUS_OK) ? "J" : "S");
            } else {
                _oled.print("-");
            }
        } else if ((maskB & (maskB - 1)) == 0) {
            uint8_t bPin = 0;
            while (!(maskB & (1U << bPin))) bPin++;
            _oled.print(bPin + 1);
        } else {
            // Кілька контактів на стороні Б: '+' якщо це штатне розгалуження 1-на-N, '*' якщо аварійне КЗ
            _oled.print((st == PIN_STATUS_OK) ? "+" : "*");
        }
    }

    _oled.drawLine(0, 49, 127, 49, SSD1306_WHITE);

    // Нижній інформаційний рядок (y = 54)
    _oled.setCursor(0, 54);
    if (report.verdict == VERDICT_NO_CABLE) {
        _oled.printf("%.11s [%u->%uP]", proj.name, profile.pinsA, profile.pinsB);
    } else if (report.verdict == VERDICT_PASS) {
        uint8_t expWires = ProfileManager::countExpectedWires(profile.expectedNet);
        _oled.printf("OK: %u/%u LINES MATCH", expWires, expWires);
    } else if (report.verdict == VERDICT_PROBE_ACTIVE) {
        if (report.errorCount > 0) {
            uint8_t idx = _errCycleIdx % report.errorCount;
            _oled.print(report.diagMessages[idx]);
        } else {
            _oled.printf("PROBE: %u ACTIVE LINES", report.activeWiresCount);
        }
    } else {
        uint8_t shownCount = (report.errorCount > MAX_DIAG_MESSAGES) ? MAX_DIAG_MESSAGES : report.errorCount;
        if (shownCount > 0) {
            uint8_t idx = _errCycleIdx % shownCount;
            if (shownCount > 1) {
                _oled.printf("%u/%u %s", idx + 1, shownCount, report.diagMessages[idx]);
            } else {
                _oled.print(report.diagMessages[0]);
            }
        } else {
            _oled.print("MISMATCH DETECTED");
        }
    }
}

void DisplayManager::renderPinDetailsView(const TestReport& report, const CableProfile& profile) {
    _oled.setCursor(0, 0);
    _oled.printf("MAP %.9s [P%u/2]", profile.name, _detailPage + 1);
    _oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    uint8_t startPin = _detailPage * 4;
    for (uint8_t row = 0; row < 4; row++) {
        uint8_t p = startPin + row;
        uint8_t y = 14 + row * 12;
        _oled.setCursor(0, y);

        uint8_t maskB = report.targetMaskB[p];
        uint8_t maskA = report.localShortMaskA[p];
        PinStatus st = report.pinStatusA[p];

        _oled.printf("A%u->", p + 1);
        if (maskB == 0 && maskA == 0) {
            _oled.print("--   ");
        } else if (maskB != 0 && ((maskB & (maskB - 1)) == 0)) {
            uint8_t bPin = 0;
            while (!(maskB & (1U << bPin))) bPin++;
            _oled.printf("B%u  ", bPin + 1);
        } else if (maskB != 0) {
            // Кілька пінів на стороні Б (1-на-N або КЗ) — показуємо перші два номери
            uint8_t b1 = 0xFF, b2 = 0xFF;
            for (uint8_t b = 0; b < 8; b++) {
                if (maskB & (1U << b)) {
                    if (b1 == 0xFF) b1 = b + 1;
                    else if (b2 == 0xFF) b2 = b + 1;
                }
            }
            _oled.printf("B%u,%u", b1, b2);
        } else {
            uint8_t aOther = 0;
            while (!(maskA & (1U << aOther))) aOther++;
            _oled.printf("A%u  ", aOther + 1);
        }

        _oled.setCursor(64, y);
        switch (st) {
            case PIN_STATUS_OK:          _oled.print("[  OK  ]"); break;
            case PIN_STATUS_OPEN:        _oled.print("[ OPEN ]"); break;
            case PIN_STATUS_CROSS:       _oled.print("[CROSS ]"); break;
            case PIN_STATUS_SHORT_AB:    _oled.print("[SHORT!]"); break;
            case PIN_STATUS_SHORT_LOCAL: _oled.print("[SHRT-A]"); break;
            case PIN_STATUS_EXTRA:       _oled.print("[EXTRA ]"); break;
            case PIN_STATUS_UNUSED:      _oled.print("[UNUSED]"); break;
        }
    }
}

void DisplayManager::renderProjectMenu(const ProfileManager& profileMgr) {
    _oled.setCursor(0, 0);
    _oled.print("SELECT PROJECT:");
    _oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Пункти меню:
    // 0 .. projectCount-1 : Проєкти
    // projectCount        : [Save Active Cab]
    // projectCount + 1    : [Wi-Fi Web Setup]
    // projectCount + 2    : < Exit to Tester
    uint8_t pCount = profileMgr.getProjectCount();
    uint8_t totalItems = pCount + 3;

    uint8_t firstVis = 0;
    if (_menuCursor >= 4) {
        firstVis = _menuCursor - 3;
    }

    for (uint8_t row = 0; row < 4; row++) {
        uint8_t idx = firstVis + row;
        if (idx >= totalItems) break;
        uint8_t y = 14 + row * 12;
        _oled.setCursor(0, y);
        _oled.print((idx == _menuCursor) ? "> " : "  ");

        if (idx < pCount) {
            const ProjectDef& prj = profileMgr.getProject(idx);
            _oled.printf("%u.%.12s (%u)", idx + 1, prj.name, prj.cableCount);
        } else if (idx == pCount) {
            _oled.print("[Save Active Cab]");
        } else if (idx == pCount + 1) {
            _oled.print("[Wi-Fi Web Setup]");
        } else {
            _oled.print("< Exit to Tester");
        }
    }
}

void DisplayManager::renderCableMenu(const ProfileManager& profileMgr) {
    const ProjectDef& prj = profileMgr.getProject(_pendingProjIdx);
    _oled.setCursor(0, 0);
    _oled.printf("PRJ: %.15s", prj.name);
    _oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Пункти: 0..cableCount-1 = Кабелі, cableCount = "< Back to Projects"
    uint8_t totalItems = prj.cableCount + 1;
    uint8_t firstVis = 0;
    if (_menuCursor >= 4) {
        firstVis = _menuCursor - 3;
    }

    for (uint8_t row = 0; row < 4; row++) {
        uint8_t idx = firstVis + row;
        if (idx >= totalItems) break;
        uint8_t y = 14 + row * 12;
        _oled.setCursor(0, y);
        _oled.print((idx == _menuCursor) ? "> " : "  ");

        if (idx < prj.cableCount) {
            const CableProfile& cab = prj.cables[idx];
            _oled.printf("%.11s [%u->%u]", cab.name, cab.pinsA, cab.pinsB);
        } else {
            _oled.print("< Back to Projects");
        }
    }
}

void DisplayManager::renderWifiConfigScreen() {
    _oled.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    _oled.setCursor(6, 5);
    _oled.print("WI-FI WEB CONFIG AP");
    _oled.drawLine(2, 15, 125, 15, SSD1306_WHITE);

    _oled.setCursor(6, 20);
    _oled.print("SSID: CableChecker");
    _oled.setCursor(6, 31);
    _oled.print("URL : 192.168.4.1");
    _oled.setCursor(6, 42);
    _oled.printf("Clients: %u connected", _wifiClients);
    _oled.setCursor(6, 53);
    _oled.print("Click/Hold -> EXIT");
}

void DisplayManager::renderToast() {
    _oled.clearDisplay();
    _oled.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    _oled.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, SSD1306_WHITE);
    _oled.setTextSize(1);
    _oled.setCursor(8, 18);
    _oled.print(_toastLine1);
    _oled.setCursor(8, 36);
    _oled.print(_toastLine2);
}

void DisplayManager::update(const TestReport& report, const ProfileManager& profileMgr) {
    if (!_oledReady) return;

    uint32_t now = millis();

    // 1. Контроль часу показу спливаючого повідомлення (Toast)
    if (_toastActive) {
        if ((now - _toastStartMs) >= _toastDurationMs) {
            _toastActive = false;
            _dirty = true;
        } else {
            if (_dirty) {
                renderToast();
                _oled.display();
                _dirty = false;
            }
            return;
        }
    }

    // 2. Перевірка змін стану на робочому екрані тестування
    if (_screenState == UI_STATE_TESTING) {
        if (report.verdict != _lastVerdict ||
            profileMgr.getActiveProjectIdx() != _lastProjIdx ||
            profileMgr.getActiveCableIdx() != _lastCabIdx ||
            memcmp(report.rawNet, _lastRenderedNet, sizeof(_lastRenderedNet)) != 0) {
            _lastVerdict = report.verdict;
            _lastProjIdx = profileMgr.getActiveProjectIdx();
            _lastCabIdx  = profileMgr.getActiveCableIdx();
            memcpy(_lastRenderedNet, report.rawNet, sizeof(_lastRenderedNet));
            _errCycleIdx = 0;
            _lastErrCycleMs = now;
            _dirty = true;
        }

        if (report.errorCount > 1 && (now - _lastErrCycleMs >= ERROR_CYCLE_MS)) {
            _lastErrCycleMs = now;
            _errCycleIdx++;
            _dirty = true;
        }
    }

    if (!_dirty) return;

    _oled.clearDisplay();
    switch (_screenState) {
        case UI_STATE_TESTING:
            if (_viewMode == VIEW_MATRIX) {
                renderMatrixView(report, profileMgr.getCurrentProject(), profileMgr.getCurrentProfile());
            } else {
                renderPinDetailsView(report, profileMgr.getCurrentProfile());
            }
            break;
        case UI_STATE_MENU_PROJECT:
            renderProjectMenu(profileMgr);
            break;
        case UI_STATE_MENU_CABLE:
            renderCableMenu(profileMgr);
            break;
        case UI_STATE_WIFI_CONFIG:
            renderWifiConfigScreen();
            break;
    }
    _oled.display();
    _dirty = false;
}
