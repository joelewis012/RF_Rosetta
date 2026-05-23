#include "protocol_db.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Minimum confidence to count as a match
#define MIN_CONFIDENCE 42

// ─────────────────────────────────────────────────────────────────────────────
// Protocol Database
// Every known RF protocol with full characteristics for matching.
// ─────────────────────────────────────────────────────────────────────────────

static const ProtocolSignature DB[] = {

    // ── AUTOMOTIVE ───────────────────────────────────────────────────────────

    {
        .name            = "Car Key Fob (Fixed Code, 433 MHz)",
        .short_name      = "Key Fob Fixed",
        .category        = CategoryAutomotive,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 200, .pulse_max = 600,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 5,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Older Toyota, Honda, Hyundai, generic aftermarket",
        .description     = "Fixed-code car entry remote. Sends the same code on every button press.",
        .security_note   = "VULNERABLE: Fixed code can be captured and replayed to unlock the vehicle.",
        .extra_data      = "Button function (lock/unlock/trunk), code word",
        .confidence_bonus = 5,
    },
    {
        .name            = "Car Key Fob (Rolling Code / KeeLoq, 433 MHz)",
        .short_name      = "Key Fob KeeLoq",
        .category        = CategoryAutomotive,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 180, .pulse_max = 400,
        .bandwidth_khz   = 60,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = true,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "GM, Chrysler, Jeep, Dodge, Ford (older), Microchip KeeLoq OEM",
        .description     = "Rolling-code car entry fob using KeeLoq encryption. Code changes every press.",
        .security_note   = "Rolling code — not replay-vulnerable under normal conditions.",
        .extra_data      = "Serial number, button ID, encrypted counter value",
        .confidence_bonus = 8,
    },
    {
        .name            = "Car Key Fob (Fixed Code, 315 MHz)",
        .short_name      = "Key Fob 315",
        .category        = CategoryAutomotive,
        .freq_min        = 314700000, .freq_max = 315300000,
        .modulation      = ModulationOOK,
        .pulse_min       = 200, .pulse_max = 600,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 5,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "North American Toyota, Honda, Subaru, Nissan, Mazda (pre-2010)",
        .description     = "315 MHz fixed-code car remote. Standard in North America before rolling codes.",
        .security_note   = "VULNERABLE: Fixed 315 MHz code. Common replay attack target.",
        .extra_data      = "Button function, fixed code word, manufacturer ID",
        .confidence_bonus = 10,
    },
    {
        .name            = "Car Key Fob (Rolling Code, 315 MHz)",
        .short_name      = "Key Fob Rolling 315",
        .category        = CategoryAutomotive,
        .freq_min        = 314700000, .freq_max = 315300000,
        .modulation      = ModulationOOK,
        .pulse_min       = 180, .pulse_max = 420,
        .bandwidth_khz   = 50,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = true,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "GM North America, Chrysler, later Toyota/Honda",
        .description     = "Modern 315 MHz rolling-code fob. Secure against simple replay.",
        .security_note   = "Rolling code — not replay-vulnerable.",
        .extra_data      = "Serial number, counter (encrypted)",
        .confidence_bonus = 8,
    },
    {
        .name            = "TPMS Tyre Pressure Sensor (315 MHz)",
        .short_name      = "TPMS 315",
        .category        = CategoryAutomotive,
        .freq_min        = 314000000, .freq_max = 316000000,
        .modulation      = ModulationFSK,
        .pulse_min       = 50,  .pulse_max = 150,
        .bandwidth_khz   = 100,
        .repeating       = true,  .repeat_min = 1, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Schrader, Pacific Industries — North American market",
        .description     = "Tyre pressure monitoring sensor. Broadcasts live tyre data every 60–90 seconds or when pressure changes.",
        .security_note   = "Data is unencrypted. Sensor ID is fixed and could theoretically track a specific vehicle.",
        .extra_data      = "Sensor ID, tyre pressure (PSI), temperature (C), battery status, wheel position",
        .confidence_bonus = 12,
    },
    {
        .name            = "TPMS Tyre Pressure Sensor (433 MHz)",
        .short_name      = "TPMS 433",
        .category        = CategoryAutomotive,
        .freq_min        = 433050000, .freq_max = 434790000,
        .modulation      = ModulationFSK,
        .pulse_min       = 50,  .pulse_max = 150,
        .bandwidth_khz   = 100,
        .repeating       = true,  .repeat_min = 1, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Continental, Huf, Schrader — BMW, Mercedes, Audi, VW, Renault",
        .description     = "European 433 MHz tyre pressure sensor. Broadcasts live tyre data.",
        .security_note   = "Unencrypted broadcast. Fixed sensor ID could identify a specific vehicle.",
        .extra_data      = "Sensor ID, pressure (Bar/PSI), temperature (C), battery OK/low, alarm flag",
        .confidence_bonus = 12,
    },
    {
        .name            = "Keyless Entry / Proximity Unlock",
        .short_name      = "Keyless Entry",
        .category        = CategoryAutomotive,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationFSK,
        .pulse_min       = 80,  .pulse_max = 300,
        .bandwidth_khz   = 80,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = true,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Most modern vehicles with passive entry",
        .description     = "Passive keyless entry — car polls the fob and unlocks when close.",
        .security_note   = "Vulnerable to relay attack — attacker relays signal between car and fob to unlock without physical access.",
        .extra_data      = "Vehicle challenge, fob response (encrypted)",
        .confidence_bonus = 6,
    },

    // ── HOME & BUILDING ───────────────────────────────────────────────────────

    {
        .name            = "Wireless Doorbell",
        .short_name      = "Doorbell",
        .category        = CategoryHome,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 250, .pulse_max = 800,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 4, .repeat_max = 12,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Byron, Friedland, Honeywell, SadoTech, AVANTEK, Elro",
        .description     = "Wireless doorbell button. High repetition count (4–12x) is distinctive.",
        .security_note   = "Fixed code. Anyone nearby could trigger your doorbell chime.",
        .extra_data      = "Button ID, chime channel, melody code",
        .confidence_bonus = 5,
    },
    {
        .name            = "Garage / Gate Remote (Fixed Code)",
        .short_name      = "Garage Fixed",
        .category        = CategoryHome,
        .freq_min        = 286000000, .freq_max = 434000000,
        .modulation      = ModulationOOK,
        .pulse_min       = 200, .pulse_max = 700,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 5,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Older Hormann, FAAC, Came, generic DIP-switch remotes",
        .description     = "Fixed-code garage or gate opener remote. Very common, very old standard.",
        .security_note   = "VULNERABLE: Fixed code is highly vulnerable to capture and replay. Extremely common attack target.",
        .extra_data      = "DIP switch code word, button ID, frequency",
        .confidence_bonus = 5,
    },
    {
        .name            = "Garage / Gate Remote (Rolling Code)",
        .short_name      = "Garage Rolling",
        .category        = CategoryHome,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationGFSK,
        .pulse_min       = 100, .pulse_max = 350,
        .bandwidth_khz   = 60,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = true,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Hormann BiSecure, Nice, Came, BFT, Marantec Digital",
        .description     = "Modern rolling-code garage/gate remote. Changes code on every press.",
        .security_note   = "Rolling code — not replay-vulnerable.",
        .extra_data      = "Serial number, rolling counter (encrypted)",
        .confidence_bonus = 8,
    },
    {
        .name            = "Wireless Mains Socket Remote",
        .short_name      = "Socket Remote",
        .category        = CategoryHome,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 250, .pulse_max = 600,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 4, .repeat_max = 8,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Brennenstuhl, Energenie, Byron, Lloytron, Status, Intertechno",
        .description     = "Remote-controlled mains socket. Turns plugged-in devices on/off.",
        .security_note   = "Fixed code. Anyone in range can switch your sockets on or off.",
        .extra_data      = "Channel, unit address, On/Off command",
        .confidence_bonus = 3,
    },
    {
        .name            = "Wireless Blinds / Curtain Motor",
        .short_name      = "Blind Remote",
        .category        = CategoryHome,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 300, .pulse_max = 900,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 3, .repeat_max = 6,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Somfy, Velux, Dooya, Zemismart, Rollertrol",
        .description     = "Remote control for motorised blinds or curtains.",
        .security_note   = "Fixed code. Minimal security risk — blinds only.",
        .extra_data      = "Motor ID, direction (up/down/stop), group address",
        .confidence_bonus = 4,
    },
    {
        .name            = "RF Light Switch / Dimmer Remote",
        .short_name      = "RF Light Switch",
        .category        = CategoryHome,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 250, .pulse_max = 700,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 3, .repeat_max = 8,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Livolo, Siemens RF, TechSmart, Samotech",
        .description     = "Wireless light switch or dimmer remote. Controls mains lighting.",
        .security_note   = "Fixed code. Lighting can be toggled by anyone nearby.",
        .extra_data      = "Switch ID, group, on/off/dim level",
        .confidence_bonus = 3,
    },
    {
        .name            = "Wireless Thermostat",
        .short_name      = "Thermostat",
        .category        = CategoryHome,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationFSK,
        .pulse_min       = 150, .pulse_max = 500,
        .bandwidth_khz   = 60,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Honeywell Evohome, Drayton Wiser, Salus, Regin",
        .description     = "Wireless heating thermostat communicating with a boiler receiver.",
        .security_note   = "Unencrypted on many models. Heating can potentially be interfered with.",
        .extra_data      = "Target temperature, current temp, call-for-heat status, device ID",
        .confidence_bonus = 5,
    },

    // ── SECURITY ─────────────────────────────────────────────────────────────

    {
        .name            = "Wireless Alarm PIR Motion Sensor",
        .short_name      = "PIR Sensor",
        .category        = CategorySecurity,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 300, .pulse_max = 900,
        .bandwidth_khz   = 30,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 4,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Optex, Visonic, Risco, Pyronix, Yale, generic alarm kits",
        .description     = "Motion-triggered alarm sensor. Fires a burst when movement detected.",
        .security_note   = "Many use fixed codes. Vulnerable to jamming and replay on older systems.",
        .extra_data      = "Zone ID, motion state, tamper bit, low battery flag",
        .confidence_bonus = 5,
    },
    {
        .name            = "Wireless Door / Window Contact Sensor",
        .short_name      = "Door Sensor",
        .category        = CategorySecurity,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 300, .pulse_max = 900,
        .bandwidth_khz   = 30,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 4,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Yale, Honeywell, Pyronix, Visonic, Elro, Ajax",
        .description     = "Magnetic contact sensor — broadcasts open/closed state of door or window.",
        .security_note   = "Fixed code on most devices. Tamper and open/closed status visible in the clear.",
        .extra_data      = "Sensor ID, open/closed state, tamper bit, battery level",
        .confidence_bonus = 5,
    },
    {
        .name            = "Panic / Personal Alarm Button",
        .short_name      = "Panic Button",
        .category        = CategorySecurity,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 200, .pulse_max = 600,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 6, .repeat_max = 20,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Various personal alarm, lone-worker, and medical alert devices",
        .description     = "Personal panic or medical alert button. Very high repeat count (6–20x) is distinctive.",
        .security_note   = "Fixed code — could be triggered remotely via replay.",
        .extra_data      = "Device ID, alarm type code",
        .confidence_bonus = 8,
    },
    {
        .name            = "Wireless Smoke / CO Detector",
        .short_name      = "Smoke Detector",
        .category        = CategorySecurity,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationFSK,
        .pulse_min       = 200, .pulse_max = 700,
        .bandwidth_khz   = 60,
        .repeating       = true,  .repeat_min = 3, .repeat_max = 8,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Ei Electronics, Google Nest Protect (RF variant), Kidde, Aico",
        .description     = "Wireless smoke or CO alarm that communicates with interconnected detectors.",
        .security_note   = "Alarm state broadcast openly — can identify if alarm is sounding.",
        .extra_data      = "Device ID, alarm type (smoke/CO/heat), test mode flag, fault status",
        .confidence_bonus = 6,
    },
    {
        .name            = "Wireless Alarm Siren / Strobe",
        .short_name      = "Alarm Siren",
        .category        = CategorySecurity,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 300, .pulse_max = 800,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 4,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Pyronix, Risco, Optex, Texecom",
        .description     = "Wireless receiver for an alarm bell box or strobe light.",
        .security_note   = "Trigger codes on some systems are fixed and could be replayed.",
        .extra_data      = "Zone ID, arm/disarm state, tamper status",
        .confidence_bonus = 4,
    },
    {
        .name            = "Alarm Keyfob / Arm-Disarm Remote",
        .short_name      = "Alarm Keyfob",
        .category        = CategorySecurity,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 200, .pulse_max = 600,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 4,
        .fixed_code      = false, .rolling_code = true,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Texecom, Risco, Ajax, Pyronix",
        .description     = "Remote fob to arm/disarm a home alarm system.",
        .security_note   = "Modern systems use rolling codes. Older systems may use fixed codes.",
        .extra_data      = "User ID, action (arm/disarm/panic), rolling counter",
        .confidence_bonus = 5,
    },

    // ── WEATHER & ENVIRONMENT ─────────────────────────────────────────────────

    {
        .name            = "Wireless Weather Station (433 MHz)",
        .short_name      = "Weather Station",
        .category        = CategoryWeather,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 400, .pulse_max = 2000,
        .bandwidth_khz   = 30,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Acurite, Oregon Scientific, Bresser, Fine Offset, Froggit, TFA Dostmann, Auriol",
        .description     = "Outdoor weather sensor. Broadcasts temperature, humidity and more periodically.",
        .security_note   = "No security — environmental data only. No personal risk.",
        .extra_data      = "Temperature (C/F), humidity (%), station ID, channel, battery, rain/wind if equipped",
        .confidence_bonus = 10,
    },
    {
        .name            = "Wireless Weather Station (868 MHz)",
        .short_name      = "Weather Station 868",
        .category        = CategoryWeather,
        .freq_min        = 868000000, .freq_max = 869000000,
        .modulation      = ModulationFSK,
        .pulse_min       = 200, .pulse_max = 1000,
        .bandwidth_khz   = 60,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Some Netatmo, EcoWitt, Davis instruments (European models)",
        .description     = "868 MHz European weather sensor. Less common than 433 MHz variant.",
        .security_note   = "No security — environmental data only.",
        .extra_data      = "Temperature (C), humidity (%), pressure if equipped, station ID",
        .confidence_bonus = 8,
    },
    {
        .name            = "Soil Moisture / Plant Sensor",
        .short_name      = "Soil Sensor",
        .category        = CategoryWeather,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 300, .pulse_max = 1500,
        .bandwidth_khz   = 30,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "EcoWitt, Bresser, Flower Power, various garden sensor brands",
        .description     = "Wireless soil moisture and temperature sensor for gardens.",
        .security_note   = "No security concerns — sensor data only.",
        .extra_data      = "Soil moisture (%), soil temperature (C), sensor channel, battery",
        .confidence_bonus = 5,
    },
    {
        .name            = "Wireless Rain Gauge",
        .short_name      = "Rain Gauge",
        .category        = CategoryWeather,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 400, .pulse_max = 1800,
        .bandwidth_khz   = 30,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Acurite, Oregon Scientific, Fine Offset, ambient weather stations",
        .description     = "Wireless tipping bucket rain gauge. Sends a burst on each tip.",
        .security_note   = "No security — environmental data only.",
        .extra_data      = "Rain tip count, accumulated total (mm), station ID",
        .confidence_bonus = 4,
    },
    {
        .name            = "Wireless Anemometer / Wind Sensor",
        .short_name      = "Wind Sensor",
        .category        = CategoryWeather,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 400, .pulse_max = 1800,
        .bandwidth_khz   = 30,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Acurite, Oregon Scientific, Davis Vantage (wireless)",
        .description     = "Wireless wind speed and direction sensor.",
        .security_note   = "No security — environmental data only.",
        .extra_data      = "Wind speed (m/s or mph), wind direction (degrees/cardinal), gust speed",
        .confidence_bonus = 4,
    },

    // ── IoT & SMART HOME ──────────────────────────────────────────────────────

    {
        .name            = "Z-Wave Device (EU 868 MHz)",
        .short_name      = "Z-Wave EU",
        .category        = CategoryIoT,
        .freq_min        = 868000000, .freq_max = 869000000,
        .modulation      = ModulationGFSK,
        .pulse_min       = 10, .pulse_max = 100,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 1, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Fibaro, Aeotec, Homey, Zipato, Qubino, Popp",
        .description     = "Z-Wave smart home mesh network node. 868 MHz in Europe.",
        .security_note   = "Z-Wave S2 security uses AES-128. Older S0 devices are weaker.",
        .extra_data      = "Home ID, node ID, command class, device type, frame type",
        .confidence_bonus = 10,
    },
    {
        .name            = "Z-Wave Device (US 908 MHz)",
        .short_name      = "Z-Wave US",
        .category        = CategoryIoT,
        .freq_min        = 908000000, .freq_max = 909000000,
        .modulation      = ModulationGFSK,
        .pulse_min       = 10, .pulse_max = 100,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 1, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Aeotec, GE/Jasco, Leviton, Zooz, HomeSeer",
        .description     = "North American Z-Wave smart home device. 908 MHz US band.",
        .security_note   = "Z-Wave S2 encrypted. Older S0 units may be weaker.",
        .extra_data      = "Home ID, node ID, command class, device type",
        .confidence_bonus = 10,
    },
    {
        .name            = "LoRa Node / IoT Beacon (EU 868 MHz)",
        .short_name      = "LoRa EU",
        .category        = CategoryIoT,
        .freq_min        = 863000000, .freq_max = 870000000,
        .modulation      = ModulationCSS,
        .pulse_min       = 10,  .pulse_max = 50,
        .bandwidth_khz   = 125,
        .repeating       = false, .repeat_min = 1, .repeat_max = 1,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "The Things Network, Helium, Dragino, RAK Wireless, Semtech",
        .description     = "LoRaWAN IoT node — long range, low power. Used for asset tracking and environmental monitoring.",
        .security_note   = "LoRaWAN uses AES-128. Application payload is encrypted end-to-end.",
        .extra_data      = "DevAddr, frame counter, spreading factor (SF7-SF12), bandwidth, RSSI, SNR",
        .confidence_bonus = 12,
    },
    {
        .name            = "LoRa Node / IoT Beacon (US 915 MHz)",
        .short_name      = "LoRa US",
        .category        = CategoryIoT,
        .freq_min        = 902000000, .freq_max = 928000000,
        .modulation      = ModulationCSS,
        .pulse_min       = 10,  .pulse_max = 50,
        .bandwidth_khz   = 125,
        .repeating       = false, .repeat_min = 1, .repeat_max = 1,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Helium Network, AWS IoT, Multitech, Laird, Dragino",
        .description     = "North American LoRaWAN IoT node. 915 MHz US band.",
        .security_note   = "AES-128 encrypted. Payload is secure.",
        .extra_data      = "DevAddr, frame counter, spreading factor, bandwidth",
        .confidence_bonus = 12,
    },

    // ── UTILITY / SMART METERS ────────────────────────────────────────────────

    {
        .name            = "Smart Electricity Meter / AMR (433 MHz)",
        .short_name      = "Smart Meter 433",
        .category        = CategoryUtility,
        .freq_min        = 433050000, .freq_max = 434790000,
        .modulation      = ModulationFSK,
        .pulse_min       = 50,  .pulse_max = 200,
        .bandwidth_khz   = 100,
        .repeating       = false, .repeat_min = 1, .repeat_max = 1,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Landis+Gyr, Itron, Elster, Kamstrup",
        .description     = "Smart electricity meter broadcasting usage data via automatic meter reading (AMR).",
        .security_note   = "Older AMR meters broadcast consumption data unencrypted. Your energy usage is visible to anyone nearby.",
        .extra_data      = "Meter ID, current consumption (kWh), accumulated total, rate period (day/night)",
        .confidence_bonus = 8,
    },
    {
        .name            = "Smart Electricity Meter / AMR (868 MHz)",
        .short_name      = "Smart Meter 868",
        .category        = CategoryUtility,
        .freq_min        = 868000000, .freq_max = 869000000,
        .modulation      = ModulationFSK,
        .pulse_min       = 50,  .pulse_max = 200,
        .bandwidth_khz   = 100,
        .repeating       = false, .repeat_min = 1, .repeat_max = 1,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Landis+Gyr, Iskraemeco, Sagemcom, Siemens",
        .description     = "European 868 MHz smart electricity meter using wireless M-Bus or OMS protocol.",
        .security_note   = "Many European meters broadcast unencrypted. Energy usage readable by anyone in range.",
        .extra_data      = "Meter ID, consumption (kWh), power (W), meter type, manufacturer code",
        .confidence_bonus = 8,
    },
    {
        .name            = "Smart Gas Meter / AMR",
        .short_name      = "Gas Meter",
        .category        = CategoryUtility,
        .freq_min        = 868000000, .freq_max = 869000000,
        .modulation      = ModulationFSK,
        .pulse_min       = 50,  .pulse_max = 200,
        .bandwidth_khz   = 100,
        .repeating       = false, .repeat_min = 1, .repeat_max = 1,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Elster, Landis+Gyr, Honeywell gas meters",
        .description     = "Wireless gas meter reading. Uses wireless M-Bus protocol on 868 MHz.",
        .security_note   = "Gas consumption readable openly on many deployments.",
        .extra_data      = "Meter ID, volume (m3), rate, manufacturer, alarm flags",
        .confidence_bonus = 7,
    },
    {
        .name            = "Smart Water Meter / AMR",
        .short_name      = "Water Meter",
        .category        = CategoryUtility,
        .freq_min        = 433050000, .freq_max = 434790000,
        .modulation      = ModulationFSK,
        .pulse_min       = 50,  .pulse_max = 200,
        .bandwidth_khz   = 100,
        .repeating       = false, .repeat_min = 1, .repeat_max = 1,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Sensus, Mueller, Itron, Zenner",
        .description     = "Wireless water meter. Broadcasts consumption data for remote reading.",
        .security_note   = "Water consumption readable openly. Leak alerts also broadcast.",
        .extra_data      = "Meter ID, volume (L or m3), leak alarm, tamper bit",
        .confidence_bonus = 7,
    },

    // ── INDUSTRIAL ────────────────────────────────────────────────────────────

    {
        .name            = "Industrial ISM Beacon / SCADA Sensor",
        .short_name      = "SCADA Sensor",
        .category        = CategoryIndustrial,
        .freq_min        = 863000000, .freq_max = 870000000,
        .modulation      = ModulationFSK,
        .pulse_min       = 50,  .pulse_max = 300,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Various industrial IoT — Pepperl+Fuchs, Banner, Emerson, ABB wireless sensors",
        .description     = "Industrial wireless sensor node — temperature, pressure, flow, position.",
        .security_note   = "Often unencrypted. May expose machine status or process data.",
        .extra_data      = "Device ID, sensor type, measurement value, units, alarm flags",
        .confidence_bonus = 3,
    },
    {
        .name            = "Asset Tracking Beacon",
        .short_name      = "Asset Tracker",
        .category        = CategoryIndustrial,
        .freq_min        = 863000000, .freq_max = 928000000,
        .modulation      = ModulationGFSK,
        .pulse_min       = 30,  .pulse_max = 200,
        .bandwidth_khz   = 150,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Samsara, Calamp, Spireon, Quake Global, various fleet trackers",
        .description     = "Wireless beacon attached to vehicles, pallets or equipment for location tracking.",
        .security_note   = "Asset ID and sometimes location data broadcast openly.",
        .extra_data      = "Asset ID, battery level, motion status, GPS fix flag",
        .confidence_bonus = 4,
    },

    // ── MEDICAL ───────────────────────────────────────────────────────────────

    {
        .name            = "Continuous Glucose Monitor — Dexcom",
        .short_name      = "CGM Dexcom",
        .category        = CategoryMedical,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 100, .pulse_max = 400,
        .bandwidth_khz   = 60,
        .repeating       = true,  .repeat_min = 2, .repeat_max = 4,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Dexcom G5, G6, G7",
        .description     = "Continuous glucose monitor worn by diabetics. Broadcasts readings every 5 minutes.",
        .security_note   = "Encrypted. Note: detecting this near a person reveals they are a CGM user (medical information).",
        .extra_data      = "Transmitter ID, sequence number, glucose reading (encrypted), trend arrow",
        .confidence_bonus = 10,
    },
    {
        .name            = "Continuous Glucose Monitor — Freestyle Libre",
        .short_name      = "CGM Libre",
        .category        = CategoryMedical,
        .freq_min        = 868000000, .freq_max = 869000000,
        .modulation      = ModulationGFSK,
        .pulse_min       = 50,  .pulse_max = 200,
        .bandwidth_khz   = 100,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Abbott FreeStyle Libre 2, Libre 3",
        .description     = "Abbott FreeStyle Libre glucose sensor. Broadcasts alarms and readings.",
        .security_note   = "Encrypted. Detection reveals CGM use — sensitive medical information.",
        .extra_data      = "Sensor ID, alarm type (high/low glucose), age of sensor",
        .confidence_bonus = 10,
    },
    {
        .name            = "Hearing Aid Sync Signal",
        .short_name      = "Hearing Aid",
        .category        = CategoryMedical,
        .freq_min        = 860000000, .freq_max = 870000000,
        .modulation      = ModulationGFSK,
        .pulse_min       = 20,  .pulse_max = 100,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 1, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = true,  .security_concern = false, .needs_esp32 = false,
        .brands          = "Phonak, Oticon, ReSound, Widex, Starkey (2.4 GHz variants use Bluetooth)",
        .description     = "Hearing aid wireless sync using the Made for Hearing Aid (MFiHA) or NFMI protocol.",
        .security_note   = "Detecting this near a person reveals hearing aid use — sensitive medical information.",
        .extra_data      = "Device pairing signal, audio stream headers, volume/program info",
        .confidence_bonus = 8,
    },
    {
        .name            = "Medical Personal Alert / Telecare",
        .short_name      = "Medical Alert",
        .category        = CategoryMedical,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 300, .pulse_max = 900,
        .bandwidth_khz   = 40,
        .repeating       = true,  .repeat_min = 5, .repeat_max = 20,
        .fixed_code      = true,  .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Tunstall, Careium, Careline, Lifeline, Taking Care",
        .description     = "Personal alarm worn by elderly or vulnerable users. Very high repeat count on activation.",
        .security_note   = "Fixed code. Use responsibly — do not replay medical alerts.",
        .extra_data      = "Device ID, alarm type, user code",
        .confidence_bonus = 7,
    },

    // ── CONSUMER ELECTRONICS ──────────────────────────────────────────────────

    {
        .name            = "Wireless Headphones (2.4 GHz Pairing)",
        .short_name      = "Wireless Headphones",
        .category        = CategoryConsumer,
        .freq_min        = 2400000000U, .freq_max = 2484000000U,
        .modulation      = ModulationGFSK,
        .pulse_min       = 5,   .pulse_max = 50,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 1, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = true,
        .brands          = "Sony, Jabra, Sennheiser, Logitech (non-Bluetooth wireless)",
        .description     = "Proprietary 2.4 GHz wireless audio pairing or sync burst.",
        .security_note   = "Generally proprietary — low security risk for audio.",
        .extra_data      = "Device ID, pairing sequence",
        .confidence_bonus = 3,
    },
    {
        .name            = "Game Controller Sync (2.4 GHz)",
        .short_name      = "Game Controller",
        .category        = CategoryConsumer,
        .freq_min        = 2400000000U, .freq_max = 2484000000U,
        .modulation      = ModulationGFSK,
        .pulse_min       = 5,   .pulse_max = 50,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 1, .repeat_max = 3,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = true,
        .brands          = "Xbox (non-BT), PlayStation DualSense older, Nintendo Switch Pro",
        .description     = "2.4 GHz proprietary sync burst from a wireless game controller.",
        .security_note   = "Proprietary protocol. Controller input can be intercepted on some older designs.",
        .extra_data      = "Controller ID, pairing handshake",
        .confidence_bonus = 3,
    },
    {
        .name            = "Wireless Keyboard / Mouse (2.4 GHz)",
        .short_name      = "Wireless HID",
        .category        = CategoryConsumer,
        .freq_min        = 2400000000U, .freq_max = 2484000000U,
        .modulation      = ModulationGFSK,
        .pulse_min       = 5,   .pulse_max = 50,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = true,
        .brands          = "Logitech Unifying, Microsoft, Perixx, various budget peripherals",
        .description     = "2.4 GHz wireless keyboard or mouse using a USB dongle receiver.",
        .security_note   = "MOUSEJACK: Many 2.4 GHz HID devices are vulnerable to keystroke injection from up to 100m away.",
        .extra_data      = "Device ID, HID payload (keystrokes / mouse movement)",
        .confidence_bonus = 5,
    },

    // ── BABY MONITORS ─────────────────────────────────────────────────────────

    {
        .name            = "Baby Monitor (433 MHz Analogue)",
        .short_name      = "Baby Monitor 433",
        .category        = CategoryMisc,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationFM,
        .pulse_min       = 0,   .pulse_max = 0,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 0, .repeat_max = 0,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Motorola (older), BT, Philips Avent (older), generic",
        .description     = "Analogue FM baby monitor. Continuous audio broadcast.",
        .security_note   = "NO ENCRYPTION — audio from the room is broadcast openly. Anyone nearby can listen.",
        .extra_data      = "Continuous audio (wideband FM), channel number",
        .confidence_bonus = 6,
    },
    {
        .name            = "Baby Monitor (864 MHz Analogue)",
        .short_name      = "Baby Monitor 864",
        .category        = CategoryMisc,
        .freq_min        = 863000000, .freq_max = 865000000,
        .modulation      = ModulationFM,
        .pulse_min       = 0,   .pulse_max = 0,
        .bandwidth_khz   = 200,
        .repeating       = false, .repeat_min = 0, .repeat_max = 0,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = true, .needs_esp32 = false,
        .brands          = "Philips Avent, Motorola, Chicco, BT (older models)",
        .description     = "Analogue FM baby monitor on 864 MHz. Wideband continuous audio.",
        .security_note   = "NO ENCRYPTION — audio from the room is broadcast openly.",
        .extra_data      = "Continuous audio (wideband FM), CTCSS/DCS sub-tone if present",
        .confidence_bonus = 6,
    },

    // ── MISCELLANEOUS ─────────────────────────────────────────────────────────

    {
        .name            = "Radiosonde (Weather Balloon)",
        .short_name      = "Radiosonde",
        .category        = CategoryMisc,
        .freq_min        = 400000000, .freq_max = 406000000,
        .modulation      = ModulationFSK,
        .pulse_min       = 20,  .pulse_max = 100,
        .bandwidth_khz   = 150,
        .repeating       = false, .repeat_min = 1, .repeat_max = 1,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Vaisala RS41, Graw DFM-09, Meteomodem M20",
        .description     = "Meteorological radiosonde launched on weather balloons. Telemetry from the upper atmosphere.",
        .security_note   = "No security — scientific telemetry data.",
        .extra_data      = "Sonde ID, GPS position, altitude, temperature, humidity, pressure, battery",
        .confidence_bonus = 15,
    },
    {
        .name            = "RC Vehicle / Drone Remote (27/40/433 MHz)",
        .short_name      = "RC Remote",
        .category        = CategoryMisc,
        .freq_min        = 26000000, .freq_max = 434000000,
        .modulation      = ModulationOOK,
        .pulse_min       = 500, .pulse_max = 2000,
        .bandwidth_khz   = 50,
        .repeating       = true,  .repeat_min = 10, .repeat_max = 0,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Various toy and hobby RC, older drones (newer use 2.4 GHz)",
        .description     = "Radio control transmitter for a toy car, boat, or older drone.",
        .security_note   = "Unencrypted. On some protocols, RC signals from nearby controllers can interfere.",
        .extra_data      = "Channel, throttle/steering pulse widths (PWM), transmitter ID",
        .confidence_bonus = 4,
    },
    {
        .name            = "Wireless Pool / Spa Sensor",
        .short_name      = "Pool Sensor",
        .category        = CategoryMisc,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationFSK,
        .pulse_min       = 200, .pulse_max = 800,
        .bandwidth_khz   = 60,
        .repeating       = false, .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Hayward, Pentair, Intex, Bestway, Ondilo ICO",
        .description     = "Wireless pool or spa monitor — water temperature, pH, chlorine level.",
        .security_note   = "No security risk — pool chemistry data only.",
        .extra_data      = "Temperature (C), pH level, ORP (chlorine), salinity, device ID",
        .confidence_bonus = 5,
    },
    {
        .name            = "Animal / Wildlife Tracking Tag",
        .short_name      = "Animal Tag",
        .category        = CategoryMisc,
        .freq_min        = 148000000, .freq_max = 154000000,
        .modulation      = ModulationFSK,
        .pulse_min       = 50,  .pulse_max = 500,
        .bandwidth_khz   = 30,
        .repeating       = true,  .repeat_min = 1, .repeat_max = 2,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Lotek, Telonics, Wildlife Computers, Vectronic",
        .description     = "VHF telemetry tag attached to wildlife for scientific tracking.",
        .security_note   = "No security concerns — scientific tracking data.",
        .extra_data      = "Tag frequency (unique per animal), pulse rate (activity indicator)",
        .confidence_bonus = 10,
    },
    {
        .name            = "Generic 433 MHz OOK Signal",
        .short_name      = "Generic OOK",
        .category        = CategoryMisc,
        .freq_min        = 433050000, .freq_max = 433920000,
        .modulation      = ModulationOOK,
        .pulse_min       = 100, .pulse_max = 3000,
        .bandwidth_khz   = 50,
        .repeating       = true,  .repeat_min = 1, .repeat_max = 0,
        .fixed_code      = false, .rolling_code = false,
        .encrypted       = false, .security_concern = false, .needs_esp32 = false,
        .brands          = "Unknown 433 MHz ISM band device",
        .description     = "Unidentified OOK signal on the 433 MHz ISM band.",
        .security_note   = "Unknown origin. Review signal details manually.",
        .extra_data      = "Pulse timing data, repeat count, estimated bit count",
        .confidence_bonus = 0,
    },
};

