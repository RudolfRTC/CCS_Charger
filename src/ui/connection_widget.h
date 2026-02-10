#pragma once

#include "can/can_interface.h"
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

namespace ccs {

class ConnectionWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionWidget(QWidget* parent = nullptr);

    void setChannels(const std::vector<CanInterface::ChannelInfo>& channels);
    void setConnected(bool connected);
    void setStatus(CanStatus status);
    void setHeartbeat(bool ok);
    void setModuleInfo(const QString& version);

    uint16_t selectedChannel() const;
    uint32_t selectedBaudRate() const;
    bool useSimulation() const;

signals:
    void connectRequested();
    void disconnectRequested();
    void refreshChannelsRequested();
    void simulationToggled(bool enabled);

private:
    void setupUi();

    QComboBox* m_channelCombo = nullptr;
    QComboBox* m_baudRateCombo = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QCheckBox* m_simCheckbox = nullptr;
    QLabel* m_statusIndicator = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_heartbeatIndicator = nullptr;
    QLabel* m_moduleInfoLabel = nullptr;
    bool m_connected = false;
};

} // namespace ccs
