#include "can/can_interface.h"
#include <QDebug>
#include <cstring>

namespace ccs {

// ─── CanReaderThread ─────────────────────────────────────
// Note: The reader thread is designed for the PCAN polling approach.
// For SimulatedCanInterface, frames are injected directly via injectFrame().
void CanReaderThread::run()
{
    // This is used by PcanDriver's polling loop.
    // The PcanDriver overrides or uses this thread to poll CAN_Read in a loop.
    // For now, this is a stub - the actual polling is in PcanDriver.
    while (m_running) {
        msleep(1); // yield
    }
}

// ─── SimulatedCanInterface ───────────────────────────────
SimulatedCanInterface::SimulatedCanInterface(QObject* parent)
    : CanInterface(parent)
{
    m_simTimer = new QTimer(this);
    connect(m_simTimer, &QTimer::timeout, this, &SimulatedCanInterface::onSimulationTick);
}

SimulatedCanInterface::~SimulatedCanInterface()
{
    close();
}

bool SimulatedCanInterface::open(uint16_t /*channel*/, uint32_t /*baudRate*/)
{
    m_open = true;
    m_aliveCounter = 0;
    m_stateMachineState = 0; // Default
    m_simTimer->start(100); // 100ms tick simulating CMS message cycle
    emit statusChanged(CanStatus::Ok);
    return true;
}

void SimulatedCanInterface::close()
{
    m_open = false;
    m_simTimer->stop();
    m_stateMachineState = 0;
    emit statusChanged(CanStatus::Disconnected);
}

bool SimulatedCanInterface::write(const CanFrame& frame)
{
    if (!m_open) return false;

    // In simulation, process VCU→CMS messages to drive state transitions
    // Check if this is EVStatusControl (0x1302)
    if (frame.id == 0x1302 && frame.extended) {
        // Decode EVReady (bits 4-5)
        uint8_t evReady = (frame.data[0] >> 4) & 0x03;
        // Decode ChargeProgressIndication (bits 0-1)
        uint8_t chargeProgress = frame.data[0] & 0x03;

        // Simple state machine simulation
        if (evReady == 1 && m_stateMachineState < 3) {
            // Move to Parameter state when EVReady is set
            m_stateMachineState = 3;
        }
        if (chargeProgress == 0 && m_stateMachineState == 5) {
            // ChargeProgressIndication=Start (0) moves PreCharge→Charge
            m_stateMachineState = 6;
        }
    }

    m_txQueue.push_back(frame);
    return true;
}

std::vector<CanInterface::ChannelInfo> SimulatedCanInterface::availableChannels()
{
    return {
        {.name = "Simulated CAN 1", .handle = 0x0001, .description = "Virtual CAN bus (no hardware)"},
        {.name = "Simulated CAN 2", .handle = 0x0002, .description = "Virtual CAN bus 2 (no hardware)"}
    };
}

CanStatus SimulatedCanInterface::status() const
{
    return m_open ? CanStatus::Ok : CanStatus::Disconnected;
}

void SimulatedCanInterface::injectFrame(const CanFrame& frame)
{
    emit frameReceived(frame);
}

void SimulatedCanInterface::onSimulationTick()
{
    if (!m_open) return;

    auto now = std::chrono::steady_clock::now();

    // Simulate ChargeInfo (0x0600) - 100ms cycle
    {
        CanFrame f;
        f.id = 0x0600;
        f.extended = true;
        f.dlc = 8;
        f.timestamp = now;
        std::memset(f.data.data(), 0, 8);

        // ControlPilotDutyCycle: bits 0-6, value 5 (5%)
        f.data[0] = 5;
        // StateMachineState: bits 8-11
        f.data[1] = (m_stateMachineState & 0x0F);
        // ControlPilotState: bits 12-15, state B=1
        f.data[1] |= (1 << 4);
        // ActualChargeProtocol: bits 16-19, DIN=1
        f.data[2] = 1;
        // ProximityPinState: bits 20-23, Type2_CCS=3
        f.data[2] |= (3 << 4);
        // SwS2Close: bits 24-25, CommandOpen=0
        f.data[3] = 0;
        // VoltageMatch: bits 26-27
        f.data[3] |= ((m_stateMachineState >= 5 ? 1 : 0) << 2);
        // EVSECompatible: bits 28-29
        f.data[3] |= (1 << 4);
        // BCBStatus: bits 30-31
        // TCPStatus: bits 32-33
        f.data[4] = (m_stateMachineState >= 3 ? 1 : 0); // TCP connected
        // AliveCounter: bits 36-39
        f.data[4] |= ((m_aliveCounter & 0x0F) << 4);
        m_aliveCounter = (m_aliveCounter + 1) % 15;

        emit frameReceived(f);
    }

    // Simulate EVSEDCStatus (0x1402) - 100ms cycle
    {
        CanFrame f;
        f.id = 0x1402;
        f.extended = true;
        f.dlc = 8;
        f.timestamp = now;
        std::memset(f.data.data(), 0, 8);

        // EVSEPresentCurrent: bits 0-15, scale 0.1, offset -3250
        // Simulate 0A → raw = (0 + 3250) / 0.1 = 32500
        uint16_t rawCurrent = 32500;
        if (m_stateMachineState == 6) rawCurrent = 32600; // 10A during charge
        f.data[0] = rawCurrent & 0xFF;
        f.data[1] = (rawCurrent >> 8) & 0xFF;

        // EVSEPresentVoltage: bits 16-31, scale 0.1, offset 0
        // Simulate 400V → raw = 4000
        uint16_t rawVoltage = 0;
        if (m_stateMachineState >= 5) rawVoltage = 4000; // 400V during precharge/charge
        f.data[2] = rawVoltage & 0xFF;
        f.data[3] = (rawVoltage >> 8) & 0xFF;

        // EVSEIsolationStatus: bits 32-34
        uint8_t isoStatus = (m_stateMachineState >= 4) ? 1 : 7; // Valid or SNA
        f.data[4] = isoStatus;

        // EVSEVoltageLimitAchieved: bits 36-37
        // EVSENotification: bits 38-39
        // EVSEStatusCode: bits 40-43
        uint8_t statusCode = (m_stateMachineState >= 4) ? 1 : 0; // Ready or NotReady
        f.data[5] = statusCode;

        // EVSECurrentLimitAchieved: bits 44-45
        // EVSEPowerLimitAchieved: bits 46-47
        // EVSENotificationMaxDelay: bits 48-63
        f.data[6] = 0xFF; // SNA
        f.data[7] = 0xFF; // SNA

        emit frameReceived(f);
    }

    // Simulate EVSEDCMaxLimits (0x1400) - 100ms cycle
    {
        CanFrame f;
        f.id = 0x1400;
        f.extended = true;
        f.dlc = 8;
        f.timestamp = now;
        std::memset(f.data.data(), 0, 8);

        // EVSEMaxCurrent: bits 0-15, scale 0.1 → 200A = 2000
        uint16_t maxCur = 2000;
        f.data[0] = maxCur & 0xFF;
        f.data[1] = (maxCur >> 8) & 0xFF;

        // EVSEMaxVoltage: bits 16-31, scale 0.1 → 500V = 5000
        uint16_t maxVol = 5000;
        f.data[2] = maxVol & 0xFF;
        f.data[3] = (maxVol >> 8) & 0xFF;

        // EVSEMaxPower: bits 32-47, scale 100 → 100kW = 1000
        uint16_t maxPow = 1000;
        f.data[4] = maxPow & 0xFF;
        f.data[5] = (maxPow >> 8) & 0xFF;

        // EVSEEnergyToBeDelivered: bits 48-63, scale 100
        f.data[6] = 0xFF; // SNA
        f.data[7] = 0xFF;

        emit frameReceived(f);
    }

    // Simulate ErrorCodes (0x2002) - 1000ms cycle (simplified: send every tick)
    {
        CanFrame f;
        f.id = 0x2002;
        f.extended = true;
        f.dlc = 8;
        f.timestamp = now;
        std::memset(f.data.data(), 0, 8);
        // ErrorCodeLevel0 = 1 (STATUS_OK)
        f.data[0] = 1;
        f.data[1] = 0;
        emit frameReceived(f);
    }

    // Simulate SoftwareInfo (0x2001) every N ticks
    static int swInfoCounter = 0;
    if (++swInfoCounter >= 100) { // every 10s
        swInfoCounter = 0;
        CanFrame f;
        f.id = 0x2001;
        f.extended = true;
        f.dlc = 8;
        f.timestamp = now;
        std::memset(f.data.data(), 0, 8);
        f.data[0] = 1; // Major
        f.data[1] = 2; // Minor
        f.data[2] = 0; // Patch
        f.data[3] = 1; // Config (CAN)
        emit frameReceived(f);
    }

    // Auto-advance state machine in simulation for demonstration
    static int stateTimer = 0;
    stateTimer++;
    if (m_stateMachineState == 0 && stateTimer > 10) {
        m_stateMachineState = 1; // Init
    }
}

} // namespace ccs
