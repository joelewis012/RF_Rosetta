#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>
#include <furi_hal.h>
#include "rf_gpio_config.h"

// ─────────────────────────────────────────────────────────────────────────────
// External CC1101 bit-bang SPI driver
//
// Uses the pins from the supplied RFGPIOConfig (see rf_gpio_config.h).
// Default 3-in-1 board wiring:
//   mosi = PA7 (pin 2)   miso = PA6 (pin 3)
//   csn  = PA4 (pin 4)   sck  = PB3 (pin 5)
//   aux  = PB2 (pin 6)   (GDO0 — data output / packet indicator)
//
// VERSION register returns 0x14 when CC1101 is present and responding.
// ─────────────────────────────────────────────────────────────────────────────

bool     cc1101_ext_init(const RFGPIOConfig* gpio);
void     cc1101_ext_deinit(const RFGPIOConfig* gpio);
bool     cc1101_ext_is_connected(const RFGPIOConfig* gpio);
void     cc1101_ext_load_preset(const RFGPIOConfig* gpio, const uint8_t* preset);
void     cc1101_ext_set_frequency(const RFGPIOConfig* gpio, uint32_t freq_hz);
void     cc1101_ext_rx(const RFGPIOConfig* gpio);
void     cc1101_ext_idle(const RFGPIOConfig* gpio);
float    cc1101_ext_get_rssi(const RFGPIOConfig* gpio);
uint16_t cc1101_ext_capture_pulses(const RFGPIOConfig* gpio,
                                   uint32_t* buf, uint16_t buf_size,
                                   uint16_t timeout_ms);
