#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <chrono>

namespace ccs {

/// Generates session reports with charging statistics
class SessionReport : public QObject {
    Q_OBJECT
public:
    explicit SessionReport(QObject* parent = nullptr);

    void startSession();
    void endSession();
    void updateValues(double voltage, double current, double soc);

    bool saveReport(const QString& filePath) const;

    // Accessors
    bool isActive() const { return m_active; }
    QDateTime startTime() const { return m_startTime; }
    QDateTime endTime() const { return m_endTime; }
    double maxVoltage() const { return m_maxVoltage; }
    double maxCurrent() const { return m_maxCurrent; }
    double maxPower() const { return m_maxPower; }
    double energyEstimateWh() const { return m_energyWh; }
    double startSoC() const { return m_startSoC; }
    double endSoC() const { return m_endSoC; }
    int durationSeconds() const;

private:
    bool m_active = false;
    QDateTime m_startTime;
    QDateTime m_endTime;
    std::chrono::steady_clock::time_point m_lastUpdate;

    double m_maxVoltage = 0.0;
    double m_maxCurrent = 0.0;
    double m_maxPower = 0.0;
    double m_energyWh = 0.0;
    double m_startSoC = -1.0;
    double m_endSoC = 0.0;
    double m_lastVoltage = 0.0;
    double m_lastCurrent = 0.0;
};

} // namespace ccs
