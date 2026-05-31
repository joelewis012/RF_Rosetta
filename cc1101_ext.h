#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>
#include <furi_hal.h>

// ─────────────────────────────────────────────────────────────────────────────
// External CC1101 bit-bang SPI driver
//
// Uses same GPIO pins as NRF24 (physical switch selects which chip is active):
//   PA7 (pin 2) = MOSI
//   PA6 (pin 3) = MISO (also CHIP_RDY signal from CC1101)
//   PA4 (pin 4) = CSn
//   PB3 (pin 5) = SCK
//
// CC1101 VERSION register returns 0x14 when chip is present.
// ─────────────────────────────────────────────────────────────────────────────

// Initialise GPIO and reset CC1101.
// Returns true if chip responds (VERSION == 0x14), false if not connected.
bool cc1101_ext_init(void);

// Release GPIO pins back to analog.
void cc1101_ext_deinit(void);

// True if the chip is connected and responding.
// Must call cc1101_ext_init() first.
bool cc1101_ext_is_connected(void);

// Load a preset register array (addr,val pairs terminated by 0x00,0x00).
void cc1101_ext_load_preset(const uint8_t* preset);

// Set the tuned frequency in Hz.
void cc1101_ext_set_frequency(uint32_t freq_hz);

// Put chip into RX mode.
void cc1101_ext_rx(void);

// Put chip into idle.
void cc1101_ext_idle(void);

// Read current RSSI in dBm.
float cc1101_ext_get_rssi(void);
