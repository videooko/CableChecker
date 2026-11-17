#ifndef TEST_RESULTS_H
#define TEST_RESULTS_H

#include <Arduino.h>
#include "config.h"

// Загальний стан тестування
enum TestVerdict : uint8_t {
    VERDICT_NO_CABLE = 0,   // Всі лінії розімкнені
    VERDICT_PASS,           // Повний збіг з обраним профілем
    VERDICT_FAIL,           // Знайдено невідповідність профілю або КЗ
    VERDICT_PROBE_ACTIVE,   // Режим вільного продзвонювання (Auto Probe)
    VERDICT_HW_ERROR        // Помилка шини I2C або зовнішня напруга на порту
};

// Тип апаратної помилки
enum HardwareFault : uint8_t {
    HW_OK = 0,
    HW_MCP_MISSING,         // MCP23017 (0x20) не відповідає по I2C
    HW_STUCK_LOW            // На лініях присутній GND/зовнішня напруга до початку тесту
};

// Статус окремого контакту сторони А (0..7) відносно профілю
enum PinStatus : uint8_t {
    PIN_STATUS_UNUSED = 0,  // Не використовується в профілі та фізично вільний
    PIN_STATUS_OK,          // Точно відповідає еталону
    PIN_STATUS_OPEN,        // Обрив (мав бути з'єднаний, але порожній)
    PIN_STATUS_CROSS,       // Переплутаний (прийшов на один інший пін сторони Б)
    PIN_STATUS_SHORT_AB,    // КЗ між кількома лініями (А-Б)
    PIN_STATUS_SHORT_LOCAL, // Локальне КЗ на своїй же стороні (А-А або Б-Б)
    PIN_STATUS_EXTRA        // Зайвий провідник, якого не повинно бути в профілі
};

// Максимальна кількість текстових повідомлень про помилки в одному звіті
#define MAX_DIAG_MESSAGES   8
#define DIAG_TEXT_LEN       22

struct TestReport {
    TestVerdict verdict;
    HardwareFault hwFault;
    uint16_t stuckLowMask;          // Які піни примусово притягнуті до GND ззовні

    uint16_t rawNet[TOTAL_PINS];    // Фактична матриця з'єднань (16x16 біт)
    PinStatus pinStatusA[PINS_PER_SIDE];
    uint8_t targetMaskB[PINS_PER_SIDE]; // Куди на сторону Б прийшов кожен пін А
    uint8_t localShortMaskA[PINS_PER_SIDE]; // З якими пінами А замкнений пін А

    uint8_t activeWiresCount;       // Кількість активних ліній сторон А або Б
    uint8_t errorCount;             // Загальна кількість виявлених помилок
    char diagMessages[MAX_DIAG_MESSAGES][DIAG_TEXT_LEN];
};

#endif // TEST_RESULTS_H
