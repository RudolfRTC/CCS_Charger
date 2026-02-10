#pragma once

#include <QObject>
#include <QString>
#include <cstdint>

namespace ccs {

/// CMS State Machine states - from DBC ChargeInfo::StateMachineState
enum class CmsState : uint8_t {
    Default      = 0,
    Init         = 1,
    Authentication = 2,
    Parameter    = 3,
    Isolation    = 4,
    PreCharge    = 5,
    Charge       = 6,
    Welding      = 7,
    StopCharge   = 8,
    SessionStop  = 9,
    ShutOff      = 10,
    Paused       = 11,
    Error        = 12,
    SNA          = 15
};

inline const char* cmsStateToString(CmsState s) {
    switch (s) {
        case CmsState::Default:        return "Default";
        case CmsState::Init:           return "Init";
        case CmsState::Authentication: return "Authentication";
        case CmsState::Parameter:      return "Parameter";
        case CmsState::Isolation:      return "Isolation";
        case CmsState::PreCharge:      return "PreCharge";
        case CmsState::Charge:         return "Charge";
        case CmsState::Welding:        return "Welding";
        case CmsState::StopCharge:     return "StopCharge";
        case CmsState::SessionStop:    return "SessionStop";
        case CmsState::ShutOff:        return "ShutOff";
        case CmsState::Paused:         return "Paused";
        case CmsState::Error:          return "Error";
        case CmsState::SNA:            return "SNA";
    }
    return "Unknown";
}

/// Control Pilot states
enum class ControlPilotState : uint8_t {
    A = 0,       // No vehicle connected
    B = 1,       // Vehicle connected, not ready
    C = 2,       // Vehicle connected, ready (charging)
    D = 3,       // With ventilation
    E = 4,       // No power
    F = 5,       // Error
    Invalid = 14,
    SNA = 15
};

/// EVSE Status Code
enum class EvseStatusCode : uint8_t {
    NotReady = 0,
    Ready = 1,
    Shutdown = 2,
    UtilityInterruptEvent = 3,
    IsolationMonitoringActive = 4,
    EmergencyShutdown = 5,
    Malfunction = 6,
    SNA = 15
};

/// EVSE Isolation Status
enum class EvseIsolationStatus : uint8_t {
    Invalid = 0,
    Valid = 1,
    Warning = 2,
    Fault = 3,
    NoIMD = 4,
    Checking = 5,
    SNA = 7
};

/// Charge Protocol
enum class ChargeProtocol : uint8_t {
    NotDefined = 0,
    DIN70121 = 1,
    ISO15118 = 2,
    NotSupported = 3,
    SNA = 15
};

/// EV Charge Progress
enum class ChargeProgressIndication : uint8_t {
    Start = 0,
    Stop = 1,
    SNA = 3
};

/// Charge Stop Indication
enum class ChargeStopIndication : uint8_t {
    Terminate = 0,
    NoStop = 2,
    SNA = 3
};

/// BCB Control
enum class BCBControl : uint8_t {
    Stop = 0,
    Start = 1,
    SNA = 3
};

} // namespace ccs
