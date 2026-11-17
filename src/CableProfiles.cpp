#include "CableProfiles.h"
#include <LittleFS.h>

void ProfileManager::clearNet(uint16_t net[TOTAL_PINS]) {
    memset(net, 0, sizeof(uint16_t) * TOTAL_PINS);
}

void ProfileManager::linkPins(uint16_t net[TOTAL_PINS], uint8_t globalPin1, uint8_t globalPin2) {
    if (globalPin1 >= TOTAL_PINS || globalPin2 >= TOTAL_PINS || globalPin1 == globalPin2) return;
    net[globalPin1] |= (1U << globalPin2);
    net[globalPin2] |= (1U << globalPin1);
}

void ProfileManager::connectAB(uint16_t net[TOTAL_PINS], uint8_t pinA1to8, uint8_t pinB1to8) {
    if (pinA1to8 < 1 || pinA1to8 > 8 || pinB1to8 < 1 || pinB1to8 > 8) return;
    linkPins(net, pinA1to8 - 1, (pinB1to8 - 1) + PINS_PER_SIDE);
}

// Транзитивне замикання графа (алгоритм Уоршелла на бітових масках):
// Якщо A1 з'єднаний з B1 та B2, то фізично B1 і B2 також з'єднані між собою в один вузол.
void ProfileManager::normalizeNetTransitive(uint16_t net[TOTAL_PINS]) {
    // 1. Симетризація та додавання головної діагоналі
    for (uint8_t i = 0; i < TOTAL_PINS; i++) {
        net[i] |= (1U << i);
        for (uint8_t j = 0; j < TOTAL_PINS; j++) {
            if (net[i] & (1U << j)) {
                net[j] |= (1U << i);
            }
        }
    }
    // 2. Транзитивне поширення вузлів
    for (uint8_t k = 0; k < TOTAL_PINS; k++) {
        for (uint8_t i = 0; i < TOTAL_PINS; i++) {
            if (net[i] & (1U << k)) {
                net[i] |= net[k];
            }
        }
    }
    // 3. Прибираємо петлі сам-на-себе (діагональ)
    for (uint8_t i = 0; i < TOTAL_PINS; i++) {
        net[i] &= ~(1U << i);
    }
}

void ProfileManager::detectUsedPinCounts(const uint16_t net[TOTAL_PINS], uint8_t& outPinsA, uint8_t& outPinsB) {
    uint8_t maxA = 1;
    uint8_t maxB = 1;
    for (uint8_t i = 0; i < PINS_PER_SIDE; i++) {
        if (net[i] != 0) maxA = i + 1;
        if (net[i + PINS_PER_SIDE] != 0) maxB = i + 1;
    }
    outPinsA = maxA;
    outPinsB = maxB;
}

uint8_t ProfileManager::countExpectedWires(const uint16_t net[TOTAL_PINS]) {
    uint8_t cnt = 0;
    for (uint8_t i = 0; i < PINS_PER_SIDE; i++) {
        if (net[i] != 0) cnt++;
    }
    if (cnt == 0) {
        for (uint8_t i = PINS_PER_SIDE; i < TOTAL_PINS; i++) {
            if (net[i] != 0) cnt++;
        }
    }
    return cnt;
}

uint32_t ProfileManager::computeCrc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320UL;
            else         crc >>= 1;
        }
    }
    return ~crc;
}

static void setCableMeta(CableProfile& c, const char* name, uint8_t pA, uint8_t pB, bool probe) {
    strncpy(c.name, name, NAME_MAX_LEN - 1);
    c.name[NAME_MAX_LEN - 1] = '\0';
    c.pinsA = (pA >= 1 && pA <= 8) ? pA : 8;
    c.pinsB = (pB >= 1 && pB <= 8) ? pB : 8;
    c.isProbeMode = probe;
    ProfileManager::clearNet(c.expectedNet);
}

