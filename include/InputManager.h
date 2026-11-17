#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "config.h"

enum UserEvent : uint8_t {
    EVENT_NONE = 0,
    EVENT_ROTATE_CW,        // Поворот енкодера вправо (за годинниковою стрілкою)
    EVENT_ROTATE_CCW,       // Поворот енкодера вліво (проти годинникової стрілки)
    EVENT_CLICK,            // Коротке натискання кнопки (OK / Select / View Toggle)
    EVENT_DOUBLE_CLICK,     // Подвійне натискання кнопки
    EVENT_LONG_PRESS        // Утримання кнопки (> 0.85 с: Меню / Назад)
};

class InputManager {
public:
    void begin();
    void update();

    // Отримання чергової події користувача
    UserEvent pollEvent();

    // Неблокуючий звуковий сигнал
    void beep(uint16_t durationMs, uint8_t count = 1, uint16_t gapMs = 60);

    // Обробник переривання фаз енкодера в IRAM
    static void IRAM_ATTR handleEncoderIsr();

private:
    UserEvent _pendingEvent = EVENT_NONE;

    // Стан кнопки
    bool _btnRawLast = true;
    bool _btnStable = true;
    uint32_t _lastDebounceMs = 0;
    uint32_t _pressStartMs = 0;
    uint32_t _lastReleaseMs = 0;
    uint8_t _clickCount = 0;
    bool _longHandled = false;

    // Стан другої кнопки (для INPUT_MODE == 1)
    bool _secRawLast = true;
    bool _secStable = true;
    uint32_t _secDebounceMs = 0;

    // Стан неблокуючого зумера
    bool _buzzerActive = false;
    bool _buzzerPinHigh = false;
    uint8_t _buzzerRemaining = 0;
    uint16_t _buzzerOnMs = 0;
    uint16_t _buzzerGapMs = 0;
    uint32_t _buzzerTimerMs = 0;

    void updateButton(uint32_t now);
    void updateEncoderSteps();
    void updateSecondaryButton(uint32_t now);
    void updateBuzzer(uint32_t now);
};

#endif // INPUT_MANAGER_H
