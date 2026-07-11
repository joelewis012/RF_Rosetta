#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>
#include <furi_hal.h>
#include "rf_gpio_config.h"

// ─────────────────────────────────────────────────────────────────────────────
// NRF24 2.4 GHz channel scanner
//
// Uses the pins from the supplied RFGPIOConfig (see rf_gpio_config.h).
// Default 3-in-1 board wiring:
//   mosi = PA7 (pin 2)   miso = PA6 (pin 3)
//   csn  = PA4 (pin 4)   sck  = PB3 (pin 5)
//   aux  = PB2 (pin 6)   (CE — RX/TX enable)
//
// The 3-in-1 dev board has a physical switch to select CC1101 vs NRF24.
// Switch must be in NRF24 position before using this scanner.
// ─────────────────────────────────────────────────────────────────────────────

#define NRF24_CHANNELS 125   // channels 0-124 = 2.401-2.525 GHz

typedef struct {
    uint8_t  hits[NRF24_CHANNELS];
    uint8_t  max_hits;
    uint32_t sweep_count;
    bool     running;
} NRF24ScanResult;

void nrf24_scanner_init(const RFGPIOConfig* gpio);
bool nrf24_is_connected(const RFGPIOConfig* gpio);
void nrf24_scanner_deinit(const RFGPIOConfig* gpio);
void nrf24_scanner_sweep(const RFGPIOConfig* gpio, NRF24ScanResult* result);
void nrf24_scanner_reset(NRF24ScanResult* result);

// ─────────────────────────────────────────────────────────────────────────────
// Packet capture (promiscuous mode trick)
// ─────────────────────────────────────────────────────────────────────────────

#define NRF24_PKT_MAX 32

typedef struct {
    uint8_t  channel;
    uint8_t  payload[NRF24_PKT_MAX];
    uint8_t  length;
    bool     valid;
    uint32_t freq_khz;
} NRF24Packet;

bool nrf24_capture_packet(const RFGPIOConfig* gpio, uint8_t channel,
                           uint16_t timeout_ms, NRF24Packet* out);

// ─────────────────────────────────────────────────────────────────────────────
// Packet decode
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    NRF24TypeUnknown,
    NRF24TypeMouseJack,
    NRF24TypeShockBurst,
    NRF24TypeBLEAdv,
    NRF24TypeLogitek,
} NRF24PacketType;

typedef struct {
    NRF24PacketType type;
    char            summary[64];
    char            detail[128];
    bool            security_flag;
} NRF24Decode;

NRF24PacketType nrf24_decode_packet(const NRF24Packet* pkt, NRF24Decode* out);
