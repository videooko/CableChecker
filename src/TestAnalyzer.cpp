#include "TestAnalyzer.h"
#include "CableTester.h"

void TestAnalyzer::formatPinName(uint8_t idx, char* buf, size_t bufLen) {
    if (idx < PINS_PER_SIDE) {
        snprintf(buf, bufLen, "A%u", idx + 1);
    } else {
        snprintf(buf, bufLen, "B%u", (idx - PINS_PER_SIDE) + 1);
    }
}

void TestAnalyzer::addDiagMessage(TestReport& report, const char* msg) {
    if (report.errorCount < MAX_DIAG_MESSAGES) {
        strncpy(report.diagMessages[report.errorCount], msg, DIAG_TEXT_LEN - 1);
        report.diagMessages[report.errorCount][DIAG_TEXT_LEN - 1] = '\0';
    }
    report.errorCount++;
}

uint8_t TestAnalyzer::countBits8(uint8_t mask) {
    uint8_t c = 0;
    while (mask) {
        mask &= (mask - 1);
        c++;
    }
    return c;
}

int8_t TestAnalyzer::firstBitIndex8(uint8_t mask) {
    if (mask == 0) return -1;
    int8_t idx = 0;
    while (!(mask & (1U << idx))) {
        idx++;
    }
    return idx;
}

