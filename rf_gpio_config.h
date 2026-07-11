#pragma once
#include <furi.h>
#include <furi_hal.h>

// ─────────────────────────────────────────────────────────────────────────────
// Shared GPIO configuration struct — used by cc1101_ext, nrf24_scanner,
// signal_capture, and the main app. Kept in its own header to avoid
// circular includes between rf_rosetta.h and signal_capture.h.
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    const GpioPin* mosi;
    const GpioPin* miso;
    const GpioPin* csn;
    const GpioPin* sck;
    const GpioPin* aux;    // CC1101 GDO0 or NRF24 CE
} RFGPIOConfig;

typedef enum {
    BoardPreset3in1,
    BoardPresetDevBoard,
    BoardPresetWiFiDevBoard,
    BoardPresetCC1101Breadboard,
    BoardPresetNRF24Standalone,
    BoardPresetMissileRF,
    BoardPresetCustom,
    BoardPresetCount,
} BoardPreset;

static const char* const BOARD_PRESET_NAMES[] = {
    "3-in-1 Board",
    "Flipper Dev Board",
    "WiFi Dev Board",
    "CC1101 Breadboard",
    "NRF24 Standalone",
    "Missile RF",
    "Custom",
};

RFGPIOConfig rf_rosetta_gpio_preset(BoardPreset preset);
