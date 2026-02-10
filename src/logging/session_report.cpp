#include "logging/session_report.h"
#include <QFile>
#include <QTextStream>
#include <cmath>

namespace ccs {

SessionReport::SessionReport(QObject* parent)
    : QObject(parent)
{
}

void SessionReport::startSession()
{
    m_active = true;
    m_startTime = QDateTime::currentDateTime();
    m_lastUpdate = std::chrono::steady_clock::now();
    m_maxVoltage = 0.0;
    m_maxCurrent = 0.0;
    m_maxPower = 0.0;
    m_energyWh = 0.0;
    m_startSoC = -1.0;
    m_endSoC = 0.0;
    m_lastVoltage = 0.0;
    m_lastCurrent = 0.0;
}

void SessionReport::endSession()
{
    m_active = false;
    m_endTime = QDateTime::currentDateTime();
}

void SessionReport::updateValues(double voltage, double current, double soc)
{
    if (!m_active) return;

    auto now = std::chrono::steady_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdate).count();
    m_lastUpdate = now;

    double power = voltage * current;

    // Track maximums
    m_maxVoltage = std::max(m_maxVoltage, voltage);
    m_maxCurrent = std::max(m_maxCurrent, current);
    m_maxPower = std::max(m_maxPower, power);

    // Energy integration (trapezoidal rule)
    if (dt > 0 && dt < 5000) { // Ignore gaps > 5s
        double avgPower = (power + m_lastVoltage * m_lastCurrent) / 2.0;
        m_energyWh += avgPower * (dt / 3600000.0); // ms to hours
    }

    m_lastVoltage = voltage;
    m_lastCurrent = current;

    // SoC tracking
    if (m_startSoC < 0.0 && soc >= 0.0) {
        m_startSoC = soc;
    }
    m_endSoC = soc;
}

int SessionReport::durationSeconds() const
{
    if (m_active) {
        return m_startTime.secsTo(QDateTime::currentDateTime());
    }
    return m_startTime.secsTo(m_endTime);
}

bool SessionReport::saveReport(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "=== CCS Charging Session Report ===\n\n";
    out << "Start Time:     " << m_startTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "End Time:       " << m_endTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "Duration:       " << durationSeconds() << " seconds\n\n";

    out << "--- Peak Values ---\n";
    out << "Max Voltage:    " << QString::number(m_maxVoltage, 'f', 1) << " V\n";
    out << "Max Current:    " << QString::number(m_maxCurrent, 'f', 1) << " A\n";
    out << "Max Power:      " << QString::number(m_maxPower / 1000.0, 'f', 2) << " kW\n\n";

    out << "--- Energy ---\n";
    out << "Energy Delivered: " << QString::number(m_energyWh, 'f', 1) << " Wh ("
        << QString::number(m_energyWh / 1000.0, 'f', 3) << " kWh)\n\n";

    out << "--- State of Charge ---\n";
    if (m_startSoC >= 0.0) {
        out << "Start SoC:      " << QString::number(m_startSoC, 'f', 1) << " %\n";
        out << "End SoC:        " << QString::number(m_endSoC, 'f', 1) << " %\n";
        out << "Delta SoC:      " << QString::number(m_endSoC - m_startSoC, 'f', 1) << " %\n";
    } else {
        out << "SoC data not available\n";
    }

    out << "\n=== End of Report ===\n";
    return true;
}

} // namespace ccs
