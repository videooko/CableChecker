#ifndef WEB_CONFIG_MANAGER_H
#define WEB_CONFIG_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "config.h"
#include "CableProfiles.h"
#include "CableTester.h"

class WebConfigManager {
public:
    void begin(ProfileManager* profileMgr, CableTester* tester);

    // Запуск точки доступу Wi-Fi та HTTP-сервера (192.168.4.1)
    bool startServer();

    // Повне вимкнення HTTP-сервера та радіомодуля Wi-Fi
    void stopServer();

    bool isRunning() const { return _running; }
    uint8_t getConnectedClients() const;
    bool consumeConfigUpdatedFlag();

    void update();

private:
    ProfileManager* _profileMgr = nullptr;
    CableTester* _tester = nullptr;
    ESP8266WebServer _server{80};
    bool _running = false;
    bool _configUpdated = false;

    void handleRoot();
    void handleGetCatalog();
    void handleGetLiveScan();
    void handleSaveCatalog();
    void handleFactoryReset();
};

#endif // WEB_CONFIG_MANAGER_H
