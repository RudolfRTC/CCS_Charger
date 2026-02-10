# PCB Design Guide: CCS DC Charging Controller
## Based on Chargebyte Charge Module S (I2CMS)

**Version:** 1.0
**Date:** 2026-02-10
**Compatibility:** ISC_CMS_Automotive DBC v1.4.0

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Block Diagram](#2-block-diagram)
3. [Component Selection](#3-component-selection)
4. [Power Supply Design](#4-power-supply-design)
5. [Charge Module S Integration](#5-charge-module-s-integration)
6. [CAN Bus Interface](#6-can-bus-interface)
7. [Control Pilot Circuit](#7-control-pilot-circuit)
8. [Proximity Pilot Circuit](#8-proximity-pilot-circuit)
9. [PLC Coupling Circuit](#9-plc-coupling-circuit)
10. [Contactor Control](#10-contactor-control)
11. [Safety & Protection](#11-safety--protection)
12. [Connector Specifications](#12-connector-specifications)
13. [PCB Layout Guidelines](#13-pcb-layout-guidelines)
14. [Bill of Materials (BOM)](#14-bill-of-materials-bom)
15. [Testing & Commissioning](#15-testing--commissioning)

---

## 1. System Overview

This PCB acts as the **VCU (Vehicle Control Unit)** for CCS DC fast charging using the Chargebyte Charge Module S. The module handles all high-level communication (ISO 15118 / DIN 70121) with the charging station via PLC on the Control Pilot line. The VCU communicates with the CMS via CAN bus at 500 kbit/s using extended (29-bit) IDs.

### What the CMS does (built-in):
- ISO 15118-2 and DIN 70121 protocol stack
- SLAC (Signal Level Attenuation Characterization)
- PLC communication via QCA7005 HomePlug GreenPHY
- TLS 1.2 support
- State machine management (13 states)

### What the VCU PCB must do:
- Power the CMS module (3.3V, up to 350 mA)
- Provide CAN bus interface to the CMS
- Provide PLC coupling circuit (Control Pilot ↔ CMS powerline)
- Optionally handle Control Pilot state switching (B↔C)
- Optionally handle Proximity Pilot detection
- Control DC contactors based on CMS state
- Interface with BMS for battery parameters (SoC, voltage, current limits)
- Provide safety mechanisms (emergency stop, isolation monitoring)

### Charge Session Flow:
```
Plugin → CP State B → SLAC → ISO/DIN Negotiation → Authentication →
Parameter Exchange → Isolation Test → PreCharge → Charge →
Stop → Welding Detection → Session Stop → Unplug
```

### CMS State Machine States:
| Value | State          | Description                            |
|-------|----------------|----------------------------------------|
| 0     | Default        | Idle, waiting for plug                 |
| 1     | Init           | SLAC process, PLC link setup           |
| 2     | Authentication | TLS/Certificate exchange               |
| 3     | Parameter      | Charge parameter negotiation           |
| 4     | Isolation      | EVSE isolation monitoring              |
| 5     | PreCharge      | EVSE ramping voltage to match battery  |
| 6     | Charge         | Active DC charging                     |
| 7     | Welding        | Contactor welding detection            |
| 8     | StopCharge     | Charging termination in progress       |
| 9     | SessionStop    | Session cleanup                        |
| 10    | ShutOff        | Module shutting off                    |
| 11    | Paused         | Charging paused                        |
| 12    | Error          | Error condition                        |

---

## 2. Block Diagram

```
                                    +------------------+
  CCS Inlet                        |   VCU PCB        |
  ========                         |                  |
                                   |  +-----------+   |
  Control Pilot ──── PLC Coupler ──┤──│ Charge    │   |
                     + CP Circuit  |  │ Module S  │   |
                                   |  │ (I2CMS)   │   |
  Proximity Pilot ── PP Circuit ───┤──│           │   |
                                   |  │ Pin 1-60  │   |
                                   |  +-----------+   |
                                   |    │CAN_TX/RX    |
                                   |    │             |
                                   |  +-----------+   |       +--------+
                                   |  │ CAN       │   │ CAN   │        │
                                   |  │ Transceiver├───┤ Bus ──│  MCU   │
                                   |  │ (MCP2562) │   │       │(STM32) │
                                   |  +-----------+   |       +--------+
                                   |                  |         │
  12V Automotive ── DC/DC 3.3V ────┤                  |    BMS Interface
  Power Supply      + 5V           |                  |    (CAN/UART/SPI)
                                   |                  |
                                   |  Contactor       |    DC Contactors
                                   |  Drivers ────────┤──── (+) and (-)
                                   |                  |
                                   |  Emergency       |
                                   |  Stop Circuit ───┤
                                   +------------------+
```

---

## 3. Component Selection

### 3.1 Core Components

| Component | Recommended Part | Notes |
|-----------|-----------------|-------|
| Communication Module | Chargebyte Charge Module S (I2CMS-DMBC) | 60-pin, 50.8 x 30.48 mm |
| MCU (VCU) | STM32F446RE or STM32G474RE | ARM Cortex-M4, built-in CAN, 3.3V |
| CAN Transceiver | MCP2562FD-E/SN | 3.3V, CAN FD capable, 8-SOIC |
| PLC Coupling Transformer | I2PLCTR-2 (from Chargebyte) | Non-standard, must order from Chargebyte |
| Voltage Regulator (3.3V) | TLV1117-33IDCYR or AMS1117-3.3 | LDO, min 500 mA output |
| Voltage Regulator (5V) | LM2596-5.0 or TPS5430 | Step-down from 12V automotive |
| Crystal (MCU) | 8 MHz, 20 ppm | For STM32 HSE |

### 3.2 CMS Module Variant Selection

| Order Code | Interface | CP Handling | Use Case |
|-----------|-----------|-------------|----------|
| I2CMS-**DMBC** | CAN + CP by customer | VCU handles CP/PP | Flexible, recommended |
| I2CMS-**DMNC** | CAN + Native GPIO | CMS handles CP/PP | Simpler, less control |

**Recommendation:** Use **I2CMS-DMBC** variant ("B" = By customer for IEC 61851 interface). This gives maximum control over the CP/PP circuit from the VCU side while still using CAN for communication.

---

## 4. Power Supply Design

### 4.1 Input: 12V Automotive Battery

The PCB receives 12V from the vehicle's auxiliary battery.

### 4.2 Protection on 12V Input
```
12V_BATT ──┤►├── Reverse polarity protection (SMBJ18CA TVS) ──── Fuse (2A) ──── 12V_PROT
            Schottky diode or P-MOSFET
```

Required protections:
- **Reverse polarity**: P-channel MOSFET (e.g., SI4435DDY) or Schottky diode
- **TVS diode**: SMBJ18CA for transient suppression
- **Fuse**: 2A automotive blade fuse or polyfuse
- **EMI filter**: Common-mode choke + decoupling capacitors

### 4.3 5V Rail (for CAN transceiver, optional peripherals)

```
12V_PROT ──── [TPS5430 / LM2596-5.0] ──── 5V (max 1A)
                 │
                 C_in: 22 uF / 25V electrolytic + 100 nF ceramic
                 C_out: 220 uF / 10V electrolytic + 100 nF ceramic
                 L: 33 uH inductor (shielded)
```

### 4.4 3.3V Rail (for CMS + MCU)

The CMS requires **3.3V at up to 350 mA**. The MCU typically needs 50-100 mA. Total 3.3V budget: ~500 mA.

```
5V ──── [AMS1117-3.3 / TLV1117-33] ──── 3.3V (max 800 mA)
            │
            C_in: 10 uF ceramic (X5R/X7R)
            C_out: 22 uF ceramic + 100 nF ceramic
```

**Critical for CMS pin 1 (VDD):**
- Place a **100 nF + 10 uF** decoupling capacitor pair directly at CMS pin 1 (VDD) and pins 2,8,9,10 (GND).
- Keep trace length from regulator to CMS under 20 mm.
- Use a dedicated 3.3V copper pour under the CMS module.

### 4.5 Power Budget Summary

| Consumer | Voltage | Max Current | Notes |
|----------|---------|-------------|-------|
| Charge Module S | 3.3V | 350 mA | Primary consumer |
| STM32 MCU | 3.3V | 100 mA | Including peripherals |
| CAN Transceiver | 5V or 3.3V | 75 mA | MCP2562 typical |
| Contactor drivers | 12V | 200 mA each | 2x contactors |
| Status LEDs, misc | 3.3V | 50 mA | |
| **Total from 12V** | | **~1.2 A** | |

---

## 5. Charge Module S Integration

### 5.1 Footprint

The CMS is a 60-pin SMD module (castellated pads). Dimensions: **50.8 mm x 30.48 mm**.

Pad specifications (from datasheet Figure 2):
- Pad width: **2.10 mm**
- Pad height: **1.49 mm**
- Pad pitch: **2.54 mm** (center-to-center)
- Pin 1 marking: Rectangular pad, top-left corner
- **Restricted area**: Keep the area between pads free of copper on the base PCB

### 5.2 Pin Connections (Active pins used)

| CMS Pin | Name | Connect To | Notes |
|---------|------|------------|-------|
| **1** | VDD | 3.3V rail | Supply, 100nF + 10uF decoupling |
| **2, 8, 9, 10** | GND | Ground plane | Connect all 4 GND pins |
| **3** | RXIN_N | PLC coupling circuit | Powerline RX negative |
| **4** | RXIN_P | PLC coupling circuit | Powerline RX positive |
| **5** | TXOUT_N | PLC coupling circuit | Powerline TX negative |
| **6** | TXOUT_P | PLC coupling circuit | Powerline TX positive |
| **7** | ZC_IN | **GND** | For EV application: tie to GND |
| **27** | CAN_RX | CAN transceiver RXD | 3.3V logic level |
| **28** | CAN_TX | CAN transceiver TXD | 3.3V logic level |
| **29** | PP_value | PP circuit ADC output | Proximity Pilot sensing |
| **37** | EV_CP_Edge | CP circuit edge detect | PWM duty cycle detection |
| **38** | CP_State_C | CP State switching | Output to switch B↔C |
| **59** | RESET | MCU GPIO (optional) | Active low, with pull-up |

### 5.3 Unused Pins

- Pins 11-26 (MII/RMII): Reserved - leave **unconnected** (NC)
- Pins 30-34 (SPI): Not implemented - leave NC
- Pins 35-36 (I2C): Reserved - leave NC
- Pins 39-44 (CP reserved): Leave NC
- Pins 45-50 (GPIO): Optional - can connect to MCU for additional signals
- Pins 51-58 (Trace/JTAG): Leave NC unless debugging is needed
- Pin 60 (JTAG_TMS): Leave NC

### 5.4 CMS Decoupling

Place these capacitors as close as possible to the CMS module:

```
CMS Pin 1 (VDD) ────┬──── 100 nF (C0402 or C0603, X7R) ──── GND
                     │
                     └──── 10 uF (C0805, X5R/X7R) ──── GND
```

This corresponds to **C4 = 0.1 uF** in the reference schematic (datasheet Figure 3).

---

## 6. CAN Bus Interface

### 6.1 CAN Parameters
- **Baud rate**: 500 kbit/s (CMS default)
- **Frame format**: Extended (29-bit) IDs
- **Cycle time**: 100 ms for most messages
- **Bus nodes**: VCU ↔ CMS (on this PCB, short traces)

### 6.2 CAN Transceiver Circuit

```
              MCP2562FD-E/SN
         ┌──────────────────┐
         │  1 TXD    VDD 8  │──── 3.3V (or 5V)
MCU_TX ──│  2 VSS    CANH 7 │──── CAN_H ──→ to CMS Pin 28 & external CAN
         │  3 VIO    CANL 6 │──── CAN_L ──→ to CMS Pin 27 & external CAN
MCU_RX ──│  4 RXD   STBY 5  │──── GND (normal mode) or MCU GPIO
         └──────────────────┘
```

Wait - the CMS has **logic-level CAN** on pins 27 (CAN_RX) and 28 (CAN_TX). The CMS module has its own CAN controller but **no CAN transceiver**. You need to provide a CAN transceiver between the CMS and the physical CAN bus.

### 6.3 CAN Bus Topology

There are two options:

**Option A: Shared CAN bus (recommended for simplicity)**
```
CMS (pins 27/28) ──── CAN Transceiver ──── CAN_H/CAN_L bus ──── CAN Transceiver ──── MCU (CAN_TX/RX)
```
Both CMS and MCU share one physical CAN bus. The MCU needs its own CAN transceiver too.

**Option B: Dedicated CAN link (shorter traces, lower EMI)**
```
CMS (pins 27/28) ──┐
                    ├──── Single CAN Transceiver pair ──── MCU (CAN_TX/RX)
                    │     (point-to-point on PCB)
```
For on-board communication only, you can use a single transceiver pair with very short bus length.

**Recommended: Option A** - Use two CAN transceivers. This allows future expansion (BMS on same bus, diagnostic tools, etc.)

### 6.4 CAN Bus Termination

Each end of the CAN bus needs a 120-ohm termination resistor:

```
CAN_H ──── [120 Ohm] ──── CAN_L     (at each bus end)
```

If the PCB is one end of the bus, place a 120-ohm termination resistor on-board. Optionally use a jumper or DIP switch to enable/disable it.

### 6.5 CAN Bus Protection

Add ESD protection on CAN lines going to external connectors:
- **TVS diode**: PESD2CAN (dual bidirectional TVS for CAN)
- **Common-mode choke**: Optional, for EMC compliance (e.g., ACM2012-102)

---

## 7. Control Pilot Circuit

The Control Pilot (CP) is the primary signaling line between the EV and the EVSE, defined by IEC 61851-1. The EVSE generates a +/-12V PWM signal at 1 kHz. The EV loads the CP line to indicate its state.

### 7.1 CP States (IEC 61851-1)

| State | Voltage (positive) | Meaning |
|-------|-------------------|---------|
| A | +12V (no PWM) | No vehicle connected |
| B | +9V / -12V | Vehicle connected, not ready |
| C | +6V / -12V | Vehicle ready, charging allowed |
| D | +3V / -12V | With ventilation required |
| E | 0V | Error - short to PE |
| F | -12V | EVSE not available |

### 7.2 CP Circuit for CMS (from Datasheet Reference Schematic)

This circuit is for the **I2CMS-DMBC** variant where the customer (VCU) handles the CP interface.

```
                                                    To CMS Pin 37
                                                    (EV_CP_Edge)
Control Pilot ──────┬─────────────────────────────────────┘
(from CCS inlet)    │
                    │         D3: 1N4148W-E3-18
                    │              ┌──►──┐
                    │              │     │
                R2: 10 Ohm        │   R3: 10 kOhm ── 3.3V
                    │              │     │
                    ├──────────────┤     │
                    │              │   D4 (Schottky)
                    │              │     │
                D2: PESD12        │   R4: 2.7 kOhm
                TVL1BA,115        │     │
                    │              │   R5: 1.3 kOhm ── GND
                    │              │
                C1: 1000 pF       C2: 470 pF
                    │              │
                    │            C3: 100 pF
                   GND             │
                                   │
                    R1: 220 Ohm    │
                    │              │
                    L1: 220 uH     │
                    │              │
                    D1: SMAJ12CA   │        To CMS Pin 38
                    │              │        (CP_State_C)
                   GND             │            │
                                   │          ┌─┘
                              T1 (NPN)        │
                                   │        R6: 470 Ohm
                                   │          │
                                 R7: 10 kOhm  │
                                   │          │
                                  GND        GND
```

### 7.3 CP Component Details

| Ref | Value | Part Number | Purpose |
|-----|-------|-------------|---------|
| R1 | 220 Ohm | Standard 0603 | PLC signal coupling series resistor |
| R2 | 10 Ohm | Standard 0603 | CP line series resistance |
| R3 | 10 kOhm | Standard 0603 | Pull-up to 3.3V for edge detect |
| R4 | 2.7 kOhm | Standard 0603 | Voltage divider for edge detect |
| R5 | 1.3 kOhm | Standard 0603 | Voltage divider for edge detect |
| R6 | 470 Ohm | Standard 0603 | State C switching current limit |
| R7 | 10 kOhm | Standard 0603 | Transistor base pull-down |
| C1 | 1000 pF | C0603, C0G/NP0 | PLC coupling capacitor |
| C2 | 470 pF | C0603, C0G/NP0 | Edge detector filter |
| C3 | 100 pF | C0402, C0G/NP0 | Edge detector HF filter |
| D1 | SMAJ12CA | TVS, DO-214AC | CP line transient protection |
| D2 | PESD12TVL1BA,115 | ESD protection | CP line ESD/surge |
| D3 | 1N4148W-E3-18 | Signal diode, SOD-323 | Positive half rectification |
| D4 | Schottky | BAT54S or similar | Clamping diode |
| L1 | 220 uH | Shielded inductor | PLC signal coupling inductor |
| T1 | NPN | BC847 or 2N7002 (NMOS) | State C switching transistor |

### 7.4 How CP State Switching Works

CMS pin 38 (CP_State_C) drives T1:
- **T1 ON** (CP_State_C = HIGH): Adds R6 (470 Ohm) load to CP → pulls voltage from +9V to +6V → **State C** (ready to charge)
- **T1 OFF** (CP_State_C = LOW): No additional load → **State B** (connected, not ready)

The voltage divider R4/R5 scales the CP signal to 3.3V-safe levels for CMS pin 37 (EV_CP_Edge) which detects the PWM duty cycle.

---

## 8. Proximity Pilot Circuit

The Proximity Pilot (PP) indicates the cable current rating and connector state (plugged/unplugged, button pressed).

### 8.1 PP Resistance Values (IEC 61851-1, Type 2)

| PP Resistance | Max Current | ProximityPinState |
|---------------|-------------|-------------------|
| 100 Ohm | Not connected | 0 (Not_Connected) |
| 1.5 kOhm | 13 A | 3 (Type2_Connected13A) |
| 680 Ohm | 20 A | 4 (Type2_Connected20A) |
| 220 Ohm | 32 A | 5 (Type2_Connected32A) |
| 100 Ohm (with extra R) | 63 A | 6 (Type2_Connected63A) |

### 8.2 PP Circuit (from Datasheet Reference Schematic)

```
                                          3.3V
                                           │
                                        R10: 1 kOhm
                                           │
Proximity Pilot ──── R8: 220 Ohm ────┬──── R9: 220 Ohm ──┬──── LED D6 ──── To CMS Pin 29
(from CCS inlet)                      │                    │                 (PP_value)
                                      │                  C6: 0.1 uF
                                    L2: 220 uH             │
                                      │                   GND
                                    C5: 470 pF
                                      │
                                    D5 (TVS)
                                      │
                                     GND
```

### 8.3 PP Component Details

| Ref | Value | Purpose |
|-----|-------|---------|
| R8 | 220 Ohm | Series protection |
| R9 | 220 Ohm | Voltage divider / current limit |
| R10 | 1 kOhm | Pull-up to 3.3V |
| C5 | 470 pF | PLC filter on PP line |
| C6 | 0.1 uF | ADC smoothing capacitor |
| L2 | 220 uH | PLC signal isolation |
| D5 | TVS | Transient protection |
| D6 | LED (optional) | Connection indicator |

CMS pin 29 (PP_value) reads the PP voltage via internal ADC. The CMS firmware determines the cable rating and reports it in the `ChargeInfo` message (ProximityPinState signal).

---

## 9. PLC Coupling Circuit

The PLC (Power Line Communication) happens on the Control Pilot line. The CMS module uses QCA7005 HomePlug GreenPHY to communicate with the EVSE's SECC at up to 10 Mbit/s.

### 9.1 PLC Coupling Transformer

The key component is the **I2PLCTR-2** coupling transformer provided by Chargebyte (in-tech smart charging). This is a **non-standard part** - you cannot substitute it with a generic transformer.

```
                    I2PLCTR-2
                 ┌─────────────┐
                 │  4       6  │
  R2 (10 Ohm) ──│             │──── CMS Pin 5 (TXOUT_N)
  from CP line   │             │
                 │  (primary)  │
                 │  8       5  │──── CMS Pin 6 (TXOUT_P)
                 │             │
                 │  (secondary)│
                 │  1       3  │──── CMS Pin 3 (RXIN_N)
                 │             │
                 │             │──── CMS Pin 4 (RXIN_P)
                 │  7          │
                 └─────────────┘
```

### 9.2 How to order the I2PLCTR-2

Contact Chargebyte / in-tech smart charging GmbH:
- Website: https://in-tech-smartcharging.com
- The transformer and other non-standard parts are provided by them for customers integrating the CMS module.

### 9.3 PLC Signal Path

```
EVSE ═══ CP Line ═══ [R2=10Ohm] ═══ [I2PLCTR-2] ═══ CMS RX/TX pins
                                          │
                            [R1=220Ohm + L1=220uH + C1=1000pF]
                            (PLC coupling network to CP line)
```

The PLC signals are coupled onto the Control Pilot line through the transformer and LC network. The EVSE has a matching PLC coupler on its side.

---

## 10. Contactor Control

For DC fast charging, two DC contactors must be controlled: **DC+** and **DC-**. The VCU (MCU) controls these based on the CMS state machine.

### 10.1 Contactor Driver Circuit

```
MCU GPIO ──── R: 1 kOhm ──── Gate ┐
                                   │  N-Channel MOSFET
                            Source ┤  (IRLZ44N or similar logic-level)
                              GND  │
                                   │ Drain
                                   │
                    ┌── Flyback diode (1N4007 or SL1M) ──┐
                    │                                      │
              Contactor coil (+) ────────────── Contactor coil (-)
                    │
                  12V_PROT
```

Each contactor needs its own driver circuit. Use logic-level N-channel MOSFETs since the MCU operates at 3.3V.

### 10.2 Contactor Control Logic (based on CMS state)

| CMS State | DC+ Contactor | DC- Contactor | Notes |
|-----------|---------------|---------------|-------|
| Default | OPEN | OPEN | Idle |
| Init → Isolation | OPEN | OPEN | Communication phase |
| PreCharge | OPEN | **CLOSE** | DC- first, EVSE ramps voltage |
| PreCharge (VoltageMatch=True) | **CLOSE** | **CLOSE** | Voltage within 20V of battery |
| Charge | **CLOSE** | **CLOSE** | Active charging |
| StopCharge | **CLOSE** → OPEN | **CLOSE** → OPEN | Open after current < threshold |
| Welding | OPEN | OPEN | Testing for welded contacts |
| SessionStop | OPEN | OPEN | |
| Error | **OPEN** | **OPEN** | Immediate open on error |

**CRITICAL**: On **Emergency Stop** or CMS **Error** state, both contactors must open immediately.

### 10.3 PreCharge Sequence

During PreCharge, the EVSE ramps its output voltage to match the EV battery voltage. The VCU must:

1. Close DC- contactor first (DC+ stays open)
2. Set `EVPreChargeVoltage` to actual battery voltage (within 10V accuracy)
3. CMS limits PreCharge current to 2A (per specification)
4. Monitor `VoltageMatch` signal from CMS
5. When `VoltageMatch = True` (EVSE voltage within 20V of battery): close DC+ contactor
6. Set `ChargeProgressIndication = Start` to proceed to Charge state

### 10.4 Contactor Welding Detection

After charging stops, if `EVWeldingDetectionEnable = True`:
1. Open both contactors
2. The EVSE measures voltage to detect if contactors are welded
3. CMS reports result via EVSE status
4. Only after welding test can the session fully stop

---

## 11. Safety & Protection

### 11.1 Emergency Stop

The PCB must have a hardware emergency stop input:

```
E-STOP Button ──── Pull-up to 3.3V ──── MCU GPIO (interrupt-capable)
(NC contact)          10 kOhm                │
                                             │
                                     On trigger:
                                     1. Set EVReady = False
                                     2. Open all contactors
                                     3. Set ChargeStopIndication = Terminate
```

The software must respond within **50 ms** of an emergency stop trigger.

### 11.2 CMS Heartbeat Monitoring

The CMS sends an `AliveCounter` (0-14, incrementing) in the `ChargeInfo` message every 100 ms. The VCU must monitor:

- If AliveCounter doesn't change for **1500 ms** → CMS is not responsive → emergency stop
- If no `ChargeInfo` message received for **1000 ms** (10 missed cycles) → communication lost → emergency stop

### 11.3 Voltage/Current Safety Limits

Implement in MCU firmware:

| Parameter | Hard Limit | User Settable Max | Notes |
|-----------|-----------|-------------------|-------|
| Voltage | 6500 V | 500 V (typical) | DBC range |
| Current | 3250 A | 200 A (typical) | DBC range |
| Power | 3.27 MW | 100 kW (typical) | Calculated |

### 11.4 Isolation Monitoring

Monitor `EVSEIsolationStatus` from the `EVSEDCStatus` message:

| Value | Status | Action |
|-------|--------|--------|
| 0 | Invalid | Do not proceed |
| 1 | Valid | OK, continue |
| 2 | Warning | Proceed with caution |
| 3 | Fault | Stop immediately |
| 4 | No IMD | No isolation monitoring |
| 5 | Checking | Wait |

### 11.5 Status LED Indicators

Recommended status LEDs on the PCB:

| LED | Color | Meaning |
|-----|-------|---------|
| PWR | Green | 3.3V supply OK |
| CAN | Yellow | CAN bus active (blink on TX/RX) |
| CMS | Blue | CMS alive (blink with AliveCounter) |
| CHARGE | Green | Charging active (solid in Charge state) |
| ERROR | Red | Error state or safety fault |
| CP | White | Control Pilot state B or C |

---

## 12. Connector Specifications

### 12.1 Recommended Connectors

| Connector | Type | Pins | Purpose |
|-----------|------|------|---------|
| J1 | Molex Micro-Fit 3.0 (2-pin) | 2 | 12V power input |
| J2 | JST-PH or Molex (2-pin) | 2 | Control Pilot (from CCS inlet) |
| J3 | JST-PH or Molex (2-pin) | 2 | Proximity Pilot (from CCS inlet) |
| J4 | Molex Micro-Fit 3.0 (4-pin) | 4 | CAN bus (CANH, CANL, +12V, GND) |
| J5 | Molex Micro-Fit 3.0 (4-pin) | 4 | Contactor outputs (DC+, DC-, common) |
| J6 | 2x5 pin header (1.27mm) | 10 | SWD debug (STM32) |
| J7 | JST-PH (4-pin) | 4 | BMS interface (CAN or UART) |
| J8 | Molex (2-pin) | 2 | Emergency stop input |

### 12.2 CCS Inlet Connector Pinout (Type 2 / Combo 2)

For reference, the CCS Combo 2 connector has:
- **CP**: Control Pilot (pin in AC part)
- **PP**: Proximity Pilot (pin in AC part)
- **PE**: Protective Earth
- **DC+**: DC positive (large pin)
- **DC-**: DC negative (large pin)

Your PCB connects to CP and PP via wires from the CCS inlet. DC+/DC- go through the contactors to the battery.

---

## 13. PCB Layout Guidelines

### 13.1 General
- **Layers**: Minimum 4-layer PCB recommended (Top, GND, Power, Bottom)
- **Board thickness**: 1.6 mm standard
- **Copper weight**: 1 oz (35 um) for signal layers, 2 oz (70 um) for power if needed
- **Min trace width**: 0.2 mm (8 mil) for signals
- **Min clearance**: 0.2 mm (8 mil)

### 13.2 CMS Module Footprint Area
- Follow the recommended footprint from datasheet (Section 4, Figure 2)
- Pad size: 2.10 mm x 1.49 mm
- Pad pitch: 2.54 mm
- Keep the **restricted area** (between the pads, under the module) free of copper
- Pin 1 is rectangular-shaped on the top side of the module
- Use reflow soldering (max 2 reflow cycles per IPC/JEDEC J-STD-020)

### 13.3 CAN Bus Traces
- Route CAN_H and CAN_L as **differential pair**
- Trace width: 0.25 mm with 0.15 mm gap (100-ohm differential impedance)
- Keep CAN traces away from PLC/CP traces (min 5 mm separation)
- Place 120-ohm termination resistor close to the CAN connector

### 13.4 PLC / Control Pilot Traces
- Keep PLC traces (RXIN, TXOUT) **short** (< 30 mm from transformer to CMS)
- Route RXIN_P/RXIN_N and TXOUT_P/TXOUT_N as differential pairs
- Shield PLC traces from CAN bus and power supply traces
- Place PLC coupling transformer and CP circuit components close together

### 13.5 Power Supply
- Dedicate Layer 3 (or part of it) as a 3.3V power plane under the CMS
- Use GND plane (Layer 2) as continuous ground reference
- Keep switching regulator (12V→5V) far from PLC circuits (switching noise interferes with PLC)
- Use star-ground topology: separate analog ground (CP/PP ADC) from digital ground, connect at one point

### 13.6 Component Placement Priority
1. CMS module (center or upper area of PCB)
2. PLC coupling transformer and CP circuit (adjacent to CMS, near edge with CP connector)
3. CAN transceiver (near CMS CAN pins)
4. MCU (near CAN transceiver)
5. Power supply (far corner, away from PLC)
6. Connectors (board edges)

### 13.7 Thermal Considerations
- CMS module dissipates up to 3.3V x 350mA = **1.16W**
- Ensure adequate copper for heat dissipation under CMS GND pads (pins 2, 8, 9, 10)
- Use thermal vias under GND pads to inner ground plane
- LDO regulator may need a thermal pad or small heatsink (at max load)

---

## 14. Bill of Materials (BOM)

### 14.1 Core Components

| Qty | Reference | Value / Part | Package | Description |
|-----|-----------|-------------|---------|-------------|
| 1 | U1 | Chargebyte I2CMS-DMBC | 60-pin SMD | Charge Module S |
| 1 | U2 | STM32F446RET6 | LQFP-64 | MCU (VCU) |
| 2 | U3, U4 | MCP2562FD-E/SN | SOIC-8 | CAN transceivers |
| 1 | U5 | AMS1117-3.3 | SOT-223 | 3.3V LDO regulator |
| 1 | U6 | TPS5430DDA | SOIC-8 | 12V → 5V buck converter |
| 1 | TR1 | I2PLCTR-2 | Custom | PLC coupling transformer (from Chargebyte) |

### 14.2 Passive Components

| Qty | Reference | Value | Package | Notes |
|-----|-----------|-------|---------|-------|
| 2 | R1, R8 | 220 Ohm | 0603 | PLC coupling |
| 1 | R2 | 10 Ohm | 0603 | CP series |
| 2 | R3, R7 | 10 kOhm | 0603 | Pull-up/pull-down |
| 1 | R4 | 2.7 kOhm | 0603 | CP voltage divider |
| 1 | R5 | 1.3 kOhm | 0603 | CP voltage divider |
| 1 | R6 | 470 Ohm | 0603 | State C switching |
| 1 | R9 | 220 Ohm | 0603 | PP circuit |
| 1 | R10 | 1 kOhm | 0603 | PP pull-up |
| 2 | R_TERM | 120 Ohm | 0603 | CAN bus termination |
| 2 | R_GATE | 1 kOhm | 0603 | MOSFET gate resistors |
| 1 | C1 | 1000 pF | 0603, C0G | PLC coupling |
| 2 | C2, C5 | 470 pF | 0603, C0G | CP/PP filter |
| 1 | C3 | 100 pF | 0402, C0G | CP HF filter |
| 1 | C4 | 100 nF | 0603, X7R | CMS decoupling |
| 1 | C_CMS | 10 uF | 0805, X5R | CMS bulk decoupling |
| 1 | C6 | 100 nF | 0603, X7R | PP ADC filter |
| 4 | C_DEC | 100 nF | 0402, X7R | MCU decoupling (per VDD pin) |
| 2 | C_CAN | 100 nF | 0603, X7R | CAN transceiver decoupling |
| 2 | L1, L2 | 220 uH | Shielded, 5x5mm | PLC coupling inductors |
| 1 | L_BUCK | 33 uH | Shielded, 6x6mm | Buck converter inductor |

### 14.3 Semiconductors

| Qty | Reference | Part | Package | Notes |
|-----|-----------|------|---------|-------|
| 1 | D1 | SMAJ12CA | DO-214AC | CP TVS |
| 1 | D2 | PESD12TVL1BA,115 | SOT-23 | CP ESD |
| 1 | D3 | 1N4148W-E3-18 | SOD-323 | CP rectifier |
| 1 | D4 | BAT54S | SOT-23 | CP clamping |
| 1 | D5 | SMAJ5.0A | DO-214AC | PP TVS |
| 1 | D_TVS_CAN | PESD2CAN | SOT-23 | CAN ESD |
| 2 | D_FLY | 1N4007 | SMA | Contactor flyback |
| 1 | T1 | BC847 | SOT-23 | CP State C switch |
| 2 | Q_CONT | IRLZ44N or IRL540N | TO-220 or D-PAK | Contactor drivers |
| 1 | Y1 | 8 MHz crystal | HC49 or 3215 | MCU HSE |

### 14.4 Connectors

| Qty | Reference | Type | Notes |
|-----|-----------|------|-------|
| 1 | J1 | Molex Micro-Fit 3.0, 2-pin | 12V power |
| 1 | J2 | JST-PH, 2-pin | Control Pilot |
| 1 | J3 | JST-PH, 2-pin | Proximity Pilot |
| 1 | J4 | Molex Micro-Fit 3.0, 4-pin | CAN bus |
| 1 | J5 | Molex Micro-Fit 3.0, 4-pin | Contactor outputs |
| 1 | J6 | 2x5 SWD header, 1.27mm | Debug |
| 1 | J7 | JST-PH, 4-pin | BMS interface |
| 1 | J8 | Molex, 2-pin | Emergency stop |

---

## 15. Testing & Commissioning

### 15.1 Pre-Power Checks
1. Visual inspection of all solder joints, especially CMS module
2. Check for shorts between VDD and GND (should be > 10 kOhm)
3. Check for shorts on CAN_H/CAN_L
4. Verify all component values against BOM

### 15.2 Power-Up Sequence
1. Apply 12V, verify 5V rail (should be 5.0V +/- 0.1V)
2. Verify 3.3V rail (should be 3.3V +/- 0.05V)
3. Measure current draw: without CMS ~50 mA, with CMS ~200-350 mA
4. Check CMS reset pin behavior (held low during boot, then high)

### 15.3 CAN Communication Test
1. Connect a CAN analyzer (e.g., PCAN-USB)
2. Set 500 kbit/s, extended frame format
3. CMS should start sending `ChargeInfo` (0x0600) every 100 ms after boot
4. `AliveCounter` should increment every cycle (0→14, wrapping)
5. `StateMachineState` should be 0 (Default) when no plug is connected

### 15.4 First Charging Test
1. Start with a **CCS simulator** before testing with a real charger
2. Verify the full state machine sequence: Default → Init → Auth → Parameter → ...
3. Monitor all CAN messages with the CCS Charger desktop application (this repository)
4. Verify contactor timing matches the state machine transitions
5. Test emergency stop response time (< 50 ms)

### 15.5 CMS Module Reset

To reset the CMS module via CAN, send a standard (11-bit) frame:
- **ID**: 0x667
- **DLC**: 2
- **Data**: [0xFF, 0x00]

This triggers a software reset of the CMS module.

---

## Appendix A: CAN Message Quick Reference

### VCU → CMS (messages you send)

| CAN ID | Name | Cycle | Key Signals |
|--------|------|-------|-------------|
| 0x1300 | EVDCMaxLimits | 100 ms | EVMaxCurrent, EVMaxVoltage, EVMaxPower, EVFullSoC, EVBulkSoC |
| 0x1301 | EVDCChargeTargets | 100 ms | EVTargetCurrent, EVTargetVoltage, EVPreChargeVoltage |
| 0x1302 | EVStatusControl | 100 ms | ChargeProgressIndication, EVReady, ChargeStopIndication |
| 0x1303 | EVStatusDisplay | 100 ms | EVSoC, EVErrorCode, EVChargingComplete |
| 0x1304 | EVPlugStatus | 100 ms | EVControlPilotState, EVProximityPinState |
| 0x1305 | EVDCEnergyLimits | 100 ms | EVEnergyCapacity, EVEnergyRequest |

### CMS → VCU (messages you receive)

| CAN ID | Name | Cycle | Key Signals |
|--------|------|-------|-------------|
| 0x0600 | ChargeInfo | 100 ms | StateMachineState, AliveCounter, VoltageMatch, CP state |
| 0x1400 | EVSEDCMaxLimits | 100 ms | EVSEMaxCurrent, EVSEMaxVoltage, EVSEMaxPower |
| 0x1401 | EVSEDCRegulationLimits | 100 ms | EVSEMinVoltage, EVSEMinCurrent |
| 0x1402 | EVSEDCStatus | 100 ms | EVSEPresentVoltage, EVSEPresentCurrent, IsolationStatus |
| 0x2001 | SoftwareInfo | 10 s | Firmware version |
| 0x2002 | ErrorCodes | 1 s | Error codes level 0-3 |
| 0x2003 | SLACInfo | Event | SLAC state, link status, attenuation |

**Note**: All CAN IDs use **extended (29-bit) format**. The raw DBC IDs have bit 31 set (0x80000000) to indicate extended frame format.

### Appendix B: CAN ID Conversion

The DBC file uses the Vector convention where bit 31 marks an extended frame:
```
DBC ID = 0x80000000 | actual_29bit_ID

Example:
DBC ID 2147487744 = 0x80000600 → Extended CAN ID 0x0600
DBC ID 2147488512 = 0x80001300 → Extended CAN ID 0x1300
```

---

## Appendix C: Recommended PCB Dimensions

For a compact design:
- **Board size**: 80 mm x 60 mm (fits CMS module + MCU + all circuits)
- **Mounting holes**: 4x M3, at corners (3.2 mm diameter)
- **Keep-out zones**: 5 mm from board edges for connectors

For a more spacious layout with better EMC:
- **Board size**: 100 mm x 80 mm
- This allows better separation between PLC and power supply sections

---

## Appendix D: Firmware Integration Notes

The existing software in this repository (`src/`) implements the complete VCU protocol as a **desktop application** using Qt6. For the embedded MCU firmware, you need to re-implement the same protocol logic targeting STM32 (or your chosen MCU). Key modules to port:

1. **CAN frame encode/decode** → Use STM32 HAL CAN drivers
2. **DBC signal codec** → Port `signal_codec.h/cpp` (pure C++ math, no Qt dependency)
3. **State machine logic** → Port `charge_module.cpp` state handling
4. **Safety monitor** → Port `safety_monitor.h/cpp` (heartbeat, limits, timeouts)
5. **Cyclic TX** → Use a hardware timer (100 ms) to trigger CAN message transmission

The DBC file (`ISC_CMS_Automotive.dbc`) defines all message/signal definitions and is the authoritative source.
