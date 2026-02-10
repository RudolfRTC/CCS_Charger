#include "module/charge_module.h"
#include <QDebug>
#include <cstring>

namespace ccs {

// CAN IDs (extended) per DBC
namespace canid {
    constexpr uint32_t ChargeInfo            = 0x0600;
    constexpr uint32_t EVDCMaxLimits         = 0x1300; // VCU → CMS
    constexpr uint32_t EVDCChargeTargets     = 0x1301; // VCU → CMS
    constexpr uint32_t EVStatusControl       = 0x1302; // VCU → CMS
    constexpr uint32_t EVStatusDisplay       = 0x1303; // VCU → CMS
    constexpr uint32_t EVPlugStatus          = 0x1304; // VCU → CMS
    constexpr uint32_t EVDCEnergyLimits      = 0x1305; // VCU → CMS
    constexpr uint32_t EVSEDCMaxLimits       = 0x1400; // CMS → VCU
    constexpr uint32_t EVSEDCRegulationLimits = 0x1401; // CMS → VCU
    constexpr uint32_t EVSEDCStatus          = 0x1402; // CMS → VCU
    constexpr uint32_t SoftwareInfo          = 0x2001; // CMS → VCU
    constexpr uint32_t ErrorCodes            = 0x2002; // CMS → VCU
    constexpr uint32_t SLACInfo              = 0x2003; // CMS → VCU
    constexpr uint32_t ModuleReset           = 0x0667; // Standard frame!
}

ChargeModule::ChargeModule(QObject* parent)
    : QObject(parent)
{
    m_cyclicTimer = new QTimer(this);
    m_cyclicTimer->setInterval(100); // 100ms cycle per DBC
    connect(m_cyclicTimer, &QTimer::timeout, this, &ChargeModule::onCyclicTx);

    // Safety monitor connections
    connect(&m_safety, &SafetyMonitor::emergencyStopTriggered, this, [this](const QString& reason) {
        qWarning() << "Safety: Emergency stop -" << reason;
        // Immediately set EVReady=false, ChargeProgressIndication=Stop
        m_evParams.evReady = false;
        m_evParams.chargeProgress = ChargeProgressIndication::Stop;
        m_evParams.chargeStop = ChargeStopIndication::Terminate;
        // Send immediately
        if (m_running) {
            sendEvStatusControl();
        }
    });
}

void ChargeModule::setCanInterface(CanInterface* iface)
{
    if (m_can) {
        disconnect(m_can, &CanInterface::frameReceived, this, &ChargeModule::onFrameReceived);
    }
    m_can = iface;
    if (m_can) {
        connect(m_can, &CanInterface::frameReceived, this, &ChargeModule::onFrameReceived,
                Qt::QueuedConnection);
    }
}

void ChargeModule::loadDbc(const QString& dbcPath)
{
    DbcParser parser;
    if (parser.parse(dbcPath)) {
        m_dbc = parser.database();
        m_codec.setDatabase(m_dbc);
        qDebug() << "DBC loaded:" << m_dbc.name << "with" << m_dbc.messages.size() << "messages";
    } else {
        qWarning() << "Failed to load DBC:" << parser.lastError();
    }
}

void ChargeModule::start()
{
    if (m_running) return;

    // Initialize EV parameters to safe defaults (SNA values where appropriate)
    // Per datasheet: CMS will not start until mandatory signals are non-SNA
    m_running = true;
    m_cyclicTimer->start();
    qDebug() << "ChargeModule: cyclic TX started (100ms)";
}

void ChargeModule::stop()
{
    m_running = false;
    m_cyclicTimer->stop();

    // Send safe state: EVReady=false, ChargeProgress=Stop, ChargeStop=Terminate
    m_evParams.evReady = false;
    m_evParams.chargeProgress = ChargeProgressIndication::Stop;
    m_evParams.chargeStop = ChargeStopIndication::Terminate;
    if (m_can && m_can->isOpen()) {
        sendEvStatusControl();
    }

    qDebug() << "ChargeModule: stopped, safe state sent";
}

// ─── Parameter setters ───────────────────────────────────

void ChargeModule::setEvMaxVoltage(double v)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evMaxVoltage = m_safety.clampVoltage(v);
}

