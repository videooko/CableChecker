#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "TestResults.h"
#include "CableProfiles.h"

enum UiScreenState : uint8_t {
    UI_STATE_TESTING = 0,       // Робочий екран перевірки кабелю (Matrix або Pin Details)
    UI_STATE_MENU_PROJECT,      // Меню вибору Проєкту (+ Save Live / Wi-Fi Setup)
    UI_STATE_MENU_CABLE,        // Меню вибору Кабелю в межах обраного проєкту
    UI_STATE_WIFI_CONFIG        // Екран активного Wi-Fi Web-конфігуратора (192.168.4.1)
};

enum UiViewMode : uint8_t {
    VIEW_MATRIX = 0,            // Графічна матриця A1..A8 <-> B1..B8
    VIEW_PIN_DETAILS            // Посторінковий текстовий список Pin Mapping (A1..A4 / A5..A8)
};

class DisplayManager {
public:
    DisplayManager();

    bool begin();
    void showStartupScreen(bool mcpOk);

    // Неблокуюче спливаюче повідомлення
    void showToast(const char* title, const char* subtitle, uint16_t durationMs = POPUP_DURATION_MS);

    // Керування станами UI та меню
    UiScreenState getScreenState() const { return _screenState; }
    void setScreenState(UiScreenState st);

    void toggleViewMode();
    UiViewMode getViewMode() const { return _viewMode; }
    void nextDetailPage();

    // Навігація в меню проєктів/кабелів
    uint8_t getMenuCursor() const { return _menuCursor; }
    uint8_t getPendingProjectIdx() const { return _pendingProjIdx; }
    void setPendingProjectIdx(uint8_t idx) { _pendingProjIdx = idx; }
    void moveMenuCursor(int8_t delta, uint8_t totalItems);
    void resetMenuCursor(uint8_t initial = 0);

    void setWifiClientCount(uint8_t clients);
    void markDirty() { _dirty = true; }

    // Головне оновлення екрана (викликає I2C передачу лише за потреби)
    void update(const TestReport& report, const ProfileManager& profileMgr);

private:
    Adafruit_SSD1306 _oled;
    bool _oledReady = false;
    bool _dirty = true;

    UiScreenState _screenState = UI_STATE_TESTING;
    UiViewMode _viewMode = VIEW_MATRIX;
    uint8_t _detailPage = 0;

    uint8_t _menuCursor = 0;
    uint8_t _pendingProjIdx = 0;
    uint8_t _wifiClients = 0;

    // Стан автопрокрутки помилок у нижньому рядку
    uint8_t _errCycleIdx = 0;
    uint32_t _lastErrCycleMs = 0;

    // Стан неблокуючого Toast-повідомлення
    bool _toastActive = false;
    uint32_t _toastStartMs = 0;
    uint16_t _toastDurationMs = 0;
    char _toastLine1[22];
    char _toastLine2[22];

    // Кеш попереднього стану для визначення змін (dirty check)
    uint16_t _lastRenderedNet[TOTAL_PINS];
    TestVerdict _lastVerdict = VERDICT_NO_CABLE;
    uint8_t _lastProjIdx = 0xFF;
    uint8_t _lastCabIdx  = 0xFF;

    void renderMatrixView(const TestReport& report, const ProjectDef& proj, const CableProfile& profile);
    void renderPinDetailsView(const TestReport& report, const CableProfile& profile);
    void renderProjectMenu(const ProfileManager& profileMgr);
    void renderCableMenu(const ProfileManager& profileMgr);
    void renderWifiConfigScreen();
    void renderToast();
    void renderTopBar(const TestReport& report, const CableProfile& profile);
};

#endif // DISPLAY_MANAGER_H
