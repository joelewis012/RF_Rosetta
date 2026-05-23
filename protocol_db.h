#pragma once

#include <stdint.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────────────────────
// Modulation Types
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    ModulationOOK,     // On-Off Keying        — most 433MHz remotes/sensors
    ModulationFSK,     // Frequency Shift Key  — TPMS, meters, industrial
    ModulationGFSK,    // Gaussian FSK         — Z-Wave, Bluetooth-like
    ModulationBPSK,    // Binary Phase Shift   — some pagers
    ModulationCSS,     // Chirp Spread Spec    — LoRa
    ModulationFM,      // Wideband FM          — analogue baby monitors
    ModulationUnknown,
} Modulation;

// ─────────────────────────────────────────────────────────────────────────────
// Signal Categories
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    CategoryAutomotive,
    CategoryHome,
    CategorySecurity,
    CategoryIoT,
    CategoryWeather,
    CategoryUtility,
    CategoryIndustrial,
    CategoryMedical,
    CategoryConsumer,
    CategoryMisc,
} ProtocolCategory;

// ─────────────────────────────────────────────────────────────────────────────
// Protocol Signature — one entry per known protocol in the database
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    const char*      name;             // Full display name
    const char*      short_name;       // ≤16 chars for tight spaces
    ProtocolCategory category;
    uint32_t         freq_min;         // Hz — lower bound
    uint32_t         freq_max;         // Hz — upper bound
    Modulation       modulation;
    uint16_t         pulse_min;        // µs — min expected pulse width
    uint16_t         pulse_max;        // µs — max expected pulse width
    uint16_t         bandwidth_khz;    // expected channel bandwidth
    bool             repeating;        // does device repeat bursts?
    uint8_t          repeat_min;       // min repetitions
    uint8_t          repeat_max;       // max repetitions (0 = uncapped)
    bool             fixed_code;       // same code every press/broadcast
    bool             rolling_code;     // changes per transmission
    bool             encrypted;        // uses known encryption
    bool             security_concern; // should we warn the user?
    bool             needs_esp32;      // requires ESP32 WiFi module
    const char*      brands;           // known brands / devices
    const char*      description;      // one plain-English line
    const char*      security_note;    // shown in security section
    const char*      extra_data;       // what can be decoded
    uint8_t          confidence_bonus; // extra weight for very distinctive sigs
} ProtocolSignature;

// ─────────────────────────────────────────────────────────────────────────────
// Captured Signal Features — populated by signal_capture after CC1101 data
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint32_t frequency;          // Hz
    float    rssi;               // dBm
    float    noise_floor;        // dBm
    float    snr;                // dB
    Modulation modulation;
    uint16_t bandwidth_khz;
    uint16_t pulse_avg;          // µs — average pulse width
    uint16_t pulse_min;          // µs
    uint16_t pulse_max;          // µs
    bool     repeating;
    uint8_t  repeat_count;
    uint16_t packet_bits;        // estimated bit count
    bool     fixed_code_likely;  // heuristic
    uint32_t raw_pulses[512];    // raw pulse timings in µs
    uint16_t pulse_count;
    uint32_t timestamp;          // furi tick at capture
} SignalCapture;

// ─────────────────────────────────────────────────────────────────────────────
// Match Result
// ─────────────────────────────────────────────────────────────────────────────

#define DECODED_LEN 256

typedef struct {
    bool                    matched;
    const ProtocolSignature* protocol;   // NULL if no match
    uint8_t                 confidence;  // 0–100
    char                    decoded[DECODED_LEN]; // decoded data string
    char                    conf_label[12];       // "High" / "Medium" / "Low"
} ProtocolMatch;

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

uint16_t            protocol_db_count(void);
const ProtocolSignature* protocol_db_get(uint16_t index);
const char*         protocol_category_str(ProtocolCategory cat);
const char*         modulation_str(Modulation mod);
bool                protocol_db_match(const SignalCapture* cap, ProtocolMatch* out);
void                protocol_db_decode(const SignalCapture* cap, ProtocolMatch* match);
