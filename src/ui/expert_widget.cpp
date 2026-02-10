#include "ui/expert_widget.h"
#include "ui/theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QLabel>
#include <QDateTime>

namespace ccs {

ExpertWidget::ExpertWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ExpertWidget::setChargeModule(ChargeModule* module)
{
    m_module = module;
    if (m_module) {
        connect(m_module, &ChargeModule::rawFrameReceived, this, &ExpertWidget::onRawFrameReceived);
        connect(m_module, &ChargeModule::rawFrameSent, this, &ExpertWidget::onRawFrameSent);
        connect(m_module, &ChargeModule::evseDataUpdated, this, &ExpertWidget::onDecodedUpdate);
    }
}

void ExpertWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    auto* splitter = new QSplitter(Qt::Vertical);

    // ─── Raw CAN frames section ──────────────
    auto* rawWidget = new QWidget();
    auto* rawLayout = new QVBoxLayout(rawWidget);
    rawLayout->setContentsMargins(0, 0, 0, 0);

    auto* rawHeader = new QHBoxLayout();
    auto* rawTitle = new QLabel("Raw CAN Frames");
    rawTitle->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold;").arg(Theme::AccentCyan.name()));
    rawHeader->addWidget(rawTitle);
    rawHeader->addStretch();

    m_autoScrollCheck = new QCheckBox("Auto-scroll");
    m_autoScrollCheck->setChecked(true);
    rawHeader->addWidget(m_autoScrollCheck);

    m_clearRawBtn = new QPushButton("Clear");
    m_clearRawBtn->setMaximumWidth(60);
    connect(m_clearRawBtn, &QPushButton::clicked, this, [this]() {
        m_rawTable->setRowCount(0);
        m_rawRowCount = 0;
    });
    rawHeader->addWidget(m_clearRawBtn);

    rawLayout->addLayout(rawHeader);

    m_rawTable = new QTableWidget(0, 6);
    m_rawTable->setHorizontalHeaderLabels({"Time", "Dir", "CAN ID", "Ext", "DLC", "Data"});
    m_rawTable->horizontalHeader()->setStretchLastSection(true);
    m_rawTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_rawTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_rawTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_rawTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_rawTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_rawTable->setAlternatingRowColors(true);
    m_rawTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rawTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rawTable->verticalHeader()->setDefaultSectionSize(20);
    m_rawTable->verticalHeader()->hide();
    rawLayout->addWidget(m_rawTable);

    splitter->addWidget(rawWidget);

    // ─── Decoded signals section ─────────────
    auto* decodedWidget = new QWidget();
    auto* decodedLayout = new QVBoxLayout(decodedWidget);
    decodedLayout->setContentsMargins(0, 0, 0, 0);

    auto* decodedTitle = new QLabel("Decoded Signals (Live)");
    decodedTitle->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold;").arg(Theme::AccentCyan.name()));
    decodedLayout->addWidget(decodedTitle);

    m_decodedTable = new QTableWidget(0, 5);
    m_decodedTable->setHorizontalHeaderLabels({"Message", "Signal", "Physical Value", "Raw", "Description"});
    m_decodedTable->horizontalHeader()->setStretchLastSection(true);
    m_decodedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_decodedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_decodedTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_decodedTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_decodedTable->setAlternatingRowColors(true);
    m_decodedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_decodedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_decodedTable->verticalHeader()->setDefaultSectionSize(20);
    m_decodedTable->verticalHeader()->hide();
    m_decodedTable->setColumnWidth(0, 200);
    m_decodedTable->setColumnWidth(1, 250);
    decodedLayout->addWidget(m_decodedTable);