void ChargeModule::setEvMaxCurrent(double i)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evMaxCurrent = m_safety.clampCurrent(i);
}

void ChargeModule::setEvMaxPower(double p)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evMaxPower = m_safety.clampPower(p);
}

void ChargeModule::setEvTargetVoltage(double v)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evTargetVoltage = m_safety.clampVoltage(v);
}

void ChargeModule::setEvTargetCurrent(double i)
{
    QMutexLocker lock(&m_mutex);
    // During PreCharge, target current is fixed at 2A per datasheet
    if (m_evseData.stateMachineState == CmsState::PreCharge) {
        m_evParams.evTargetCurrent = std::min(i, 2.0);
    } else {
        m_evParams.evTargetCurrent = m_safety.clampCurrent(i);
    }
}

void ChargeModule::setEvPreChargeVoltage(double v)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evPreChargeVoltage = m_safety.clampVoltage(v);
}

void ChargeModule::setEvSoC(double soc)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evSoC = std::clamp(soc, 0.0, 100.0);
}

void ChargeModule::setEvReady(bool ready)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evReady = ready;
}

void ChargeModule::setChargeProgressIndication(ChargeProgressIndication ind)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.chargeProgress = ind;
}

void ChargeModule::setChargeStopIndication(ChargeStopIndication ind)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.chargeStop = ind;
}

void ChargeModule::setWeldingDetectionEnable(bool enable)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evWeldingDetectionEnable = enable;
}

void ChargeModule::setEvErrorCode(uint8_t code)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evErrorCode = code;
}

void ChargeModule::setEvFullSoC(double soc)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evFullSoC = std::clamp(soc, 0.0, 100.0);
}

void ChargeModule::setEvBulkSoC(double soc)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evBulkSoC = std::clamp(soc, 0.0, 100.0);
}

void ChargeModule::setEvEnergyCapacity(double wh)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evEnergyCapacity = std::clamp(wh, 0.0, 3276700.0);
}

void ChargeModule::setEvEnergyRequest(double wh)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evEnergyRequest = std::clamp(wh, 0.0, 3276700.0);
}

void ChargeModule::setChargeProtocolPriority(uint8_t prio)
{
    QMutexLocker lock(&m_mutex);
    m_evParams.chargeProtocolPriority = prio;
}

// ─── High-level actions ──────────────────────────────────

void ChargeModule::requestStartCharging()
{
    QMutexLocker lock(&m_mutex);
    m_evParams.evReady = true;
    m_evParams.chargeStop = ChargeStopIndication::NoStop;
    m_evParams.evErrorCode = 0; // NO_ERROR
    // ChargeProgressIndication will be set to Start when VoltageMatch is True (PreCharge→Charge transition)
    qDebug() << "ChargeModule: Charging requested";
}

void ChargeModule::requestStopCharging()
{
    QMutexLocker lock(&m_mutex);
    // Per datasheet: set ChargeProgressIndication to Stop, then ChargeStopIndication to Terminate
    m_evParams.chargeProgress = ChargeProgressIndication::Stop;
    m_evParams.chargeStop = ChargeStopIndication::Terminate;
    qDebug() << "ChargeModule: Stop charging requested";
}

void ChargeModule::emergencyStop()
{
    m_safety.triggerEmergencyStop("User-initiated emergency stop");
}

void ChargeModule::resetModule()
{
    if (!m_can || !m_can->isOpen()) return;

    // Module reset: CAN ID 0x667, standard frame, payload [0xFF, 0x00]
    CanFrame frame;
    frame.id = canid::ModuleReset;
    frame.extended = false; // Standard frame per datasheet
    frame.dlc = 2;
    frame.data[0] = 0xFF;
    frame.data[1] = 0x00;
    frame.timestamp = std::chrono::steady_clock::now();
    sendFrame(frame);
    qDebug() << "ChargeModule: Reset command sent (0x667)";
}

// ─── Frame reception ─────────────────────────────────────

