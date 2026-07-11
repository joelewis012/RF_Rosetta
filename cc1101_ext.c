#include "cc1101_ext.h"
#include <furi.h>
#include <furi_hal.h>
#include <string.h>

#define CC_SRES        0x30
#define CC_SRX         0x34
#define CC_SIDLE       0x36
#define CC_SPWD        0x39

#define CC_REG_FREQ2   0x0D
#define CC_REG_FREQ1   0x0E
#define CC_REG_FREQ0   0x0F
#define CC_REG_VERSION 0x31
#define CC_REG_RSSI    0x34

#define CC_READ        0x80
#define CC_BURST       0x40
#define CC_WRITE       0x00

// ─────────────────────────────────────────────────────────────────────────────
// Bit-bang SPI (CPOL=0, CPHA=0, MSB first)
// ─────────────────────────────────────────────────────────────────────────────

static uint8_t spi_xfer(const RFGPIOConfig* g, uint8_t data) {
    uint8_t result = 0;
    for(int8_t i = 7; i >= 0; i--) {
        furi_hal_gpio_write(g->mosi, (data >> i) & 1);
        furi_hal_gpio_write(g->sck,  true);
        result = (uint8_t)((result << 1) | (furi_hal_gpio_read(g->miso) ? 1 : 0));
        furi_hal_gpio_write(g->sck,  false);
    }
    return result;
}

static void csn_low(const RFGPIOConfig* g) {
    furi_hal_gpio_write(g->csn, false);
    uint8_t t = 100;
    while(furi_hal_gpio_read(g->miso) && t--) furi_delay_us(10);
}

static void csn_high(const RFGPIOConfig* g) {
    furi_hal_gpio_write(g->csn, true);
}

static void cc1101_write_reg(const RFGPIOConfig* g, uint8_t addr, uint8_t val) {
    csn_low(g);
    spi_xfer(g, CC_WRITE | (addr & 0x3F));
    spi_xfer(g, val);
    csn_high(g);
}

static uint8_t cc1101_read_status(const RFGPIOConfig* g, uint8_t addr) {
    csn_low(g);
    spi_xfer(g, CC_READ | CC_BURST | (addr & 0x3F));
    uint8_t val = spi_xfer(g, 0x00);
    csn_high(g);
    return val;
}

static void cc1101_strobe(const RFGPIOConfig* g, uint8_t cmd) {
    csn_low(g);
    spi_xfer(g, cmd);
    csn_high(g);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool cc1101_ext_init(const RFGPIOConfig* gpio) {
    const RFGPIOConfig* g = gpio;
    furi_hal_gpio_init(g->csn,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->mosi, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->sck,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->miso, GpioModeInput,          GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->aux,  GpioModeInput,          GpioPullNo, GpioSpeedLow);

    furi_hal_gpio_write(g->csn,  true);
    furi_hal_gpio_write(g->sck,  false);
    furi_hal_gpio_write(g->mosi, false);
    furi_delay_ms(5);

    cc1101_strobe(g, CC_SRES);
    furi_delay_ms(2);

    return cc1101_ext_is_connected(gpio);
}

void cc1101_ext_deinit(const RFGPIOConfig* gpio) {
    const RFGPIOConfig* g = gpio;
    cc1101_strobe(g, CC_SPWD);
    furi_hal_gpio_init(g->csn,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->sck,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->aux,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

bool cc1101_ext_is_connected(const RFGPIOConfig* gpio) {
    return (cc1101_read_status(gpio, CC_REG_VERSION) == 0x14);
}

void cc1101_ext_load_preset(const RFGPIOConfig* gpio, const uint8_t* preset) {
    for(uint8_t i = 0; preset[i] != 0 || preset[i+1] != 0; i += 2) {
        cc1101_write_reg(gpio, preset[i], preset[i+1]);
    }
}

void cc1101_ext_set_frequency(const RFGPIOConfig* gpio, uint32_t freq_hz) {
    uint64_t tmp      = (uint64_t)freq_hz * 65536ULL;
    uint32_t freq_reg = (uint32_t)(tmp / 26000000ULL);
    cc1101_strobe(gpio, CC_SIDLE);
    cc1101_write_reg(gpio, CC_REG_FREQ2, (uint8_t)((freq_reg >> 16) & 0xFF));
    cc1101_write_reg(gpio, CC_REG_FREQ1, (uint8_t)((freq_reg >> 8)  & 0xFF));
    cc1101_write_reg(gpio, CC_REG_FREQ0, (uint8_t)(freq_reg         & 0xFF));
}

void cc1101_ext_rx(const RFGPIOConfig* gpio) {
    cc1101_write_reg(gpio, 0x00, 0x0C); // GDO0: high during packet RX
    cc1101_strobe(gpio, CC_SRX);
}

void cc1101_ext_idle(const RFGPIOConfig* gpio) {
    cc1101_strobe(gpio, CC_SIDLE);
}

float cc1101_ext_get_rssi(const RFGPIOConfig* gpio) {
    uint8_t raw = cc1101_read_status(gpio, CC_REG_RSSI);
    if(raw >= 128) return ((float)(raw - 256) / 2.0f) - 74.0f;
    return ((float)raw / 2.0f) - 74.0f;
}

uint16_t cc1101_ext_capture_pulses(const RFGPIOConfig* gpio,
                                   uint32_t* buf, uint16_t buf_size,
                                   uint16_t timeout_ms) {
    uint16_t count   = 0;
    uint32_t timeout = (uint32_t)timeout_ms * 1000;
    uint32_t elapsed = 0;

    bool last = furi_hal_gpio_read(gpio->aux);
    while(elapsed < timeout) {
        bool now = furi_hal_gpio_read(gpio->aux);
        if(now != last) break;
        furi_delay_us(10);
        elapsed += 10;
    }
    if(elapsed >= timeout) return 0;

    last = furi_hal_gpio_read(gpio->aux);
    uint32_t t   = 0;
    uint32_t cap = 0;

    while(count < buf_size && cap < 50000) {
        bool now = furi_hal_gpio_read(gpio->aux);
        if(now != last) {
            buf[count++] = t > 65535 ? 65535 : t;
            t    = 0;
            last = now;
        }
        furi_delay_us(4);
        t   += 4;
        cap += 4;
    }
    return count;
}
