#ifndef TEST_ANALYZER_H
#define TEST_ANALYZER_H

#include <Arduino.h>
#include "config.h"
#include "TestResults.h"
#include "CableProfiles.h"

class TestAnalyzer {
public:
    static void analyze(const uint16_t rawNet[TOTAL_PINS],
                        HardwareFault hwFault,
                        uint16_t stuckLowMask,
                        const CableProfile& profile,
                        TestReport& outReport);

    static void formatPinName(uint8_t idx, char* buf, size_t bufLen);

private:
    static void addDiagMessage(TestReport& report, const char* msg);
    static uint8_t countBits8(uint8_t mask);
    static int8_t firstBitIndex8(uint8_t mask);
};

#endif // TEST_ANALYZER_H
