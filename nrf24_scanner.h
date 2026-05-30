#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>
#include <furi_hal.h>

// ─────────────────────────────────────────────────────────────────────────────
// NRF24 2.4 GHz channel scanner
//
// Uses bit-bang SPI on Flipper external GPIO header:
//   PA7 (pin 2)  = MOSI
//   PA6 (pin 3)  = MISO
//   PA4 (pin 4)  = CSN
//   PB3 (pin 5)  = SCK
//   PB2 (pin 6)  = CE
//
// The 3-in-1 dev board has a physical switch to select CC1101 vs NRF24.
// Switch must be in NRF24 position before using this scanner.
// ─────────────────────────────────────────────────────────────────────────────

#define NRF24_CHANNELS 125   // channels 0-124 = 2.401-2.525 GHz

// WiFi channel centres in NRF24 channel numbers (approx)
// WiFi ch1=2.412GHz → NRF24 ch11
// WiFi ch6=2.437GHz → NRF24 ch36
// WiFi ch11=2.462GHz → NRF24 ch61
// BLE advertising: ch37=2.402GHz=NRF24 ch1, ch38=2.426GHz=NRF24 ch25, ch39=2.480GHz=NRF24 ch79

typedef struct {
    uint8_t  hits[NRF24_CHANNELS]; // RPD hit count per channel (capped at 255)
    uint8_t  max_hits;             // highest hit count (for bar scaling)
    uint32_t sweep_count;          // how many full sweeps completed
    bool     running;
} NRF24ScanResult;

// Initialise GPIO and power up NRF24 in passive RX/scanner mode
// Call once before sweeping. Do NOT call while CC1101 scanning is active.
void nrf24_scanner_init(void);

// Returns true if chip is present and responding after init.
// Read-back check: we write 0x03 to CONFIG in init, then verify it reads back.
// If MISO is floating (no chip) we get 0xFF back.
bool nrf24_is_connected(void);

// Power down NRF24 and release GPIO pins back to floating/analog
void nrf24_scanner_deinit(void);

// Sweep all 125 channels — blocks for ~125 * 200µs = ~25ms
// Updates result->hits[] and result->max_hits in place.
// Call from a timer thread (never from draw callback).
void nrf24_scanner_sweep(NRF24ScanResult* result);

// Reset hit counts (call when user wants a fresh scan)
void nrf24_scanner_reset(NRF24ScanResult* result);
