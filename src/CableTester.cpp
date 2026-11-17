#include "CableTester.h"
#include <Wire.h>

// Регістри MCP23017 (BANK = 0 за замовчуванням після скидання)
#define MCP_REG_IODIRA      0x00    // Напрямок портів A/B (1 = Input, 0 = Output)
#define MCP_REG_GPPUA       0x0C    // Внутрішні резистори підтяжки 100 кОм (1 = Enabled)
#define MCP_REG_GPIOA       0x12    // Читання рівнів на портах A/B
#define MCP_REG_OLATA       0x14    // Вихідні защілки портів A/B

bool CableTester::begin() {
    if (!isMcpConnected()) {
        return false;
    }
    // 1. Всі лінії у високоімпедансний вхід (Hi-Z)
    if (!writeReg16(MCP_REG_IODIRA, 0xFFFF)) return false;
    // 2. Вихідні защілки в 0 (GND) для емуляції Open-Drain
    if (!writeReg16(MCP_REG_OLATA, 0x0000)) return false;
    // 3. Вмикаємо внутрішні підтяжки 100 кОм паралельно до зовнішніх R10..R25 (10 кОм)
    if (!writeReg16(MCP_REG_GPPUA, 0xFFFF)) return false;

    return true;
}

bool CableTester::isMcpConnected() {
    Wire.beginTransmission(MCP_I2C_ADDR);
    return (Wire.endTransmission() == 0);
}

bool CableTester::writeReg16(uint8_t regA, uint16_t val) {
    Wire.beginTransmission(MCP_I2C_ADDR);
    Wire.write(regA);
    Wire.write(static_cast<uint8_t>(val & 0xFF));
    Wire.write(static_cast<uint8_t>((val >> 8) & 0xFF));
    return (Wire.endTransmission() == 0);
}

bool CableTester::readReg16(uint8_t regA, uint16_t& outVal) {
    Wire.beginTransmission(MCP_I2C_ADDR);
    Wire.write(regA);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    uint8_t received = Wire.requestFrom(static_cast<uint8_t>(MCP_I2C_ADDR), static_cast<uint8_t>(2));
    if (received != 2) {
        return false;
    }
    uint8_t a = Wire.read();
    uint8_t b = Wire.read();
    outVal = static_cast<uint16_t>(a) | (static_cast<uint16_t>(b) << 8);
    return true;
}

uint8_t CableTester::mapLogicalToPhysicalBit(uint8_t logicalPin) {
    if (!SWAP_PORTS_AB) return logicalPin;
    return (logicalPin < PINS_PER_SIDE) ? (logicalPin + PINS_PER_SIDE) : (logicalPin - PINS_PER_SIDE);
}

HardwareFault CableTester::scanMatrix(uint16_t outNet[TOTAL_PINS], uint16_t& outStuckLowMask) {
    memset(outNet, 0, sizeof(uint16_t) * TOTAL_PINS);
    outStuckLowMask = 0;

    // Фаза 0 + Фаза 1: Відпускаємо всі лінії у вхід та перевіряємо відсутність зовнішнього GND / КЗ на землю
    if (!writeReg16(MCP_REG_OLATA, 0x0000)) return HW_MCP_MISSING;
    if (!writeReg16(MCP_REG_GPPUA, 0xFFFF)) return HW_MCP_MISSING;
    if (!writeReg16(MCP_REG_IODIRA, 0xFFFF)) return HW_MCP_MISSING;

    delayMicroseconds(SCAN_SETTLE_US);

    uint16_t idleRaw = 0xFFFF;
    if (!readReg16(MCP_REG_GPIOA, idleRaw)) {
        return HW_MCP_MISSING;
    }

    // Якщо до подачі тестового сигналу хоча б один пін вже притягнутий до 0 (GND) —
    // на роз'ємах присутній сторонній потенціал GND або пробитий порт!
    uint16_t idleLowPhys = ~idleRaw;
    if (idleLowPhys != 0) {
        for (uint8_t p = 0; p < TOTAL_PINS; p++) {
            uint8_t physBit = mapLogicalToPhysicalBit(p);
            if (idleLowPhys & (1U << physBit)) {
                outStuckLowMask |= (1U << p);
            }
        }
        return HW_STUCK_LOW;
    }

    // Фаза 2: Послідовне псевдо-Open-Drain сканування з подвійним читанням (антибрязкіт контактів)
    for (uint8_t p = 0; p < TOTAL_PINS; p++) {
        uint8_t physBit = mapLogicalToPhysicalBit(p);
        uint16_t iodirMask = ~(1U << physBit); // Тільки пін p стає OUTPUT LOW

        if (!writeReg16(MCP_REG_IODIRA, iodirMask)) {
            writeReg16(MCP_REG_IODIRA, 0xFFFF);
            return HW_MCP_MISSING;
        }

        delayMicroseconds(SCAN_SETTLE_US);

        uint16_t sample1 = 0xFFFF, sample2 = 0xFFFF;
        if (!readReg16(MCP_REG_GPIOA, sample1) || !readReg16(MCP_REG_GPIOA, sample2)) {
            writeReg16(MCP_REG_IODIRA, 0xFFFF);
            return HW_MCP_MISSING;
        }

        // Контакт вважається надійно замкненим, якщо обидва читання показали LOW (0)
        uint16_t stableLowPhys = (~sample1) & (~sample2);

        for (uint8_t q = 0; q < TOTAL_PINS; q++) {
            if (q == p) continue;
            uint8_t qPhys = mapLogicalToPhysicalBit(q);
            if (stableLowPhys & (1U << qPhys)) {
                outNet[p] |= (1U << q);
                outNet[q] |= (1U << p); // Симетризація матриці
            }
        }
    }

    // Повертаємо всі лінії у безпечний режим Hi-Z INPUT
    writeReg16(MCP_REG_IODIRA, 0xFFFF);
    return HW_OK;
}

bool CableTester::isMatrixEmpty(const uint16_t net[TOTAL_PINS]) {
    for (uint8_t i = 0; i < TOTAL_PINS; i++) {
        if (net[i] != 0) return false;
    }
    return true;
}
