# Custom CCS Communication Module - PCB Design Guide
## Lasten EV-Side CCS Modul z QCA7005 + STM32H753

**Version:** 1.0
**Date:** 2026-02-10
**Target:** Certificiranje po ISO 15118-2 / DIN 70121 (EV-side DC)

---

## Kazalo

1. [Pregled sistema](#1-pregled-sistema)
2. [Blok diagram](#2-blok-diagram)
3. [MCU: STM32H753VIT6](#3-mcu-stm32h753vit6)
4. [PLC: QCA7005](#4-plc-qca7005)
5. [SPI vmesnik MCU ↔ QCA7005](#5-spi-vmesnik-mcu--qca7005)
6. [Napajanje](#6-napajanje)
7. [PLC sklopno vezje (CP linija)](#7-plc-sklopno-vezje-cp-linija)
8. [Control Pilot vezje (IEC 61851)](#8-control-pilot-vezje-iec-61851)
9. [Proximity Pilot vezje](#9-proximity-pilot-vezje)
10. [CAN bus vmesnik](#10-can-bus-vmesnik)
11. [Kristalni oscilatorji](#11-kristalni-oscilatorji)
12. [Reset in Boot vezje](#12-reset-in-boot-vezje)
13. [JTAG/SWD debug](#13-jtagswd-debug)
14. [Flash pomnilnik za QCA7005](#14-flash-pomnilnik-za-qca7005)
15. [Varnostna vezja](#15-varnostna-vezja)
16. [PCB layout smernice](#16-pcb-layout-smernice)
17. [Celoten BOM](#17-celoten-bom)
18. [Software arhitektura](#18-software-arhitektura)
19. [Certifikacijske zahteve](#19-certifikacijske-zahteve)
20. [Testiranje in zagon](#20-testiranje-in-zagon)

---

## 1. Pregled sistema

### Kaj gradimo

Lasten **EV-side CCS komunikacijski modul** (nadomestek za Chargebyte Charge Module S), ki ga sestavlja:

- **QCA7005** - Qualcomm Atheros HomePlug GreenPHY PLC čip (PHY + MAC)
- **STM32H753VIT6** - ARM Cortex-M7 MCU z hardversko kriptografijo (protokol stack)
- **CAN transceiver** - komunikacija z VCU (vehicle control unit)
- **PLC coupling** - sklopno vezje na Control Pilot linijo
- **CP/PP** - IEC 61851-1 signalizacija

### Funkcije modula

| Funkcija | Kdo izvaja |
|----------|-----------|
| HomePlug GreenPHY PLC (fizični sloj) | QCA7005 |
| SLAC (Signal Level Attenuation Characterization) | STM32 + QCA7005 |
| TCP/IP networking (lwIP) | STM32 |
| TLS 1.2 (mbedTLS, hw akceleracija) | STM32 (CRYP + HASH + PKA) |
| V2GTP (Vehicle-to-Grid Transfer Protocol) | STM32 |
| EXI kodiranje/dekodiranje (ISO 15118-2 / DIN 70121) | STM32 |
| Stavni avtomat (state machine) | STM32 |
| Control Pilot PWM detekcija | STM32 (timer input capture) |
| Control Pilot state switching (B↔C) | STM32 GPIO |
| Proximity Pilot ADC branje | STM32 ADC |
| CAN komunikacija z VCU | STM32 FDCAN |
| Heartbeat (AliveCounter) | STM32 |

### Potek polnjenja

```
1. EV priključi kabel → CP State A → B
2. MCU zazna CP PWM → sproži SLAC prek QCA7005
3. QCA7005 vzpostavi PLC link z EVSE (HomePlug GreenPHY)
4. MCU odpre TCP povezavo prek QCA7005 (Ethernet over SPI)
5. TLS 1.2 handshake (ISO 15118) ali brez TLS (DIN 70121)
6. V2G sporočila: SessionSetup → ServiceDiscovery → ChargeParameterDiscovery
7. CableCheck (izolacijski test) → PreCharge → PowerDelivery
8. CurrentDemand (aktivno polnjenje)
9. PowerDelivery STOP → WeldingDetection → SessionStop
10. CP State B → A, kabel odklopljen
```

---

## 2. Blok diagram

```
                          +--------------------------------------------------+
                          |          Custom CMS Module PCB                    |
                          |                                                  |
  CCS Inlet              |   +-------------+      SPI       +------------+  |
  ════════              |   |             |◄────────────►|            |  |
                          |   |   QCA7005   |  (Eth frames) | STM32H753  |  |
  Control ───── PLC ──────┤   |  HomePlug   |              |  Cortex-M7 |  |
  Pilot    Coupling     |   |  GreenPHY   | INT    RST   |            |  |
           Circuit      |   |             |──────►|◄─────|  480 MHz   |  |
                          |   +------┬------+              |  1MB RAM   |  |
                          |          │                     |  HW Crypto |  |
                          |     25 MHz XTAL                |            |  |
                          |                                +-----┬------+  |
  Control ───── CP ───────┤                                      │         |
  Pilot    Circuit      |   CP_Edge (Timer IC) ◄─────────────────┤         |
  (PWM)                 |   CP_State_C (GPIO) ◄──────────────────┤         |
                          |                                      │         |
  Proximity ── PP ────────┤   PP_ADC ◄───────────────────────────┤         |
  Pilot    Circuit      |                                      │         |
                          |                    +-------------+   │         |
                          |              CAN   | CAN         |   │         |
                          ├──────────────Bus───►| Transceiver |◄──┘         |
                          |                    | TJA1443     |             |
                          |                    +-------------+             |
                          |                                                |
  3.3V ◄── LDO ◄── 5V ◄── Buck ◄── 12V Automotive                        |
  1.8V ◄── LDO (QCA core)                                                |
                          |                                                |
                          +--------------------------------------------------+
```

---

## 3. MCU: STM32H753VIT6

### 3.1 Zakaj STM32H753

| Lastnost | Vrednost | Zakaj je potrebno |
|----------|---------|-------------------|
| Jedro | Cortex-M7 @ 480 MHz | EXI dekodiranje + TLS v realnem času |
| RAM | 1 MB (TCM + AXI SRAM) | TLS session buffers (~50KB), lwIP (~40KB), EXI (~30KB), stack |
| Flash | 2 MB | Firmware + certifikati + DBC tabele |
| HW Crypto (CRYP) | AES-128/256, DES, TDES | TLS šifriranje |
| HW Hash (HASH) | SHA-1, SHA-224, SHA-256, HMAC | TLS integrity |
| HW PKA | RSA, ECC (NIST P-256/384) | TLS handshake, Plug & Charge certifikati |
| HW RNG | True Random Number Generator | TLS nonces, session keys |
| SPI | Do 150 MHz, DMA | QCA7005 komunikacija (do 24 MHz) |
| FDCAN | 2x CAN FD controller | Komunikacija z VCU |
| Timers | Advanced timers z Input Capture | CP PWM detekcija (1 kHz, duty cycle) |
| ADC | 16-bit, 3.6 Msps | PP upor meritev |
| Temp. range | -40 do +85°C (Industrial) | Avtomobilsko okolje |
| Package | LQFP-100 (14x14mm) | Obvladljiv za prototipiranje |
| AEC-Q100 | Da (STM32H753VIT6 varianta) | Certificiranje |

### 3.2 Alternativne variante

| Če potrebuješ... | Uporabi | Razlika |
|-------------------|---------|---------|
| Manjši package | STM32H753**ZI**T6 | UFBGA-144 (7x7mm), težji za prototip |
| Brez HW crypto | STM32H743VIT6 | Cenejši, TLS počasnejši (sw crypto) |
| Višja temp. (+125°C) | STM32H753VI**H6** | Grade 1 automotive |

**Priporočilo: STM32H753VIT6** v LQFP-100 - najboljše razmerje med zmogljivostjo, crypto podporo in praktičnostjo za prototip + certificiranje.

### 3.3 Pin assignment (LQFP-100)

```
Funkcija             STM32H753 Pin    AF/Tip         Opis
─────────────────────────────────────────────────────────────────
SPI1_SCK             PA5              AF5            → QCA7005 CLK
SPI1_MISO            PA6              AF5            ← QCA7005 MISO (DO)
SPI1_MOSI            PA7              AF5            → QCA7005 MOSI (DI)
SPI1_NSS             PA4              GPIO out       → QCA7005 CS (active low)
QCA_INT              PB0              GPIO in (EXTI) ← QCA7005 INT (active low)
QCA_RESET            PB1              GPIO out       → QCA7005 RESET (active low)

FDCAN1_TX            PD1              AF9            → CAN Transceiver TXD
FDCAN1_RX            PD0              AF9            ← CAN Transceiver RXD

TIM2_CH1             PA0              AF1            ← CP_Edge (Input Capture, 1kHz PWM)
CP_STATE_C           PC0              GPIO out       → CP State C switching (NPN/NMOS)

ADC1_IN3             PA3              Analog         ← PP_value (ADC)
ADC1_IN4             PA4              Analog         ← CP voltage (optional)

USART3_TX            PD8              AF7            Debug UART TX
USART3_RX            PD9              AF7            Debug UART RX

LED_STATUS           PC13             GPIO out       Status LED
LED_ERROR            PC14             GPIO out       Error LED
LED_CHARGE           PC15             GPIO out       Charging LED

SWDIO                PA13             Debug          SWD Data
SWCLK                PA14             Debug          SWD Clock
SWO                  PB3              Debug          Trace output

HSE_IN               PH0              OSC            8 MHz crystal
HSE_OUT              PH1              OSC            8 MHz crystal

BOOT0                Pin 94           Boot           Pull-down 10k → GND
NRST                 Pin 14           Reset          100nF + 10k pull-up
```

### 3.4 MCU vezje

```
                          STM32H753VIT6 (LQFP-100)
                     ┌─────────────────────────────┐
                     │                             │
       3.3V ────┬────│ VDD (vsi VDD pini)          │
                │    │                             │
          100nF x8   │ VSS (vsi GND pini) ─────── GND
          (per pin)  │                             │
                     │ PA5  (SPI1_SCK)  ──────────►│──── QCA7005 CLK
                     │ PA6  (SPI1_MISO) ◄──────────│──── QCA7005 DO
                     │ PA7  (SPI1_MOSI) ──────────►│──── QCA7005 DI
                     │ PA4  (SPI1_NSS)  ──────────►│──── QCA7005 CS
                     │ PB0  (QCA_INT)   ◄──────────│──── QCA7005 INT
                     │ PB1  (QCA_RST)   ──────────►│──── QCA7005 RESET
                     │                             │
                     │ PD1  (FDCAN1_TX) ──────────►│──── CAN TXD
                     │ PD0  (FDCAN1_RX) ◄──────────│──── CAN RXD
                     │                             │
                     │ PA0  (TIM2_CH1)  ◄──────────│──── CP Edge Detect
                     │ PC0  (GPIO)      ──────────►│──── CP State C
                     │ PA3  (ADC1_IN3)  ◄──────────│──── PP ADC
                     │                             │
                     │ PH0  ──── XTAL 8MHz ──── PH1│
                     │          22pF    22pF        │
                     │                             │
              10k↑   │ NRST ──── 100nF ──── GND   │
              3.3V   │                             │
                     │ BOOT0 ──── 10k ──── GND    │
                     │                             │
                     │ PA13 (SWDIO)  ◄────► SWD    │
                     │ PA14 (SWCLK)  ◄──── SWD     │
                     └─────────────────────────────┘

Decoupling (OBVEZNO za vsak VDD/VDDA pin):
- 8x 100nF (C0402, X7R) - čim bližje pinom
- 2x 4.7uF (C0805, X5R) - bulk na VDD
- 1x 1uF (C0603, X7R) - na VDDA (analogno napajanje)
- 1x 10nF + 1uF na VREF+ (ADC referenca)
```

### 3.5 RAM razporeditev (1 MB)

```
Regija             Velikost    Uporaba
──────────────────────────────────────────────
DTCM (0x20000000)  128 KB      Stack, kritične spremenljivke, CAN buffri
AXI SRAM           512 KB      lwIP buffri, TLS session, EXI workspace
SRAM1              128 KB      SPI DMA buffri (QCA7005 Ethernet frames)
SRAM2              128 KB      CAN TX/RX queues, logging
SRAM3              32 KB       Shared memory (spare)
SRAM4              64 KB       Backup / non-cached (certifikati)
Backup SRAM        4 KB        RTC, persistent counters
──────────────────────────────────────────────
SKUPAJ             ~996 KB
```

Minimalne zahteve za posamezne komponente:
- **lwIP**: ~40 KB RAM (TCP/IP stack, pbuf pool)
- **mbedTLS**: ~50 KB RAM (TLS 1.2 session, handshake buffers)
- **EXI codec**: ~30 KB RAM (OpenV2G ali cbExiGen)
- **V2G state machine**: ~10 KB
- **SPI Ethernet buffers**: ~16 KB (2x MTU frames za DMA)
- **CAN buffers**: ~4 KB
- **Ostalo (stack, heap)**: ~50 KB

**Skupaj: ~200 KB** od razpoložljivih 1024 KB → obilna rezerva.

---

## 4. PLC: QCA7005

### 4.1 QCA7005 pregled

| Parameter | Vrednost |
|-----------|---------|
| Proizvajalec | Qualcomm Atheros (I2SE GmbH distributer) |
| Funkcija | HomePlug GreenPHY PHY + MAC |
| Standard | IEEE 1901 (HomePlug GreenPHY podmnožica) |
| Podatkovni tok | Do 10 Mbit/s |
| Frekvenčni pas | 2-28 MHz (OFDM) |
| Host vmesnik | SPI (slave, do 24 MHz) ali Ethernet MII/RMII |
| Napajanje | 3.3V I/O, 1.8V jedro (interni ali zunanji LDO) |
| Package | QFN-68 (8x8mm) ali QFN-48 |
| Temp. range | -40 do +85°C |
| Firmware | Naložen iz SPI Flash ali prek host SPI |

### 4.2 QCA7005 pin connections

```
Funkcija              QCA7005 Pin    Smer     Povezava
────────────────────────────────────────────────────────────────
VDD33 (I/O supply)    VDD33          POWER    3.3V rail
VDD18 (core)          VDD18          POWER    1.8V LDO izhod (ali interni)
GND                   VSS (vsi)      POWER    GND plane
EPAD (exposed pad)    Spodaj         POWER    GND (thermal via)

SPI_CLK               SPI_CLK        IN       ← STM32 PA5 (SPI1_SCK)
SPI_MOSI (DI)         SPI_SI         IN       ← STM32 PA7 (SPI1_MOSI)
SPI_MISO (DO)         SPI_SO         OUT      → STM32 PA6 (SPI1_MISO)
SPI_CS                SPI_SSN        IN       ← STM32 PA4 (active low)
SPI_INT               INTR           OUT      → STM32 PB0 (active low)

RESET                 RESET_N        IN       ← STM32 PB1 (active low)

TX+                   TX_P           OUT      → PLC coupling trafo (primary +)
TX-                   TX_N           OUT      → PLC coupling trafo (primary -)
RX+                   RX_P           IN       ← PLC coupling trafo (secondary +)
RX-                   RX_N           IN       ← PLC coupling trafo (secondary -)

XTAL_IN               XI             IN       ← 25 MHz crystal
XTAL_OUT              XO             OUT      → 25 MHz crystal

SPI_FLASH_CLK         FL_CLK         OUT      → SPI Flash CLK
SPI_FLASH_CS          FL_CS          OUT      → SPI Flash CS
SPI_FLASH_DI          FL_DI          OUT      → SPI Flash DI (MOSI)
SPI_FLASH_DO          FL_DO          IN       ← SPI Flash DO (MISO)

GPIO0                 GPIO0          CFG      Boot config (pull-up/down)
GPIO1                 GPIO1          CFG      Boot config
GPIO2                 GPIO2          CFG      Boot config

RSVD                  Ostali         -        NC ali po datasheet
```

### 4.3 QCA7005 boot konfiguracija

QCA7005 GPIO pini ob resetu določajo boot modo:

| GPIO2 | GPIO1 | GPIO0 | Boot Mode | Opis |
|-------|-------|-------|-----------|------|
| 0 | 0 | 0 | SPI Flash boot | Firmware iz zunanjega SPI flash |
| 0 | 0 | 1 | Host SPI boot | Firmware naloži host MCU prek SPI |
| 0 | 1 | 0 | MII/RMII mode | Ethernet vmesnik (ne SPI) |

**Za naš dizajn: GPIO[2:0] = 001** → Host SPI boot. STM32 naloži QCA7005 firmware prek SPI ob zagonu. To omogoča firmware update brez menjave flash čipa.

Alternativa: **GPIO[2:0] = 000** → SPI Flash boot. Firmware se shrani v zunanji SPI flash, QCA7005 se sam naloži. Enostavnejše, a manj fleksibilno.

**Priporočilo za certificiranje: SPI Flash boot (000)** - bolj zanesljivo, MCU ne rabi nalagati firmware ob vsakem zagonu.

### 4.4 QCA7005 vezje

```
                         QCA7005 (QFN-68)
                    ┌─────────────────────────┐
                    │                         │
      3.3V ───┬─────│ VDD33 (vsi 3.3V pini)  │
              │     │                         │
        100nF x6    │ VSS (GND pini) ──────── GND
        + 10uF x2   │                         │
                    │ VDD18 ──┐               │
                    │         │               │
                    │    ┌────┴────┐          │
                    │    │ LDO     │          │       ┌──── SPI Flash ────┐
                    │    │ 1.8V    │          │       │ W25Q32JVSS       │
                    │    │ AP2112  │          │       │                  │
                    │    └────┬────┘          │       │  CLK ◄── FL_CLK │
                    │         │               │       │  CS  ◄── FL_CS  │
                    │    1uF + 100nF          │       │  DI  ◄── FL_DI  │
                    │         │               │       │  DO  ──► FL_DO  │
                    │        GND              │       │  VCC ── 3.3V    │
                    │                         │       │  GND ── GND     │
   STM32 PA5 ───►──│ SPI_CLK                 │       └─────────────────┘
   STM32 PA7 ───►──│ SPI_SI (MOSI)           │
   STM32 PA6 ◄──────│ SPI_SO (MISO)           │
   STM32 PA4 ───►──│ SPI_SSN (CS, act. low)  │
   STM32 PB0 ◄──────│ INTR (act. low, OD)     │──── 10k pull-up → 3.3V
   STM32 PB1 ───►──│ RESET_N (act. low)      │──── 10k pull-up → 3.3V
                    │                         │       + 100nF → GND
                    │ XI  ─── 25 MHz ─── XO   │
                    │        15pF    15pF      │
                    │                         │
                    │ TX_P ──────────────────►─│──── PLC Coupling Trafo
                    │ TX_N ──────────────────►─│──── (TX stran)
                    │ RX_P ◄────────────────────│──── PLC Coupling Trafo
                    │ RX_N ◄────────────────────│──── (RX stran)
                    │                         │
                    │ GPIO0 ──── 10k ── GND   │ Boot: SPI Flash mode
                    │ GPIO1 ──── 10k ── GND   │
                    │ GPIO2 ──── 10k ── GND   │
                    │                         │
                    │ EPAD (thermal) ──────── GND (via array)
                    └─────────────────────────┘

Decoupling (OBVEZNO):
- 6x 100nF (C0402, X7R) na vsakem VDD33 pinu
- 2x 10uF (C0805, X5R) bulk za VDD33
- 2x 100nF + 1x 1uF na VDD18
```

---

## 5. SPI vmesnik MCU ↔ QCA7005

### 5.1 SPI parametri

| Parameter | Vrednost |
|-----------|---------|
| SPI Mode | Mode 3 (CPOL=1, CPHA=1) |
| Max Clock | 24 MHz |
| Priporočen Clock | 12 MHz (za zanesljivost) |
| Byte Order | MSB first |
| Word Size | 8-bit ali 16-bit |
| CS | Active low, hardverski ali softverski |
| Interrupt | Active low, open-drain |

### 5.2 QCA7005 SPI protokol

QCA7005 se obnaša kot **Ethernet-over-SPI** naprava. Host MCU pošilja/prejema surove Ethernet frame-e prek SPI:

```
┌──────────────────────────────────────────────┐
│ SPI Transaction Format                        │
├──────────────────────────────────────────────┤
│ CMD (2 bytes) │ Addr (2 bytes) │ Data (N)    │
├──────────────────────────────────────────────┤
│ Write: 0x0100 │ Register addr  │ Write data  │
│ Read:  0x0000 │ Register addr  │ Read data   │
│ Write Burst: za Ethernet TX framing           │
│ Read Burst:  za Ethernet RX framing           │
└──────────────────────────────────────────────┘
```

Ključni SPI registri:

| Register | Naslov | Opis |
|----------|--------|------|
| SPI_REG_BFR_SIZE | 0x0100 | Velikost notranjega bufferja |
| SPI_REG_WRBUF_SPC_AVA | 0x0200 | Razpoložljiv prostor za TX |
| SPI_REG_RDBUF_BYTE_AVA | 0x0300 | Razpoložljivi bajti za RX |
| SPI_REG_SPI_CONFIG | 0x0400 | SPI konfiguracija |
| SPI_REG_INTR_CAUSE | 0x0C00 | Vzrok prekinitve |
| SPI_REG_INTR_ENABLE | 0x0D00 | Omogočenje prekinitev |
| SPI_REG_SIGNATURE | 0x1A00 | Chip ID (mora biti 0xAA55) |

### 5.3 SPI signal integrity

Za zanesljivo SPI komunikacijo pri 12+ MHz:

```
STM32 PA5 (SCK)  ────[33R]──── QCA7005 SPI_CLK
STM32 PA7 (MOSI) ────[33R]──── QCA7005 SPI_SI
STM32 PA6 (MISO) ◄───[33R]──── QCA7005 SPI_SO
STM32 PA4 (NSS)  ────[33R]──── QCA7005 SPI_SSN

33 Ohm series rezistorji na vseh SPI linijah (source termination).
Trace dolžina: max 50 mm, matched length (±5 mm med vsemi).
```

### 5.4 Podatkovni tok

```
EVSE ══ CP ══ PLC Coupling ══ QCA7005 ══ SPI ══ STM32
                                                   │
                                              ┌────┴────┐
                                              │ lwIP    │ ← Ethernet frames
                                              │ TCP/IP  │ ← TCP socket
                                              │ TLS 1.2 │ ← šifrirano
                                              │ V2GTP   │ ← V2G Transfer Protocol
                                              │ EXI     │ ← ISO 15118-2 sporočila
                                              └─────────┘
```

Zaporedje:
1. QCA7005 prejme PLC signal → sestavi Ethernet frame → sproži SPI interrupt
2. STM32 prebere frame prek SPI → posreduje lwIP stacku
3. lwIP sestavi TCP paket → posreduje TLS sloju
4. mbedTLS dešifrira → posreduje V2GTP
5. V2GTP razpakira → EXI dekodira → aplikacijski sloj ISO 15118

---

## 6. Napajanje

### 6.1 Napetostni nivoji

| Rail | Napetost | Porabnik | Max tok | Regulator |
|------|---------|---------|---------|-----------|
| 12V_IN | 12V nom. (9-16V) | Vhod iz avtomobila | 1.5A | Zaščita + fuse |
| 5V | 5.0V | CAN transceiver | 200 mA | TPS54331 (buck) |
| 3V3 | 3.3V | STM32, QCA7005 I/O, Flash, CP/PP | 800 mA | TLV1117-33 (LDO iz 5V) |
| 1V8 | 1.8V | QCA7005 jedro | 300 mA | AP2112K-1.8 (LDO iz 3.3V) |
| VDDA | 3.3V | STM32 ADC referenca | 10 mA | Ferrite + LC filter iz 3V3 |

### 6.2 Celoten napajalni diagram

```
                                    Fuse        TVS
12V_BATT ──┤ P-FET ├──── 2A ────┤SMBJ18CA├──── 12V_PROT
            (obratna               polyfuse
             polariteta)

12V_PROT ──── [TPS54331 Buck] ──── 5V ──── CAN Transceiver (TJA1443)
                  │                  │
                  │            C_in: 10uF/25V
                  │            C_out: 22uF/10V + 100nF
                  │            L: 22uH shielded
                  │
                  └──── [TLV1117-33] ──── 3V3 ──┬── STM32H753 (VDD)
                            │                    ├── QCA7005 (VDD33)
                       C_in: 10uF               ├── SPI Flash (VCC)
                       C_out: 22uF + 100nF       ├── CP/PP vezje
                                                 │
                                                 └── [AP2112K-1.8] ── 1V8 ── QCA7005 (VDD18)
                                                          │
                                                     C_out: 1uF + 100nF

3V3 ──── [Ferrite Bead 600Ω@100MHz] ──── VDDA ──── STM32 VDDA pin
              │                                │
         100nF + 1uF                      100nF + 1uF
              │                                │
             GND                              GND
```

### 6.3 Power budget

| Porabnik | Napetost | Tipični tok | Max tok |
|----------|---------|-------------|---------|
| STM32H753 (480 MHz, crypto active) | 3.3V | 200 mA | 350 mA |
| QCA7005 I/O | 3.3V | 100 mA | 200 mA |
| QCA7005 Core | 1.8V | 150 mA | 250 mA |
| SPI Flash (active) | 3.3V | 15 mA | 25 mA |
| CAN Transceiver (TJA1443) | 5V | 50 mA | 70 mA |
| CP/PP vezje, LEDi | 3.3V | 30 mA | 50 mA |
| **Skupaj iz 12V** | | **~700 mA** | **~1.2 A** |

### 6.4 Power sequencing

```
Zagon:
1. 12V → 5V (buck, ~2 ms startup)
2. 5V → 3.3V (LDO, ~1 ms)
3. 3.3V → 1.8V (LDO, ~1 ms)
4. STM32 reset released (RC delay ~10 ms po stabilni 3.3V)
5. STM32 boot → inicializacija → QCA7005 reset release
6. QCA7005 boot iz SPI Flash (~500 ms)
7. Sistem pripravljen

Ugašanje: obratni vrstni red ni kritičen (regulatorji padejo sami).
```

---

## 7. PLC sklopno vezje (CP linija)

### 7.1 Pregled

PLC komunikacija poteka na Control Pilot liniji (frekvence 2-28 MHz). Sklopno vezje mora:
- Prenašati PLC signal (2-28 MHz) med QCA7005 in CP linijo
- Blokirati DC in nizko-frekvenčni 1 kHz PWM signal
- Ščititi QCA7005 pred ±12V na CP liniji
- Zagotavljati galvansko ločitev (transformator)

### 7.2 PLC Coupling Transformer

Za lasten dizajn imaš dve opciji:

**Opcija A: Standardni HomePlug GreenPHY coupling transformer**

Primerni transformatorji za HomePlug GreenPHY na CP liniji:

| Part | Proizvajalec | Turns Ratio | Impedanca | Frek. | Opis |
|------|-------------|-------------|-----------|-------|------|
| 750342070 | Würth Elektronik | 1:1 | 50/75 Ohm | 2-30 MHz | LAN/PLC transformator |
| HX1188FNL | Pulse Electronics | 1:1 | 100 Ohm | 1-30 MHz | Ethernet transformer |
| H5007FNL | Pulse Electronics | 1:1 | variable | 2-30 MHz | PLC coupling |

**Opcija B: Chargebyte I2PLCTR-2** (če ga naročiš)

Priporočilo: Uporabi standardni Würth 750342070 ali Pulse H5007FNL. Za certificiranje moraš testirati EMC in PLC doseg z izbranim transformatorjem.

### 7.3 PLC Coupling Circuit

```
              QCA7005                  PLC Transformer              Control Pilot
                                      (750342070 ali podoben)       (od CCS inlet)
                                    ┌─────────────────┐
  TX_P (pin) ──── [100R] ────┬─────│ 1 (TX+)  4 (CP+)│─────┬──── [10R] ──── CP Line
                              │     │                  │     │
  TX_N (pin) ──── [100R] ────┤     │ 2 (TX-)  5 (CT) │     │
                              │     │                  │     │
                         C: 100nF   │ 3 (RX+)  6 (CP-)│     ├──── [220R] ──── L: 220uH ──── CP node
                              │     │                  │     │                                  │
                             GND    │ 8 (RX-)  7 (GND)│     │                              C: 1nF
                                    └─────────────────┘     │                                  │
  RX_P (pin) ◄───────────────────── Pin 3                   │                                 GND
  RX_N (pin) ◄───────────────────── Pin 8                   │
                                                            │
                                              ESD: PESD12TVL1BA,115
                                              TVS: SMAJ12CA
                                                            │
                                                           GND
```

### 7.4 Podrobnosti PLC coupling

| Ref | Vrednost | Namen |
|-----|---------|-------|
| R_TX (2x) | 100 Ohm | TX impedančno prilagajanje |
| R_CP | 10 Ohm | Serijsko dušenje CP signala |
| R_PLC | 220 Ohm | PLC coupling serijsko |
| L_PLC | 220 uH (shielded) | PLC/CP frekvenčna ločitev |
| C_PLC | 1 nF (C0G) | PLC AC coupling na CP |
| C_TX | 100 nF (X7R) | TX DC blocking |
| TVS | SMAJ12CA | ±12V transient zaščita |
| ESD | PESD12TVL1BA,115 | ESD zaščita CP linije |

### 7.5 Impedančno prilagajanje

HomePlug GreenPHY deluje na 50/100 Ohm impedanci. Coupling mora zagotoviti:
- **TX stran**: 100 Ohm diferencialna impedanca (2x 100R → 200R, transformer ratio 1:1 → 100R na CP)
- **RX stran**: Transformator pretvori CP signal v diferencialni signal za QCA7005
- CP linija ima impedanco ~150 Ohm tipično (odvisno od kabla)

---

## 8. Control Pilot vezje (IEC 61851)

### 8.1 CP signalizacija

EVSE generira ±12V PWM signal na 1 kHz. EV bremeni CP linijo z uporom, ki določa stanje:

| Stanje | +V (brez bremena) | +V (z bremenom) | R (EV stran) | Pomen |
|--------|-------------------|-----------------|-------------|-------|
| A | +12V | +12V | Odprto | Ni priključen |
| B | +9V | +9V | 2.74 kOhm | Priključen, ni pripravljen |
| C | +6V | +6V | 882 Ohm | Pripravljen, polnjenje dovoljeno |
| D | +3V | +3V | 246 Ohm | Z ventilacijo |
| E | 0V | 0V | Kratek stik | Napaka |

### 8.2 CP Edge Detection (za MCU Timer Input Capture)

```
CP Line ──── [10R] ──┬──── [10k] ──── 3.3V (pull-up)
                      │
                      ├──── D3: 1N4148 (katoda → CP) ──┬── R4: 2.7k ──┐
                      │     (pozitivna pol-perioda)     │              │
                      │                                  D4: BAT54     ├── → STM32 PA0
                      │                                  │              │     (TIM2_CH1
                      │                                  R5: 1.3k      │      Input Capture)
                      │                                  │              │
                      │                                 GND          C_filt: 100pF
                      │                                                │
                 C1: 1nF                                              GND
                      │
                     GND
                      │
                 D2: PESD12TVL1BA (ESD)
                      │
                     GND
```

Napetostni delilnik R4/R5 skalira +12V CP na ~1.1V, +9V na ~0.86V, +6V na ~0.57V → varno za 3.3V MCU vhod.

STM32 TIM2 Input Capture meri:
- **Periodo** (mora biti ~1 ms = 1 kHz)
- **Duty cycle** (5% do 100% v korakih po 1%)
- Duty cycle kodira max dovoljeni AC tok (IEC 61851-1 Annex A)

### 8.3 CP State C Switching

```
STM32 PC0 (GPIO) ──── [1k] ──── Gate ┐
                                       │ Q1: 2N7002 (N-MOSFET)
                                Source ┤ (logic level, Vgs(th) < 2V)
                                 GND   │
                                       │ Drain
                                       │
                                 R_STATE_C: 1.3 kOhm (za state B→C)
                                       │
                                  CP Line node
```

Ko MCU postavi PC0 = HIGH → Q1 ON → 1.3 kOhm bremeni CP → napetost pade iz +9V na +6V → State C.

Za State D (3V): dodaj vzporedni upor 270 Ohm (skupno 246 Ohm), krmiljeno z dodatnim MOSFET.

### 8.4 CP napetostno merjenje (opcijsko, za diagnostiko)

```
CP Line ──── [100k] ──┬──── [33k] ──── GND
                       │
                       └──── → STM32 PA4 (ADC1_IN4)
                              (0-3.3V iz ±12V)
                              + zaščitni diodi na 3.3V in GND
```

---

## 9. Proximity Pilot vezje

### 9.1 PP ADC meritev

```
                                    3.3V
                                     │
                                   R_PU: 1 kOhm (pull-up)
                                     │
PP Pin ──── [220R] ──┬──── [220R] ──┬──── → STM32 PA3 (ADC1_IN3)
(CCS inlet)          │              │
                     │            C_PP: 100nF (filter)
                   L: 220uH        │
                     │             GND
                   C: 470pF
                     │
                   TVS: SMAJ5.0A
                     │
                    GND
```

### 9.2 ADC vrednosti za PP upore

STM32 ADC 16-bit, VREF = 3.3V, z R_PU = 1 kOhm pull-up:

| PP upor | Pričakovana napetost | ADC vrednost (16-bit) | Stanje |
|---------|---------------------|-----------------------|--------|
| Open | 3.3V | ~65535 | Ni priključen |
| 1.5 kOhm | 1.98V | ~39321 | 13A kabel |
| 680 Ohm | 1.34V | ~26542 | 20A kabel |
| 220 Ohm | 0.60V | ~11780 | 32A kabel |
| 100 Ohm | 0.30V | ~5957 | 63A kabel |

Firmware mora uporabiti histerezne pragove z ±10% toleranco.

---

## 10. CAN bus vmesnik

### 10.1 CAN transceiver

Za certificiranje uporabi **avtomobilski CAN transceiver**:

| Part | Proizvajalec | Napajanje | Posebnosti |
|------|-------------|-----------|------------|
| **TJA1443AT** | NXP | 5V | AEC-Q100, CAN FD, low power standby |
| TJA1463 | NXP | 5V | CAN FD + SIC (Signal Improvement) |
| MCP2562FD | Microchip | 3.3V/5V | CAN FD, VIO pin za 3.3V logiko |
| TCAN1044V | TI | 5V | AEC-Q100, CAN FD |

**Priporočilo: TJA1443AT** - NXP avtomobilski standard, AEC-Q100, CAN FD.

### 10.2 CAN vezje

```
              TJA1443AT/3
         ┌──────────────────┐
         │ 1 TXD    VCC  8  │──── 5V ──── 100nF ──── GND
STM32 ───│ 2 GND    CANH 7  │──── [PESD2CAN] ──┬──── CAN_H (konektor)
PD1 ────►│ 3 VIO    CANL 6  │──── [PESD2CAN] ──┤──── CAN_L (konektor)
STM32 ◄──│ 4 RXD    STB  5  │──── GND           │
PD0      └──────────────────┘                    │
              │                            [120R] (terminacija)
              │                                  │
         VIO: 3.3V ── 100nF ── GND        CAN_H ─┘
```

**VIO pin** (TJA1443): Poveži na 3.3V → logični nivoji na TXD/RXD so 3.3V kompatibilni s STM32.

### 10.3 CAN parametri

| Parameter | Vrednost |
|-----------|---------|
| Baud rate | 500 kbit/s (standard CAN) |
| Frame format | Extended (29-bit ID) |
| Terminacija | 120 Ohm na vsakem koncu busa |
| ESD zaščita | PESD2CAN ali NUP2105L |
| Common mode choke | ACM2012-102 (opcijsko, za EMC) |

### 10.4 CAN ID-ji

Modul komunicira z VCU po istem DBC protokolu kot originalni Chargebyte CMS:

**Modul → VCU:**
| ID (ext) | Ime | Cikel |
|----------|-----|-------|
| 0x0600 | ChargeInfo | 100 ms |
| 0x1400 | EVSEDCMaxLimits | 100 ms |
| 0x1401 | EVSEDCRegulationLimits | 100 ms |
| 0x1402 | EVSEDCStatus | 100 ms |
| 0x2001 | SoftwareInfo | 10 s |
| 0x2002 | ErrorCodes | 1 s |
| 0x2003 | SLACInfo | Event |

**VCU → Modul:**
| ID (ext) | Ime | Cikel |
|----------|-----|-------|
| 0x1300 | EVDCMaxLimits | 100 ms |
| 0x1301 | EVDCChargeTargets | 100 ms |
| 0x1302 | EVStatusControl | 100 ms |
| 0x1303 | EVStatusDisplay | 100 ms |
| 0x1304 | EVPlugStatus | 100 ms |
| 0x1305 | EVDCEnergyLimits | 100 ms |

---

## 11. Kristalni oscilatorji

### 11.1 STM32H753 - 8 MHz HSE

```
PH0 (OSC_IN) ──┬──── XTAL 8 MHz ────┬──── PH1 (OSC_OUT)
                │    (±20 ppm, 10pF) │
              C_L1: 22pF           C_L2: 22pF
                │                    │
               GND                  GND
```

| Parameter | Vrednost |
|-----------|---------|
| Frekvenca | 8 MHz |
| Toleranca | ±20 ppm (za USB ne potrebujemo, ±30 ppm OK) |
| Load capacitance | 10 pF → C_L = 22 pF (z upoštevanjem PCB stray ~3pF) |
| Package | HC49/SMD (3.2x2.5mm ali 5x3.2mm) |
| Priporočen del | ABM8-8.000MHZ-10-1-U-T (Abracon) |

STM32 PLL: 8 MHz HSE → PLL → 480 MHz SYSCLK

### 11.2 QCA7005 - 25 MHz

```
XI ──┬──── XTAL 25 MHz ────┬──── XO
     │    (±25 ppm, 8pF)   │
   C_L1: 15pF            C_L2: 15pF
     │                     │
    GND                   GND
```

| Parameter | Vrednost |
|-----------|---------|
| Frekvenca | 25 MHz |
| Toleranca | ±25 ppm |
| Load capacitance | 8 pF → C_L = 15 pF |
| Package | HC49/SMD (5x3.2mm) |
| Priporočen del | ABM8-25.000MHZ-B2-T (Abracon) |

**POMEMBNO**: 25 MHz kristal mora biti čim bližje QCA7005 (< 10 mm). Dolge povezave povzročijo nestabilno PLC komunikacijo.

---

## 12. Reset in Boot vezje

### 12.1 STM32 Reset

```
3.3V ──── [10k] ──┬──── NRST (STM32 pin 14)
                   │
                 [100nF]
                   │
                  GND
                   │
            [Tipka RESET] (opcijsko, za razvoj)
                   │
                  GND
```

### 12.2 STM32 Boot

```
BOOT0 (pin 94) ──── [10k] ──── GND    (normalni boot iz Flash)
```

Za programiranje prek DFU (USB bootloader): BOOT0 → 3.3V (jumper).

### 12.3 QCA7005 Reset

```
STM32 PB1 ──── [33R] ──┬──── QCA7005 RESET_N
                        │
                      [10k] ── 3.3V (pull-up)
                        │
                      [100nF] ── GND (debounce)
```

Reset sekvenca (firmware):
1. PB1 = LOW (QCA7005 v resetu)
2. Počakaj 10 ms
3. PB1 = HIGH (sprosti reset)
4. Počakaj 500 ms (QCA7005 boot iz SPI Flash)
5. Preberi SPI Signature register (mora biti 0xAA55)

---

## 13. JTAG/SWD debug

### 13.1 SWD konektor (10-pin Cortex Debug)

```
Pin  Signal        STM32 Pin
──────────────────────────────
1    VCC (3.3V)    -
2    SWDIO         PA13
3    GND           -
4    SWCLK         PA14
5    GND           -
6    SWO           PB3 (opcijsko)
7    KEY           -
8    NC            -
9    GND           -
10   NRST          Pin 14
```

Uporabi 10-pin 1.27mm pitch header (ARM standard).
Debugger: ST-Link V3, J-Link, ali CMSIS-DAP.

---

## 14. Flash pomnilnik za QCA7005

### 14.1 SPI NOR Flash

QCA7005 potrebuje zunanji SPI Flash za firmware:

| Part | Velikost | Package | Opis |
|------|---------|---------|------|
| **W25Q32JVSSIQ** | 4 MB (32 Mbit) | SOIC-8 (208mil) | Winbond, AEC-Q100, -40..+125°C |
| W25Q16JVSSIQ | 2 MB (16 Mbit) | SOIC-8 | Dovolj za GreenPHY FW |
| IS25LP032D | 4 MB | SOIC-8 | ISSI alternativa |

**Priporočilo: W25Q32JVSSIQ** - 4 MB, AEC-Q100, dovolj prostora za firmware + PIB (Parameter Information Block).

### 14.2 Flash vezje

```
                W25Q32JVSSIQ
           ┌────────────────────┐
           │ 1 CS    VCC  8    │──── 3.3V ──── 100nF ──── GND
QCA FL_CS ──│ 2 DO    HOLD 7   │──── 3.3V (10k pull-up, disable hold)
QCA FL_DO ◄─│ 3 WP    CLK  6   │──── QCA FL_CLK
           │ 4 GND   DI   5   │──── QCA FL_DI
           │                    │
           └────────────────────┘
              │
             GND

WP (Write Protect): 3.3V (10k pull-up) → writeable
HOLD: 3.3V (10k pull-up) → not held
```

### 14.3 Firmware za QCA7005

QCA7005 firmware (PIB + NVM) se naloži iz flash ob zagonu:
- **PIB** (Parameter Information Block): MAC naslov, omrežni ključi, konfiguracija
- **NVM** (Non-Volatile Memory image): Dejanski firmware

Firmware je na voljo od Qualcomm/I2SE ali je del open-source open-plc-utils.
Za SLAC: moraš imeti SLAC-enabled firmware (ISO 15118-3 compliant).

---

## 15. Varnostna vezja

### 15.1 ESD zaščita na vseh zunanjih priključkih

| Linija | ESD zaščita | Part |
|--------|-----------|------|
| CP (Control Pilot) | Bidirectional TVS | PESD12TVL1BA,115 + SMAJ12CA |
| PP (Proximity Pilot) | TVS | SMAJ5.0A |
| CAN_H, CAN_L | Dual TVS | PESD2CAN |
| 12V vhod | TVS | SMBJ18CA |

### 15.2 Overvoltage zaščita

Vsi zunanji signali morajo biti zaščiteni pred prenapetostjo:
- CP linija: ±12V nominalno, do ±24V transient → TVS na ±15V
- Logični vhodi MCU: Schottky clamping diode na 3.3V in GND

### 15.3 Thermal shutdown

STM32H753 ima interni temperaturni senzor. Firmware naj:
- Bere temperaturo vsakih 10 s
- Pri > 100°C: zmanjšaj frekvence
- Pri > 115°C: ustavi polnjenje, obvesti VCU

### 15.4 Watchdog

Uporabi STM32 **IWDG** (Independent Watchdog):
- Timeout: 500 ms
- Firmware mora periodično resetirati watchdog
- Če se firmware obesi → avtomatski MCU reset

---

## 16. PCB layout smernice

### 16.1 Splošno

| Parameter | Vrednost |
|-----------|---------|
| Število slojev | **6-layer** (priporočeno za EMC certificiranje) |
| Stackup | Signal-GND-Signal-Power-GND-Signal |
| Debelina PCB | 1.6 mm |
| Cu teža | 1 oz (35 um) zunanje, 0.5 oz notranje |
| Min trace | 0.15 mm (6 mil) |
| Min clearance | 0.15 mm (6 mil) |
| Min via | 0.3 mm drill, 0.6 mm pad |
| Surface finish | ENIG (za reflow SMD) |
| Solder mask | Zelena, obe strani |
| Silkscreen | Bela, obe strani |

### 16.2 Slojni stackup (6-layer)

```
Layer 1 (Top):     Signali + komponente (QCA7005, STM32, pasivne)
Layer 2 (GND):     Neprekinjena GND ravnina (KRITIČNO!)
Layer 3 (Signal):  SPI signali, CAN, CP/PP
Layer 4 (Power):   3.3V, 1.8V, 5V power planes
Layer 5 (GND):     GND ravnina
Layer 6 (Bottom):  Signali + komponente (flash, konektorji, LEDi)
```

**KRITIČNO**: Sloj 2 mora biti **neprekinjen GND plane** brez razrezov pod:
- QCA7005 (return path za PLC signale)
- STM32 (return path za SPI)
- CAN transceiver (return path za diferencialni signal)

### 16.3 Razporeditev komponent

```
┌──────────────────────────────────────────────────┐
│  J_12V    [Buck]    [LDO 3.3]   [LDO 1.8]       │ ← Napajanje (zgornji rob)
│                                                   │
│  J_CAN   [CAN xcvr]                              │ ← CAN (levi rob)
│                                                   │
│              ┌────────────┐    ┌───────────┐      │
│              │  STM32H753 │    │  QCA7005  │      │ ← Srce modula (center)
│              │  (LQFP-100)│    │  (QFN-68) │      │
│              │  14x14mm   │    │   8x8mm   │      │
│              └────────────┘    └───────────┘      │
│                                    │              │
│              [SPI Flash]     [25MHz XTAL]         │ ← Flash ob QCA7005
│  [8MHz XTAL]                                      │
│                                                   │
│  J_SWD     ┌──────────────────┐                   │ ← PLC coupling (spodnji rob)
│            │  PLC Transformer │    J_CP   J_PP    │
│            │  + coupling      │                   │
│            └──────────────────┘                   │
│                                                   │
│  [LED] [LED] [LED]                    J_AUX       │ ← LEDi + debug
└──────────────────────────────────────────────────┘
       Pribl. 65 mm x 50 mm
```

### 16.4 Kritične povezave

**SPI (STM32 ↔ QCA7005):**
- Matched trace length (±2 mm)
- Trace width: 0.2 mm (50 Ohm single-ended)
- Referencirano na GND layer 2
- 33R series termination na vsakem signalu
- Dolžina: < 40 mm

**PLC (QCA7005 TX/RX ↔ transformer):**
- Diferencialni par (TX_P/TX_N, RX_P/RX_N)
- 100 Ohm diferencialna impedanca
- Trace width: 0.2 mm, gap: 0.15 mm
- Dolžina: < 15 mm (čim krajša!)
- Brez via-ov v PLC diferencialnemu paru

**25 MHz kristal:**
- Max 5 mm od QCA7005 XI/XO pinov
- GND guard ring okrog kristala
- Brez drugih signalov pod kristalom

**CAN bus:**
- Diferencialni par CAN_H/CAN_L
- 120 Ohm diferencialna impedanca (trace: 0.2mm, gap: 0.3mm)
- Min 5 mm od PLC sledi

### 16.5 Grounding

```
                    ┌─── Digital GND (STM32, QCA7005, Flash, CAN)
                    │
Enotna GND ────────┤─── Analog GND (ADC, CP/PP meritve)
ravnina             │    (povezi v eni točki pod STM32 VSSA)
                    │
                    └─── Power GND (regulatorji, bulk C)
```

**NE REŽI** GND ravnine! Uporabi enotno GND ravnino na Layer 2. Analogno ločitev doseži z ustrezno razporeditvijo komponent.

### 16.6 Dimenzije modula

Za nadomestek originalnega CMS modula (50.8 x 30.48 mm):
- To je pretesno za lasten dizajn z ločenimi čipi
- **Priporočena velikost: 65 x 50 mm** (ali 70 x 50 mm)
- Če mora biti majhno: 55 x 40 mm je minimum z 6-layer PCB

---

## 17. Celoten BOM

### 17.1 Aktivne komponente

| Qty | Ref | Part Number | Package | Opis | AEC-Q100 |
|-----|-----|-------------|---------|------|----------|
| 1 | U1 | STM32H753VIT6 | LQFP-100 | MCU Cortex-M7, HW crypto | Da |
| 1 | U2 | QCA7005-AL3B | QFN-68 | HomePlug GreenPHY PLC | - |
| 1 | U3 | TJA1443AT/3 | SOIC-8 | CAN FD transceiver | Da |
| 1 | U4 | W25Q32JVSSIQ | SOIC-8 | 4MB SPI NOR Flash | Da |
| 1 | U5 | TPS54331DR | SOIC-8 | 12V→5V buck (3A) | - |
| 1 | U6 | TLV1117-33IDCYR | SOT-223 | 5V→3.3V LDO (800mA) | - |
| 1 | U7 | AP2112K-1.8TRG1 | SOT-23-5 | 3.3V→1.8V LDO (600mA) | - |

### 17.2 Kristali

| Qty | Ref | Vrednost | Package | Za |
|-----|-----|---------|---------|-----|
| 1 | Y1 | 8 MHz, 10pF, ±20ppm | 3.2x2.5mm SMD | STM32 HSE |
| 1 | Y2 | 25 MHz, 8pF, ±25ppm | 5x3.2mm SMD | QCA7005 |

### 17.3 PLC Coupling

| Qty | Ref | Part | Package | Opis |
|-----|-----|------|---------|------|
| 1 | TR1 | 750342070 (Würth) | SMD | PLC coupling transformer |
| 1 | L_PLC | 220 uH, shielded | 5x5mm | PLC induktor |
| 1 | C_PLC | 1 nF, C0G, 50V | 0603 | PLC coupling kapacitor |
| 2 | R_TX | 100 Ohm, 1% | 0402 | TX impedančno prilagajanje |
| 1 | R_CP | 10 Ohm | 0603 | CP serijsko dušenje |
| 1 | R_PLC | 220 Ohm | 0603 | PLC coupling |
| 1 | C_TX | 100 nF, X7R | 0603 | TX DC blocking |

### 17.4 Control Pilot

| Qty | Ref | Vrednost | Package | Opis |
|-----|-----|---------|---------|------|
| 1 | R_CP2 | 10 Ohm | 0603 | CP serijski |
| 1 | R_PU | 10 kOhm | 0603 | CP pull-up |
| 1 | R_DIV1 | 2.7 kOhm, 1% | 0603 | Napetostni delilnik |
| 1 | R_DIV2 | 1.3 kOhm, 1% | 0603 | Napetostni delilnik |
| 1 | R_SC | 1.3 kOhm | 0603 | State C bremenski upor |
| 1 | R_GATE | 1 kOhm | 0603 | MOSFET gate |
| 1 | C_CP1 | 1 nF, C0G | 0603 | CP filter |
| 1 | C_CP2 | 470 pF, C0G | 0603 | Edge filter |
| 1 | C_CP3 | 100 pF, C0G | 0402 | HF filter |
| 1 | D_CP1 | 1N4148W | SOD-323 | Rectifier |
| 1 | D_CP2 | BAT54S | SOT-23 | Clamp |
| 1 | D_CP_TVS | SMAJ12CA | DO-214AC | TVS |
| 1 | D_CP_ESD | PESD12TVL1BA,115 | SOT-23 | ESD |
| 1 | Q1 | 2N7002 | SOT-23 | State C MOSFET |

### 17.5 Proximity Pilot

| Qty | Ref | Vrednost | Package | Opis |
|-----|-----|---------|---------|------|
| 1 | R_PP1 | 220 Ohm | 0603 | Serijski |
| 1 | R_PP2 | 220 Ohm | 0603 | Serijski |
| 1 | R_PP_PU | 1 kOhm | 0603 | Pull-up |
| 1 | L_PP | 220 uH, shielded | 5x5mm | PLC filter |
| 1 | C_PP1 | 470 pF, C0G | 0603 | PLC filter |
| 1 | C_PP2 | 100 nF | 0603 | ADC filter |
| 1 | D_PP_TVS | SMAJ5.0A | DO-214AC | TVS |

### 17.6 CAN bus

| Qty | Ref | Vrednost | Package | Opis |
|-----|-----|---------|---------|------|
| 1 | R_TERM | 120 Ohm, 1% | 0603 | Bus terminacija |
| 1 | D_CAN | PESD2CAN | SOT-23 | ESD zaščita |
| 1 | C_CAN | 100 nF | 0603 | Transceiver decouple |
| 1 | C_VIO | 100 nF | 0603 | VIO decouple |

### 17.7 Napajanje

| Qty | Ref | Vrednost | Package | Opis |
|-----|-----|---------|---------|------|
| 1 | Q_REV | SI4435DDY | SO-8 | Obratna polariteta (P-FET) |
| 1 | D_TVS_12V | SMBJ18CA | SMB | 12V TVS |
| 1 | F1 | 2A polyfuse | 1812 | Pretokovni |
| 1 | L_BUCK | 22 uH, 3A, shielded | 6.3x6.3mm | Buck induktor |
| 2 | C_12V | 10 uF / 25V | 0805, X7R | Buck vhod |
| 2 | C_5V | 22 uF / 10V | 0805, X5R | Buck izhod |
| 2 | C_3V3_BULK | 22 uF / 10V | 0805, X5R | LDO 3.3V izhod |
| 1 | C_1V8_BULK | 1 uF / 10V | 0603, X7R | LDO 1.8V izhod |
| 1 | FB1 | Ferrite 600Ω@100MHz | 0603 | VDDA filter |

### 17.8 Decoupling (skupaj)

| Qty | Ref | Vrednost | Package | Za |
|-----|-----|---------|---------|-----|
| 12 | C_DEC_100n | 100 nF, X7R | 0402 | STM32 VDD (8x) + QCA7005 VDD33 (4x) |
| 4 | C_DEC_10u | 10 uF, X5R | 0805 | STM32 bulk (2x) + QCA7005 bulk (2x) |
| 2 | C_DEC_1u | 1 uF, X7R | 0603 | VDDA + VDD18 |
| 2 | C_DEC_HSE | 22 pF, C0G | 0402 | 8 MHz kristal |
| 2 | C_DEC_25M | 15 pF, C0G | 0402 | 25 MHz kristal |

### 17.9 Konektorji

| Qty | Ref | Tip | Opis |
|-----|-----|-----|------|
| 1 | J1 | Molex Micro-Fit 3.0, 2-pin | 12V vhod |
| 1 | J2 | JST-PH, 2-pin | Control Pilot |
| 1 | J3 | JST-PH, 2-pin | Proximity Pilot |
| 1 | J4 | Molex Micro-Fit 3.0, 4-pin | CAN bus (H, L, 12V, GND) |
| 1 | J5 | 2x5 header, 1.27mm | SWD debug |
| 1 | J6 | JST-PH, 3-pin | UART debug (TX, RX, GND) |

### 17.10 LED indikatorji

| Qty | Ref | Barva | Opis |
|-----|-----|-------|------|
| 1 | LED1 | Zelena | Power ON |
| 1 | LED2 | Modra | PLC Link aktiven |
| 1 | LED3 | Zelena | Polnjenje aktivno |
| 1 | LED4 | Rdeča | Napaka |
| 4 | R_LED | 1 kOhm | 0603, LED serijski upori |

---

## 18. Software arhitektura

### 18.1 RTOS in middleware

| Komponenta | Priporočena rešitev | Licenca | Opis |
|-----------|---------------------|---------|------|
| **RTOS** | FreeRTOS | MIT | Real-time scheduling, task management |
| **TCP/IP** | lwIP 2.x | BSD | Lightweight TCP/IP stack |
| **TLS** | mbedTLS 3.x | Apache 2.0 | TLS 1.2, X.509, HW crypto integration |
| **EXI codec** | OpenV2G ali cbExiGen | GPL / Commercial | EXI kodiranje za ISO 15118 / DIN 70121 |
| **V2G stack** | Lasten ali EVerest port | - | ISO 15118-2 / DIN 70121 message handling |
| **QCA7005 driver** | qcaspi (Linux port) | GPL | SPI Ethernet driver za QCA7005 |
| **CAN** | STM32 HAL FDCAN | ST | CAN driver |
| **HAL** | STM32CubeH7 | BSD | Hardware abstraction |

### 18.2 FreeRTOS Task struktura

```
┌─────────────────────────────────────────────────┐
│ FreeRTOS Tasks                                   │
├──────────────┬──────────┬───────────────────────┤
│ Task         │ Priorita │ Opis                  │
├──────────────┼──────────┼───────────────────────┤
│ QCA_SPI_Task │ Highest  │ SPI DMA, Eth frame    │
│              │          │ TX/RX z QCA7005       │
├──────────────┼──────────┼───────────────────────┤
│ lwIP_Task    │ High     │ TCP/IP processing     │
├──────────────┼──────────┼───────────────────────┤
│ V2G_Task     │ High     │ ISO 15118 / DIN 70121 │
│              │          │ state machine +       │
│              │          │ EXI encode/decode     │
├──────────────┼──────────┼───────────────────────┤
│ TLS_Task     │ Normal   │ mbedTLS handshake +   │
│              │          │ encrypt/decrypt       │
├──────────────┼──────────┼───────────────────────┤
│ CAN_Task     │ Normal   │ CAN TX/RX z VCU,     │
│              │          │ DBC signal encoding   │
├──────────────┼──────────┼───────────────────────┤
│ CP_PP_Task   │ Normal   │ CP PWM merjenje,      │
│              │          │ PP ADC branje,        │
│              │          │ State B/C krmiljenje  │
├──────────────┼──────────┼───────────────────────┤
│ Safety_Task  │ Highest  │ Watchdog, heartbeat,  │
│              │          │ temp monitoring       │
├──────────────┼──────────┼───────────────────────┤
│ LED_Task     │ Lowest   │ Status LED krmiljenje │
└──────────────┴──────────┴───────────────────────┘
```

### 18.3 ISO 15118-2 State Machine

```
                     ┌──────────────┐
                     │   IDLE       │ CP = State A (no vehicle)
                     └──────┬───────┘
                            │ CP = State B (vehicle detected)
                     ┌──────▼───────┐
                     │   SLAC       │ QCA7005 SLAC process
                     └──────┬───────┘
                            │ SLAC matched, PLC link up
                     ┌──────▼───────┐
                     │   SDP        │ SECC Discovery Protocol (UDP)
                     └──────┬───────┘
                            │ SECC found, TCP connect
                     ┌──────▼───────┐
                     │   TLS        │ TLS 1.2 Handshake (ISO) / skip (DIN)
                     └──────┬───────┘
                            │ Secure channel established
                     ┌──────▼───────┐
                     │ SessionSetup │ → SessionSetupRes
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ ServiceDisc  │ → ServiceDiscoveryRes
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ PaymentSel   │ → PaymentServiceSelectionRes
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ Authorization│ → AuthorizationRes (EIM ali PnC)
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ ChargeParam  │ → ChargeParameterDiscoveryRes
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ CableCheck   │ → CableCheckRes (izolacijski test)
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ PreCharge    │ → PreChargeRes (napetost match)
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ PowerDelivery│ → PowerDeliveryRes (START)
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ CurrentDemand│ → CurrentDemandRes (loop)
                     └──────┬───────┘
                            │ Charging complete / stop
                     ┌──────▼───────┐
                     │ PowerDelivery│ → PowerDeliveryRes (STOP)
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ WeldingDet   │ → WeldingDetectionRes
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │ SessionStop  │ → SessionStopRes
                     └──────┬───────┘
                     ┌──────▼───────┐
                     │   IDLE       │
                     └──────────────┘
```

### 18.4 mbedTLS HW Crypto konfiguracija

STM32H753 ima hardverske kriptografske pospeševalnike. V `mbedtls_config.h`:

```
Omogoči HW akceleratorje:
- MBEDTLS_AES_ALT          → STM32 CRYP periferija (AES-128/256-CBC/GCM)
- MBEDTLS_SHA256_ALT       → STM32 HASH periferija
- MBEDTLS_SHA1_ALT         → STM32 HASH periferija
- MBEDTLS_GCM_ALT          → STM32 CRYP (AES-GCM za TLS 1.2)
- MBEDTLS_ECDSA_VERIFY_ALT → STM32 PKA (ECC verifikacija)
- MBEDTLS_ECDH_COMPUTE_SHARED_ALT → STM32 PKA

Potrebni TLS 1.2 cipher suites za ISO 15118:
- TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256  (obvezno)
- TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256   (priporočeno)

Potrebni certifikati:
- Root CA (V2G Root CA)
- Sub CA 1 + Sub CA 2
- Contract Certificate (za Plug & Charge)
- OEM Provisioning Certificate

Shranjeni v STM32 internal flash (zaščitena regija) ali secure element.
```

### 18.5 QCA7005 SPI Driver (qcaspi)

Jedro driverja za QCA7005 prek SPI:

```
Inicializacija:
1. Reset QCA7005 (PB1 LOW → HIGH)
2. Počakaj boot (500 ms)
3. Preberi SPI Signature (0x1A00) → mora biti 0xAA55
4. Konfiguriraj SPI registre (buffer size, interrupts)
5. Omogoči RX interrupt

TX (pošlji Ethernet frame):
1. Preveri WRBUF_SPC_AVA register (dovolj prostora?)
2. Zapiši Ethernet frame v SPI write buffer (burst write)
3. QCA7005 ga pošlje prek PLC

RX (prejmi Ethernet frame):
1. QCA7005 sproži INT (active low)
2. Preberi RDBUF_BYTE_AVA register (koliko podatkov?)
3. Preberi Ethernet frame iz SPI read buffer (burst read)
4. Posreduj lwIP stacku (netif→input)
```

---

## 19. Certifikacijske zahteve

### 19.1 Standardi

| Standard | Opis | Zahteva |
|----------|------|---------|
| **ISO 15118-2** | V2G Communication (Application layer) | Sporočila, state machine, EXI |
| **ISO 15118-3** | V2G Physical/Data Link (PLC, SLAC) | HomePlug GreenPHY, SLAC |
| **DIN 70121** | DC charging (Nemška pred-norma) | Poenostavljen protokol, brez TLS |
| **IEC 61851-1** | EV charging - General | CP/PP signalizacija, varnost |
| **IEC 61851-23** | DC charging specific | DC-specific zahteve |
| **ISO 15118-4/-5** | Conformance tests | Testni primeri za -2 in -3 |
| **EMC: CISPR 25** | Automotive EMC | Emisije in imunost |
| **EMC: ISO 11452** | Automotive immunity | RF imunost |
| **ESD: IEC 61000-4-2** | ESD immunity | ±8kV kontakt, ±15kV air |
| **AEC-Q100** | IC qualification | Temperaturni testi, zanesljivost |

### 19.2 Testna oprema za certificiranje

| Oprema | Proizvajalec | Namen |
|--------|-------------|-------|
| **CCS Charge Controller Tester** | Comemso | ISO 15118 conformance testing |
| **VT System** | Vector | V2G Protocol testing |
| **CANoe** | Vector | CAN bus analiza + simulacija |
| **KPIT EvTESTer** | KPIT | ISO 15118 interoperability |
| **Keysight E36200** | Keysight | DC power supply za simulacijo |
| **SECC Simulator** | Various | Simulira polnilno postajo |

### 19.3 Conformance test categories (ISO 15118-4/-5)

1. **Physical layer** (ISO 15118-5): PLC signal levels, modulation, SLAC timing
2. **Data link layer** (ISO 15118-5): HomePlug GreenPHY MAC behavior
3. **Network/Transport** (ISO 15118-4): SDP, TCP connection handling
4. **Security** (ISO 15118-4): TLS handshake, certificate validation
5. **Application** (ISO 15118-4): V2G message sequence, timing, content
6. **Interoperability**: Test z dejanskimi polnilnicami (CharIN testivals)

### 19.4 EMC design pravila za certificiranje

- 6-layer PCB (neprekinjena GND ravnina)
- TVS/ESD zaščita na vseh zunanjih vmesnikih
- Common-mode choke na CAN bus
- Ferrite bead na napajanju
- No split ground planes
- Matched differential pairs (CAN, PLC)
- Shielded inductors (ne open-core)
- Decoupling na vsakem IC power pinu
- Kratek crystal loop (< 10 mm)

---

## 20. Testiranje in zagon

### 20.1 Faza 1: Bare board test

1. Vizualna inspekcija (AOI če je možno)
2. Kratki stik test (VDD-GND na vseh regulatorjih)
3. Pripni 12V → preveri 5V, 3.3V, 1.8V (multimeter)
4. Preveri tokovno porabo brez MCU/QCA (< 10 mA)

### 20.2 Faza 2: MCU zagon

1. Priklopi ST-Link na SWD konektor
2. Detektiraj STM32H753 (STM32CubeProgrammer)
3. Naloži test firmware (LED blink)
4. Preveri vse GPIO pine z osciloskopom
5. Preveri SPI bus (SCK, MOSI, MISO) → logic analyzer
6. Preveri FDCAN TX/RX → CAN analyzer

### 20.3 Faza 3: QCA7005 zagon

1. MCU sprosti QCA7005 iz reseta
2. Preberi SPI Signature register → mora biti 0xAA55
3. Naloži PIB/NVM v SPI Flash (ali prek host SPI)
4. QCA7005 boot → preveri da PLC modem oddaja
5. Test z drugim HomePlug GreenPHY modulom (loopback test)

### 20.4 Faza 4: SLAC test

1. Poveži CP linijo na CCS simulator (ali drugo EVSE z PLC)
2. Sproži SLAC proceduro
3. Monitor: SLAC state (Unmatched → Matching → Matched)
4. Preveri PLC link quality (attenuation < 30 dB)

### 20.5 Faza 5: V2G komunikacija

1. Vzpostavi TCP povezavo prek PLC do EVSE simulatorja
2. Pošlji SDP (SECC Discovery)
3. TLS handshake (ISO 15118) ali plain TCP (DIN 70121)
4. Izvedi celoten SessionSetup → ... → SessionStop cikel
5. Preveri vse V2G sporočila z Wireshark (V2G dissector)

### 20.6 Faza 6: Polni sistem test

1. Poveži CAN bus na VCU
2. Preveri CAN komunikacijo (DBC sporočila)
3. Test s CCS simulatorjem (Comemso ali Vector)
4. Test z dejansko polnilno postajo (začni z nizko močjo)
5. Preveri celoten charging session od priključka do odkopa

---

## Appendix A: Orodja za razvoj

| Orodje | Namen |
|--------|-------|
| STM32CubeIDE | IDE za STM32 razvoj (GCC + debugger) |
| STM32CubeMX | Pin konfiguracija, clock tree, periferije |
| STM32CubeProgrammer | Flash programming, SWD |
| KiCad 8 | PCB schematic + layout (open source) |
| Altium Designer | PCB schematic + layout (komercialno) |
| Wireshark + V2G Plugin | Analiza V2G TCP prometa |
| CANdb++ / SavvyCAN | DBC editor, CAN analiza |
| Logic analyzer (Saleae) | SPI, I2C, UART debug |
| Osciloskop (4ch, 200MHz+) | CP/PP signali, PLC signali, power |

## Appendix B: Pomembni viri

| Vir | Opis |
|-----|------|
| QCA7005 Datasheet | Qualcomm Atheros (prek NDA ali I2SE GmbH) |
| QCA7000/7005 Reference Design | Application note za HW dizajn |
| open-plc-utils | Open source QCA7005 utilities (GitHub) |
| OpenV2G | Open source EXI codec za V2G (SourceForge) |
| ISO 15118-2:2014 | Standard dokument (kupiti pri ISO) |
| ISO 15118-3:2015 | PLC fizični sloj standard |
| IEC 61851-1 | EV charging CP/PP standard |
| STM32H753 Reference Manual (RM0433) | 3000+ strani, vse periferije |
| STM32H753 Datasheet | Pinout, električne specifikacije |
| AN5361 (STM32 + mbedTLS HW Crypto) | ST Application Note za HW crypto |
