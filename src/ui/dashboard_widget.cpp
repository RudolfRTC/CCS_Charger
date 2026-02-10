#include "ui/dashboard_widget.h"
#include "ui/theme.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QTime>

namespace ccs {

DashboardWidget::DashboardWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DashboardWidget::setChargeModule(ChargeModule* module)
{
    m_module = module;

    if (m_module) {
        connect(m_module, &ChargeModule::evseDataUpdated, this, &DashboardWidget::updateDisplay);

        // Wire controls to module
        connect(m_startBtn, &QPushButton::clicked, this, &DashboardWidget::startChargingRequested);
        connect(m_stopBtn, &QPushButton::clicked, this, &DashboardWidget::stopChargingRequested);
        connect(m_emergencyBtn, &QPushButton::clicked, this, &DashboardWidget::emergencyStopRequested);

        // Wire parameter changes
        auto applyParams = [this]() {
            if (!m_module) return;
            m_module->setEvTargetVoltage(m_targetVoltage->value());
            m_module->setEvTargetCurrent(m_targetCurrent->value());
            m_module->setEvMaxVoltage(m_maxVoltage->value());
            m_module->setEvMaxCurrent(m_maxCurrent->value());
            m_module->setEvPreChargeVoltage(m_prechargeVoltage->value());
            m_module->setEvSoC(m_socInput->value());
            emit parametersChanged();
        };

        connect(m_targetVoltage, &QDoubleSpinBox::editingFinished, applyParams);
        connect(m_targetCurrent, &QDoubleSpinBox::editingFinished, applyParams);
        connect(m_maxVoltage, &QDoubleSpinBox::editingFinished, applyParams);
        connect(m_maxCurrent, &QDoubleSpinBox::editingFinished, applyParams);
        connect(m_prechargeVoltage, &QDoubleSpinBox::editingFinished, applyParams);
        connect(m_socInput, &QDoubleSpinBox::editingFinished, applyParams);
    }
}

void DashboardWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // Top row: Big value cards
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(12);

    cardsLayout->addWidget(createValueCard("PRESENT VOLTAGE", &m_voltageValue, &m_voltageUnit, Theme::AccentCyan));
    cardsLayout->addWidget(createValueCard("PRESENT CURRENT", &m_currentValue, &m_currentUnit, Theme::AccentBlue));
    cardsLayout->addWidget(createValueCard("POWER", &m_powerValue, &m_powerUnit, Theme::AccentPurple));
    cardsLayout->addWidget(createValueCard("STATE OF CHARGE", &m_socValue, &m_socUnit, Theme::AccentGreen));
    mainLayout->addLayout(cardsLayout);

    // SoC bar
    m_socBar = new QProgressBar();
    m_socBar->setRange(0, 100);
    m_socBar->setValue(0);
    m_socBar->setTextVisible(true);
    m_socBar->setFixedHeight(20);
    mainLayout->addWidget(m_socBar);

    // Middle row: State + Controls + Parameters
    auto* midLayout = new QHBoxLayout();
    midLayout->setSpacing(12);

    midLayout->addWidget(createInfoPanel(), 1);
    midLayout->addWidget(createControlPanel(), 0);
    midLayout->addWidget(createParameterPanel(), 1);
    midLayout->addWidget(createFaultPanel(), 1);

    mainLayout->addLayout(midLayout, 1);
}

QWidget* DashboardWidget::createValueCard(const QString& title, QLabel** valueLabel, QLabel** unitLabel, const QColor& accent)
{
    auto* card = new QFrame();
    card->setStyleSheet(QString(
        "QFrame { background-color: %1; border: 1px solid %2; border-radius: 8px; }"
    ).arg(Theme::BgLight.name(), Theme::Border.name()));
    card->setMinimumHeight(100);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);

    auto* titleLbl = new QLabel(title);
    titleLbl->setObjectName("titleLabel");
    titleLbl->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; letter-spacing: 1px;").arg(accent.name()));
    layout->addWidget(titleLbl);

    auto* valLayout = new QHBoxLayout();
    valLayout->setAlignment(Qt::AlignBottom | Qt::AlignLeft);

    *valueLabel = new QLabel("---");
    (*valueLabel)->setObjectName("valueLabel");
    (*valueLabel)->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(Theme::TextPrimary.name()));
    valLayout->addWidget(*valueLabel);

    *unitLabel = new QLabel("");
    (*unitLabel)->setObjectName("unitLabel");
    (*unitLabel)->setStyleSheet(QString("color: %1; font-size: 16px; margin-bottom: 6px;").arg(Theme::TextSecondary.name()));
    valLayout->addWidget(*unitLabel);
    valLayout->addStretch();

    layout->addLayout(valLayout);

    return card;
}

