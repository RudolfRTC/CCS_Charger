# CCS Charger - Charge Module S Controller

Desktop C++ / Qt 6 application that integrates with the Chargebyte **Charge Module S** (CMS) via CAN bus, enabling CCS DC charging control for a conventional battery pack.

## Architecture

```
src/
├── can/           # CAN abstraction: PCAN-Basic driver + simulated interface
│   ├── can_frame.h            # CanFrame struct, CanStatus enum
│   ├── can_interface.h/cpp    # Abstract CanInterface, SimulatedCanInterface
│   └── pcan_driver.h/cpp      # PCAN-Basic DLL wrapper (dynamic loading)
├── dbc/           # DBC parser and signal codec
│   ├── dbc_parser.h/cpp       # .dbc file parser (messages, signals, attributes, value tables)
│   └── signal_codec.h/cpp     # Encode/decode CAN frames ↔ physical values
├── module/        # Charge Module S protocol layer
│   ├── charge_module.h/cpp    # Main controller: cyclic TX, RX decode, parameter management
│   ├── state_machine.h/cpp    # CMS state enum, Control Pilot, EVSE status enums
│   └── safety_monitor.h/cpp   # Limits, heartbeat, timeouts, emergency stop, error codes
├── logging/       # Diagnostics and session tracking
│   ├── can_logger.h/cpp       # Raw CAN CSV + decoded signal CSV logging
│   └── session_report.h/cpp   # Session statistics (peak V/I/P, energy, SoC, duration)
├── ui/            # Qt Widgets UI (dark/neon "space-like" theme)
│   ├── mainwindow.h/cpp       # Top-level window, menu, status bar, wiring
│   ├── connection_widget.h/cpp# CAN channel selection, connect/disconnect, status LEDs
│   ├── dashboard_widget.h/cpp # Big value cards, state display, controls, fault list
│   ├── chart_widget.h/cpp     # Real-time V/I line chart (QtCharts)
│   ├── expert_widget.h/cpp    # Raw CAN frame table + decoded signal table
│   └── theme.h/cpp            # Color palette, global stylesheet
└── main.cpp
```

## Prerequisites

### Windows
1. **Qt 6.5+** with QtCharts module (install via Qt Online Installer)
2. **CMake 3.20+**
3. **MSVC 2022** or **MinGW 12+** (C++20 support required)
4. **PEAK PCAN-Basic** driver and DLL:
   - Download from: https://www.peak-system.com/PCAN-Basic.239.0.html
   - Install the PCAN device driver
   - Place `PCANBasic.dll` either in the app directory or in `C:\Windows\System32`

### Linux
1. **Qt 6.5+** with QtCharts: `sudo apt install qt6-base-dev qt6-charts-dev`
2. **CMake 3.20+**: `sudo apt install cmake`
3. **GCC 12+** or **Clang 15+** (C++20)
4. **PCAN driver** (optional): Install `peak-linux-driver` and `libpcanbasic`
   - Or use Simulation Mode (no hardware needed)

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64   # adjust for your Qt install
cmake --build . --parallel
```

### Windows (Visual Studio)
```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/msvc2022_64
cmake --build . --config Release
```

## Run

```bash
# Copy the DBC file next to the executable (or run from the project root)
cp ISC_CMS_Automotive.dbc build/

