#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "TestResults.h"
#include "CableProfiles.h"
#include "CableTester.h"
#include "TestAnalyzer.h"
#include "InputManager.h"
#include "DisplayManager.h"
#include "WebConfigManager.h"

static ProfileManager   profileMgr;
static CableTester      cableTester;
static InputManager     inputMgr;
static DisplayManager   displayMgr;
static WebConfigManager webConfigMgr;

static TestReport       currentReport;
static TestVerdict      lastVerdict = VERDICT_NO_CABLE;
static uint32_t         lastScanMs  = 0;

static void showActiveSelectionToast() {
    const ProjectDef& proj = profileMgr.getCurrentProject();
    const CableProfile& cab = profileMgr.getCurrentProfile();
    char line1[22], line2[22];
    snprintf(line1, sizeof(line1), "PRJ: %.15s", proj.name);
    snprintf(line2, sizeof(line2), "%.10s [%u->%uP]", cab.name,
             static_cast<unsigned>(cab.pinsA & 0x0F),
             static_cast<unsigned>(cab.pinsB & 0x0F));
    displayMgr.showToast(line1, line2, 1000);
}

static void handleTestingEvents(UserEvent ev) {
    if (ev == EVENT_ROTATE_CW) {
        if (displayMgr.getViewMode() == VIEW_PIN_DETAILS) {
            displayMgr.nextDetailPage();
            inputMgr.beep(25, 1);
        } else {
            profileMgr.nextCableInCurrentProject();
            showActiveSelectionToast();
            inputMgr.beep(35, 1);
            lastVerdict = VERDICT_NO_CABLE;
        }
    } else if (ev == EVENT_ROTATE_CCW) {
        if (displayMgr.getViewMode() == VIEW_PIN_DETAILS) {
            displayMgr.nextDetailPage();
            inputMgr.beep(25, 1);
        } else {
            profileMgr.prevCableInCurrentProject();
            showActiveSelectionToast();
            inputMgr.beep(35, 1);
            lastVerdict = VERDICT_NO_CABLE;
        }
    } else if (ev == EVENT_CLICK) {
#if (INPUT_MODE == 2)
        // Для енкодера: короткий клік миттєво перемикає вид (Matrix <-> Pin Details)
        displayMgr.toggleViewMode();
        inputMgr.beep(35, 1);
#else
        // Для однокнопкового режиму: короткий клік перемикає кабель або сторінку
        if (displayMgr.getViewMode() == VIEW_PIN_DETAILS) {
            displayMgr.nextDetailPage();
            inputMgr.beep(35, 1);
        } else {
            profileMgr.nextCableInCurrentProject();
            showActiveSelectionToast();
            inputMgr.beep(45, 1);
            lastVerdict = VERDICT_NO_CABLE;
        }
#endif
    } else if (ev == EVENT_DOUBLE_CLICK) {
        displayMgr.toggleViewMode();
        inputMgr.beep(40, 2, 45);
    } else if (ev == EVENT_LONG_PRESS) {
        // Вхід у головне меню вибору проєкту
        displayMgr.resetMenuCursor(profileMgr.getActiveProjectIdx());
        displayMgr.setScreenState(UI_STATE_MENU_PROJECT);
        inputMgr.beep(70, 1);
    }
}