static const uint16_t DB_COUNT = sizeof(DB) / sizeof(DB[0]);

// ─────────────────────────────────────────────────────────────────────────────
// Helper strings
// ─────────────────────────────────────────────────────────────────────────────

const char* protocol_category_str(ProtocolCategory cat) {
    switch(cat) {
        case CategoryAutomotive:  return "Automotive";
        case CategoryHome:        return "Home";
        case CategorySecurity:    return "Security";
        case CategoryIoT:         return "IoT";
        case CategoryWeather:     return "Weather";
        case CategoryUtility:     return "Utility";
        case CategoryIndustrial:  return "Industrial";
        case CategoryMedical:     return "Medical";
        case CategoryConsumer:    return "Consumer";
        case CategoryMisc:        return "Misc";
        default:                  return "Unknown";
    }
}

const char* modulation_str(Modulation mod) {
    switch(mod) {
        case ModulationOOK:  return "OOK";
        case ModulationFSK:  return "FSK";
        case ModulationGFSK: return "GFSK";
        case ModulationBPSK: return "BPSK";
        case ModulationCSS:  return "LoRa/CSS";
        case ModulationFM:   return "FM";
        default:             return "Unknown";
    }
}

uint16_t protocol_db_count(void) { return DB_COUNT; }

const ProtocolSignature* protocol_db_get(uint16_t index) {
    if(index >= DB_COUNT) return NULL;
    return &DB[index];
}

