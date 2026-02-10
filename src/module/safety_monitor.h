#pragma once

#include "module/state_machine.h"
#include <QObject>
#include <QTimer>
#include <QMap>
#include <chrono>
#include <cstdint>

namespace ccs {

/// Safety monitor: validates limits, detects timeouts, manages emergency stop
class SafetyMonitor : public QObject {
    Q_OBJECT
public:
    /// Hard limits derived from DBC signal ranges
    struct Limits {
        double maxVoltage = 6500.0;  // V (DBC max for voltage signals)
        double maxCurrent = 6500.0;  // A (DBC max for current signals)
        double maxPower = 3276700.0; // W (DBC max for power signals)
        double minVoltage = 0.0;     // V
        double minCurrent = -3250.0; // A (DBC min for current, supports regen)

        // User-configurable limits (must be within hard limits)
        double userMaxVoltage = 500.0;
        double userMaxCurrent = 200.0;
        double userMaxPower = 100000.0;
    };

    /// Timeout thresholds from datasheet
    struct Timeouts {
        int canMessageTimeoutMs = 1000;     // 10 missed messages at 100ms
        int authenticationTimeoutS = 60;
        int parameterDiscoveryTimeoutS = 60;
        int cableCheckTimeoutS = 40;
        int preChargeTimeoutS = 7;
        int readyToChargeTimeoutS = 150;
        int heartbeatTimeoutMs = 1500;       // AliveCounter must change within this
    };

    explicit SafetyMonitor(QObject* parent = nullptr);

    // Limit validation
    double clampVoltage(double v) const;
    double clampCurrent(double i) const;
    double clampPower(double p) const;
    bool isVoltageInRange(double v) const;
    bool isCurrentInRange(double i) const;

    // Setpoint validation
    void setUserLimits(double maxV, double maxI, double maxP);
    const Limits& limits() const { return m_limits; }

    // Heartbeat monitoring
    void updateAliveCounter(uint8_t counter);
    bool isHeartbeatOk() const { return m_heartbeatOk; }

    // CAN message timeout monitoring
    void messageReceived(uint32_t canId);
    bool isMessageTimedOut(uint32_t canId) const;

    // Emergency stop
    void triggerEmergencyStop(const QString& reason);
    bool isEmergencyStopped() const { return m_emergencyStopped; }
    void clearEmergencyStop();

    // Error code lookup from datasheet
    static QString errorCodeDescription(uint16_t code);
    static QString errorCodeAction(uint16_t code);

signals:
    void heartbeatLost();
    void heartbeatRestored();
    void messageTimeout(uint32_t canId, const QString& messageName);
    void emergencyStopTriggered(const QString& reason);
    void emergencyStopCleared();
    void limitViolation(const QString& description);

private slots:
    void onWatchdogTick();

private:
    Limits m_limits;
    Timeouts m_timeouts;
    QTimer* m_watchdogTimer = nullptr;

    // Heartbeat tracking
    uint8_t m_lastAliveCounter = 15; // SNA
    std::chrono::steady_clock::time_point m_lastAliveTime;
    bool m_heartbeatOk = false;

    // Message timeout tracking
    struct MessageTimestamp {
        std::chrono::steady_clock::time_point lastSeen;
        bool timedOut = false;
    };
    QMap<uint32_t, MessageTimestamp> m_messageTimestamps;

    bool m_emergencyStopped = false;
};

} // namespace ccs
