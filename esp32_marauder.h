#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <stdint.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// ESP32 Marauder UART bridge
//
// Talks to an ESP32 flashed with Marauder firmware over the Flipper's
// external UART header:
//   Pin 13 = USART TX (Flipper -> ESP32 RX)
//   Pin 14 = USART RX (Flipper <- ESP32 TX)
// Baud rate: 115200 (Marauder default)
//
// CONFIDENCE NOTE: this driver is the least field-tested part of RF Rosetta.
// The furi_hal_serial async RX API signature and Marauder's exact text
// output format both vary across firmware/SDK versions. This is built to
// degrade gracefully — you always see the raw serial text even if the
// structured AP parser doesn't match your Marauder build's exact format.
// If the initial build doesn't compile, the serial callback signature is
// the first thing to check.
// ─────────────────────────────────────────────────────────────────────────────

#define MARAUDER_LINE_MAX   64
#define MARAUDER_LINES_BUF  8      // rolling buffer of recent raw lines
#define WIFI_MAX_NETWORKS   24
#define WIFI_SSID_LEN       33

typedef struct {
    char    ssid[WIFI_SSID_LEN];
    char    bssid[18];    // "AA:BB:CC:DD:EE:FF\0"
    int8_t  rssi;
    uint8_t channel;
    char    encryption[8];
    bool    valid;
} WiFiNetwork;

typedef struct {
    char    lines[MARAUDER_LINES_BUF][MARAUDER_LINE_MAX];
    uint8_t count;         // number of valid lines currently held (ring buffer)
} MarauderLineBuf;

typedef struct {
    WiFiNetwork networks[WIFI_MAX_NETWORKS];
    uint8_t     count;
    bool        scanning;
} WiFiScanResult;

typedef struct ESP32Marauder ESP32Marauder;

ESP32Marauder* esp32_marauder_alloc(void);
void esp32_marauder_free(ESP32Marauder* dev);

// Opens UART at 115200 baud and starts async RX.
void esp32_marauder_init(ESP32Marauder* dev);

// Closes UART and releases the handle.
void esp32_marauder_deinit(ESP32Marauder* dev);

// Send a raw command string over UART (newline appended automatically).
void esp32_marauder_send_cmd(ESP32Marauder* dev, const char* cmd);

void esp32_marauder_start_scan(ESP32Marauder* dev);
void esp32_marauder_stop_scan(ESP32Marauder* dev);

// Call periodically (e.g. every 150-250ms) from a timer.
// Drains any bytes received since the last call, assembles complete lines,
// pushes them into the rolling `lines` buffer, and makes a best-effort
// attempt to extract AP info into `result`.
void esp32_marauder_poll(ESP32Marauder* dev, MarauderLineBuf* lines, WiFiScanResult* result);