void ChargeModule::onFrameReceived(const CanFrame& frame)
{
    emit rawFrameReceived(frame);
    m_safety.messageReceived(frame.id);

    switch (frame.id) {
        case canid::ChargeInfo:            decodeChargeInfo(frame); break;
        case canid::EVSEDCMaxLimits:       decodeEvseMaxLimits(frame); break;
        case canid::EVSEDCRegulationLimits: decodeEvseRegulationLimits(frame); break;
        case canid::EVSEDCStatus:          decodeEvseDCStatus(frame); break;
        case canid::ErrorCodes:            decodeErrorCodes(frame); break;
        case canid::SoftwareInfo:          decodeSoftwareInfo(frame); break;
        case canid::SLACInfo:              decodeSLACInfo(frame); break;
        default: break;
    }
}

void ChargeModule::decodeChargeInfo(const CanFrame& frame)
{
    // ChargeInfo (0x0600) - use codec for precise decoding
    auto decoded = m_codec.decode(frame);
    for (const auto& sig : decoded.decodedSignals) {
        if (sig.name == "StateMachineState") {
            auto newState = static_cast<CmsState>(static_cast<uint8_t>(sig.rawValue));
            if (newState != m_evseData.stateMachineState) {
                m_evseData.stateMachineState = newState;
                if (newState != m_lastState) {
                    m_lastState = newState;
                    emit stateChanged(newState);
                }
            }
        }
        else if (sig.name == "AliveCounter") {
            m_evseData.aliveCounter = static_cast<uint8_t>(sig.rawValue);
            m_safety.updateAliveCounter(m_evseData.aliveCounter);
        }
        else if (sig.name == "ControlPilotState") {
            m_evseData.controlPilotState = static_cast<ControlPilotState>(sig.rawValue);
        }
        else if (sig.name == "ControlPilotDutyCycle") {
            m_evseData.controlPilotDutyCycle = static_cast<uint8_t>(sig.rawValue);
        }
        else if (sig.name == "ActualChargeProtocol") {
            m_evseData.actualChargeProtocol = static_cast<ChargeProtocol>(sig.rawValue);
        }
        else if (sig.name == "ProximityPinState") {
            m_evseData.proximityPinState = static_cast<uint8_t>(sig.rawValue);
        }
        else if (sig.name == "SwS2Close") {
            m_evseData.swS2Close = (sig.rawValue == 1);
        }
        else if (sig.name == "VoltageMatch") {
            m_evseData.voltageMatch = (sig.rawValue == 1);
        }
        else if (sig.name == "EVSECompatible") {
            m_evseData.evseCompatible = (sig.rawValue == 1);
        }
        else if (sig.name == "TCPStatus") {
            m_evseData.tcpConnected = (sig.rawValue == 1);
        }
        else if (sig.name == "BCBStatus") {
            m_evseData.bcbStatus = static_cast<uint8_t>(sig.rawValue);
        }
    }
    emit evseDataUpdated();
}

void ChargeModule::decodeEvseMaxLimits(const CanFrame& frame)
{
    auto decoded = m_codec.decode(frame);
    for (const auto& sig : decoded.decodedSignals) {
        if (!sig.isValid) continue;
        if (sig.name == "EVSEMaxCurrent") m_evseData.evseMaxCurrent = sig.physicalValue;
        else if (sig.name == "EVSEMaxVoltage") m_evseData.evseMaxVoltage = sig.physicalValue;
        else if (sig.name == "EVSEMaxPower") m_evseData.evseMaxPower = sig.physicalValue;
        else if (sig.name == "EVSEEnergyToBeDelivered") m_evseData.evseEnergyToBeDelivered = sig.physicalValue;
    }
    emit evseDataUpdated();
}