// ─────────────────────────────────────────────────────────────────────────────
// Matching Engine
// Score each protocol against the captured signal features.
// Returns 0–100 confidence.
// ─────────────────────────────────────────────────────────────────────────────

static uint8_t score_protocol(const SignalCapture* cap, const ProtocolSignature* proto) {
    uint32_t score   = 0;
    uint32_t max_pts = 0;

    // ── Frequency (35 points — most important) ──────────────────────────────
    max_pts += 35;
    if(cap->frequency >= proto->freq_min && cap->frequency <= proto->freq_max) {
        score += 35;
    } else {
        // Partial: within 2x the band width
        uint32_t centre = proto->freq_min / 2 + proto->freq_max / 2;
        uint32_t span   = proto->freq_max - proto->freq_min;
        uint32_t delta  = (cap->frequency > centre)
                          ? cap->frequency - centre
                          : centre - cap->frequency;
        if(delta < span) score += 15;
    }

    // ── Modulation (25 points) ───────────────────────────────────────────────
    max_pts += 25;
    if(cap->modulation == proto->modulation) {
        score += 25;
    } else if(proto->modulation == ModulationUnknown || cap->modulation == ModulationUnknown) {
        score += 8; // partial when one side is unknown
    }

    // ── Pulse width (20 points) ──────────────────────────────────────────────
    max_pts += 20;
    // FM / LoRa entries don't use pulse_min/max in the same way
    if(proto->pulse_min > 0 && proto->pulse_max > 0) {
        if(cap->pulse_avg >= proto->pulse_min && cap->pulse_avg <= proto->pulse_max) {
            score += 20;
        } else if(cap->pulse_avg >= proto->pulse_min * 7 / 10 &&
                  cap->pulse_avg <= proto->pulse_max * 13 / 10) {
            score += 10; // close enough
        }
    } else {
        score += 10; // not enough data to penalise
        max_pts -= 10;
    }

    // ── Repetition (10 points) ───────────────────────────────────────────────
    max_pts += 10;
    if(cap->repeating == proto->repeating) {
        score += 5;
        if(proto->repeating && proto->repeat_min > 0 &&
           cap->repeat_count >= proto->repeat_min) {
            score += 5;
        } else if(!proto->repeating) {
            score += 5;
        }
    }

    // ── Bandwidth (10 points) ────────────────────────────────────────────────
    max_pts += 10;
    if(proto->bandwidth_khz > 0) {
        int16_t bw_delta = (int16_t)cap->bandwidth_khz - (int16_t)proto->bandwidth_khz;
        if(bw_delta < 0) bw_delta = -bw_delta;
        if(bw_delta < 20)       score += 10;
        else if(bw_delta < 60)  score += 5;
    } else {
        max_pts -= 10; // don't penalise unknown bandwidth
    }

    // ── Confidence bonus ─────────────────────────────────────────────────────
    score   += proto->confidence_bonus;
    max_pts += proto->confidence_bonus;

    if(max_pts == 0) return 0;
    uint32_t pct = (score * 100) / max_pts;
    return (uint8_t)(pct > 100 ? 100 : pct);
}

