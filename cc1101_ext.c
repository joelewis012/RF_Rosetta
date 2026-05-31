#include "cc1101_ext.h"
#include <furi.h>
#include <furi_hal.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// GPIO pins (same physical bus as NRF24, board switch selects which chip)
// ─────────────────────────────────────────────────────────────────────────────

#define EXT_CSN  (&gpio_ext_pa4)
#define EXT_MOSI (&gpio_ext_pa7)
#define EXT_MISO (&gpio_ext_pa6)
#define EXT_SCK  (&gpio_ext_pb3)

// ─────────────────────────────────────────────────────────────────────────────
// CC1101 command strobes and register addresses
// ─────────────────────────────────────────────────────────────────────────────

#define CC_SRES   0x30  // Reset
#define CC_SRX    0x34  // Enter RX
#define CC_SIDLE  0x36  // Enter IDLE
#define CC_SPWD   0x39  // Power down

#define CC_REG_FREQ2   0x0D
#define CC_REG_FREQ1   0x0E
#define CC_REG_FREQ0   0x0F
#define CC_REG_VERSION 0x31  // Status reg, returns 0x14
#define CC_REG_RSSI    0x34  // Status reg, current RSSI

// Header byte flags
#define CC_READ    0x80  // R/W = 1
#define CC_BURST   0x40  // Burst access (required for status registers)
#define CC_WRITE   0x00

// ─────────────────────────────────────────────────────────────────────────────
// Bit-bang SPI (CPOL=0, CPHA=0, MSB first)
// ─────────────────────────────────────────────────────────────────────────────

static uint8_t ext_spi_xfer(uint8_t data) {
    uint8_t result = 0;
    for(int8_t i = 7; i >= 0; i--) {
        furi_hal_gpio_write(EXT_MOSI, (data >> i) & 1);
        furi_hal_gpio_write(EXT_SCK, true);
        result = (result << 1) | (furi_hal_gpio_read(EXT_MISO) ? 1 : 0);
        furi_hal_gpio_write(EXT_SCK, false);
    }
    return result;
}

// Wait for CC1101 CHIP_RDY (MISO goes low after CSn asserted)
static void ext_wait_ready(void) {
    uint8_t timeout = 100;
    while(furi_hal_gpio_read(EXT_MISO) && timeout--) {
        furi_delay_us(10);
    }
}

static void ext_csn_low(void)  { furi_hal_gpio_write(EXT_CSN, false); ext_wait_ready(); }
static void ext_csn_high(void) { furi_hal_gpio_write(EXT_CSN, true); }

// Write a configuration register (addresses 0x00-0x2E)
static void cc1101_write_reg(uint8_t addr, uint8_t val) {
    ext_csn_low();
    ext_spi_xfer(CC_WRITE | (addr & 0x3F));
    ext_spi_xfer(val);
    ext_csn_high();
}

// Read a status register (addresses 0x30-0x3D, needs burst bit)
static uint8_t cc1101_read_status(uint8_t addr) {
    ext_csn_low();
    ext_spi_xfer(CC_READ | CC_BURST | (addr & 0x3F));
    uint8_t val = ext_spi_xfer(0x00);
    ext_csn_high();
    return val;
}

// Send a command strobe
static void cc1101_strobe(uint8_t cmd) {
    ext_csn_low();
    ext_spi_xfer(cmd);
    ext_csn_high();
}

// Write a burst of configuration registers from a preset array
// Format: {addr, val, addr, val, ..., 0x00, 0x00}
void cc1101_ext_load_preset(const uint8_t* preset) {
    for(uint8_t i = 0; preset[i] != 0 || preset[i + 1] != 0; i += 2) {
        cc1101_write_reg(preset[i], preset[i + 1]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool cc1101_ext_init(void) {
    // Configure GPIO
    furi_hal_gpio_init(EXT_CSN,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(EXT_MOSI, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(EXT_SCK,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(EXT_MISO, GpioModeInput,          GpioPullNo, GpioSpeedLow);

    furi_hal_gpio_write(EXT_CSN,  true);
    furi_hal_gpio_write(EXT_SCK,  false);
    furi_hal_gpio_write(EXT_MOSI, false);

    furi_delay_ms(5);

    // Reset chip
    cc1101_strobe(CC_SRES);
    furi_delay_ms(2);

    return cc1101_ext_is_connected();
}

void cc1101_ext_deinit(void) {
    cc1101_strobe(CC_SPWD);
    furi_hal_gpio_init(EXT_CSN,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(EXT_MOSI, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(EXT_SCK,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(EXT_MISO, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

bool cc1101_ext_is_connected(void) {
    // VERSION register returns 0x14 for CC1101
    uint8_t version = cc1101_read_status(CC_REG_VERSION);
    return (version == 0x14);
}

void cc1101_ext_set_frequency(uint32_t freq_hz) {
    // freq_reg = freq_hz / (26MHz / 2^16)
    uint32_t freq_reg = (uint32_t)((double)freq_hz / (26000000.0 / 65536.0));
    cc1101_strobe(CC_SIDLE);
    cc1101_write_reg(CC_REG_FREQ2, (freq_reg >> 16) & 0xFF);
    cc1101_write_reg(CC_REG_FREQ1, (freq_reg >> 8)  & 0xFF);
    cc1101_write_reg(CC_REG_FREQ0,  freq_reg        & 0xFF);
}

void cc1101_ext_rx(void) {
    cc1101_strobe(CC_SRX);
}

void cc1101_ext_idle(void) {
    cc1101_strobe(CC_SIDLE);
}

float cc1101_ext_get_rssi(void) {
    uint8_t raw = cc1101_read_status(CC_REG_RSSI);
    float rssi;
    if(raw >= 128) {
        rssi = ((float)(raw - 256) / 2.0f) - 74.0f;
    } else {
        rssi = ((float)raw / 2.0f) - 74.0f;
    }
    return rssi;
}
