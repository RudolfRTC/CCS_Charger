#include "ui/connection_widget.h"
#include "ui/theme.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QGroupBox>

namespace ccs {

ConnectionWidget::ConnectionWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ConnectionWidget::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(12);

    // Channel selection
    auto* channelGroup = new QGroupBox("CAN Interface");
    auto* channelLayout = new QHBoxLayout(channelGroup);

    m_simCheckbox = new QCheckBox("Simulation Mode");
    m_simCheckbox->setChecked(true);
    connect(m_simCheckbox, &QCheckBox::toggled, this, &ConnectionWidget::simulationToggled);
    channelLayout->addWidget(m_simCheckbox);

    m_channelCombo = new QComboBox();
    m_channelCombo->setMinimumWidth(180);
    m_channelCombo->setToolTip("Select PCAN channel");
    channelLayout->addWidget(m_channelCombo);

    m_refreshBtn = new QPushButton("Scan");
    m_refreshBtn->setToolTip("Scan for available PCAN channels");
    m_refreshBtn->setMaximumWidth(70);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ConnectionWidget::refreshChannelsRequested);
    channelLayout->addWidget(m_refreshBtn);

    // Baud rate
    m_baudRateCombo = new QComboBox();
    m_baudRateCombo->addItem("500 kbit/s", 500000);
    m_baudRateCombo->addItem("250 kbit/s", 250000);
    m_baudRateCombo->addItem("125 kbit/s", 125000);
    m_baudRateCombo->addItem("1 Mbit/s", 1000000);
    m_baudRateCombo->setCurrentIndex(0); // 500k default per datasheet
    channelLayout->addWidget(m_baudRateCombo);

    // Connect button
    m_connectBtn = new QPushButton("Connect");
    m_connectBtn->setMinimumWidth(100);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        if (m_connected)
            emit disconnectRequested();
        else
            emit connectRequested();
    });
    channelLayout->addWidget(m_connectBtn);

    mainLayout->addWidget(channelGroup, 1);

    // Status indicators
    auto* statusGroup = new QGroupBox("Status");
    auto* statusLayout = new QHBoxLayout(statusGroup);

    m_statusIndicator = new QLabel();
    m_statusIndicator->setFixedSize(14, 14);
    m_statusIndicator->setStyleSheet(QString("background-color: %1; border-radius: 7px; border: 1px solid %2;")
        .arg(Theme::AccentRed.name(), Theme::Border.name()));
    statusLayout->addWidget(m_statusIndicator);

    m_statusLabel = new QLabel("Disconnected");
    m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(Theme::TextSecondary.name()));
    statusLayout->addWidget(m_statusLabel);

    statusLayout->addSpacing(12);

    auto* hbLabel = new QLabel("HB:");
    hbLabel->setStyleSheet(QString("color: %1;").arg(Theme::TextSecondary.name()));
    statusLayout->addWidget(hbLabel);

    m_heartbeatIndicator = new QLabel();
    m_heartbeatIndicator->setFixedSize(14, 14);
    m_heartbeatIndicator->setStyleSheet(QString("background-color: %1; border-radius: 7px; border: 1px solid %2;")
        .arg(Theme::TextDisabled.name(), Theme::Border.name()));
    statusLayout->addWidget(m_heartbeatIndicator);

    statusLayout->addSpacing(12);

    m_moduleInfoLabel = new QLabel("Module: --");
    m_moduleInfoLabel->setStyleSheet(QString("color: %1;").arg(Theme::TextSecondary.name()));
    statusLayout->addWidget(m_moduleInfoLabel);

    mainLayout->addWidget(statusGroup, 0);
}

void ConnectionWidget::setChannels(const std::vector<CanInterface::ChannelInfo>& channels)
{
    m_channelCombo->clear();
    for (const auto& ch : channels) {
        m_channelCombo->addItem(ch.name, ch.handle);
    }
}

void ConnectionWidget::setConnected(bool connected)
{
    m_connected = connected;
    m_connectBtn->setText(connected ? "Disconnect" : "Connect");
    m_channelCombo->setEnabled(!connected);
    m_baudRateCombo->setEnabled(!connected);
    m_simCheckbox->setEnabled(!connected);
    m_refreshBtn->setEnabled(!connected);

    if (connected) {
        m_statusIndicator->setStyleSheet(QString("background-color: %1; border-radius: 7px; border: 1px solid %2;")
            .arg(Theme::AccentGreen.name(), Theme::Border.name()));
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(Theme::AccentGreen.name()));
    } else {
        m_statusIndicator->setStyleSheet(QString("background-color: %1; border-radius: 7px; border: 1px solid %2;")
            .arg(Theme::AccentRed.name(), Theme::Border.name()));
        m_statusLabel->setText("Disconnected");
        m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(Theme::TextSecondary.name()));
    }
}

void ConnectionWidget::setStatus(CanStatus status)
{
    QColor color;
    switch (status) {
        case CanStatus::Ok:          color = Theme::AccentGreen; break;
        case CanStatus::BusWarning:  color = Theme::AccentYellow; break;
        case CanStatus::BusPassive:  color = Theme::AccentOrange; break;
        case CanStatus::BusOff:      color = Theme::AccentRed; break;
        case CanStatus::Error:       color = Theme::AccentRed; break;
        case CanStatus::Disconnected: color = Theme::TextDisabled; break;
    }
    m_statusIndicator->setStyleSheet(QString("background-color: %1; border-radius: 7px; border: 1px solid %2;")
        .arg(color.name(), Theme::Border.name()));
    m_statusLabel->setText(canStatusToString(status));
}

void ConnectionWidget::setHeartbeat(bool ok)
{
    QColor color = ok ? Theme::AccentGreen : Theme::AccentRed;
    m_heartbeatIndicator->setStyleSheet(QString("background-color: %1; border-radius: 7px; border: 1px solid %2;")
        .arg(color.name(), Theme::Border.name()));
}

void ConnectionWidget::setModuleInfo(const QString& version)
{
    m_moduleInfoLabel->setText("Module: " + version);
}

uint16_t ConnectionWidget::selectedChannel() const
{
    return m_channelCombo->currentData().toUInt();
}

uint32_t ConnectionWidget::selectedBaudRate() const
{
    return m_baudRateCombo->currentData().toUInt();
}

bool ConnectionWidget::useSimulation() const
{
    return m_simCheckbox->isChecked();
}

} // namespace ccs