QGroupBox* DashboardWidget::createInfoPanel()
{
    auto* group = new QGroupBox("Module Status");
    auto* layout = new QGridLayout(group);
    layout->setSpacing(6);

    int row = 0;

    auto addRow = [&](const QString& label, QLabel** valueLbl, const QColor& color = Theme::TextPrimary) {
        auto* lbl = new QLabel(label + ":");
        lbl->setStyleSheet(QString("color: %1; font-size: 11px;").arg(Theme::TextSecondary.name()));
        *valueLbl = new QLabel("--");
        (*valueLbl)->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(color.name()));
        layout->addWidget(lbl, row, 0);
        layout->addWidget(*valueLbl, row, 1);
        row++;
    };

    addRow("State Machine", &m_stateLabel, Theme::AccentCyan);
    addRow("Protocol", &m_protocolLabel);
    addRow("Control Pilot", &m_pilotStateLabel);
    addRow("Isolation", &m_isolationLabel);
    addRow("EVSE Status", &m_evseStatusLabel);
    addRow("Compatible", &m_compatLabel);
    addRow("Voltage Match", &m_voltMatchLabel);

    layout->addWidget(new QLabel(""), row, 0); // spacer
    row++;

    auto addEvseRow = [&](const QString& label, QLabel** valueLbl) {
        auto* lbl = new QLabel(label + ":");
        lbl->setStyleSheet(QString("color: %1; font-size: 11px;").arg(Theme::TextSecondary.name()));
        *valueLbl = new QLabel("--");
        (*valueLbl)->setStyleSheet(QString("color: %1; font-size: 11px;").arg(Theme::TextPrimary.name()));
        layout->addWidget(lbl, row, 0);
        layout->addWidget(*valueLbl, row, 1);
        row++;
    };

    addEvseRow("EVSE Max V", &m_evseMaxVLabel);
    addEvseRow("EVSE Max I", &m_evseMaxILabel);
    addEvseRow("EVSE Max P", &m_evseMaxPLabel);

    layout->setRowStretch(row, 1);
    return group;
}

QGroupBox* DashboardWidget::createControlPanel()
{
    auto* group = new QGroupBox("Charge Control");
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    m_startBtn = new QPushButton("START CHARGING");
    m_startBtn->setObjectName("startButton");
    m_startBtn->setMinimumHeight(50);
    layout->addWidget(m_startBtn);

    m_stopBtn = new QPushButton("STOP CHARGING");
    m_stopBtn->setObjectName("stopButton");
    m_stopBtn->setMinimumHeight(50);
    layout->addWidget(m_stopBtn);

    layout->addStretch();

    m_emergencyBtn = new QPushButton("EMERGENCY STOP");
    m_emergencyBtn->setObjectName("emergencyButton");
    m_emergencyBtn->setMinimumHeight(60);
    layout->addWidget(m_emergencyBtn);

    return group;
}

QGroupBox* DashboardWidget::createParameterPanel()
{
    auto* group = new QGroupBox("EV Parameters");
    auto* layout = new QGridLayout(group);
    layout->setSpacing(6);

    int row = 0;
    auto addParam = [&](const QString& label, QDoubleSpinBox** spin, double min, double max,
                        double step, int decimals, double defaultVal, const QString& suffix) {
        auto* lbl = new QLabel(label + ":");
        *spin = new QDoubleSpinBox();
        (*spin)->setRange(min, max);
        (*spin)->setSingleStep(step);
        (*spin)->setDecimals(decimals);
        (*spin)->setValue(defaultVal);
        (*spin)->setSuffix(" " + suffix);
        layout->addWidget(lbl, row, 0);
        layout->addWidget(*spin, row, 1);
        row++;
    };

    addParam("Target Voltage", &m_targetVoltage, 0, 1000, 1.0, 1, 400.0, "V");
    addParam("Target Current", &m_targetCurrent, 0, 500, 1.0, 1, 50.0, "A");
    addParam("Max Voltage", &m_maxVoltage, 0, 1000, 1.0, 1, 500.0, "V");
    addParam("Max Current", &m_maxCurrent, 0, 500, 1.0, 1, 200.0, "A");
    addParam("PreCharge Voltage", &m_prechargeVoltage, 0, 1000, 1.0, 1, 400.0, "V");
    addParam("Battery SoC", &m_socInput, 0, 100, 0.5, 1, 50.0, "%");

    layout->setRowStretch(row, 1);
    return group;
}

QGroupBox* DashboardWidget::createFaultPanel()
{
    auto* group = new QGroupBox("Faults & Events");
    auto* layout = new QVBoxLayout(group);

    m_faultList = new QListWidget();
    m_faultList->setStyleSheet(QString(
        "QListWidget { background-color: %1; border: 1px solid %2; font-family: 'Consolas', monospace; font-size: 11px; }"
        "QListWidget::item { padding: 4px; }"
    ).arg(Theme::BgLight.name(), Theme::Border.name()));
    m_faultList->setMaximumHeight(250);
    layout->addWidget(m_faultList);

    return group;
}