bool protocol_db_match(const SignalCapture* cap, ProtocolMatch* out) {
    out->matched   = false;
    out->protocol  = NULL;
    out->confidence = 0;
    out->decoded[0] = '\0';
    out->conf_label[0] = '\0';

    uint8_t best_conf  = 0;
    int16_t best_index = -1;

    for(uint16_t i = 0; i < DB_COUNT; i++) {
        uint8_t conf = score_protocol(cap, &DB[i]);
        if(conf > best_conf) {
            best_conf  = conf;
            best_index = (int16_t)i;
        }
    }

    if(best_index >= 0 && best_conf >= MIN_CONFIDENCE) {
        out->matched    = true;
        out->protocol   = &DB[best_index];
        out->confidence = best_conf;
        if(best_conf >= 80)      snprintf(out->conf_label, sizeof(out->conf_label), "High");
        else if(best_conf >= 60) snprintf(out->conf_label, sizeof(out->conf_label), "Medium");
        else                     snprintf(out->conf_label, sizeof(out->conf_label), "Low");
        return true;
    }

    snprintf(out->conf_label, sizeof(out->conf_label), "No match");
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Decoder — fills decoded[] with human-readable data extracted from the signal.
// Real decoding would parse raw_pulses; here we provide best-effort summaries.
// ─────────────────────────────────────────────────────────────────────────────

void protocol_db_decode(const SignalCapture* cap, ProtocolMatch* match) {
    if(!match->matched || !match->protocol) return;
    const ProtocolSignature* p = match->protocol;
    char* buf = match->decoded;
    int   len = DECODED_LEN;
    int   pos = 0;

    // Always include signal stats
    pos += snprintf(buf + pos, len - pos,
        "RSSI:     %.0f dBm\n"
        "Noise:    %.0f dBm\n"
        "SNR:      %.1f dB\n"
        "Pulses:   %u captured\n"
        "Bits est: ~%u\n"
        "Repeats:  %u\n",
        (double)cap->rssi,
        (double)cap->noise_floor,
        (double)cap->snr,
        cap->pulse_count,
        cap->packet_bits,
        cap->repeat_count);

    // Code type note
    if(p->fixed_code) {
        pos += snprintf(buf + pos, len - pos, "Code:     Fixed (same every TX)\n");
    } else if(p->rolling_code) {
        pos += snprintf(buf + pos, len - pos, "Code:     Rolling (changes each TX)\n");
    }

    // Encryption
    pos += snprintf(buf + pos, len - pos,
        "Encrypt:  %s\n",
        p->encrypted ? "Yes" : "No");

    // Extra data note
    if(p->extra_data && pos < len - 2) {
        pos += snprintf(buf + pos, len - pos, "\nCAN DECODE:\n%s\n", p->extra_data);
    }

    // ESP32 note
    if(p->needs_esp32 && pos < len - 2) {
        pos += snprintf(buf + pos, len - pos,
            "\n[!] Requires ESP32 WiFi module\n"
            "    for full signal capture.\n");
    }
}
