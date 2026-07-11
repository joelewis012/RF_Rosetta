#include "nrf24_scanner.h"
#include <furi.h>
#include <furi_hal.h>
#include <string.h>

#define NRF_REG_CONFIG    0x00
#define NRF_REG_EN_AA     0x01
#define NRF_REG_EN_RXADDR 0x02
#define NRF_REG_SETUP_AW  0x03
#define NRF_REG_RF_CH     0x05
#define NRF_REG_RF_SETUP  0x06
#define NRF_REG_RPD       0x09
#define NRF_CMD_R_RX_PLD  0x61
#define NRF_CMD_FLUSH_RX  0xE2
#define NRF_CMD_NOP       0xFF

// ─────────────────────────────────────────────────────────────────────────────
// Bit-bang SPI (CPOL=0, CPHA=0, MSB first)
// ─────────────────────────────────────────────────────────────────────────────

static uint8_t nrf_spi_xfer(const RFGPIOConfig* g, uint8_t data) {
    uint8_t result = 0;
    for(int8_t i = 7; i >= 0; i--) {
        furi_hal_gpio_write(g->mosi, (data >> i) & 1);
        furi_hal_gpio_write(g->sck,  true);
        result = (uint8_t)((result << 1) | (furi_hal_gpio_read(g->miso) ? 1 : 0));
        furi_hal_gpio_write(g->sck,  false);
    }
    return result;
}

static void nrf_write_reg(const RFGPIOConfig* g, uint8_t reg, uint8_t val) {
    furi_hal_gpio_write(g->csn, false);
    nrf_spi_xfer(g, 0x20 | (reg & 0x1F));
    nrf_spi_xfer(g, val);
    furi_hal_gpio_write(g->csn, true);
}