void ChargeModule::decodeEvseRegulationLimits(const CanFrame& frame)
{
    auto decoded = m_codec.decode(frame);
    for (const auto& sig : decoded.decodedSignals) {
        if (!sig.isValid) continue;
        if (sig.name == "EVSEMinCurrent") m_evseData.evseMinCurrent = sig.physicalValue;
        else if (sig.name == "EVSEMinVoltage") m_evseData.evseMinVoltage = sig.physicalValue;
        else if (sig.name == "EVSEPeakCurrentRipple") m_evseData.evsePeakCurrentRipple = sig.physicalValue;
        else if (sig.name == "EVSECurrentRegulationTolerance") m_evseData.evseCurrentRegulationTolerance = sig.physicalValue;
    }
    emit evseDataUpdated();
}

void ChargeModule::decodeEvseDCStatus(const CanFrame& frame)
{
    auto decoded = m_codec.decode(frame);
    for (const auto& sig : decoded.decodedSignals) {
        if (sig.name == "EVSEPresentVoltage" && sig.isValid)
            m_evseData.evsePresentVoltage = sig.physicalValue;
        else if (sig.name == "EVSEPresentCurrent" && sig.isValid)
            m_evseData.evsePresentCurrent = sig.physicalValue;
        else if (sig.name == "EVSEIsolationStatus")
            m_evseData.evseIsolationStatus = static_cast<EvseIsolationStatus>(sig.rawValue);
        else if (sig.name == "EVSEStatusCode")
            m_evseData.evseStatusCode = static_cast<EvseStatusCode>(sig.rawValue);
        else if (sig.name == "EVSENotification")
            m_evseData.evseNotification = static_cast<uint8_t>(sig.rawValue);
        else if (sig.name == "EVSENotificationMaxDelay")
            m_evseData.evseNotificationMaxDelay = static_cast<uint16_t>(sig.rawValue);
        else if (sig.name == "EVSECurrentLimitAchieved")
            m_evseData.evseCurrentLimitAchieved = (sig.rawValue == 1);
        else if (sig.name == "EVSEVoltageLimitAchieved")
            m_evseData.evseVoltageLimitAchieved = (sig.rawValue == 1);
        else if (sig.name == "EVSEPowerLimitAchieved")
            m_evseData.evsePowerLimitAchieved = (sig.rawValue == 1);
    }

    // Safety: react to EVSE emergency/malfunction
    if (m_evseData.evseStatusCode == EvseStatusCode::EmergencyShutdown ||
        m_evseData.evseStatusCode == EvseStatusCode::Malfunction) {
        m_safety.triggerEmergencyStop("EVSE emergency/malfunction detected");
    }

    emit evseDataUpdated();
}

void ChargeModule::decodeErrorCodes(const CanFrame& frame)
{
    auto decoded = m_codec.decode(frame);
    for (const auto& sig : decoded.decodedSignals) {
        uint16_t code = static_cast<uint16_t>(sig.rawValue);
        if (sig.name == "ErrorCodeLevel0") {
            if (code != m_evseData.errorCode0 && code > 1) {
                emit errorCodeReceived(code, SafetyMonitor::errorCodeDescription(code));
            }
            m_evseData.errorCode0 = code;
        }
        else if (sig.name == "ErrorCodeLevel1") m_evseData.errorCode1 = code;
        else if (sig.name == "ErrorCodeLevel2") m_evseData.errorCode2 = code;
        else if (sig.name == "ErrorCodeLevel3") m_evseData.errorCode3 = code;
    }
    emit evseDataUpdated();
}

void ChargeModule::decodeSoftwareInfo(const CanFrame& frame)
{
    auto decoded = m_codec.decode(frame);
    for (const auto& sig : decoded.decodedSignals) {
        if (sig.name == "SoftwareVersionMajor") m_evseData.swVersionMajor = static_cast<uint8_t>(sig.rawValue);
        else if (sig.name == "SoftwareVersionMinor") m_evseData.swVersionMinor = static_cast<uint8_t>(sig.rawValue);
        else if (sig.name == "SoftwareVersionPatch") m_evseData.swVersionPatch = static_cast<uint8_t>(sig.rawValue);
        else if (sig.name == "SoftwareVersionConfig") m_evseData.swVersionConfig = static_cast<uint8_t>(sig.rawValue);
    }
    emit evseDataUpdated();
}

