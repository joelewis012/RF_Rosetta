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

Point your Flipper at any unknown wireless signal and find out exactly what it is.
car key, tyre sensor, alarm, smart meter, medical device, weather station, or one of
50+ other identified protocols. In seconds.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-FAP-orange)](https://flipperzero.one)
[![Version](https://img.shields.io/badge/version-1.0-green)]()

</div>

---

## What is RF Rosetta?

RF Rosetta is a **passive RF signal identification tool** for the Flipper Zero. It
listens across the Sub-GHz spectrum, captures any signal that rises above the noise
floor, analyses its physical characteristics. frequency, modulation, pulse timing,
repetition pattern, bandwidth and matches them against a database of 50+ known
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

1. **Sweep** — scans across major ISM bands (315, 433, 868, 915 MHz and more)
2. **Catch** — detects when a signal rises above the configurable noise threshold
3. **Analyse** — extracts frequency, modulation, pulse timing, bandwidth, and repetition characteristics
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
| 🎈 **Misc** | Baby monitors (433 & 864 MHz), radiosondes (weather balloons), RC vehicle remotes, pool/spa sensors, wildlife tracking tags |

> **Note:** 2.4 GHz protocols (Zigbee, Bluetooth, 2.4 GHz HID) require the ESP32 WiFi
> module. RF Rosetta flags these automatically and indicates when the module is needed.

---

## What it tells you

For every identified signal, RF Rosetta provides:

**Signal measurements**
- Exact frequency in MHz
- RSSI (signal strength in dBm)
- Noise floor and SNR at time of capture
- Modulation type (OOK, FSK, GFSK, LoRa/CSS, FM)
- Channel bandwidth
- Pulse width (min / avg / max in µs)
- Estimated bit count
- Repetition count

**Protocol details**
- Full protocol name and category
- Confidence score (0–100%) with label (High / Medium / Low)
- Known brands and devices that use this protocol
- Plain-English description of what the device does

**Security analysis**
- Fixed code vs rolling code Tells you immediately if a signal is replay-vulnerable
- Encryption: yes/no/type
- Known vulnerability flags (e.g. MouseJack, relay attack, jamming vulnerability)
- Clear, plain-language security note for every protocol

**Decodable data**
- What data can theoretically be extracted (e.g. TPMS: pressure, temp, battery; weather station: temperature, humidity; smart meter: energy consumption)
- Decoded raw capture stats

---

## Examples

**Passing car → TPMS sensor detected**
```
MATCHED  94%
TPMS 433 MHz
Automotive  FSK  433MHz
Confidence: High

Decodable: Sensor ID, tyre pressure (Bar/PSI),
           temperature (C), battery status
Security: Unencrypted. Fixed sensor ID could
          identify a specific vehicle.
```

**Neighbour's smart meter**
```
MATCHED  81%
Smart Electricity Meter AMR
Utility  FSK  868MHz
Confidence: High

! See security details

Security: Older AMR meters broadcast consumption
          data without encryption. Your energy
          usage is visible to anyone nearby.
Decodable: Meter ID, current consumption (kWh),
           accumulated total, rate period
```

**Garage door remote**
```
MATCHED  88%
Garage / Gate Remote (Fixed Code)
Home  OOK  433MHz
Confidence: High

! Security concern

Security: VULNERABLE: Fixed code is highly
          vulnerable to capture and replay.
          Extremely common attack target.
```

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
# Install ufbt if you haven't already
pip install ufbt

# Clone and build
git clone https://github.com/yourusername/rf_rosetta
cd rf_rosetta
ufbt

# Deploy to connected Flipper
ufbt launch
```

---

## Usage

### Scanning for signals

1. Launch **RF Rosetta** from the Sub-GHz apps menu
2. Select **Scan for Signals**
3. The Flipper begins sweeping across all major ISM bands
4. Signal bars show real-time RSSI; the sparkline tracks recent history
5. When a signal is detected, capture is triggered automatically
6. The result screen shows the identification and confidence

### Saving a signal
- On the result screen, press **OK / Centre** to save
- Saved signals are stored to SD and survive power-off
- View saved signals from the main menu

### Viewing full details
- On the result screen, press **Right** or **Details**
- Scroll through the full breakdown with Up/Down

### Settings

| Setting | Options | Notes |
|---|---|---|
| Scan Mode | Sub-GHz / RF Narrow / RF Wide | Narrow = more sensitive; Wide = catches more types |
| Antenna | Internal / External | See GPIO wiring below |
| Threshold | -90 to -60 dBm | Lower = more sensitive, more false triggers |
| SD Logging | On / Off | Appends all captures to `SD:/rf_rosetta/log.txt` |

---

## External Antenna Wiring

RF Rosetta supports switching to an external antenna via GPIO for improved range and
directional capture.

| Flipper GPIO Pin | Function |
|---|---|
| `PA7` (Pin 2) | Antenna switch control (HIGH = external) |
| `GND` (Pin 8 or 9) | Ground reference |

Connect an SMA RF switch module between `PA7` and your external antenna. Set
**Antenna → External** in Settings to activate.

> Standard 433 MHz or 868 MHz whip antennas will significantly increase detection
> range. Directional Yagi antennas allow you to pinpoint a signal source.

---

## Protocol Database

The database lives in `protocol_db.c`. Each entry is a `ProtocolSignature` struct with
all characteristics needed for matching. Adding a new protocol is as simple as adding a
new entry to the `DB[]` array.

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

Pull requests adding new protocols are very welcome — especially for industrial,
utility, and regional-specific devices.

---

## SD Card Files

RF Rosetta creates a folder at `SD:/rf_rosetta/` containing:

| File | Contents |
|---|---|
| `log.txt` | Timestamped log of every captured signal |
| `saved.bin` | Your saved/bookmarked signal captures |

---

## FAQ

**Will this work on all Flipper Zero hardware?**
Yes, It uses the built-in CC1101 Sub-GHz radio. No additional hardware is required
for the Sub-GHz, 315 MHz, 433 MHz, 868 MHz and 915 MHz protocols. 2.4 GHz protocols
require the ESP32 WiFi dev board.

**Does it decode the actual content of signals?**
RF Rosetta identifies the protocol and tells you what *could* be decoded. Full
decoding (e.g. extracting actual TPMS pressure values or meter readings as numbers)
is on the roadmap for protocols where this is technically feasible.

**Is this legal to use?**
RF Rosetta is a passive listener. It does not transmit anything. Passive reception
of RF signals is legal in virtually all jurisdictions. What you do with the
information is your responsibility.

**Can it pick up 2.4 GHz signals?**
Not natively, the CC1101 covers 300–928 MHz. The app flags 2.4 GHz protocols in
results and notes that an ESP32 module is required.

**A signal isn't being identified. What should I do?**
Check the full details screen for the raw signal characteristics, then open a GitHub
issue with those details. Unknown signals help grow the database.

---

## Roadmap

- [ ] Live TPMS pressure and temperature decoding
- [ ] Smart meter consumption number extraction
- [ ] Weather station temperature/humidity live decode
- [ ] ESP32 companion for 2.4 GHz protocol capture
- [ ] Signal fingerprinting (detect if the same device is seen again)
- [ ] Directional hints (getting stronger / weaker)
- [ ] Community protocol submission via GitHub Issues template

---

## Contributing

Contributions are welcome, especially:
- New protocol signatures in `protocol_db.c`
- Bug reports with captured signal details
- Corrections to existing protocol entries
- ESP32 companion code for 2.4 GHz protocols

Please open an issue before a large PR so we can discuss approach.

---

## License

GPL-3.0 — see [LICENSE](LICENSE)

Aligned with the Flipper Zero firmware license. Any derivative works must remain
open source under the same terms.

---

<div align="center">

**Built for the Flipper Zero community.**

*If RF Rosetta identified something interesting — share it!*

</div>
