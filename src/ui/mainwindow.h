#pragma once

#include "module/charge_module.h"
#include "can/can_interface.h"
#include "can/pcan_driver.h"
#include "logging/can_logger.h"
#include "logging/session_report.h"
#include "ui/connection_widget.h"
#include "ui/dashboard_widget.h"
#include "ui/chart_widget.h"
#include "ui/expert_widget.h"

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>

namespace ccs {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnect();
    void onDisconnect();
    void onRefreshChannels();
    void onSimulationToggled(bool enabled);
    void onStartCharging();
    void onStopCharging();
    void onEmergencyStop();
    void onStateChanged(CmsState state);
    void onErrorCode(uint16_t code, const QString& desc);
    void onStatusUpdate();

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void initModule();

    // Core
    ChargeModule* m_module = nullptr;
    CanInterface* m_canInterface = nullptr;
    PcanDriver* m_pcanDriver = nullptr;
    SimulatedCanInterface* m_simInterface = nullptr;

    // Logging
    CanLogger* m_logger = nullptr;
    SessionReport* m_sessionReport = nullptr;

    // UI
    ConnectionWidget* m_connectionWidget = nullptr;
    DashboardWidget* m_dashboardWidget = nullptr;
    ChartWidget* m_chartWidget = nullptr;
    ExpertWidget* m_expertWidget = nullptr;
    QTabWidget* m_tabWidget = nullptr;

    // Status bar
    QLabel* m_statusState = nullptr;
    QLabel* m_statusFrames = nullptr;
    QLabel* m_statusSession = nullptr;
    QTimer* m_statusTimer = nullptr;

    uint64_t m_rxFrameCount = 0;
    uint64_t m_txFrameCount = 0;
    bool m_useSimulation = true;

    QString m_dbcPath;
};

} // namespace ccs