void ChargeModule::decodeSLACInfo(const CanFrame& frame)
{
    auto decoded = m_codec.decode(frame);
    for (const auto& sig : decoded.decodedSignals) {
        if (sig.name == "SLACState") m_evseData.slacState = static_cast<uint8_t>(sig.rawValue);
        else if (sig.name == "LinkStatus") m_evseData.linkStatus = static_cast<uint8_t>(sig.rawValue);
        else if (sig.name == "MeasuredAttenuation" && sig.isValid) m_evseData.measuredAttenuation = sig.physicalValue;
    }
    emit evseDataUpdated();
}

// ─── Cyclic TX ───────────────────────────────────────────

void ChargeModule::onCyclicTx()
{
    if (!m_can || !m_can->isOpen() || !m_running) return;

    QMutexLocker lock(&m_mutex);

    // If emergency stopped, only send safe state
    if (m_safety.isEmergencyStopped()) {
        m_evParams.evReady = false;
        m_evParams.chargeProgress = ChargeProgressIndication::Stop;
        m_evParams.chargeStop = ChargeStopIndication::Terminate;
    }

    // Send all VCU → CMS messages at 100ms cycle per DBC
    sendEvDCMaxLimits();
    sendEvDCChargeTargets();
    sendEvStatusControl();
    sendEvStatusDisplay();
    sendEvPlugStatus();
    sendEvDCEnergyLimits();
}

void ChargeModule::sendEvDCMaxLimits()
{
    // EVDCMaxLimits (0x1300): EVMaxCurrent, EVMaxVoltage, EVMaxPower, EVFullSoC, EVBulkSoC
    CanFrame frame;
    frame.id = canid::EVDCMaxLimits;
    frame.extended = true;
    frame.dlc = 8;
    frame.timestamp = std::chrono::steady_clock::now();
    std::memset(frame.data.data(), 0, 8);

    const auto* msg = m_dbc.findMessage(canid::EVDCMaxLimits);
    if (msg) {
        for (const auto& sig : msg->dbcSignals) {
            if (sig.name == "EVMaxCurrent")
                m_codec.encodeSignal(frame, sig, m_evParams.evMaxCurrent);
            else if (sig.name == "EVMaxVoltage")
                m_codec.encodeSignal(frame, sig, m_evParams.evMaxVoltage);
            else if (sig.name == "EVMaxPower")
                m_codec.encodeSignal(frame, sig, m_evParams.evMaxPower);
            else if (sig.name == "EVFullSoC")
                m_codec.encodeSignal(frame, sig, m_evParams.evFullSoC);
            else if (sig.name == "EVBulkSoC")
                m_codec.encodeSignal(frame, sig, m_evParams.evBulkSoC);
        }
    }

    sendFrame(frame);
}

void ChargeModule::sendEvDCChargeTargets()
{
    CanFrame frame;
    frame.id = canid::EVDCChargeTargets;
    frame.extended = true;
    frame.dlc = 8;
    frame.timestamp = std::chrono::steady_clock::now();
    std::memset(frame.data.data(), 0, 8);

    const auto* msg = m_dbc.findMessage(canid::EVDCChargeTargets);
    if (msg) {
        for (const auto& sig : msg->dbcSignals) {
            if (sig.name == "EVTargetCurrent")
                m_codec.encodeSignal(frame, sig, m_evParams.evTargetCurrent);
            else if (sig.name == "EVTargetVoltage")
                m_codec.encodeSignal(frame, sig, m_evParams.evTargetVoltage);
            else if (sig.name == "EVPreChargeVoltage")
                m_codec.encodeSignal(frame, sig, m_evParams.evPreChargeVoltage);
        }
    }

    sendFrame(frame);
}

