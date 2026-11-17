#ifndef CABLE_TESTER_H
#define CABLE_TESTER_H

#include <Arduino.h>
#include "config.h"
#include "TestResults.h"

class CableTester {
public:
    bool begin();

    // Перевірка наявності MCP23017 на шині I2C
    bool isMcpConnected();

    // Повний безпечний цикл сканування (I2C check + Safety check + Open-Drain Matrix Scan)
    HardwareFault scanMatrix(uint16_t outNet[TOTAL_PINS], uint16_t& outStuckLowMask);

    // Допоміжна перевірка, чи кабель повністю відсутній
    static bool isMatrixEmpty(const uint16_t net[TOTAL_PINS]);

private:
    bool writeReg16(uint8_t regA, uint16_t val);
    bool readReg16(uint8_t regA, uint16_t& outVal);
    static uint8_t mapLogicalToPhysicalBit(uint8_t logicalPin);
};

#endif // CABLE_TESTER_H