static void executeProjectMenuSelection() {
    uint8_t pCount = profileMgr.getProjectCount();
    uint8_t cur = displayMgr.getMenuCursor();

    if (cur < pCount) {
        // Обрано проєкт -> відкриваємо меню вибору кабелю в цьому проєкті
        displayMgr.setPendingProjectIdx(cur);
        uint8_t initialCab = (cur == profileMgr.getActiveProjectIdx())
                                 ? profileMgr.getActiveCableIdx()
                                 : 0;
        displayMgr.resetMenuCursor(initialCab);
        displayMgr.setScreenState(UI_STATE_MENU_CABLE);
        inputMgr.beep(55, 1);
    } else if (cur == pCount) {
        // [Save Active Cab] — швидкий запис вставленого кабелю в поточний слот
        displayMgr.setScreenState(UI_STATE_TESTING);
        if (currentReport.verdict != VERDICT_NO_CABLE &&
            currentReport.verdict != VERDICT_HW_ERROR) {
            profileMgr.saveLiveMatrixToCurrentCable(currentReport.rawNet);
            const CableProfile& cab = profileMgr.getCurrentProfile();
            displayMgr.showToast("SAVED TO CABLE:", cab.name, 1300);
            inputMgr.beep(100, 2, 70);
            lastVerdict = VERDICT_PASS;
        } else {
            displayMgr.showToast("CABLE NOT FOUND!", "Insert cable first", 1300);
            inputMgr.beep(220, 1);
        }
    } else if (cur == pCount + 1) {
        // [Wi-Fi Web Setup] — запуск вбудованого Web-конфігуратора
        if (webConfigMgr.startServer()) {
            displayMgr.setScreenState(UI_STATE_WIFI_CONFIG);
            inputMgr.beep(60, 2, 60);
        } else {
            displayMgr.setScreenState(UI_STATE_TESTING);
            displayMgr.showToast("WIFI AP ERROR!", "Cannot start AP", 1300);
            inputMgr.beep(250, 1);
        }
    } else {
        // < Exit to Tester
        displayMgr.setScreenState(UI_STATE_TESTING);
        inputMgr.beep(45, 1);
    }
}

static void handleProjectMenuEvents(UserEvent ev) {
    uint8_t pCount = profileMgr.getProjectCount();
    uint8_t totalItems = pCount + 3; // Проєкти + [Save Active Cab] + [Wi-Fi Web Setup] + < Exit

    if (ev == EVENT_ROTATE_CW) {
        displayMgr.moveMenuCursor(1, totalItems);
        inputMgr.beep(20, 1);
    } else if (ev == EVENT_ROTATE_CCW) {
        displayMgr.moveMenuCursor(-1, totalItems);
        inputMgr.beep(20, 1);
    } else if (ev == EVENT_CLICK) {
#if (INPUT_MODE == 2)
        // Для енкодера: короткий клік = OK (вибір пункту)
        executeProjectMenuSelection();
#else
        displayMgr.moveMenuCursor(1, totalItems);
        inputMgr.beep(30, 1);
#endif
    } else if (ev == EVENT_LONG_PRESS || ev == EVENT_DOUBLE_CLICK) {
#if (INPUT_MODE == 2)
        // Для енкодера: утримання в меню проєктів = швидкий вихід назад у тестер
        displayMgr.setScreenState(UI_STATE_TESTING);
        inputMgr.beep(45, 1);
#else
        executeProjectMenuSelection();
#endif
    }
}

static void executeCableMenuSelection() {
    uint8_t projIdx = displayMgr.getPendingProjectIdx();
    const ProjectDef& prj = profileMgr.getProject(projIdx);
    uint8_t cur = displayMgr.getMenuCursor();

    if (cur < prj.cableCount) {
        profileMgr.selectProjectAndCable(projIdx, cur, true);
        displayMgr.setScreenState(UI_STATE_TESTING);
        showActiveSelectionToast();
        inputMgr.beep(75, 1);
        lastVerdict = VERDICT_NO_CABLE;
    } else {
        // < Back to Projects
        displayMgr.resetMenuCursor(projIdx);
        displayMgr.setScreenState(UI_STATE_MENU_PROJECT);
        inputMgr.beep(45, 1);
    }
}

static void handleCableMenuEvents(UserEvent ev) {
    uint8_t projIdx = displayMgr.getPendingProjectIdx();
    const ProjectDef& prj = profileMgr.getProject(projIdx);
    uint8_t totalItems = prj.cableCount + 1; // Кабелі + "< Back to Projects"

    if (ev == EVENT_ROTATE_CW) {
        displayMgr.moveMenuCursor(1, totalItems);
        inputMgr.beep(20, 1);
    } else if (ev == EVENT_ROTATE_CCW) {
        displayMgr.moveMenuCursor(-1, totalItems);
        inputMgr.beep(20, 1);
    } else if (ev == EVENT_CLICK) {
#if (INPUT_MODE == 2)
        // Для енкодера: короткий клік = OK (активація обраного кабелю)
        executeCableMenuSelection();
#else
        displayMgr.moveMenuCursor(1, totalItems);
        inputMgr.beep(30, 1);
#endif
    } else if (ev == EVENT_LONG_PRESS || ev == EVENT_DOUBLE_CLICK) {
#if (INPUT_MODE == 2)
        // Для енкодера: утримання = повернення до списку проєктів
        displayMgr.resetMenuCursor(projIdx);
        displayMgr.setScreenState(UI_STATE_MENU_PROJECT);
        inputMgr.beep(45, 1);
#else
        executeCableMenuSelection();
#endif
    }
}