void DashboardWidget::updateDisplay()
{
    if (!m_module) return;

    const auto& evse = m_module->evseData();

    // Big value cards
    updateValueCard(m_voltageValue, m_voltageUnit, evse.evsePresentVoltage, "V");
    updateValueCard(m_currentValue, m_currentUnit, evse.evsePresentCurrent, "A");
    double power = evse.evsePresentVoltage * evse.evsePresentCurrent;
    if (power > 1000.0)
        updateValueCard(m_powerValue, m_powerUnit, power / 1000.0, "kW", 2);
    else
        updateValueCard(m_powerValue, m_powerUnit, power, "W", 0);

    updateValueCard(m_socValue, m_socUnit, m_module->evParams().evSoC, "%");
    m_socBar->setValue(static_cast<int>(m_module->evParams().evSoC));

    // State info
    m_stateLabel->setText(cmsStateToString(evse.stateMachineState));

    // Color code the state
    QColor stateColor = Theme::TextPrimary;
    switch (evse.stateMachineState) {
        case CmsState::Charge:    stateColor = Theme::AccentGreen; break;
        case CmsState::PreCharge: stateColor = Theme::AccentCyan; break;
        case CmsState::Error:     stateColor = Theme::AccentRed; break;
        case CmsState::StopCharge:
        case CmsState::SessionStop:
        case CmsState::ShutOff:   stateColor = Theme::AccentOrange; break;
        default: break;
    }
    m_stateLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(stateColor.name()));

    // Protocol
    const char* protoNames[] = {"Not Defined", "DIN 70121", "ISO 15118", "Not Supported"};
    int protoIdx = static_cast<int>(evse.actualChargeProtocol);
    m_protocolLabel->setText(protoIdx < 4 ? protoNames[protoIdx] : "SNA");

    // Control Pilot
    const char* cpNames[] = {"A", "B", "C", "D", "E", "F"};
    int cpIdx = static_cast<int>(evse.controlPilotState);
    m_pilotStateLabel->setText(cpIdx < 6 ? cpNames[cpIdx] : "SNA");

    // Isolation
    const char* isoNames[] = {"Invalid", "Valid", "Warning", "Fault", "No IMD", "Checking"};
    int isoIdx = static_cast<int>(evse.evseIsolationStatus);
    m_isolationLabel->setText(isoIdx < 6 ? isoNames[isoIdx] : "SNA");
    QColor isoColor = (isoIdx == 1) ? Theme::AccentGreen : (isoIdx == 3) ? Theme::AccentRed : Theme::TextPrimary;
    m_isolationLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(isoColor.name()));

    // EVSE Status
    const char* statusNames[] = {"Not Ready", "Ready", "Shutdown", "Utility Interrupt", "Isolation Active", "Emergency", "Malfunction"};
    int stIdx = static_cast<int>(evse.evseStatusCode);
    m_evseStatusLabel->setText(stIdx < 7 ? statusNames[stIdx] : "SNA");

    // Compatible & VoltageMatch
    m_compatLabel->setText(evse.evseCompatible ? "Yes" : "No");
    m_compatLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;")
        .arg(evse.evseCompatible ? Theme::AccentGreen.name() : Theme::AccentRed.name()));
    m_voltMatchLabel->setText(evse.voltageMatch ? "Yes" : "No");
    m_voltMatchLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;")
        .arg(evse.voltageMatch ? Theme::AccentGreen.name() : Theme::TextSecondary.name()));

    // EVSE limits
    m_evseMaxVLabel->setText(QString("%1 V").arg(evse.evseMaxVoltage, 0, 'f', 1));
    m_evseMaxILabel->setText(QString("%1 A").arg(evse.evseMaxCurrent, 0, 'f', 1));
    m_evseMaxPLabel->setText(QString("%1 kW").arg(evse.evseMaxPower / 1000.0, 0, 'f', 1));

    // Error codes
    if (evse.errorCode0 > 1) {
        QString errMsg = QString("[%1] Error 0x%2: %3")
            .arg(QTime::currentTime().toString("hh:mm:ss"))
            .arg(evse.errorCode0, 4, 16, QChar('0'))
            .arg(SafetyMonitor::errorCodeDescription(evse.errorCode0));

        // Avoid duplicate entries
        if (m_faultList->count() == 0 || m_faultList->item(0)->text() != errMsg) {
            m_faultList->insertItem(0, errMsg);
            m_faultList->item(0)->setForeground(Theme::AccentRed);
            if (m_faultList->count() > 100) {
                delete m_faultList->takeItem(m_faultList->count() - 1);
            }
        }
    }
}

void DashboardWidget::updateValueCard(QLabel* value, QLabel* unit, double val, const QString& unitStr, int decimals)
{
    value->setText(QString::number(val, 'f', decimals));
    unit->setText(unitStr);
}

} // namespace ccs
