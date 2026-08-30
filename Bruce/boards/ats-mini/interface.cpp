#include "core/powerSave.h"
#include <globals.h>
#include <interface.h>

#include <rotary_decoder.h>

static RotaryDecoder encoder;

static bool encoderInitialized = false;
static bool backlightInitialized = false;

// PWM channel for the TFT backlight (GPIO38)
#define ATSMINI_BL_CHANNEL 0

/***************************************************************************************
** Function name: _setup_gpio()
** Location: main.cpp
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    // Rotary encoder pins with internal pull-ups
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);

    // TFT 8-bit parallel data bus pins as outputs
    pinMode(TFT_D0, OUTPUT);
    pinMode(TFT_D1, OUTPUT);
    pinMode(TFT_D2, OUTPUT);
    pinMode(TFT_D3, OUTPUT);
    pinMode(TFT_D4, OUTPUT);
    pinMode(TFT_D5, OUTPUT);
    pinMode(TFT_D6, OUTPUT);
    pinMode(TFT_D7, OUTPUT);
    pinMode(TFT_WR, OUTPUT);
    pinMode(TFT_RD, OUTPUT);
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RD, HIGH);

    // Backlight output (PWM attached lazily in _setBrightness)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    bruceConfig.startupApp = "Bruce";
    Serial.begin(115200);
}

/***************************************************************************************
** Function name: _post_setup_gpio()
** Description:   second stage gpio setup (after TFT init) - apply full brightness
***************************************************************************************/
void _post_setup_gpio() {
    _setBrightness(100);
}

/***************************************************************************************
** Function name: getBattery()
** Description:   Delivers the battery value from 0-100
**                Reads the ADC on GPIO4. Battery range: 3.5V (0%) to 4.2V (100%)
***************************************************************************************/
int getBattery() {
    static bool adcInitialized = false;
    if (!adcInitialized) {
        pinMode(BATTERY_PIN, INPUT);
        analogSetAttenuation(ADC_11db);
        adcInitialized = true;
    }

    uint32_t mv = analogReadMilliVolts(BATTERY_PIN);

    // Approximate Li-ion voltage curve (no fuel gauge IC on this board)
    const float MIN_VOLTAGE = 3500.0f; // empty
    const float MAX_VOLTAGE = 4200.0f; // full

    int percent = (int)(((float)mv - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE) * 100.0f);

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    return percent;
}

/***************************************************************************************
** Function name: isCharging()
** Description:   ATS Mini has no fuel-gauge/charger detection wired to the MCU
***************************************************************************************/
bool isCharging() { return false; }

/*********************************************************************
** Function: _setBrightness
** location: settings.cpp
** set brightness value (0-100) via PWM on GPIO38
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    if (!backlightInitialized) {
        ledcAttachChannel(TFT_BL, 16000, 10, ATSMINI_BL_CHANNEL);
        backlightInitialized = true;
    }

    if (brightval == 0) {
        ledcWrite(TFT_BL, 0);
    } else {
        int bl = MINBRIGHT + round(((1023 - MINBRIGHT) * brightval / 100));
        if (bl > 1023) bl = 1023;
        ledcWrite(TFT_BL, bl);
    }
}

/*********************************************************************
** Function: pollEncoder
** Samples the rotary encoder A/B lines (called from a dedicated FreeRTOS
** task every 4ms, decoupled from InputHandler - see main.cpp)
**********************************************************************/
void pollEncoder(void) {
    if (!encoderInitialized) {
        encoder.begin(ENCODER_PIN_A, ENCODER_PIN_B, 2);
        encoderInitialized = true;
    }
    encoder.poll();
}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = millis();  // debounce for the encoder button
    static unsigned long tm2 = millis(); // delay between Select and encoder (avoid missclick)
    static long posDifference = 0;
    static long lastPos = 0;

    long newPos = encoder.getPosition();
    if (newPos != lastPos) {
        posDifference += (newPos - lastPos);
        RotaryNetSteps += (newPos - lastPos);
        lastPos = newPos;
        tm2 = millis();
    }

    bool sel = false;
    if (millis() - tm > 200 || LongPress) {
        sel = (digitalRead(ENCODER_PUSH_BUTTON) == LOW);
    }

    if (posDifference != 0 || sel) {
        if (!wakeUpScreen()) AnyKeyPress = true;
        else return;
    }

    if (posDifference > 0) { PrevPress = true; posDifference--; tm2 = millis(); }
    if (posDifference < 0) { NextPress = true; posDifference++; tm2 = millis(); }

    if (sel && millis() - tm2 > 200) {
        posDifference = 0;
        SelPress = true;
        tm = millis();
    }
}

/*********************************************************************
** Function: checkReboot
** Long-press the encoder button to power off
**********************************************************************/
void checkReboot() {
    int c = 0;
    while (digitalRead(ENCODER_PUSH_BUTTON) == LOW) {
        delay(100);
        c++;
        if (c > 20) { // ~2s long press
            powerOff();
        }
    }
}

/*********************************************************************
** Function: powerOff
** Turns off the device (deep sleep, wake on encoder button)
**********************************************************************/
void powerOff() {
    _setBrightness(0);
    tft.writecommand(0x10); // SLPIN
    esp_sleep_enable_ext0_wakeup((gpio_num_t)ENCODER_PUSH_BUTTON, 0);
    esp_deep_sleep_start();
}

/*********************************************************************
** Function: goToDeepSleep
** Puts the device into DeepSleep
**********************************************************************/
void goToDeepSleep() { powerOff(); }
