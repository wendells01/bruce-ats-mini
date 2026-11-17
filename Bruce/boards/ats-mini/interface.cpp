// ATS Mini: ESP32-S3, GC9307 170x320 on an 8-bit parallel bus, rotary encoder
// with a push button, battery voltage divider on GPIO4. No radio, RFID, I2C or
// SD hardware is reachable, so nothing is assigned to bruceConfigPins.

#include "core/powerSave.h"
#include <globals.h>
#include <interface.h>

#include "hal/bright/bright.h"
#include "hal/device.h"
#include "hal/inputs/encoder.h"

#define ENC_A 2
#define ENC_B 1
#define ENC_SEL 21

static DeviceEncoder encoderCfg() {
    DeviceEncoder cfg;
    cfg.pin_a = ENC_A;
    cfg.pin_b = ENC_B;
    cfg.pin_sel = ENC_SEL;
    cfg.pin_esc = -1; // no dedicated back button: long-press on the knob is Esc
    cfg.pullup = true;
    return cfg;
}

void _setup_gpio() {
    // FOUR3: four quadrature steps per detent, which is what this knob produces.
    // If one detent moves two menu entries (or every other one is lost), change
    // the mode here before touching anything else.
    hal_encoder_init(encoderCfg(), EncoderLatchMode::FOUR3);
}

void _post_setup_gpio() {
    // GC9307 sub-variants (ats-mini#41): some panels come up mirrored/inverted,
    // others with a washed-out gamma. RDDID byte 3 is the only way to tell.
    uint8_t did3 = tft.native()->readcommand8(0x04, 3);
    if (did3 == 0x93) { // 0x00009307: inverted & mirrored
        tft.invertDisplay(0);
        tft.writecommand(0x36);        // MADCTL
        tft.native()->writedata(0xE8); // MV | MX | MY | BGR
    } else if (did3 == 0x85) {         // 0x04858552: high gamma
        tft.writecommand(0x26);        // GAMSET
        tft.native()->writedata(8);    // gamma curve 3
        tft.writecommand(0x55);        // WRCACE
        tft.native()->writedata(0xB1); // high enhancement, UI mode
    }

    hal_bright_attach(TFT_BL);
    hal_bright_set(TFT_BL, 100);
}

void _setBrightness(uint8_t brightval) { hal_bright_set(TFT_BL, brightval); }

void InputHandler(void) { hal_encoder_poll(encoderCfg()); }
