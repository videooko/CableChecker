#include "InputManager.h"

// Змінні стану квадратурного декодера для переривань (IRAM)
static volatile int16_t s_encSubSteps = 0;
static volatile uint8_t s_encPrevState = 0x03;

// Таблиця переходів коду Грея в оперативній пам'яті (DRAM за замовчуванням на ESP8266)
static const int8_t kEncStateTable[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

void IRAM_ATTR InputManager::handleEncoderIsr() {
    uint8_t clk = (GPIP(PIN_ENC_CLK) != 0) ? 1 : 0;
    uint8_t dt  = (GPIP(PIN_ENC_DT)  != 0) ? 1 : 0;
    uint8_t curState = (clk << 1) | dt;
    if (curState != s_encPrevState) {
        uint8_t idx = ((s_encPrevState << 2) | curState) & 0x0F;
        int8_t step = kEncStateTable[idx];
#if ENC_REVERSE_DIR
        s_encSubSteps -= step;
#else
        s_encSubSteps += step;
#endif
        s_encPrevState = curState;
    }
}

void InputManager::begin() {
#if (INPUT_MODE == 2)
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT,  INPUT_PULLUP);
    pinMode(PIN_ENC_SW,  INPUT_PULLUP);

    uint8_t clk = (digitalRead(PIN_ENC_CLK) == HIGH) ? 1 : 0;
    uint8_t dt  = (digitalRead(PIN_ENC_DT)  == HIGH) ? 1 : 0;
    s_encPrevState = (clk << 1) | dt;
    s_encSubSteps = 0;

    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), InputManager::handleEncoderIsr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT),  InputManager::handleEncoderIsr, CHANGE);
#elif (INPUT_MODE == 1)
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT,  INPUT_PULLUP);
#else
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
#endif

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

void InputManager::beep(uint16_t durationMs, uint8_t count, uint16_t gapMs) {
    if (count == 0 || durationMs == 0) return;
    _buzzerOnMs = durationMs;
    _buzzerGapMs = gapMs;
    _buzzerRemaining = count;
    _buzzerActive = true;
    _buzzerPinHigh = true;
    _buzzerTimerMs = millis();
    digitalWrite(PIN_BUZZER, HIGH);
}

void InputManager::updateBuzzer(uint32_t now) {
    if (!_buzzerActive) return;

    if (_buzzerPinHigh) {
        if (now - _buzzerTimerMs >= _buzzerOnMs) {
            digitalWrite(PIN_BUZZER, LOW);
            _buzzerPinHigh = false;
            _buzzerTimerMs = now;
            if (_buzzerRemaining > 0) {
                _buzzerRemaining--;
            }
            if (_buzzerRemaining == 0) {
                _buzzerActive = false;
            }
        }
    } else {
        if (now - _buzzerTimerMs >= _buzzerGapMs) {
            digitalWrite(PIN_BUZZER, HIGH);
            _buzzerPinHigh = true;
            _buzzerTimerMs = now;
        }
    }
}

void InputManager::updateEncoderSteps() {
#if (INPUT_MODE == 2)
    if (_pendingEvent != EVENT_NONE) return;

    noInterrupts();
    int16_t sub = s_encSubSteps;
    if (sub >= ENC_STEPS_PER_NOTCH) {
        s_encSubSteps = sub - ENC_STEPS_PER_NOTCH;
        interrupts();
        _pendingEvent = EVENT_ROTATE_CW;
    } else if (sub <= -ENC_STEPS_PER_NOTCH) {
        s_encSubSteps = sub + ENC_STEPS_PER_NOTCH;
        interrupts();
        _pendingEvent = EVENT_ROTATE_CCW;
    } else {
        interrupts();
    }
#endif
}

void InputManager::updateButton(uint32_t now) {
#if (INPUT_MODE == 2)
    const uint8_t btnPin = PIN_ENC_SW;
#else
    const uint8_t btnPin = PIN_ENC_CLK;
#endif

    bool raw = (digitalRead(btnPin) == HIGH);
    if (raw != _btnRawLast) {
        _lastDebounceMs = now;
        _btnRawLast = raw;
    }

    if ((now - _lastDebounceMs) >= BTN_DEBOUNCE_MS) {
        if (raw != _btnStable) {
            _btnStable = raw;
            if (!_btnStable) {
                // Кнопку натиснуто (LOW)
                _pressStartMs = now;
                _longHandled = false;
            } else {
                // Кнопку відпущено (HIGH)
                if (!_longHandled) {
#if (INPUT_MODE == 2)
                    // Для енкодера одиночний клік спрацьовує миттєво при відпусканні (без очікування подвійного кліку!)
                    _pendingEvent = EVENT_CLICK;
#else
                    _clickCount++;
                    _lastReleaseMs = now;
#endif
                }
            }
        }
    }

    // Перевірка утримання (Long Press)
    if (!_btnStable && !_longHandled && ((now - _pressStartMs) >= BTN_LONG_PRESS_MS)) {
        _longHandled = true;
        _clickCount = 0;
        _pendingEvent = EVENT_LONG_PRESS;
    }

#if (INPUT_MODE != 2)
    // Розрізнення одиночного та подвійного кліку для одно/двокнопкового режиму
    if (_btnStable && _clickCount > 0) {
        if (_clickCount >= 2) {
            _clickCount = 0;
            _pendingEvent = EVENT_DOUBLE_CLICK;
        } else if ((now - _lastReleaseMs) >= BTN_DOUBLE_CLICK_MS) {
            _clickCount = 0;
            _pendingEvent = EVENT_CLICK;
        }
    }
#endif
}

void InputManager::updateSecondaryButton(uint32_t now) {
#if (INPUT_MODE == 1)
    bool raw = (digitalRead(PIN_ENC_DT) == HIGH);
    if (raw != _secRawLast) {
        _secDebounceMs = now;
        _secRawLast = raw;
    }
    if ((now - _secDebounceMs) >= BTN_DEBOUNCE_MS) {
        if (raw != _secStable) {
            _secStable = raw;
            if (!_secStable) {
                _pendingEvent = EVENT_DOUBLE_CLICK;
            }
        }
    }
#else
    (void)now;
#endif
}

void InputManager::update() {
    uint32_t now = millis();
    updateBuzzer(now);
    updateEncoderSteps();
    updateButton(now);
    updateSecondaryButton(now);
}

UserEvent InputManager::pollEvent() {
    UserEvent ev = _pendingEvent;
    _pendingEvent = EVENT_NONE;
    return ev;
}
