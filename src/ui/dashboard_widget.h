#pragma once

#include "module/charge_module.h"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QGroupBox>
#include <QListWidget>

namespace ccs {

class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget* parent = nullptr);

    void setChargeModule(ChargeModule* module);
    void updateDisplay();

signals:
    void startChargingRequested();
    void stopChargingRequested();
    void emergencyStopRequested();
    void parametersChanged();

private:
    void setupUi();
    QWidget* createValueCard(const QString& title, QLabel** valueLabel, QLabel** unitLabel, const QColor& accent);
    QGroupBox* createControlPanel();
    QGroupBox* createParameterPanel();
    QGroupBox* createInfoPanel();
    QGroupBox* createFaultPanel();
    void updateValueCard(QLabel* value, QLabel* unit, double val, const QString& unitStr, int decimals = 1);

    ChargeModule* m_module = nullptr;

    // Dashboard value labels
    QLabel* m_voltageValue = nullptr;
    QLabel* m_voltageUnit = nullptr;
    QLabel* m_currentValue = nullptr;
    QLabel* m_currentUnit = nullptr;
    QLabel* m_powerValue = nullptr;
    QLabel* m_powerUnit = nullptr;
    QLabel* m_socValue = nullptr;
    QLabel* m_socUnit = nullptr;

    // State display
    QLabel* m_stateLabel = nullptr;
    QLabel* m_protocolLabel = nullptr;
    QLabel* m_pilotStateLabel = nullptr;
    QLabel* m_isolationLabel = nullptr;
    QLabel* m_evseStatusLabel = nullptr;
    QLabel* m_compatLabel = nullptr;
    QLabel* m_voltMatchLabel = nullptr;

    // EVSE limits
    QLabel* m_evseMaxVLabel = nullptr;
    QLabel* m_evseMaxILabel = nullptr;
    QLabel* m_evseMaxPLabel = nullptr;

    // Controls
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_emergencyBtn = nullptr;

    // Parameter inputs
    QDoubleSpinBox* m_targetVoltage = nullptr;
    QDoubleSpinBox* m_targetCurrent = nullptr;
    QDoubleSpinBox* m_maxVoltage = nullptr;
    QDoubleSpinBox* m_maxCurrent = nullptr;
    QDoubleSpinBox* m_prechargeVoltage = nullptr;
    QDoubleSpinBox* m_socInput = nullptr;

    // Fault display
    QListWidget* m_faultList = nullptr;

    // SoC progress bar
    QProgressBar* m_socBar = nullptr;
};

} // namespace ccs