void ProfileManager::initFactoryDefaults() {
    memset(&_data, 0, sizeof(StorageImage));
    _data.magic = STORAGE_MAGIC;
    _data.activeProjectIdx = 0;
    _data.activeCableIdx = 0;
    _data.projectCount = 3;

    // ========================================================================
    // ПРОЄКТ 0: "Standard IT" (Стандартні мережеві, телефонні та USB кабелі)
    // ========================================================================
    ProjectDef& p0 = _data.projects[0];
    strncpy(p0.name, "Standard IT", NAME_MAX_LEN - 1);
    p0.cableCount = 5;

    // 0.0: RJ45 8P Straight
    setCableMeta(p0.cables[0], "RJ45 8P 1:1", 8, 8, false);
    for (uint8_t p = 1; p <= 8; p++) connectAB(p0.cables[0].expectedNet, p, p);

    // 0.1: RJ45 T568 Crossover
    setCableMeta(p0.cables[1], "RJ45 Cross", 8, 8, false);
    connectAB(p0.cables[1].expectedNet, 1, 3);
    connectAB(p0.cables[1].expectedNet, 2, 6);
    connectAB(p0.cables[1].expectedNet, 3, 1);
    connectAB(p0.cables[1].expectedNet, 4, 4);
    connectAB(p0.cables[1].expectedNet, 5, 5);
    connectAB(p0.cables[1].expectedNet, 6, 2);
    connectAB(p0.cables[1].expectedNet, 7, 7);
    connectAB(p0.cables[1].expectedNet, 8, 8);

    // 0.2: Ethernet 4-wire 100BASE-TX (1, 2, 3, 6)
    setCableMeta(p0.cables[2], "ETH 4P 1236", 8, 8, false);
    connectAB(p0.cables[2].expectedNet, 1, 1);
    connectAB(p0.cables[2].expectedNet, 2, 2);
    connectAB(p0.cables[2].expectedNet, 3, 3);
    connectAB(p0.cables[2].expectedNet, 6, 6);

    // 0.3: RJ11 6P4C (піни 3..6)
    setCableMeta(p0.cables[3], "RJ11 6P4C", 6, 6, false);
    for (uint8_t p = 3; p <= 6; p++) connectAB(p0.cables[3].expectedNet, p, p);

    // 0.4: USB 4-Pin (1..4)
    setCableMeta(p0.cables[4], "USB 4-Pin", 4, 4, false);
    for (uint8_t p = 1; p <= 4; p++) connectAB(p0.cables[4].expectedNet, p, p);

    // ========================================================================
    // ПРОЄКТ 1: "Custom Harnesses" (Приклад проєкту з різними роз'ємами і розгалуженням)
    // ========================================================================
    ProjectDef& p1 = _data.projects[1];
    strncpy(p1.name, "Project A", NAME_MAX_LEN - 1);
    p1.cableCount = 3;

    // 1.0: Кабель живлення 4P -> 4P (прямий)
    setCableMeta(p1.cables[0], "Power 4P", 4, 4, false);
    for (uint8_t p = 1; p <= 4; p++) connectAB(p1.cables[0].expectedNet, p, p);

    // 1.1: Перехідний кабель датчика 4P -> 6P (асиметричні роз'єми)
    setCableMeta(p1.cables[1], "Sens 4->6P", 4, 6, false);
    connectAB(p1.cables[1].expectedNet, 1, 1);
    connectAB(p1.cables[1].expectedNet, 2, 3);
    connectAB(p1.cables[1].expectedNet, 3, 4);
    connectAB(p1.cables[1].expectedNet, 4, 6);

    // 1.2: Складний джгут із розгалуженням GND (A1 -> B1 та B4)
    setCableMeta(p1.cables[2], "Split GND 4P", 4, 4, false);
    connectAB(p1.cables[2].expectedNet, 1, 1);
    connectAB(p1.cables[2].expectedNet, 1, 4); // Спільний GND на B1 і B4
    connectAB(p1.cables[2].expectedNet, 2, 2);
    connectAB(p1.cables[2].expectedNet, 3, 3);
    normalizeNetTransitive(p1.cables[2].expectedNet);

    // ========================================================================
    // ПРОЄКТ 2: "Lab & Probe" (Вільне продзвонювання та швидкі слоти навчання)
    // ========================================================================
    ProjectDef& p2 = _data.projects[2];
    strncpy(p2.name, "Lab & Slots", NAME_MAX_LEN - 1);
    p2.cableCount = 4;

    setCableMeta(p2.cables[0], "Live Probe", 8, 8, true);
    setCableMeta(p2.cables[1], "Saved Slot 1", 8, 8, false);
    for (uint8_t p = 1; p <= 8; p++) connectAB(p2.cables[1].expectedNet, p, p);

    setCableMeta(p2.cables[2], "Saved Slot 2", 8, 8, false);
    for (uint8_t p = 1; p <= 8; p++) connectAB(p2.cables[2].expectedNet, p, p);

    setCableMeta(p2.cables[3], "Saved Slot 3", 8, 8, false);
    for (uint8_t p = 1; p <= 8; p++) connectAB(p2.cables[3].expectedNet, p, p);
}

void ProfileManager::begin() {
    _fsMounted = LittleFS.begin();
    if (!_fsMounted || !loadFromStorage()) {
        initFactoryDefaults();
        saveToStorage();
    }
}

bool ProfileManager::loadFromStorage() {
    if (!_fsMounted || !LittleFS.exists(STORAGE_FILE_PATH)) {
        return false;
    }
    File f = LittleFS.open(STORAGE_FILE_PATH, "r");
    if (!f || f.size() != sizeof(StorageImage)) {
        if (f) f.close();
        return false;
    }
    StorageImage temp;
    size_t readBytes = f.read(reinterpret_cast<uint8_t*>(&temp), sizeof(StorageImage));
    f.close();
    if (readBytes != sizeof(StorageImage) || temp.magic != STORAGE_MAGIC) {
        return false;
    }
    uint32_t calcCrc = computeCrc32(reinterpret_cast<const uint8_t*>(&temp),
                                    sizeof(StorageImage) - sizeof(uint32_t));
    if (calcCrc != temp.crc32 || temp.projectCount == 0 || temp.projectCount > MAX_PROJECTS) {
        return false;
    }
    if (temp.activeProjectIdx >= temp.projectCount) {
        temp.activeProjectIdx = 0;
    }
    if (temp.projects[temp.activeProjectIdx].cableCount == 0) {
        return false;
    }
    if (temp.activeCableIdx >= temp.projects[temp.activeProjectIdx].cableCount) {
        temp.activeCableIdx = 0;
    }
    memcpy(&_data, &temp, sizeof(StorageImage));
    return true;
}