cd build
./CCSCharger
```

The application starts in **Simulation Mode** by default (no PCAN hardware needed). Uncheck "Simulation Mode" to use a real PCAN-USB adapter.

## Usage

### 1. Connect
- Select CAN channel and baud rate (default: 500 kbit/s per CMS datasheet)
- Click **Connect**
- Status LED turns green, heartbeat indicator shows AliveCounter activity

### 2. Configure EV Parameters
- Set **Target Voltage**, **Target Current**, **Max Voltage**, **Max Current** in the Parameters panel
- Set **Battery SoC** (0-100%)
- Set **PreCharge Voltage** (battery's present voltage)

### 3. Start Charging
- Click **START CHARGING** — sets EVReady=True, ChargeStopIndication=NoStop
- CMS state machine progresses: Init → Auth → Parameter → Isolation → PreCharge → Charge
- During PreCharge, target current is automatically limited to 2A per datasheet
- When VoltageMatch is True, set ChargeProgressIndication=Start to enter Charge state

### 4. Stop Charging
- Click **STOP CHARGING** — sets ChargeProgressIndication=Stop, ChargeStopIndication=Terminate
- CMS progresses through StopCharge → Welding → SessionStop → ShutOff

### 5. Emergency Stop
- Click **EMERGENCY STOP** — immediately sets EVReady=False, sends disable/stop per spec
- All outgoing messages revert to safe state on next cycle (100ms)

### 6. Logging
- **File → Start Raw CAN Log** — saves timestamped CAN frames to CSV
- **File → Start Decoded Signal Log** — saves decoded signal values to CSV
- **File → Save Session Report** — generates summary with peak V/I/P, energy estimate, SoC delta

## CAN Message Schedule

All VCU → CMS messages are sent at **100ms** cycle per DBC `GenMsgCycleTime`:

| CAN ID (ext) | Message | Direction |
|---|---|---|
| 0x1300 | EVDCMaxLimits | VCU → CMS |
| 0x1301 | EVDCChargeTargets | VCU → CMS |
| 0x1302 | EVStatusControl | VCU → CMS |
| 0x1303 | EVStatusDisplay | VCU → CMS |
| 0x1304 | EVPlugStatus | VCU → CMS |
| 0x1305 | EVDCEnergyLimits | VCU → CMS |
| 0x0600 | ChargeInfo | CMS → VCU |
| 0x1400 | EVSEDCMaxLimits | CMS → VCU |
| 0x1401 | EVSEDCRegulationLimits | CMS → VCU |
| 0x1402 | EVSEDCStatus | CMS → VCU |
| 0x2001 | SoftwareInfo | CMS → VCU (10s) |
| 0x2002 | ErrorCodes | CMS → VCU (1s) |

Module reset: Standard CAN ID **0x667**, payload `[0xFF, 0x00]`.

## Safety Features

- **Hard limit clamping**: All setpoints clamped to DBC-defined ranges before encoding
- **Heartbeat monitoring**: AliveCounter checked every 100ms; alert if stale for >1500ms
- **CAN message timeout**: Alert if any expected message missing for >1000ms (per datasheet)
- **Emergency stop**: Immediately disables charging on button press, EVSE emergency, or timeout
- **Fail-safe default**: On any error/disconnect, EVReady=False, ChargeProgress=Stop, ChargeStop=Terminate
- **Error code display**: All CMS error codes decoded with descriptions and recommended actions

## Troubleshooting

| Problem | Solution |
|---|---|
| "Failed to load PCAN-Basic library" | Install PCAN-Basic driver; ensure DLL is in PATH or app directory |
| No PCAN channels found | Check USB connection; install PCAN device driver |
| CMS stays in Default state | Ensure all mandatory signals are non-SNA (EVMaxVoltage, EVMaxCurrent, EVReady, EVErrorCode, EVSoC) |
| LIMITS_MSG_TIMEOUT (0xA2) | VCU→CMS messages must arrive within 1000ms; check CAN connection |
| PreCharge timeout (0xDA) | PreCharge must complete within 7s; check precharge voltage vs EVSE voltage |
| DBC file not found | Place `ISC_CMS_Automotive.dbc` next to the executable or use File → Load DBC |

## Key Datasheet Constraints

- CAN bus: **500 kbit/s**, **Extended CAN IDs** (29-bit)
- CMS is the **EVCC** (EV Communication Controller); our app acts as **VCU** (Vehicle Control Unit)
- PreCharge target current is fixed at **2A** maximum
- VoltageMatch triggers when PreCharge voltage is within **±20V** of EVSE present voltage
- CAN message timeout is **1000ms** (10 consecutive missed 100ms messages)
- Module reset via standard frame **0x667** with payload **[0xFF, 0x00]**
