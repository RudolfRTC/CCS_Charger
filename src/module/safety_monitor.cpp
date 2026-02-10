#include "module/safety_monitor.h"
#include <QDebug>
#include <algorithm>

namespace ccs {

SafetyMonitor::SafetyMonitor(QObject* parent)
    : QObject(parent)
{
    m_lastAliveTime = std::chrono::steady_clock::now();

    m_watchdogTimer = new QTimer(this);
    m_watchdogTimer->setInterval(100); // Check every 100ms
    connect(m_watchdogTimer, &QTimer::timeout, this, &SafetyMonitor::onWatchdogTick);
    m_watchdogTimer->start();
}

double SafetyMonitor::clampVoltage(double v) const
{
    return std::clamp(v, m_limits.minVoltage, std::min(m_limits.maxVoltage, m_limits.userMaxVoltage));
}

double SafetyMonitor::clampCurrent(double i) const
{
    return std::clamp(i, m_limits.minCurrent, std::min(m_limits.maxCurrent, m_limits.userMaxCurrent));
}

double SafetyMonitor::clampPower(double p) const
{
    return std::clamp(p, 0.0, std::min(m_limits.maxPower, m_limits.userMaxPower));
}

bool SafetyMonitor::isVoltageInRange(double v) const
{
    return v >= m_limits.minVoltage && v <= m_limits.userMaxVoltage;
}

bool SafetyMonitor::isCurrentInRange(double i) const
{
    return i >= m_limits.minCurrent && i <= m_limits.userMaxCurrent;
}

void SafetyMonitor::setUserLimits(double maxV, double maxI, double maxP)
{
    m_limits.userMaxVoltage = std::clamp(maxV, 0.0, m_limits.maxVoltage);
    m_limits.userMaxCurrent = std::clamp(maxI, 0.0, m_limits.maxCurrent);
    m_limits.userMaxPower = std::clamp(maxP, 0.0, m_limits.maxPower);
}

void SafetyMonitor::updateAliveCounter(uint8_t counter)
{
    if (counter == 15) return; // SNA - ignore

    if (counter != m_lastAliveCounter) {
        m_lastAliveCounter = counter;
        m_lastAliveTime = std::chrono::steady_clock::now();
        if (!m_heartbeatOk) {
            m_heartbeatOk = true;
            emit heartbeatRestored();
        }
    }
}

void SafetyMonitor::messageReceived(uint32_t canId)
{
    m_messageTimestamps[canId] = {std::chrono::steady_clock::now(), false};
}

bool SafetyMonitor::isMessageTimedOut(uint32_t canId) const
{
    auto it = m_messageTimestamps.find(canId);
    if (it == m_messageTimestamps.end()) return true;
    return it->timedOut;
}

void SafetyMonitor::triggerEmergencyStop(const QString& reason)
{
    if (!m_emergencyStopped) {
        m_emergencyStopped = true;
        qWarning() << "EMERGENCY STOP:" << reason;
        emit emergencyStopTriggered(reason);
    }
}

void SafetyMonitor::clearEmergencyStop()
{
    if (m_emergencyStopped) {
        m_emergencyStopped = false;
        emit emergencyStopCleared();
    }
}

void SafetyMonitor::onWatchdogTick()
{
    auto now = std::chrono::steady_clock::now();

    // Check heartbeat (AliveCounter)
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastAliveTime).count();
    if (m_heartbeatOk && elapsed > m_timeouts.heartbeatTimeoutMs) {
        m_heartbeatOk = false;
        emit heartbeatLost();
    }

    // Check message timeouts
    for (auto it = m_messageTimestamps.begin(); it != m_messageTimestamps.end(); ++it) {
        auto msgElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->lastSeen).count();
        if (!it->timedOut && msgElapsed > m_timeouts.canMessageTimeoutMs) {
            it->timedOut = true;
            emit messageTimeout(it.key(), QString("0x%1").arg(it.key(), 4, 16, QChar('0')));
        }
    }
}