void TestAnalyzer::analyze(const uint16_t rawNet[TOTAL_PINS],
                           HardwareFault hwFault,
                           uint16_t stuckLowMask,
                           const CableProfile& profile,
                           TestReport& outReport) {
    memset(&outReport, 0, sizeof(TestReport));
    outReport.hwFault = hwFault;
    outReport.stuckLowMask = stuckLowMask;
    memcpy(outReport.rawNet, rawNet, sizeof(uint16_t) * TOTAL_PINS);

    // 1. Перевірка апаратних помилок (відсутній MCP23017 або лінія притягнута до GND ззовні)
    if (hwFault != HW_OK) {
        outReport.verdict = VERDICT_HW_ERROR;
        if (hwFault == HW_MCP_MISSING) {
            addDiagMessage(outReport, "I2C ERR: MCP23017!");
        } else if (hwFault == HW_STUCK_LOW) {
            for (uint8_t i = 0; i < TOTAL_PINS; i++) {
                if (stuckLowMask & (1U << i)) {
                    char pName[5], buf[DIAG_TEXT_LEN];
                    formatPinName(i, pName, sizeof(pName));
                    snprintf(buf, sizeof(buf), "GND FAULT ON %s!", pName);
                    addDiagMessage(outReport, buf);
                }
            }
        }
        return;
    }

    // 2. Заповнення масок для кожного з 8 контактів сторони А та підрахунок активних ліній
    uint8_t activeLines = 0;
    for (uint8_t i = 0; i < PINS_PER_SIDE; i++) {
        outReport.localShortMaskA[i] = static_cast<uint8_t>(rawNet[i] & 0xFF);
        outReport.targetMaskB[i]     = static_cast<uint8_t>((rawNet[i] >> PINS_PER_SIDE) & 0xFF);
        if (rawNet[i] != 0) {
            activeLines++;
        }
    }
    if (activeLines == 0) {
        for (uint8_t b = PINS_PER_SIDE; b < TOTAL_PINS; b++) {
            if (rawNet[b] != 0) activeLines++;
        }
    }
    outReport.activeWiresCount = activeLines;

    // 3. Якщо кабель не підключений взагалі
    if (CableTester::isMatrixEmpty(rawNet)) {
        outReport.verdict = VERDICT_NO_CABLE;
        return;
    }

    // 4. Режим вільного продзвонювання (LIVE PROBE)
    if (profile.isProbeMode) {
        for (uint8_t i = 0; i < PINS_PER_SIDE; i++) {
            uint8_t maskA = outReport.localShortMaskA[i];
            uint8_t maskB = outReport.targetMaskB[i];
            uint8_t bCount = countBits8(maskB);

            if (maskA == 0 && maskB == 0) {
                outReport.pinStatusA[i] = PIN_STATUS_UNUSED;
            } else if (maskA != 0) {
                outReport.pinStatusA[i] = PIN_STATUS_SHORT_LOCAL;
            } else if (bCount > 1) {
                outReport.pinStatusA[i] = PIN_STATUS_SHORT_AB;
            } else {
                outReport.pinStatusA[i] = PIN_STATUS_OK;
            }
        }
        for (uint8_t i = 0; i < TOTAL_PINS; i++) {
            for (uint8_t j = i + 1; j < TOTAL_PINS; j++) {
                bool sameSide = (i < PINS_PER_SIDE && j < PINS_PER_SIDE) ||
                                (i >= PINS_PER_SIDE && j >= PINS_PER_SIDE);
                if (sameSide && (rawNet[i] & (1U << j))) {
                    char p1[5], p2[5], msg[DIAG_TEXT_LEN];
                    formatPinName(i, p1, sizeof(p1));
                    formatPinName(j, p2, sizeof(p2));
                    snprintf(msg, sizeof(msg), "SHORT: %s - %s", p1, p2);
                    addDiagMessage(outReport, msg);
                }
            }
        }
        outReport.verdict = VERDICT_PROBE_ACTIVE;
        return;
    }

    // 5. Порівняння з еталонним профілем кабелю (включно з розгалуженнями 1-на-N та перемичками)
    const uint16_t* refNet = profile.expectedNet;

    for (uint8_t i = 0; i < PINS_PER_SIDE; i++) {
        uint8_t maskA = outReport.localShortMaskA[i];
        uint8_t maskB = outReport.targetMaskB[i];
        uint8_t refMaskA = static_cast<uint8_t>(refNet[i] & 0xFF);
        uint8_t refMaskB = static_cast<uint8_t>((refNet[i] >> PINS_PER_SIDE) & 0xFF);

        if (rawNet[i] == refNet[i]) {
            outReport.pinStatusA[i] = (refNet[i] == 0) ? PIN_STATUS_UNUSED : PIN_STATUS_OK;
        } else if (rawNet[i] == 0 && refNet[i] != 0) {
            outReport.pinStatusA[i] = PIN_STATUS_OPEN;
        } else if (refNet[i] == 0 && rawNet[i] != 0) {
            outReport.pinStatusA[i] = (maskA != 0) ? PIN_STATUS_SHORT_LOCAL : PIN_STATUS_EXTRA;
        } else if ((maskA & ~refMaskA) != 0) {
            // Є непередбачене замикання між контактами сторони А
            outReport.pinStatusA[i] = PIN_STATUS_SHORT_LOCAL;
        } else if (countBits8(maskB) == 1 && countBits8(refMaskB) == 1 && maskB != refMaskB) {
            int8_t bIdx = firstBitIndex8(maskB);
            uint8_t bGlobal = static_cast<uint8_t>(bIdx + PINS_PER_SIDE);
            uint8_t aPinsOnB = static_cast<uint8_t>(rawNet[bGlobal] & 0xFF);
            if (countBits8(aPinsOnB) > 1) {
                outReport.pinStatusA[i] = PIN_STATUS_SHORT_AB;
            } else {
                outReport.pinStatusA[i] = PIN_STATUS_CROSS;
            }
        } else if ((maskB & ~refMaskB) == 0 && (refMaskB & ~maskB) != 0) {
            // Частковий обрив гілки у розгалуженому джгуті (1-на-N)
            outReport.pinStatusA[i] = PIN_STATUS_OPEN;
        } else {
            outReport.pinStatusA[i] = PIN_STATUS_SHORT_AB;
        }
    }

    // 6. Формування списку всіх несправностей (SHORT -> CROSS -> EXTRA -> OPEN)
    // 6.1. Зайві з'єднання (КЗ, переплутані жили, зайві контакти поза роз'ємом)
    for (uint8_t i = 0; i < TOTAL_PINS; i++) {
        uint16_t extra = rawNet[i] & (~refNet[i]);
        for (uint8_t j = i + 1; j < TOTAL_PINS; j++) {
            if (!(extra & (1U << j))) continue;

            char p1[5], p2[5], msg[DIAG_TEXT_LEN];
            formatPinName(i, p1, sizeof(p1));
            formatPinName(j, p2, sizeof(p2));

            bool isSameSide = (i < PINS_PER_SIDE && j < PINS_PER_SIDE) ||
                              (i >= PINS_PER_SIDE && j >= PINS_PER_SIDE);
            if (isSameSide) {
                snprintf(msg, sizeof(msg), "SHORT: %s - %s", p1, p2);
                addDiagMessage(outReport, msg);
            } else if (i < PINS_PER_SIDE && j >= PINS_PER_SIDE) {
                if (outReport.pinStatusA[i] == PIN_STATUS_CROSS) {
                    snprintf(msg, sizeof(msg), "CROSS: %s -> %s", p1, p2);
                } else if (outReport.pinStatusA[i] == PIN_STATUS_EXTRA) {
                    snprintf(msg, sizeof(msg), "EXTRA: %s - %s", p1, p2);
                } else {
                    snprintf(msg, sizeof(msg), "SHORT: %s - %s", p1, p2);
                }
                addDiagMessage(outReport, msg);
            }
        }
    }

    // 6.2. Відсутні з'єднання (OPEN), включно з розгалуженнями 1-на-N та перемичками B-B
    for (uint8_t i = 0; i < TOTAL_PINS; i++) {
        if (i < PINS_PER_SIDE && outReport.pinStatusA[i] == PIN_STATUS_CROSS) {
            continue; // Переплутаний пін вже описаний як CROSS
        }
        uint16_t missing = refNet[i] & (~rawNet[i]);
        for (uint8_t j = i + 1; j < TOTAL_PINS; j++) {
            if (!(missing & (1U << j))) continue;
            // Для симетричності A-B виводимо повідомлення від сторони A -> B або B -> B
            if (i < PINS_PER_SIDE && j < PINS_PER_SIDE && (refNet[i] >> PINS_PER_SIDE) != 0) {
                continue; // Транзитивне відображення гілки B-B пропускаємо, якщо є основна лінія A-B
            }
            if (i >= PINS_PER_SIDE && j >= PINS_PER_SIDE) {
                // Пропускаємо транзитивну пару B-B, якщо хоча б один з них має очікуваний зв'язок з A
                if ((refNet[i] & 0xFF) != 0 || (refNet[j] & 0xFF) != 0) continue;
            }
            char p1[5], p2[5], msg[DIAG_TEXT_LEN];
            formatPinName(i, p1, sizeof(p1));
            formatPinName(j, p2, sizeof(p2));
            snprintf(msg, sizeof(msg), "OPEN:  %s - %s", p1, p2);
            addDiagMessage(outReport, msg);
        }
    }

    // 7. Загальний вердикт
    if (memcmp(rawNet, refNet, sizeof(uint16_t) * TOTAL_PINS) == 0) {
        outReport.verdict = VERDICT_PASS;
    } else {
        outReport.verdict = VERDICT_FAIL;
        if (outReport.errorCount == 0) {
            addDiagMessage(outReport, "MISMATCH WITH REF");
        }
    }
}
