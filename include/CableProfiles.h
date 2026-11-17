#ifndef CABLE_PROFILES_H
#define CABLE_PROFILES_H

#include <Arduino.h>
#include "config.h"

// Опис одного кабелю (шаблону розпайки) у складі проєкту
struct CableProfile {
    char name[NAME_MAX_LEN];            // Назва кабелю (напр. "PWR 4P", "RS485", "RJ45 1:1")
    uint8_t pinsA;                      // Кількість контактів роз'єму А (1..8)
    uint8_t pinsB;                      // Кількість контактів роз'єму Б (1..8)
    bool isProbeMode;                   // true = режим вільного продзвонювання без еталона
    uint16_t expectedNet[TOTAL_PINS];   // Повна транзитивно замкнена матриця з'єднань 16x16
};

// Опис проєкту, що містить набір кабелів
struct ProjectDef {
    char name[NAME_MAX_LEN];            // Назва проєкту (напр. "Standard IT", "Drone V2")
    uint8_t cableCount;                 // Кількість кабелів у проєкті (1..MAX_CABLES_PER_PROJECT)
    CableProfile cables[MAX_CABLES_PER_PROJECT];
};

// Образ даних, що зберігається у файловій системі LittleFS
struct StorageImage {
    uint32_t magic;
    uint8_t activeProjectIdx;
    uint8_t activeCableIdx;
    uint8_t projectCount;
    ProjectDef projects[MAX_PROJECTS];
    uint32_t crc32;
};

class ProfileManager {
public:
    void begin();

    // Доступ до проєктів та кабелів
    uint8_t getProjectCount() const { return _data.projectCount; }
    uint8_t getActiveProjectIdx() const { return _data.activeProjectIdx; }
    uint8_t getActiveCableIdx() const { return _data.activeCableIdx; }

    const ProjectDef& getProject(uint8_t projIdx) const;
    ProjectDef& getProjectMutable(uint8_t projIdx);

    const ProjectDef& getCurrentProject() const;
    const CableProfile& getCurrentProfile() const;

    // Навігація по проєктах та кабелях
    void selectProjectAndCable(uint8_t projIdx, uint8_t cableIdx, bool persist = true);
    void nextCableInCurrentProject();
    void prevCableInCurrentProject();

    // Збереження поточного фізичного кабелю у поточний слот або як новий кабель у проєкті
    bool saveLiveMatrixToCurrentCable(const uint16_t net[TOTAL_PINS]);
    bool addLearnedCableToProject(uint8_t projIdx, const char* cableName,
                                  uint8_t pinsA, uint8_t pinsB,
                                  const uint16_t net[TOTAL_PINS]);

    // Керування файловою системою LittleFS
    bool saveToStorage();
    bool loadFromStorage();
    void resetToFactoryDefaults();

    // Допоміжні функції для побудови та нормалізації складних джгутів (1-на-N, перемички)
    static void clearNet(uint16_t net[TOTAL_PINS]);
    static void linkPins(uint16_t net[TOTAL_PINS], uint8_t globalPin1, uint8_t globalPin2);
    static void connectAB(uint16_t net[TOTAL_PINS], uint8_t pinA1to8, uint8_t pinB1to8);
    static void normalizeNetTransitive(uint16_t net[TOTAL_PINS]);
    static uint8_t countExpectedWires(const uint16_t net[TOTAL_PINS]);
    static void detectUsedPinCounts(const uint16_t net[TOTAL_PINS], uint8_t& outPinsA, uint8_t& outPinsB);

    // Доступ до сирих даних для WebConfigManager
    StorageImage& rawStorage() { return _data; }

private:
    StorageImage _data;
    bool _fsMounted = false;

    void initFactoryDefaults();
    static uint32_t computeCrc32(const uint8_t* data, size_t len);
};

#endif // CABLE_PROFILES_H