    splitter->addWidget(decodedWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
}

void ExpertWidget::addRawRow(const QString& dir, const CanFrame& frame)
{
    // Limit table size
    if (m_rawRowCount >= MAX_RAW_ROWS) {
        m_rawTable->removeRow(0);
        m_rawRowCount--;
    }

    int row = m_rawTable->rowCount();
    m_rawTable->insertRow(row);

    auto setItem = [&](int col, const QString& text, const QColor& color = Theme::TextPrimary) {
        auto* item = new QTableWidgetItem(text);
        item->setForeground(color);
        m_rawTable->setItem(row, col, item);
    };

    setItem(0, QTime::currentTime().toString("hh:mm:ss.zzz"), Theme::TextSecondary);
    setItem(1, dir, dir == "TX" ? Theme::AccentGreen : Theme::AccentCyan);
    setItem(2, "0x" + frame.idString(), Theme::AccentCyan);
    setItem(3, frame.extended ? "EXT" : "STD", Theme::TextSecondary);
    setItem(4, QString::number(frame.dlc), Theme::TextSecondary);
    setItem(5, frame.toHexString(), Theme::TextPrimary);

    m_rawRowCount++;

    if (m_autoScrollCheck->isChecked()) {
        m_rawTable->scrollToBottom();
    }
}

void ExpertWidget::onRawFrameReceived(const CanFrame& frame)
{
    addRawRow("RX", frame);
}

void ExpertWidget::onRawFrameSent(const CanFrame& frame)
{
    addRawRow("TX", frame);
}

void ExpertWidget::onDecodedUpdate()
{
    if (!m_module) return;

    const auto& evse = m_module->evseData();
    const auto& ev = m_module->evParams();

    // Build a flat list of all known signal values
    struct SigRow {
        QString msg;
        QString signal;
        QString physical;
        QString raw;
        QString desc;
    };

    QVector<SigRow> rows;

    auto addRow = [&](const QString& msg, const QString& sig, double phys, const QString& unit, const QString& desc = "") {
        rows.append({msg, sig, QString("%1 %2").arg(phys, 0, 'f', 2).arg(unit), "", desc});
    };

    auto addRowEnum = [&](const QString& msg, const QString& sig, int raw, const QString& desc) {
        rows.append({msg, sig, desc, QString::number(raw), ""});
    };

    // ChargeInfo
    addRowEnum("ChargeInfo", "StateMachineState", static_cast<int>(evse.stateMachineState), cmsStateToString(evse.stateMachineState));
    addRowEnum("ChargeInfo", "AliveCounter", evse.aliveCounter, "");
    addRowEnum("ChargeInfo", "ControlPilotState", static_cast<int>(evse.controlPilotState), "");
    addRow("ChargeInfo", "ControlPilotDutyCycle", evse.controlPilotDutyCycle, "%");
    addRowEnum("ChargeInfo", "ActualChargeProtocol", static_cast<int>(evse.actualChargeProtocol), "");
    addRowEnum("ChargeInfo", "VoltageMatch", evse.voltageMatch ? 1 : 0, evse.voltageMatch ? "True" : "False");
    addRowEnum("ChargeInfo", "EVSECompatible", evse.evseCompatible ? 1 : 0, evse.evseCompatible ? "True" : "False");
    addRowEnum("ChargeInfo", "TCPStatus", evse.tcpConnected ? 1 : 0, evse.tcpConnected ? "Connected" : "Not Connected");

    // EVSEDCStatus
    addRow("EVSEDCStatus", "EVSEPresentVoltage", evse.evsePresentVoltage, "V");
    addRow("EVSEDCStatus", "EVSEPresentCurrent", evse.evsePresentCurrent, "A");
    addRowEnum("EVSEDCStatus", "EVSEIsolationStatus", static_cast<int>(evse.evseIsolationStatus), "");
    addRowEnum("EVSEDCStatus", "EVSEStatusCode", static_cast<int>(evse.evseStatusCode), "");

    // EVSEDCMaxLimits
    addRow("EVSEDCMaxLimits", "EVSEMaxVoltage", evse.evseMaxVoltage, "V");
    addRow("EVSEDCMaxLimits", "EVSEMaxCurrent", evse.evseMaxCurrent, "A");
    addRow("EVSEDCMaxLimits", "EVSEMaxPower", evse.evseMaxPower, "W");

    // ErrorCodes
    addRowEnum("ErrorCodes", "ErrorCodeLevel0", evse.errorCode0, SafetyMonitor::errorCodeDescription(evse.errorCode0));

    // EV Parameters (our output)
    addRow("EVDCMaxLimits", "EVMaxVoltage", ev.evMaxVoltage, "V");
    addRow("EVDCMaxLimits", "EVMaxCurrent", ev.evMaxCurrent, "A");
    addRow("EVDCChargeTargets", "EVTargetVoltage", ev.evTargetVoltage, "V");
    addRow("EVDCChargeTargets", "EVTargetCurrent", ev.evTargetCurrent, "A");
    addRow("EVDCChargeTargets", "EVPreChargeVoltage", ev.evPreChargeVoltage, "V");
    addRowEnum("EVStatusControl", "EVReady", ev.evReady ? 1 : 0, ev.evReady ? "True" : "False");
    addRowEnum("EVStatusControl", "ChargeProgressIndication", static_cast<int>(ev.chargeProgress), "");
    addRowEnum("EVStatusControl", "ChargeStopIndication", static_cast<int>(ev.chargeStop), "");
    addRow("EVStatusDisplay", "EVSoC", ev.evSoC, "%");

    // Populate table
    m_decodedTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        auto setItem = [&](int col, const QString& text, const QColor& color = Theme::TextPrimary) {
            auto* item = new QTableWidgetItem(text);
            item->setForeground(color);
            m_decodedTable->setItem(i, col, item);
        };

        setItem(0, rows[i].msg, Theme::AccentCyan);
        setItem(1, rows[i].signal, Theme::TextPrimary);
        setItem(2, rows[i].physical, Theme::AccentGreen);
        setItem(3, rows[i].raw, Theme::TextSecondary);
        setItem(4, rows[i].desc, Theme::TextSecondary);
    }
}

} // namespace ccs
