#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// АПАРАТНА КОНФІГУРАЦІЯ ПЛАТИ (Схема комутації REV 1.0: Wemos D1 Mini + WCMCU-2317)
// ============================================================================

// --- Шина I2C ---
#define PIN_SDA             D2      // GPIO4 -> U6 pin 7 (SDA) + R29
#define PIN_SCL             D1      // GPIO5 -> U6 pin 8 (SCL) + R28
#define I2C_CLOCK_HZ        100000UL // 100 кГц (оптимально при живленні MCP23017 від +5V)

// --- Підключення Енкодера (EC11 / KY-040) та Зумера ---
// На платі на колодку U9 (DFPlayer) вже виведені безпечні GPIO з резисторами 1 кОм:
//   - Фаза A (CLK) енкодера -> U9 pin 2 (RX) -> через R26 (1 кОм) на D5 (GPIO14)
//   - Фаза B (DT)  енкодера -> U9 pin 3 (TX) -> через R27 (1 кОм) на D6 (GPIO12)
//   - Кнопка (SW)  енкодера -> D3 (GPIO0, майданчик R30 або ніжка D3 Wemos, має pull-up 10k)
//   - GND          енкодера -> U9 pin 7 або pin 10 (GND)
#define PIN_ENC_CLK         D5      // GPIO14: Фаза A (CLK) енкодера (або кнопка в INPUT_MODE 0/1)
#define PIN_ENC_DT          D6      // GPIO12: Фаза B (DT) енкодера (або 2-га кнопка в INPUT_MODE 1)
#define PIN_ENC_SW          D3      // GPIO0:  Кнопка енкодера (SW, активний LOW)
#define PIN_BUZZER          D7      // GPIO13 -> U1 pin 2 (DHT22 DATA)

// Режим органів керування:
// 0 = Одна кнопка на D5
// 1 = Дві кнопки (D5 + D6)
// 2 = Енкодер EC11: D5 (CLK) + D6 (DT) на перериваннях + Кнопка на D3 (SW) [ЗА ЗАМОВЧУВАННЯМ]
#define INPUT_MODE          2

// Налаштування чутливості та напрямку енкодера:
// Більшість енкодерів EC11 дають 4 переходи Грея на 1 механічний клац (або 2 для напівкрокових).
#define ENC_STEPS_PER_NOTCH 4
#define ENC_REVERSE_DIR     false   // Змінити на true, якщо ліво/право переплутані місцями

// --- Адреси I2C периферії ---
#define MCP_I2C_ADDR        0x20    // U6 WCMCU-2317 (A0=A1=A2=GND)
#define OLED_I2C_ADDR       0x3C    // SSD1306 128x64

// --- Параметри дисплея ---
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64

// --- Конфігурація портів тестування (RJ3.1 та RJ3.2) ---
#define SWAP_PORTS_AB       false
#define TOTAL_PINS          16
#define PINS_PER_SIDE       8

// --- Ліміти бази даних проєктів та кабелів (LittleFS) ---
#define MAX_PROJECTS            8
#define MAX_CABLES_PER_PROJECT  8
#define NAME_MAX_LEN            16  // Довжина назви проєкту або кабелю (з '\0')
#define STORAGE_FILE_PATH       "/projects_v3.dat"
#define STORAGE_MAGIC           0xCAB1E301UL

// --- Параметри вбудованого Wi-Fi Web-конфігуратора ---
#define WIFI_AP_SSID            "CableChecker-Setup"
#define WIFI_AP_PASS            ""  // Відкрита мережа (або задайте пароль >= 8 символів)

// --- Таймінги сканування та інтерфейсу ---
#define SCAN_SETTLE_US      250
#define SCAN_INTERVAL_MS    50      // Період опитування матриці (20 Гц)
#define ERROR_CYCLE_MS      2200    // Період автопрокрутки списку помилок у нижньому рядку
#define POPUP_DURATION_MS   1000    // Час показу спливаючого повідомлення (неблокуючий)

// --- Таймінги кнопки (мс) ---
#define BTN_DEBOUNCE_MS     30
#define BTN_DOUBLE_CLICK_MS 280
#define BTN_LONG_PRESS_MS   850     // Швидкий відгук на утримання (0.85 с) для входу/виходу з меню

#endif // CONFIG_H
