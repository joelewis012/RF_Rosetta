#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "protocol_db.h"

// ─────────────────────────────────────────────────────────────────────────────
// Scan Modes
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    ScanModeSubGHz,    // 300–928 MHz, OOK preset, standard sensitivity
    ScanModeRFNarrow,  // Narrow bandwidth — better sensitivity, misses wideband
    ScanModeRFWide,    // Wide bandwidth  — catches more, noisier
} ScanMode;

// ─────────────────────────────────────────────────────────────────────────────
// Antenna Selection
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    AntennaInternal,  // Built-in Flipper antenna
    AntennaExternal,  // External via GPIO pin
} AntennaMode;

// GPIO pin used to switch to external antenna (active HIGH)
#define RF_ROSETTA_ANTENNA_GPIO_PIN (&gpio_ext_pa7)

// ─────────────────────────────────────────────────────────────────────────────
// Scan frequency sweep table
// Frequencies the scanner steps through when in sweep mode
// ─────────────────────────────────────────────────────────────────────────────

extern const uint32_t SWEEP_FREQUENCIES[];
extern const uint16_t SWEEP_FREQ_COUNT;

// ─────────────────────────────────────────────────────────────────────────────
// RSSI History — rolling buffer for the sparkline graph
// ─────────────────────────────────────────────────────────────────────────────

#define RSSI_HISTORY_LEN 48

typedef struct {
    float   values[RSSI_HISTORY_LEN];
    uint8_t head;       // next write position
    uint8_t count;      // how many valid entries
} RSSIHistory;

void rssi_history_push(RSSIHistory* h, float rssi);
float rssi_history_get(const RSSIHistory* h, uint8_t index); // 0=oldest

// ─────────────────────────────────────────────────────────────────────────────
// Signal Capture Context
// Manages CC1101 state, pulse capture, and feature extraction
// ─────────────────────────────────────────────────────────────────────────────

typedef struct SignalCaptureCtx SignalCaptureCtx;

// Allocate and initialise capture context
SignalCaptureCtx* signal_capture_alloc(void);

// Free context and release CC1101
void signal_capture_free(SignalCaptureCtx* ctx);

// Configure scan mode and antenna before starting
void signal_capture_set_mode(SignalCaptureCtx* ctx, ScanMode mode);
void signal_capture_set_antenna(SignalCaptureCtx* ctx, AntennaMode antenna);

// Set the specific frequency to listen on (0 = sweep all)
void signal_capture_set_frequency(SignalCaptureCtx* ctx, uint32_t freq_hz);

// RSSI threshold above which we consider a signal present (dBm)
void signal_capture_set_threshold(SignalCaptureCtx* ctx, float threshold_dbm);

// Start passive listening — call this once, then poll
bool signal_capture_start(SignalCaptureCtx* ctx);

// Stop listening and release CC1101
void signal_capture_stop(SignalCaptureCtx* ctx);

// Poll current RSSI without committing to a capture (call from timer)
float signal_capture_poll_rssi(SignalCaptureCtx* ctx);

// Measure current noise floor (brief sweep, blocking ~200ms)
float signal_capture_noise_floor(SignalCaptureCtx* ctx);

// Returns true if a signal burst was captured and features extracted.
// Fills `out`. Call after signal_capture_poll_rssi exceeds threshold.
bool signal_capture_acquire(SignalCaptureCtx* ctx, SignalCapture* out);

// Advance the sweep to the next frequency in the table
uint32_t signal_capture_next_freq(SignalCaptureCtx* ctx);

// Current frequency being listened on
uint32_t signal_capture_current_freq(SignalCaptureCtx* ctx);
