#include "nrf24_scanner.h"
#include <furi.h>
#include <furi_hal.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// GPIO pin references (extern declared in furi_hal_gpio.h)
// ─────────────────────────────────────────────────────────────────────────────

#define NRF_MOSI (&gpio_ext_pa7)
#define NRF_MISO (&gpio_ext_pa6)
#define NRF_CSN  (&gpio_ext_pa4)
#define NRF_SCK  (&gpio_ext_pb3)
#define NRF_CE   (&gpio_ext_pb2)

// ─────────────────────────────────────────────────────────────────────────────
// NRF24 register addresses
// ─────────────────────────────────────────────────────────────────────────────

#define NRF_REG_CONFIG    0x00
#define NRF_REG_EN_AA     0x01
#define NRF_REG_EN_RXADDR 0x02
#define NRF_REG_SETUP_AW  0x03
#define NRF_REG_RF_CH     0x05
#define NRF_REG_RF_SETUP  0x06
#define NRF_REG_RPD       0x09  // Received Power Detector (bit 0)

#define NRF_CMD_W_REG     0x20
#define NRF_CMD_R_REG     0x00
#define NRF_CMD_NOP       0xFF

// ─────────────────────────────────────────────────────────────────────────────
// Bit-bang SPI
// CPOL=0, CPHA=0 (SPI mode 0), MSB first
// ─────────────────────────────────────────────────────────────────────────────

static uint8_t nrf24_spi_xfer(uint8_t data) {
    uint8_t result = 0;
    for(int8_t i = 7; i >= 0; i--) {
        furi_hal_gpio_write(NRF_MOSI, (data >> i) & 1);
        furi_hal_gpio_write(NRF_SCK, true);
        result = (result << 1) | (furi_hal_gpio_read(NRF_MISO) ? 1 : 0);
        furi_hal_gpio_write(NRF_SCK, false);
    }
    return result;
}

static void nrf24_write_reg(uint8_t reg, uint8_t val) {
    furi_hal_gpio_write(NRF_CSN, false);
    nrf24_spi_xfer(NRF_CMD_W_REG | (reg & 0x1F));
    nrf24_spi_xfer(val);
    furi_hal_gpio_write(NRF_CSN, true);
}

static uint8_t nrf24_read_reg(uint8_t reg) {
    furi_hal_gpio_write(NRF_CSN, false);
    nrf24_spi_xfer(NRF_CMD_R_REG | (reg & 0x1F));
    uint8_t val = nrf24_spi_xfer(NRF_CMD_NOP);
    furi_hal_gpio_write(NRF_CSN, true);
    return val;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void nrf24_scanner_init(void) {
    // Configure GPIO
    furi_hal_gpio_init(NRF_CSN,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF_MOSI, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF_SCK,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF_CE,   GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF_MISO, GpioModeInput,          GpioPullNo, GpioSpeedLow);

    // Safe initial states
    furi_hal_gpio_write(NRF_CSN,  true);   // CS deasserted
    furi_hal_gpio_write(NRF_SCK,  false);  // clock idle low
    furi_hal_gpio_write(NRF_CE,   false);  // RX disabled
    furi_hal_gpio_write(NRF_MOSI, false);

    furi_delay_ms(5); // VCC ramp-up / power-on reset

    // CONFIG: PWR_UP=1, PRIM_RX=1, CRC disabled (0b00000011)
    nrf24_write_reg(NRF_REG_CONFIG, 0x03);
    furi_delay_ms(2); // oscillator startup

    // RF_SETUP: 1 Mbps data rate, 0 dBm PA output (0b00000110)
    nrf24_write_reg(NRF_REG_RF_SETUP, 0x06);

    // Disable auto-ACK and all RX pipes (we just need RPD)
    nrf24_write_reg(NRF_REG_EN_AA,     0x00);
    nrf24_write_reg(NRF_REG_EN_RXADDR, 0x00);
}

void nrf24_scanner_deinit(void) {
    furi_hal_gpio_write(NRF_CE, false);
    nrf24_write_reg(NRF_REG_CONFIG, 0x00); // power down

    // Release pins back to floating analog — don't drive bus
    furi_hal_gpio_init(NRF_CSN,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF_MOSI, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF_SCK,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF_CE,   GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF_MISO, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void nrf24_scanner_sweep(NRF24ScanResult* result) {
    result->max_hits = 0;

    for(uint8_t ch = 0; ch < NRF24_CHANNELS; ch++) {
        // Tune to channel
        nrf24_write_reg(NRF_REG_RF_CH, ch);

        // Pulse CE high to enable RX — needs 130µs settling
        furi_hal_gpio_write(NRF_CE, true);
        furi_delay_us(170);

        // Read Received Power Detector — bit 0 = signal present
        uint8_t rpd = nrf24_read_reg(NRF_REG_RPD) & 0x01;

        furi_hal_gpio_write(NRF_CE, false);

        if(rpd) {
            if(result->hits[ch] < 255) result->hits[ch]++;
        }
        if(result->hits[ch] > result->max_hits) {
            result->max_hits = result->hits[ch];
        }
    }

    result->sweep_count++;
    result->running = true;
}

void nrf24_scanner_reset(NRF24ScanResult* result) {
    memset(result->hits, 0, sizeof(result->hits));
    result->max_hits   = 0;
    result->sweep_count = 0;
}