static uint8_t nrf_read_reg(const RFGPIOConfig* g, uint8_t reg) {
    furi_hal_gpio_write(g->csn, false);
    nrf_spi_xfer(g, reg & 0x1F);
    uint8_t val = nrf_spi_xfer(g, NRF_CMD_NOP);
    furi_hal_gpio_write(g->csn, true);
    return val;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void nrf24_scanner_init(const RFGPIOConfig* gpio) {
    const RFGPIOConfig* g = gpio;
    furi_hal_gpio_init(g->csn,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->mosi, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->sck,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->aux,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(g->miso, GpioModeInput,          GpioPullNo, GpioSpeedLow);

    furi_hal_gpio_write(g->csn,  true);
    furi_hal_gpio_write(g->sck,  false);
    furi_hal_gpio_write(g->aux,  false);
    furi_hal_gpio_write(g->mosi, false);
    furi_delay_ms(5);

    nrf_write_reg(gpio, NRF_REG_CONFIG,    0x03);
    furi_delay_ms(2);
    nrf_write_reg(gpio, NRF_REG_RF_SETUP,  0x06);
    nrf_write_reg(gpio, NRF_REG_EN_AA,     0x00);
    nrf_write_reg(gpio, NRF_REG_EN_RXADDR, 0x00);
}

bool nrf24_is_connected(const RFGPIOConfig* gpio) {
    return (nrf_read_reg(gpio, NRF_REG_CONFIG) == 0x03);
}

void nrf24_scanner_deinit(const RFGPIOConfig* gpio) {
    furi_hal_gpio_write(gpio->aux, false);
    nrf_write_reg(gpio, NRF_REG_CONFIG, 0x00);
    furi_hal_gpio_init(gpio->csn,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(gpio->mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(gpio->sck,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(gpio->aux,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(gpio->miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void nrf24_scanner_sweep(const RFGPIOConfig* gpio, NRF24ScanResult* result) {
    result->max_hits = 0;
    for(uint8_t ch = 0; ch < NRF24_CHANNELS; ch++) {
        nrf_write_reg(gpio, NRF_REG_RF_CH, ch);
        furi_hal_gpio_write(gpio->aux, true);
        furi_delay_us(170);
        uint8_t rpd = nrf_read_reg(gpio, NRF_REG_RPD) & 0x01;
        furi_hal_gpio_write(gpio->aux, false);
        if(rpd && result->hits[ch] < 255) result->hits[ch]++;
        if(result->hits[ch] > result->max_hits) result->max_hits = result->hits[ch];
    }
    result->sweep_count++;
    result->running = true;
}

void nrf24_scanner_reset(NRF24ScanResult* result) {
    memset(result->hits, 0, sizeof(result->hits));
    result->max_hits    = 0;
    result->sweep_count = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Packet capture (promiscuous mode trick)
// ─────────────────────────────────────────────────────────────────────────────

bool nrf24_capture_packet(const RFGPIOConfig* gpio, uint8_t channel,
                           uint16_t timeout_ms, NRF24Packet* out) {
    out->valid    = false;
    out->channel  = channel;
    out->freq_khz = 2401000 + (uint32_t)channel * 1000;

    nrf_write_reg(gpio, NRF_REG_CONFIG,    0x03);
    nrf_write_reg(gpio, NRF_REG_EN_AA,     0x00);
    nrf_write_reg(gpio, NRF_REG_EN_RXADDR, 0x01);
    nrf_write_reg(gpio, NRF_REG_SETUP_AW,  0x00);
    nrf_write_reg(gpio, NRF_REG_RF_CH,     channel);
    nrf_write_reg(gpio, 0x0A, 0xAA);
    nrf_write_reg(gpio, 0x11, 0x20);

    furi_hal_gpio_write(gpio->csn, false);
    nrf_spi_xfer(gpio, NRF_CMD_FLUSH_RX);
    furi_hal_gpio_write(gpio->csn, true);
    nrf_write_reg(gpio, 0x07, 0x70);

    furi_hal_gpio_write(gpio->aux, true);

    bool got = false;
    uint32_t limit = (uint32_t)timeout_ms * 10;
    for(uint32_t t = 0; t < limit; t++) {
        furi_delay_us(100);
        if(nrf_read_reg(gpio, 0x07) & 0x40) {
            out->length = 32;
            furi_hal_gpio_write(gpio->csn, false);
            nrf_spi_xfer(gpio, NRF_CMD_R_RX_PLD);
            for(uint8_t i = 0; i < out->length; i++)
                out->payload[i] = nrf_spi_xfer(gpio, NRF_CMD_NOP);
            furi_hal_gpio_write(gpio->csn, true);
            out->valid = true;
            got = true;
            nrf_write_reg(gpio, 0x07, 0x40);
            break;
        }
    }

    furi_hal_gpio_write(gpio->aux, false);
    furi_hal_gpio_write(gpio->csn, false);
    nrf_spi_xfer(gpio, NRF_CMD_FLUSH_RX);
    furi_hal_gpio_write(gpio->csn, true);
    nrf_write_reg(gpio, NRF_REG_CONFIG,    0x03);
    nrf_write_reg(gpio, NRF_REG_EN_AA,     0x00);
    nrf_write_reg(gpio, NRF_REG_EN_RXADDR, 0x00);
    nrf_write_reg(gpio, NRF_REG_SETUP_AW,  0x03);
    nrf_write_reg(gpio, NRF_REG_RF_SETUP,  0x06);

    return got;
}

// ─────────────────────────────────────────────────────────────────────────────
// Packet decode
// ─────────────────────────────────────────────────────────────────────────────

NRF24PacketType nrf24_decode_packet(const NRF24Packet* pkt, NRF24Decode* out) {
    out->type          = NRF24TypeUnknown;
    out->security_flag = false;

    if(!pkt || !pkt->valid || pkt->length < 4) {
        snprintf(out->summary, sizeof(out->summary), "Too short");
        snprintf(out->detail,  sizeof(out->detail),  "Need >= 4 bytes");
        return NRF24TypeUnknown;
    }

    const uint8_t* d = pkt->payload;
    uint8_t        n = pkt->length;

    bool ble_ch = (pkt->channel == 2 || pkt->channel == 26 || pkt->channel == 80);
    if(ble_ch && n >= 8) {
        uint8_t pdu = d[2] & 0x0F;
        if(pdu <= 7 && 9 < n) {
            out->type = NRF24TypeBLEAdv;
            snprintf(out->summary, sizeof(out->summary), "BLE Adv PDU %d", pdu);
            snprintf(out->detail, sizeof(out->detail),
                "BLE Advertising\nPDU: %d\nAddr: %02X:%02X:%02X\n      %02X:%02X:%02X\nch%d",
                pdu, d[9],d[8],d[7],d[6],d[5],d[4], pkt->channel);
            return NRF24TypeBLEAdv;
        }
    }

    bool logi = (pkt->channel % 3 == 2) && pkt->channel < 50;
    if(logi && n >= 5 && d[0] != 0x00 && d[4] == 0x00) {
        out->type = NRF24TypeLogitek;
        out->security_flag = true;
        snprintf(out->summary, sizeof(out->summary), "Logitech Unifying 0x%02X", d[0]);
        snprintf(out->detail, sizeof(out->detail),
            "Logitech Unifying\nDevice: 0x%02X\nType: 0x%02X\n[!] MouseJack risk", d[0], d[1]);
        return NRF24TypeLogitek;
    }

    if(n >= 5 && d[1] == 0x00 && d[0] != 0xFF && d[0] != 0xAA) {
        out->type = NRF24TypeMouseJack;
        out->security_flag = true;
        snprintf(out->summary, sizeof(out->summary), "MouseJack! Unencrypted 0x%02X", d[0]);
        snprintf(out->detail, sizeof(out->detail),
            "[!] MouseJack Risk\nDev: 0x%02X\nEncrypt: NONE\nHID injectable!\nmousejack.com", d[0]);
        return NRF24TypeMouseJack;
    }

    out->type = NRF24TypeShockBurst;
    char hex[28] = {0};
    uint8_t show = n > 8 ? 8 : n;
    int hp = 0;
    for(uint8_t i = 0; i < show; i++)
        hp += snprintf(hex+hp, (int)sizeof(hex)-hp, "%02X ", d[i]);
    snprintf(out->summary, sizeof(out->summary),
        "ShockBurst ch%d %luMHz", pkt->channel, (unsigned long)(pkt->freq_khz/1000));
    snprintf(out->detail, sizeof(out->detail),
        "Nordic ShockBurst\nCh: %d\nFreq: %lu MHz\nBytes: %s\nLen: %d",
        pkt->channel, (unsigned long)(pkt->freq_khz/1000), hex, n);
    return NRF24TypeShockBurst;
}