void ChargeModule::sendEvStatusControl()
{
    CanFrame frame;
    frame.id = canid::EVStatusControl;
    frame.extended = true;
    frame.dlc = 8;
    frame.timestamp = std::chrono::steady_clock::now();
    std::memset(frame.data.data(), 0, 8);

    const auto* msg = m_dbc.findMessage(canid::EVStatusControl);
    if (msg) {
        for (const auto& sig : msg->dbcSignals) {
            if (sig.name == "ChargeProgressIndication")
                m_codec.encodeSignalRaw(frame, sig, static_cast<uint64_t>(m_evParams.chargeProgress));
            else if (sig.name == "ChargeStopIndication")
                m_codec.encodeSignalRaw(frame, sig, static_cast<uint64_t>(m_evParams.chargeStop));
            else if (sig.name == "EVReady")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evReady ? 1ULL : 0ULL);
            else if (sig.name == "EVWeldingDetectionEnable")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evWeldingDetectionEnable ? 1ULL : 0ULL);
            else if (sig.name == "ChargeProtocolPriority")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.chargeProtocolPriority);
            else if (sig.name == "BCBControl")
                m_codec.encodeSignalRaw(frame, sig, static_cast<uint64_t>(m_evParams.bcbControl));
        }
    }

    sendFrame(frame);
}

void ChargeModule::sendEvStatusDisplay()
{
    CanFrame frame;
    frame.id = canid::EVStatusDisplay;
    frame.extended = true;
    frame.dlc = 8;
    frame.timestamp = std::chrono::steady_clock::now();
    std::memset(frame.data.data(), 0, 8);

    const auto* msg = m_dbc.findMessage(canid::EVStatusDisplay);
    if (msg) {
        for (const auto& sig : msg->dbcSignals) {
            if (sig.name == "EVSoC")
                m_codec.encodeSignal(frame, sig, m_evParams.evSoC);
            else if (sig.name == "EVErrorCode")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evErrorCode);
            else if (sig.name == "EVChargingComplete")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evChargingComplete ? 1ULL : 0ULL);
            else if (sig.name == "EVBulkChargingComplete")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evBulkChargingComplete ? 1ULL : 0ULL);
            else if (sig.name == "EVCabinConditioning")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evCabinConditioning ? 1ULL : 0ULL);
            else if (sig.name == "EVRESSConditioning")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evRessConditioning ? 1ULL : 0ULL);
            else if (sig.name == "EVTimeToFullSoC")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evTimeToFullSoC);
            else if (sig.name == "EVTimeToBulkSoC")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evTimeToBulkSoC);
        }
    }

    sendFrame(frame);
}

void ChargeModule::sendEvPlugStatus()
{
    CanFrame frame;
    frame.id = canid::EVPlugStatus;
    frame.extended = true;
    frame.dlc = 8;
    frame.timestamp = std::chrono::steady_clock::now();
    std::memset(frame.data.data(), 0, 8);

    const auto* msg = m_dbc.findMessage(canid::EVPlugStatus);
    if (msg) {
        for (const auto& sig : msg->dbcSignals) {
            if (sig.name == "EVControlPilotDutyCycle")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evControlPilotDutyCycle);
            else if (sig.name == "EVControlPilotState")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evControlPilotState);
            else if (sig.name == "EVProximityPinState")
                m_codec.encodeSignalRaw(frame, sig, m_evParams.evProximityPinState);
        }
    }

    sendFrame(frame);
}

void ChargeModule::sendEvDCEnergyLimits()
{
    CanFrame frame;
    frame.id = canid::EVDCEnergyLimits;
    frame.extended = true;
    frame.dlc = 8;
    frame.timestamp = std::chrono::steady_clock::now();
    std::memset(frame.data.data(), 0, 8);

    const auto* msg = m_dbc.findMessage(canid::EVDCEnergyLimits);
    if (msg) {
        for (const auto& sig : msg->dbcSignals) {
            if (sig.name == "EVEnergyCapacity")
                m_codec.encodeSignal(frame, sig, m_evParams.evEnergyCapacity);
            else if (sig.name == "EVEnergyRequest")
                m_codec.encodeSignal(frame, sig, m_evParams.evEnergyRequest);
        }
    }

    sendFrame(frame);
}

void ChargeModule::sendFrame(const CanFrame& frame)
{
    if (m_can && m_can->isOpen()) {
        m_can->write(frame);
        emit rawFrameSent(frame);
    }
}

} // namespace ccs
