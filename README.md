<div align="center">

```
██████╗ ███████╗    ██████╗  ██████╗ ███████╗███████╗████████╗████████╗ █████╗
██╔══██╗██╔════╝    ██╔══██╗██╔═══██╗██╔════╝██╔════╝╚══██╔══╝╚══██╔══╝██╔══██╗
██████╔╝█████╗      ██████╔╝██║   ██║███████╗█████╗     ██║      ██║   ███████║
██╔══██╗██╔══╝      ██╔══██╗██║   ██║╚════██║██╔══╝     ██║      ██║   ██╔══██║
██║  ██║██║         ██║  ██║╚██████╔╝███████║███████╗   ██║      ██║   ██║  ██║
╚═╝  ╚═╝╚═╝         ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚══════╝   ╚═╝      ╚═╝   ╚═╝  ╚═╝
```

# RF Rosetta

### *Every signal has a story. Now you can read it.*

**The first universal RF signal identifier for the Flipper Zero.**

Point your Flipper at any unknown wireless signal and find out exactly what it is —
car key, tyre sensor, alarm, smart meter, medical device, weather station, or one of
50+ other identified protocols. In seconds.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-FAP-orange)](https://flipperzero.one)
[![Version](https://img.shields.io/badge/version-1.0-green)]()

---

### If RF Rosetta has been useful, consider buying me a coffee ☕

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-Support%20this%20project-yellow?logo=buy-me-a-coffee)](https://buymeacoffee.com/Joelewis012)

---

</div>

## What is RF Rosetta?

RF Rosetta is a **passive RF signal identification tool** for the Flipper Zero. It
listens across the Sub-GHz spectrum, captures any signal that rises above the noise
floor, analyses its physical characteristics — frequency, modulation, pulse timing,
repetition pattern, bandwidth — and matches them against a database of 50+ known
wireless protocols.

It then tells you not just *what it is*, but everything meaningful about it: what
device made it, what data it contains, whether it's encrypted, whether it has known
security vulnerabilities, and what could theoretically be decoded from it.

No external app required. No cloud. Everything runs on the Flipper.

---

## How it works

```
  LISTENING              CAUGHT               ANALYSING              IDENTIFIED
  ─────────              ──────               ─────────              ──────────
  Sweeping               Signal               Frequency ✓            Car Key Fob
  433MHz...              burst                Modulation ✓           KeeLoq Rolling
                         detected             Pulse width ✓          87% confidence
  ≈≈≈≈≈≈≈≈≈≈                                 Repetition ✓
                                              Bandwidth  ✓           Security: Safe
                                              Matching...            Rolling code ✓
```

1. **Sweep** — scans across all major ISM bands (315, 433, 868, 915 MHz and more)
2. **Catch** — detects when a signal rises above the configurable noise threshold
3. **Analyse** — extracts frequency, modulation, pulse timing, bandwidth, and repetition
4. **Match** — scores the signal against every entry in the protocol database
5. **Report** — displays the result with confidence rating, security notes, and decodable data

---

## What it can identify

| Category | Protocols |
|---|---|
| 🚗 **Automotive** | Car key fobs (fixed & rolling / KeeLoq), TPMS tyre pressure sensors (315 & 433 MHz), keyless entry |
| 🏠 **Home** | Wireless doorbells, garage/gate remotes (fixed & rolling code), mains socket remotes, motorised blinds, RF light switches, wireless thermostats |
| 🔒 **Security** | PIR motion sensors, door/window contact sensors, panic buttons, smoke/CO detectors, wireless sirens, alarm keyfobs |
| 🌦 **Weather** | Weather stations (433 & 868 MHz), soil moisture sensors, rain gauges, anemometers |
| 📡 **IoT** | Z-Wave devices (EU 868 MHz & US 908 MHz), LoRa nodes (EU 868 MHz & US 915 MHz) |
| ⚡ **Utility** | Smart electricity meters AMR (433 & 868 MHz), gas meters, water meters |
| 🏭 **Industrial** | SCADA field sensors, asset tracking beacons, ISM band nodes |
| 💊 **Medical** | Dexcom CGM, Abbott FreeStyle Libre, hearing aid sync signals, personal medical alerts |
| 🎮 **Consumer** | Wireless headphones pairing, game controller sync, wireless keyboards/mice (MouseJack) |
| 🎈 **Misc** | Baby monitors, radiosondes (weather balloons), RC vehicle remotes, pool/spa sensors, wildlife tracking tags |

---

## What it tells you

For every identified signal, RF Rosetta provides:

**Signal measurements**
- Exact frequency in MHz
- RSSI (signal strength in dBm) with real-time trend (▲ getting closer / ▼ moving away)
- Noise floor and SNR at time of capture
- Modulation type (OOK, FSK, GFSK, LoRa/CSS, FM)
- Channel bandwidth, pulse width, estimated bit count, repetition count

**Protocol details**
- Full protocol name, category, and confidence score (0–100%)
- Known brands and devices that use this protocol
- Plain-English description of what the device does

**Security analysis**
- Fixed code vs rolling code — tells you if a signal is replay-vulnerable
- Encryption status and known vulnerability flags (MouseJack, relay attack, etc.)
- Plain-language security note for every protocol

**Signal fingerprinting**
- Detects if the same device transmits again in the same session
- `[x2]` badge shows repeat count — useful for confirming you're tracking the right signal

---

## Scan Modes

| Mode | Preset | Best For |
|---|---|---|
| **All (default)** | Cycles OOK → FSK-N → FSK-W | Unknown signals — catches everything |
| **OOK only** | OOK 650kHz BW | Remotes, doorbells, garage doors, sensors |
| **FSK Narrow** | 2-FSK 238kHz deviation | TPMS, weather stations, utility meters |
| **FSK Wide** | 2-FSK 476kHz deviation | Industrial sensors, pagers, unknown FSK |

**Sweep speed** — on the scanning screen, Left/Right arrows adjust dwell time per frequency
(shown bottom-right, e.g. `300ms`). Slower = more sensitive. Faster = wider coverage.

---

## NRF24 2.4 GHz Scanner

RF Rosetta includes a built-in **NRF24L01 channel scanner** for 2.4 GHz. Launch it
from **NRF24 2.4GHz Scan** on the main menu.

- Sweeps all 125 channels (2.401–2.525 GHz)
- Live bar graph with WiFi (ch1/6/11) and BLE advertising channel markers
- Press **OK** to attempt packet capture on the busiest channel
- Captured packet shown as hex bytes with frequency

> Requires your 3-in-1 dev board with the physical switch in NRF24 position.
> The app detects whether the chip is connected — if not, it shows a "Not Detected" screen.

---

## Dev Board Support (3-in-1)

RF Rosetta is designed for the **3-in-1 Flipper Zero expansion board** (CC1101 + NRF24 + ESP32).

| Module | What it does in RF Rosetta |
|---|---|
| **CC1101 (high gain)** | Sub-GHz scanning — better range than internal antenna |
| **NRF24L01** | 2.4 GHz channel scanner and packet capture |
| **ESP32 (Marauder)** | Coming soon — WiFi network scanner |

### GPIO Pinout

All three chips share the same SPI bus. The physical switch on the board selects
which chip is active. ESP32 is always available via UART.

| Flipper GPIO | Signal | CC1101 / NRF24 |
|---|---|---|
| Pin 2 (PA7) | MOSI | Both |
| Pin 3 (PA6) | MISO | Both |
| Pin 4 (PA4) | CSN | Both |
| Pin 5 (PB3) | SCK | Both |
| Pin 6 (PB2) | CE | NRF24 only |
| Pin 13 | UART TX | ESP32 RX |
| Pin 14 | UART RX | ESP32 TX |

To use the external CC1101: set **Antenna → External** in Settings.
RF Rosetta will detect the chip automatically. If it can't find it, it falls back
to the Flipper's internal antenna and shows a status message.

---

## Installation

### Via FAP Catalog (Recommended)
1. Open the Flipper Zero app on your phone
2. Go to **Apps → Sub-GHz**
3. Find **RF Rosetta** and tap Install

### Manual Install
1. Download the latest `.fap` from [Releases](../../releases)
2. Copy to `SD:/apps/Sub-GHz/rf_rosetta.fap`
3. Launch from the Flipper's Apps menu

### Build from Source
```bash
pip install ufbt
git clone https://github.com/joelewis012/rf_rosetta
cd rf_rosetta
ufbt
```

---

## Settings

| Setting | Options | Notes |
|---|---|---|
| Scan Mode | All / OOK / FSK-N / FSK-W | All is the default — covers every signal type |
| Antenna | Internal / External | External uses the CC1101 on your dev board |
| Threshold | -90 to -60 dBm | Lower = more sensitive, more false triggers |
| SD Logging | On / Off | Saves all captures to `SD:/rf_rosetta/log.txt` |

---

## Protocol Database

The database lives in `protocol_db.c`. Adding a new protocol:

```c
{
    .name          = "My New Protocol",
    .short_name    = "My Proto",
    .category      = CategoryHome,
    .freq_min      = 433050000, .freq_max = 433920000,
    .modulation    = ModulationOOK,
    .pulse_min     = 300, .pulse_max = 900,
    .bandwidth_khz = 40,
    .repeating     = true, .repeat_min = 2, .repeat_max = 4,
    .fixed_code    = true,
    .brands        = "Brand A, Brand B",
    .description   = "What this device does.",
    .security_note = "Security implications.",
    .extra_data    = "What data can be decoded.",
},
```

Pull requests for new protocols are very welcome.

---

## SD Card Files

| File | Contents |
|---|---|
| `SD:/rf_rosetta/log.txt` | Timestamped log of every captured signal |
| `SD:/rf_rosetta/saved.bin` | Saved/bookmarked signal captures |

---

## FAQ

**Will this work on all Flipper Zero hardware?**
Yes — the Sub-GHz scanner uses the built-in CC1101. NRF24 scanning requires the dev board.

**Does it decode the actual content of signals?**
RF Rosetta identifies the protocol and tells you what could be decoded. Full live
decoding (TPMS pressure values, meter readings as numbers) is on the roadmap.

**Is this legal to use?**
RF Rosetta is a passive listener — it does not transmit anything. Passive reception
is legal in virtually all jurisdictions. What you do with the information is your responsibility.

**A signal isn't being identified — what should I do?**
Check the full details screen for the raw characteristics, then open a GitHub issue.
Unknown signals help grow the database.

---

## Roadmap

- [ ] Live TPMS pressure and temperature decoding
- [ ] Smart meter consumption number extraction
- [ ] Weather station temperature/humidity live decode
- [ ] ESP32/Marauder WiFi network scanner integration
- [ ] Live TPMS pressure and temperature decoding
- [ ] Community protocol submission via GitHub Issues template

---

## Contributing

Contributions are welcome, especially:
- New protocol signatures in `protocol_db.c`
- Bug reports with captured signal details
- ESP32 companion code for WiFi scanning

Please open an issue before a large PR so we can discuss approach.

---

## License

GPL-3.0 — see [LICENSE](LICENSE)

---

<div align="center">

**Built for the Flipper Zero community.**

*If RF Rosetta identified something interesting — share it!*

☕ **[Buy me a coffee](https://buymeacoffee.com/Joelewis012)** if this saved you time.

</div>