static void handleWifiConfigEvents(UserEvent ev) {
    if (ev == EVENT_CLICK || ev == EVENT_LONG_PRESS || ev == EVENT_DOUBLE_CLICK) {
        webConfigMgr.stopServer();
        displayMgr.setScreenState(UI_STATE_TESTING);
        showActiveSelectionToast();
        inputMgr.beep(70, 1);
        lastVerdict = VERDICT_NO_CABLE;
    }
}

static void handleUserEvents() {
    UserEvent ev = inputMgr.pollEvent();
    if (ev == EVENT_NONE) return;

    switch (displayMgr.getScreenState()) {
        case UI_STATE_TESTING:
            handleTestingEvents(ev);
            break;
        case UI_STATE_MENU_PROJECT:
            handleProjectMenuEvents(ev);
            break;
        case UI_STATE_MENU_CABLE:
            handleCableMenuEvents(ev);
            break;
        case UI_STATE_WIFI_CONFIG:
            handleWifiConfigEvents(ev);
            break;
    }
}

void setup() {
    inputMgr.begin();

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(I2C_CLOCK_HZ);

    profileMgr.begin();
    bool mcpOk = cableTester.begin();
    webConfigMgr.begin(&profileMgr, &cableTester);

    displayMgr.begin();
    displayMgr.showStartupScreen(mcpOk);

    if (mcpOk) {
        inputMgr.beep(60, 1);
    } else {
        inputMgr.beep(200, 3, 100);
    }
}

void loop() {
    // 1. Неблокуюче оновлення енкодера, кнопки та зумера
    inputMgr.update();
    handleUserEvents();

    // 2. Якщо активний режим Wi-Fi Web-конфігуратора — обслуговуємо HTTP-клієнтів
    if (webConfigMgr.isRunning()) {
        webConfigMgr.update();
        displayMgr.setWifiClientCount(webConfigMgr.getConnectedClients());
        if (webConfigMgr.consumeConfigUpdatedFlag()) {
            displayMgr.showToast("WEB CONFIG SAVED!", "Projects updated", 1100);
            inputMgr.beep(80, 2, 60);
            lastVerdict = VERDICT_NO_CABLE;
        }
    }

    // 3. Періодичне сканування матриці (на екрані тестування)
    uint32_t now = millis();
    if (displayMgr.getScreenState() == UI_STATE_TESTING &&
        (now - lastScanMs >= SCAN_INTERVAL_MS)) {
        lastScanMs = now;

        uint16_t rawMatrix[TOTAL_PINS];
        uint16_t stuckLowMask = 0;
        HardwareFault hwFault = cableTester.scanMatrix(rawMatrix, stuckLowMask);

        const CableProfile& activeProfile = profileMgr.getCurrentProfile();
        TestAnalyzer::analyze(rawMatrix, hwFault, stuckLowMask, activeProfile, currentReport);

        if (currentReport.verdict != lastVerdict) {
            if (currentReport.verdict == VERDICT_PASS) {
                inputMgr.beep(85, 1);
            } else if (currentReport.verdict == VERDICT_FAIL && lastVerdict == VERDICT_NO_CABLE) {
                inputMgr.beep(55, 2, 55);
            } else if (currentReport.verdict == VERDICT_PROBE_ACTIVE && lastVerdict == VERDICT_NO_CABLE) {
                inputMgr.beep(40, 1);
            } else if (currentReport.verdict == VERDICT_HW_ERROR) {
                inputMgr.beep(180, 3, 80);
            }
            lastVerdict = currentReport.verdict;
        }
    }

    // 4. Оновлення OLED-екрана лише за потреби
    displayMgr.update(currentReport, profileMgr);
}
