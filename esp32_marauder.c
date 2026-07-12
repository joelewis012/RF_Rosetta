#include "esp32_marauder.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <string.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// Internal state
// ─────────────────────────────────────────────────────────────────────────────

#define MARAUDER_RX_BUF_SIZE 512

struct ESP32Marauder {
    FuriHalSerialHandle* serial;
    uint8_t  rx_buf[MARAUDER_RX_BUF_SIZE];  // single-producer (ISR) / single-consumer (poll) ring buffer
    volatile uint16_t rx_head;              // written by RX callback
    uint16_t rx_tail;                       // read by poll()
    char     line_acc[MARAUDER_LINE_MAX];   // in-progress line assembly
    uint8_t  line_acc_len;
};

// ─────────────────────────────────────────────────────────────────────────────
// UART RX callback
//
// CONFIDENCE NOTE: this uses the event-based furi_hal_serial async RX API
// (callback receives an event mask, then calls furi_hal_serial_async_rx_receive
// to pull the byte). Some SDK versions instead deliver the byte directly as
// the callback's second argument. If this fails to compile with an
// "incompatible pointer type" or "too many arguments" error, that's the
// fix needed — swap to the direct-byte callback signature.
// ─────────────────────────────────────────────────────────────────────────────

static void marauder_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    ESP32Marauder* dev = context;
    if(event & FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx_receive(handle);
        uint16_t next = (uint16_t)((dev->rx_head + 1) % MARAUDER_RX_BUF_SIZE);
        if(next != dev->rx_tail) {
            dev->rx_buf[dev->rx_head] = data;
            dev->rx_head = next;
        }
        // Buffer full: drop the byte. Acceptable for a text display —
        // we'll pick up the stream again on the next line boundary.
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Alloc / free / init / deinit
// ─────────────────────────────────────────────────────────────────────────────

ESP32Marauder* esp32_marauder_alloc(void) {
    ESP32Marauder* dev = malloc(sizeof(ESP32Marauder));
    furi_assert(dev);
    memset(dev, 0, sizeof(ESP32Marauder));
    return dev;
}

void esp32_marauder_free(ESP32Marauder* dev) {
    if(!dev) return;
    if(dev->serial) esp32_marauder_deinit(dev);
    free(dev);
}

void esp32_marauder_init(ESP32Marauder* dev) {
    dev->rx_head      = 0;
    dev->rx_tail      = 0;
    dev->line_acc_len = 0;

    dev->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!dev->serial) return;  // pins in use elsewhere, or not present on this hardware

    furi_hal_serial_init(dev->serial, 115200);
    furi_hal_serial_async_rx_start(dev->serial, marauder_rx_callback, dev, false);
}