bool ProfileManager::saveToStorage() {
    if (!_fsMounted) return false;
    _data.magic = STORAGE_MAGIC;
    _data.crc32 = computeCrc32(reinterpret_cast<const uint8_t*>(&_data),
                               sizeof(StorageImage) - sizeof(uint32_t));
    File f = LittleFS.open(STORAGE_FILE_PATH, "w");
    if (!f) return false;
    size_t written = f.write(reinterpret_cast<const uint8_t*>(&_data), sizeof(StorageImage));
    f.close();
    return (written == sizeof(StorageImage));
}

void ProfileManager::resetToFactoryDefaults() {
    initFactoryDefaults();
    saveToStorage();
}

const ProjectDef& ProfileManager::getProject(uint8_t projIdx) const {
    if (projIdx >= _data.projectCount) return _data.projects[0];
    return _data.projects[projIdx];
}

ProjectDef& ProfileManager::getProjectMutable(uint8_t projIdx) {
    if (projIdx >= _data.projectCount) return _data.projects[0];
    return _data.projects[projIdx];
}

const ProjectDef& ProfileManager::getCurrentProject() const {
    return getProject(_data.activeProjectIdx);
}

const CableProfile& ProfileManager::getCurrentProfile() const {
    const ProjectDef& proj = getCurrentProject();
    uint8_t cIdx = (_data.activeCableIdx < proj.cableCount) ? _data.activeCableIdx : 0;
    return proj.cables[cIdx];
}

void ProfileManager::selectProjectAndCable(uint8_t projIdx, uint8_t cableIdx, bool persist) {
    if (projIdx >= _data.projectCount) projIdx = 0;
    _data.activeProjectIdx = projIdx;
    uint8_t cnt = _data.projects[projIdx].cableCount;
    if (cnt == 0) cnt = 1;
    _data.activeCableIdx = (cableIdx < cnt) ? cableIdx : 0;
    if (persist) {
        saveToStorage();
    }
}

void ProfileManager::nextCableInCurrentProject() {
    const ProjectDef& proj = getCurrentProject();
    if (proj.cableCount == 0) return;
    _data.activeCableIdx = (_data.activeCableIdx + 1) % proj.cableCount;
    saveToStorage();
}

void ProfileManager::prevCableInCurrentProject() {
    const ProjectDef& proj = getCurrentProject();
    if (proj.cableCount == 0) return;
    _data.activeCableIdx = (_data.activeCableIdx == 0)
                               ? (proj.cableCount - 1)
                               : (_data.activeCableIdx - 1);
    saveToStorage();
}

bool ProfileManager::saveLiveMatrixToCurrentCable(const uint16_t net[TOTAL_PINS]) {
    ProjectDef& proj = _data.projects[_data.activeProjectIdx];
    if (_data.activeCableIdx >= proj.cableCount) return false;
    CableProfile& cab = proj.cables[_data.activeCableIdx];
    if (cab.isProbeMode) {
        // Якщо ми були на Live Probe і є сусідній слот у проєкті — пишемо в нього або додаємо новий
        if (_data.activeCableIdx + 1 < proj.cableCount) {
            _data.activeCableIdx++;
        } else if (proj.cableCount < MAX_CABLES_PER_PROJECT) {
            uint8_t newIdx = proj.cableCount++;
            snprintf(proj.cables[newIdx].name, NAME_MAX_LEN, "Learned #%u", newIdx);
            _data.activeCableIdx = newIdx;
        }
    }
    CableProfile& target = proj.cables[_data.activeCableIdx];
    target.isProbeMode = false;
    memcpy(target.expectedNet, net, sizeof(uint16_t) * TOTAL_PINS);
    normalizeNetTransitive(target.expectedNet);
    detectUsedPinCounts(target.expectedNet, target.pinsA, target.pinsB);
    return saveToStorage();
}

bool ProfileManager::addLearnedCableToProject(uint8_t projIdx, const char* cableName,
                                              uint8_t pinsA, uint8_t pinsB,
                                              const uint16_t net[TOTAL_PINS]) {
    if (projIdx >= _data.projectCount) return false;
    ProjectDef& proj = _data.projects[projIdx];
    if (proj.cableCount >= MAX_CABLES_PER_PROJECT) return false;

    CableProfile& cab = proj.cables[proj.cableCount];
    setCableMeta(cab, cableName, pinsA, pinsB, false);
    memcpy(cab.expectedNet, net, sizeof(uint16_t) * TOTAL_PINS);
    normalizeNetTransitive(cab.expectedNet);
    _data.activeProjectIdx = projIdx;
    _data.activeCableIdx = proj.cableCount;
    proj.cableCount++;
    return saveToStorage();
}