QString SafetyMonitor::errorCodeDescription(uint16_t code)
{
    // Error codes from User Guide Section 11
    switch (code) {
        case 0:   return "UNPLUGGED - EV and EVSE not connected";
        case 1:   return "STATUS_OK - Plugged in, no errors";
        case 139:  return "SM_SAP_RESPONSE_FAILED";
        case 140:  return "SM_SESSIONSETUP_RESPONSE_FAILED";
        case 141:  return "SM_SERVICEDISCOVERY_RESPONSE_FAILED";
        case 142:  return "SM_SERVICEPAYMENTSELECTION_RESPONSE_FAILED";
        case 143:  return "SM_CONTRACT_AUTHENTICATION_RESPONSE_FAILED";
        case 144:  return "SM_CHARGE_PARAMETER_DISCOVERY_RESPONSE_FAILED";
        case 145:  return "SM_CABLE_CHECK_RESPONSE_FAILED";
        case 146:  return "SM_PRE_CHARGE_RESPONSE_FAILED";
        case 147:  return "SM_POWER_DELIVERY_PRECHARGE_RESPONSE_FAILED";
        case 148:  return "SM_CURRENT_DEMAND_RESPONSE_FAILED";
        case 149:  return "SM_POWER_DELIVERY_POSTCHARGE_RESPONSE_FAILED";
        case 150:  return "SM_WELDING_DETECTION_RESPONSE_FAILED";
        case 151:  return "SM_SESSION_STOP_RESPONSE_FAILED";
        case 152:  return "SM_CHARGE_PARAMETER_EVSESTATUSCODE_FAILED";
        case 153:  return "SM_CABLE_CHECK_EVSESTATUSCODE_FAILED";
        case 154:  return "SM_PRE_CHARGE_EVSESTATUSCODE_FAILED";
        case 155:  return "SM_PRECHARGE_EVSESTATUSCODE_FAILED";
        case 156:  return "SM_CURRENT_DEMAND_EVSESTATUSCODE_FAILED";
        case 157:  return "SM_POSTCHARGE_EVSESTATUSCODE_FAILED";
        case 158:  return "SM_CABLE_CHECK_ISOLATION_NOTVALID";
        case 159:  return "SM_SHUTDOWN_ERR";
        case 160:  return "V2G_HLC_INIT_TIMEOUT - V2G Init took >20s";
        case 161:  return "EVSE_EMERGENCY - EVSE emergency shutdown";
        case 162:  return "LIMITS_MSG_TIMEOUT - CAN message timeout (1000ms)";
        case 163:  return "STATUS_MSG_TIMEOUT - CAN message timeout (1000ms)";
        case 164:  return "PLUGSTATUS_MSG_TIMEOUT - CAN message timeout (1000ms)";
        case 196:  return "EV_SNA_ERROR - EVSE sent out-of-range value";
        case 215:  return "SM_AUTHENTICATION_ONGOING_TIMEOUT - Auth >60s";
        case 216:  return "SM_CPD_ONGOING_TIMEOUT - CPD >60s";
        case 217:  return "SM_CABLECHECKTIMER_TIMEOUT - CableCheck >40s";
        case 218:  return "SM_PRECHARGETIMER_TIMEOUT - PreCharge >7s";
        case 219:  return "SM_READYTOCHARGE_TIMEOUT - Plugin to PowerDelivery >150s";
        case 235:  return "SLAC_ATTENUATION_HIGH - Attenuation <10dB over threshold";
        case 236:  return "SLAC_ATTENUATION_TOO_HIGH - Attenuation >10dB over threshold";
        case 237:  return "LOW_VOLTAGE_DETECTED - Supply voltage below spec";
        case 240:  return "EV_ERROR_CODE_SNA - Mandatory signal not set";
        case 241:  return "EV_READY_SNA - Mandatory signal not set";
        case 242:  return "EV_SOC_SNA - Mandatory signal not set";
        case 243:  return "EV_TARGET_CUR_SNA - Mandatory signal not set";
        case 244:  return "EV_TARGET_VOL_SNA - Mandatory signal not set";
        case 245:  return "EV_CHARG_COMP_SNA - Mandatory signal not set";
        case 246:  return "EV_MAX_VOLT_SNA - Mandatory signal not set";
        case 247:  return "EV_MAX_CUR_SNA - Mandatory signal not set";
        case 248:  return "EV_PRE_VOLT_SNA - Mandatory signal not set";
        case 249:  return "EV_E_STOP_TRIGGERED - E-Stop triggered by EV";
        case 251:  return "PARAMETERS_TIMEOUT - No valid params in 60s";
        default:
            if (code >= 2 && code <= 138)
                return QString("Internal error (0x%1) - Contact chargebyte").arg(code, 2, 16, QChar('0'));
            if (code >= 167 && code <= 193)
                return QString("Range overflow error (0x%1) - Signal value out of range").arg(code, 2, 16, QChar('0'));
            return QString("Unknown error code: %1 (0x%2)").arg(code).arg(code, 4, 16, QChar('0'));
    }
}

QString SafetyMonitor::errorCodeAction(uint16_t code)
{
    if (code == 0 || code == 1) return "No action needed";
    if (code >= 139 && code <= 151) return "Communication failure - check EVSE compatibility and retry";
    if (code >= 152 && code <= 157) return "EVSE status error - check EVSE state";
    if (code == 158) return "Isolation check failed - inspect HV cables";
    if (code == 160) return "Timeout - ensure EVSE is responding; check PLC connection";
    if (code == 161) return "EVSE emergency - check EVSE for faults; do not reconnect until safe";
    if (code >= 162 && code <= 164) return "CAN timeout - ensure all required CAN messages are sent at 100ms cycle";
    if (code >= 215 && code <= 219) return "Timeout - check communication and retry charging session";
    if (code >= 240 && code <= 248) return "Set all mandatory signals to valid values before charging";
    if (code == 249) return "Emergency stop triggered - clear fault and restart";
    return "Check error details; unplug and retry; contact chargebyte if persistent";
}

} // namespace ccs