void esp32_marauder_deinit(ESP32Marauder* dev) {
    if(!dev->serial) return;
    furi_hal_serial_async_rx_stop(dev->serial);
    furi_hal_serial_deinit(dev->serial);
    furi_hal_serial_control_release(dev->serial);
    dev->serial = NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// Commands
// ─────────────────────────────────────────────────────────────────────────────

void esp32_marauder_send_cmd(ESP32Marauder* dev, const char* cmd) {
    if(!dev->serial) return;
    furi_hal_serial_tx(dev->serial, (const uint8_t*)cmd, strlen(cmd));
    furi_hal_serial_tx(dev->serial, (const uint8_t*)"\n", 1);
    furi_hal_serial_tx_wait_complete(dev->serial);
}

void esp32_marauder_start_scan(ESP32Marauder* dev) {
    esp32_marauder_send_cmd(dev, "scanap");
}

void esp32_marauder_stop_scan(ESP32Marauder* dev) {
    esp32_marauder_send_cmd(dev, "stopscan");
}

// ─────────────────────────────────────────────────────────────────────────────
// Line buffer helpers
// ─────────────────────────────────────────────────────────────────────────────

static void marauder_push_line(MarauderLineBuf* lines, const char* line) {
    if(lines->count < MARAUDER_LINES_BUF) {
        snprintf(lines->lines[lines->count], MARAUDER_LINE_MAX, "%s", line);
        lines->count++;
    } else {
        // Ring buffer full — shift everything up, drop the oldest line
        for(uint8_t i = 0; i < MARAUDER_LINES_BUF - 1; i++) {
            snprintf(lines->lines[i], MARAUDER_LINE_MAX, "%s", lines->lines[i + 1]);
        }
        snprintf(lines->lines[MARAUDER_LINES_BUF - 1], MARAUDER_LINE_MAX, "%s", line);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Best-effort AP line parser
//
// Marauder's serial text format has changed across firmware versions/forks,
// so this can't assume an exact layout. Instead it looks for the one thing
// that's almost certainly present on any AP scan result line: a MAC address
// in "AA:BB:CC:DD:EE:FF" form. If found, it treats the line as describing an
// access point and does its best to pull RSSI, channel, and SSID from
// nearby text. If your Marauder build's format doesn't match well, you'll
// still see the raw lines — this parser only adds structure on top.
// ─────────────────────────────────────────────────────────────────────────────

static bool is_hex_digit_c(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static int find_mac(const char* s, int len) {
    for(int i = 0; i + 17 <= len; i++) {
        bool ok = true;
        for(int j = 0; j < 17 && ok; j++) {
            char c = s[i + j];
            if(j % 3 == 2) {
                if(c != ':') ok = false;
            } else {
                if(!is_hex_digit_c(c)) ok = false;
            }
        }
        if(ok) return i;
    }
    return -1;
}

static void marauder_try_parse_ap(const char* line, uint8_t len, WiFiScanResult* result) {
    if(!result || result->count >= WIFI_MAX_NETWORKS) return;

    int mac_pos = find_mac(line, len);
    if(mac_pos < 0) return;  // no MAC-shaped text — not an AP line we can use

    WiFiNetwork* net = &result->networks[result->count];
    memset(net, 0, sizeof(WiFiNetwork));

    // BSSID
    snprintf(net->bssid, sizeof(net->bssid), "%.17s", line + mac_pos);

    // RSSI: first "-NN" pattern found anywhere (1-100 range covers dBm values)
    for(int i = 0; i < len; i++) {
        if(line[i] == '-' && i + 1 < len && line[i + 1] >= '0' && line[i + 1] <= '9') {
            int val = 0, digits = 0, j = i + 1;
            while(j < len && line[j] >= '0' && line[j] <= '9' && digits < 3) {
                val = val * 10 + (line[j] - '0');
                j++; digits++;
            }
            if(val >= 1 && val <= 100) {
                net->rssi = (int8_t)(-val);
                break;
            }
        }
    }

    // Channel: standalone 1-14 number, not part of a larger number or an RSSI
    for(int i = 0; i < len; i++) {
        if(line[i] >= '1' && line[i] <= '9') {
            if(i > 0 && (line[i-1] == '-' || (line[i-1] >= '0' && line[i-1] <= '9'))) continue;
            int val = line[i] - '0';
            int j = i + 1;
            if(j < len && line[j] >= '0' && line[j] <= '9') {
                val = val * 10 + (line[j] - '0');
                j++;
            }
            if(j < len && line[j] >= '0' && line[j] <= '9') continue;
            if(val >= 1 && val <= 14) {
                net->channel = (uint8_t)val;
                break;
            }
        }
    }

    // Encryption keyword — checked longest-first so "WPA2" doesn't match as "WPA"
    if(strstr(line, "WPA3"))      snprintf(net->encryption, sizeof(net->encryption), "WPA3");
    else if(strstr(line, "WPA2")) snprintf(net->encryption, sizeof(net->encryption), "WPA2");
    else if(strstr(line, "WEP"))  snprintf(net->encryption, sizeof(net->encryption), "WEP");
    else if(strstr(line, "WPA"))  snprintf(net->encryption, sizeof(net->encryption), "WPA");
    else if(strstr(line, "OPEN")) snprintf(net->encryption, sizeof(net->encryption), "OPEN");
    else                          snprintf(net->encryption, sizeof(net->encryption), "?");

    // SSID: whatever text sits before the MAC address, trimmed of
    // separators and any leading row-index number ("12) ", "#3 ", etc.)
    int ssid_len = mac_pos;
    while(ssid_len > 0 && (line[ssid_len-1] == ' ' || line[ssid_len-1] == ',' ||
                            line[ssid_len-1] == '|' || line[ssid_len-1] == '\t')) {
        ssid_len--;
    }
    int ssid_start = 0;
    while(ssid_start < ssid_len &&
          (line[ssid_start] == ' ' || line[ssid_start] == '#' ||
           (line[ssid_start] >= '0' && line[ssid_start] <= '9'))) {
        ssid_start++;
    }
    while(ssid_start < ssid_len &&
          (line[ssid_start] == ')' || line[ssid_start] == ' ' ||
           line[ssid_start] == '.' || line[ssid_start] == '-')) {
        ssid_start++;
    }
    int copy_len = ssid_len - ssid_start;
    if(copy_len > (int)(WIFI_SSID_LEN - 1)) copy_len = WIFI_SSID_LEN - 1;
    if(copy_len > 0) {
        memcpy(net->ssid, line + ssid_start, (size_t)copy_len);
        net->ssid[copy_len] = '\0';
    } else {
        snprintf(net->ssid, sizeof(net->ssid), "(unknown)");
    }

    net->valid = true;
    result->count++;
}

// ─────────────────────────────────────────────────────────────────────────────
// Poll
// ─────────────────────────────────────────────────────────────────────────────

void esp32_marauder_poll(ESP32Marauder* dev, MarauderLineBuf* lines, WiFiScanResult* result) {
    if(!dev->serial) return;

    while(dev->rx_tail != dev->rx_head) {
        uint8_t byte = dev->rx_buf[dev->rx_tail];
        dev->rx_tail = (uint16_t)((dev->rx_tail + 1) % MARAUDER_RX_BUF_SIZE);

        if(byte == '\n' || byte == '\r') {
            if(dev->line_acc_len > 0) {
                dev->line_acc[dev->line_acc_len] = '\0';
                if(lines)  marauder_push_line(lines, dev->line_acc);
                if(result) marauder_try_parse_ap(dev->line_acc, dev->line_acc_len, result);
                dev->line_acc_len = 0;
            }
        } else if(dev->line_acc_len < MARAUDER_LINE_MAX - 1) {
            // Keep only printable ASCII — Marauder occasionally sends ANSI
            // colour codes which would otherwise garble the small display.
            if(byte >= 0x20 && byte < 0x7F) {
                dev->line_acc[dev->line_acc_len++] = (char)byte;
            }
        }
    }
}
