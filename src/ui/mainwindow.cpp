#include "ui/mainwindow.h"
#include "ui/theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>

namespace ccs {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("CCS Charger - Charge Module S Controller");
    resize(1400, 900);
    setMinimumSize(1100, 700);

    // Apply theme
    qApp->setStyleSheet(Theme::globalStyleSheet());

    // Create core objects
    m_module = new ChargeModule(this);
    m_pcanDriver = new PcanDriver(this);
    m_simInterface = new SimulatedCanInterface(this);
    m_logger = new CanLogger(this);
    m_sessionReport = new SessionReport(this);

    // Find DBC file
    QDir appDir(QCoreApplication::applicationDirPath());
    // Check several locations for the DBC file
    QStringList dbcSearchPaths = {
        appDir.filePath("ISC_CMS_Automotive.dbc"),
        appDir.filePath("../ISC_CMS_Automotive.dbc"),
        appDir.filePath("../../ISC_CMS_Automotive.dbc"),
        QDir::currentPath() + "/ISC_CMS_Automotive.dbc",
    };

    for (const auto& path : dbcSearchPaths) {
        if (QFile::exists(path)) {
            m_dbcPath = path;
            break;
        }
    }

    setupUi();
    setupMenuBar();
    setupStatusBar();
    initModule();

    // Status update timer
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::onStatusUpdate);
    m_statusTimer->start(250);
}

MainWindow::~MainWindow()
{
    if (m_canInterface && m_canInterface->isOpen()) {
        m_module->stop();
        m_canInterface->close();
    }
    m_logger->stopAll();
}

void MainWindow::setupUi()
{
    auto* centralWidget = new QWidget();
    centralWidget->setStyleSheet(QString("background-color: %1;").arg(Theme::BgDark.name()));
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Connection bar
    m_connectionWidget = new ConnectionWidget();
    m_connectionWidget->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;")
        .arg(Theme::BgMedium.name(), Theme::Border.name()));
    m_connectionWidget->setMaximumHeight(80);
    mainLayout->addWidget(m_connectionWidget);

    // Tab widget
    m_tabWidget = new QTabWidget();

    // Dashboard tab
    auto* dashPage = new QWidget();
    auto* dashLayout = new QVBoxLayout(dashPage);
    dashLayout->setContentsMargins(0, 0, 0, 0);
    dashLayout->setSpacing(0);

    m_dashboardWidget = new DashboardWidget();
    dashLayout->addWidget(m_dashboardWidget, 2);

    m_chartWidget = new ChartWidget();
    m_chartWidget->setMinimumHeight(200);
    dashLayout->addWidget(m_chartWidget, 1);

    m_tabWidget->addTab(dashPage, "Dashboard");

    // Expert tab
    m_expertWidget = new ExpertWidget();
    m_tabWidget->addTab(m_expertWidget, "Expert");

    mainLayout->addWidget(m_tabWidget, 1);

    setCentralWidget(centralWidget);

    // Wire connection signals
    connect(m_connectionWidget, &ConnectionWidget::connectRequested, this, &MainWindow::onConnect);
    connect(m_connectionWidget, &ConnectionWidget::disconnectRequested, this, &MainWindow::onDisconnect);
    connect(m_connectionWidget, &ConnectionWidget::refreshChannelsRequested, this, &MainWindow::onRefreshChannels);
    connect(m_connectionWidget, &ConnectionWidget::simulationToggled, this, &MainWindow::onSimulationToggled);

    // Wire dashboard signals
    connect(m_dashboardWidget, &DashboardWidget::startChargingRequested, this, &MainWindow::onStartCharging);
    connect(m_dashboardWidget, &DashboardWidget::stopChargingRequested, this, &MainWindow::onStopCharging);
    connect(m_dashboardWidget, &DashboardWidget::emergencyStopRequested, this, &MainWindow::onEmergencyStop);
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* loadDbcAction = new QAction("Load &DBC File...", this);
    connect(loadDbcAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Open DBC File", "", "DBC Files (*.dbc);;All Files (*)");
        if (!path.isEmpty()) {
            m_dbcPath = path;
            m_module->loadDbc(m_dbcPath);
        }
    });
    fileMenu->addAction(loadDbcAction);

    fileMenu->addSeparator();

    auto* startRawLogAction = new QAction("Start &Raw CAN Log...", this);
    connect(startRawLogAction, &QAction::triggered, this, [this]() {
        QString defaultName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_raw.csv";
        QString path = QFileDialog::getSaveFileName(this, "Save Raw CAN Log", defaultName, "CSV Files (*.csv)");
        if (!path.isEmpty()) {
            m_logger->startRawLog(path);
        }
    });
    fileMenu->addAction(startRawLogAction);

    auto* startDecodedLogAction = new QAction("Start &Decoded Signal Log...", this);
    connect(startDecodedLogAction, &QAction::triggered, this, [this]() {
        QString defaultName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_decoded.csv";
        QString path = QFileDialog::getSaveFileName(this, "Save Decoded Log", defaultName, "CSV Files (*.csv)");
        if (!path.isEmpty()) {
            m_logger->startDecodedLog(path);
        }
    });
    fileMenu->addAction(startDecodedLogAction);

    auto* stopLogAction = new QAction("Stop All &Logging", this);
    connect(stopLogAction, &QAction::triggered, this, [this]() {
        m_logger->stopAll();
    });
    fileMenu->addAction(stopLogAction);

    fileMenu->addSeparator();

    auto* saveReportAction = new QAction("Save Session &Report...", this);
    connect(saveReportAction, &QAction::triggered, this, [this]() {
        QString defaultName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_session_report.txt";
        QString path = QFileDialog::getSaveFileName(this, "Save Session Report", defaultName, "Text Files (*.txt)");
        if (!path.isEmpty()) {
            m_sessionReport->saveReport(path);
        }
    });
    fileMenu->addAction(saveReportAction);

    fileMenu->addSeparator();

    auto* exitAction = new QAction("E&xit", this);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    fileMenu->addAction(exitAction);

    // Module menu
    auto* moduleMenu = menuBar()->addMenu("&Module");

    auto* resetAction = new QAction("&Reset Module", this);
    connect(resetAction, &QAction::triggered, this, [this]() {
        if (m_module) m_module->resetModule();
    });
    moduleMenu->addAction(resetAction);

    // Style the menubar
    menuBar()->setStyleSheet(QString(
        "QMenuBar { background-color: %1; color: %2; border-bottom: 1px solid %3; }"
        "QMenuBar::item { padding: 4px 12px; }"
        "QMenuBar::item:selected { background-color: %4; }"
        "QMenu { background-color: %1; color: %2; border: 1px solid %3; }"
        "QMenu::item { padding: 6px 24px; }"
        "QMenu::item:selected { background-color: %4; }"
    ).arg(Theme::BgMedium.name(), Theme::TextPrimary.name(), Theme::Border.name(), Theme::BgHighlight.name()));
}

void MainWindow::setupStatusBar()
{
    statusBar()->setStyleSheet(QString(
        "QStatusBar { background-color: %1; color: %2; border-top: 1px solid %3; font-size: 11px; }"
    ).arg(Theme::BgLight.name(), Theme::TextSecondary.name(), Theme::Border.name()));

    m_statusState = new QLabel("State: --");
    m_statusState->setStyleSheet(QString("color: %1; font-weight: bold; padding: 0 12px;").arg(Theme::AccentCyan.name()));
    statusBar()->addWidget(m_statusState);

    m_statusFrames = new QLabel("RX: 0 | TX: 0");
    m_statusFrames->setStyleSheet(QString("color: %1; padding: 0 12px;").arg(Theme::TextSecondary.name()));
    statusBar()->addWidget(m_statusFrames);

    m_statusSession = new QLabel("Session: Idle");
    m_statusSession->setStyleSheet(QString("color: %1; padding: 0 12px;").arg(Theme::TextSecondary.name()));
    statusBar()->addPermanentWidget(m_statusSession);
}

void MainWindow::initModule()
{
    // Load DBC if found
    if (!m_dbcPath.isEmpty()) {
        m_module->loadDbc(m_dbcPath);
        m_logger->setCodec(&m_module->codec());
        qDebug() << "DBC loaded from:" << m_dbcPath;
    } else {
        qWarning() << "DBC file not found - CAN decoding will be limited";
    }

    // Wire module to UI
    m_dashboardWidget->setChargeModule(m_module);
    m_expertWidget->setChargeModule(m_module);

    // Wire module signals
    connect(m_module, &ChargeModule::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_module, &ChargeModule::errorCodeReceived, this, &MainWindow::onErrorCode);

    // Wire frame logging
    connect(m_module, &ChargeModule::rawFrameReceived, m_logger, &CanLogger::logFrame);

    // Wire safety monitor to UI
    connect(m_module->safetyMonitor(), &SafetyMonitor::emergencyStopTriggered, this, [this](const QString& reason) {
        m_connectionWidget->setStatus(CanStatus::Error);
        statusBar()->showMessage("EMERGENCY STOP: " + reason, 10000);
    });

    connect(m_module->safetyMonitor(), &SafetyMonitor::heartbeatLost, this, [this]() {
        m_connectionWidget->setHeartbeat(false);
    });

    connect(m_module->safetyMonitor(), &SafetyMonitor::heartbeatRestored, this, [this]() {
        m_connectionWidget->setHeartbeat(true);
    });

    // Initialize simulation channels
    onSimulationToggled(true);
}

void MainWindow::onConnect()
{
    if (m_useSimulation) {
        m_canInterface = m_simInterface;
    } else {
        if (!m_pcanDriver->isLibraryLoaded() && !m_pcanDriver->loadLibrary()) {
            QMessageBox::warning(this, "PCAN Error",
                "Failed to load PCAN-Basic library.\n\n"
                "Please ensure PCANBasic.dll (Windows) or libpcanbasic.so (Linux) is installed.\n\n"
                "Error: " + m_pcanDriver->lastError());
            return;
        }
        m_canInterface = m_pcanDriver;
    }

    uint16_t channel = m_connectionWidget->selectedChannel();
    uint32_t baudRate = m_connectionWidget->selectedBaudRate();

    if (m_canInterface->open(channel, baudRate)) {
        m_module->setCanInterface(m_canInterface);
        m_module->start();
        m_connectionWidget->setConnected(true);
        m_connectionWidget->setStatus(CanStatus::Ok);

        // Set default EV parameters per datasheet requirements
        // CMS needs non-SNA values for: EVMaxVoltage, EVMaxCurrent, EVReady, EVErrorCode, EVSoC
        m_module->setEvMaxVoltage(500.0);
        m_module->setEvMaxCurrent(200.0);
        m_module->setEvMaxPower(100000.0);
        m_module->setEvSoC(50.0);
        m_module->setEvErrorCode(0); // NO_ERROR
        m_module->setEvFullSoC(100.0);
        m_module->setEvBulkSoC(80.0);
        m_module->setEvEnergyCapacity(60000.0); // 60 kWh
        m_module->setEvEnergyRequest(40000.0);
        m_module->setEvPreChargeVoltage(400.0);
        m_module->setEvTargetVoltage(400.0);
        m_module->setEvTargetCurrent(0.0);

        m_chartWidget->reset();

        // Reset frame counts
        m_rxFrameCount = 0;
        m_txFrameCount = 0;

        // Count frames
        connect(m_module, &ChargeModule::rawFrameReceived, this, [this]() { m_rxFrameCount++; });
        connect(m_module, &ChargeModule::rawFrameSent, this, [this]() { m_txFrameCount++; });

        // Update chart from EVSE data
        connect(m_module, &ChargeModule::evseDataUpdated, this, [this]() {
            const auto& evse = m_module->evseData();
            m_chartWidget->addDataPoint(evse.evsePresentVoltage, evse.evsePresentCurrent);

            // Update session report
            if (m_sessionReport->isActive()) {
                m_sessionReport->updateValues(evse.evsePresentVoltage, evse.evsePresentCurrent, m_module->evParams().evSoC);
            }
        });

        statusBar()->showMessage(QString("Connected to %1 interface").arg(m_useSimulation ? "Simulation" : "PCAN"), 3000);
    } else {
        QMessageBox::warning(this, "Connection Failed",
            "Failed to open CAN interface:\n" + m_canInterface->lastError());
    }
}

void MainWindow::onDisconnect()
{
    if (m_module->isRunning()) {
        m_module->stop();
    }
    if (m_canInterface && m_canInterface->isOpen()) {
        m_canInterface->close();
    }
    m_connectionWidget->setConnected(false);
    m_connectionWidget->setStatus(CanStatus::Disconnected);
    m_connectionWidget->setHeartbeat(false);

    if (m_sessionReport->isActive()) {
        m_sessionReport->endSession();
    }

    statusBar()->showMessage("Disconnected", 3000);
}

void MainWindow::onRefreshChannels()
{
    if (m_useSimulation) {
        m_connectionWidget->setChannels(m_simInterface->availableChannels());
    } else {
        // First try to load the library - give clear feedback if it fails
        if (!m_pcanDriver->isLibraryLoaded() && !m_pcanDriver->loadLibrary()) {
            QString err = m_pcanDriver->lastError();
            QMessageBox::warning(this, "PCAN Scan Failed",
                "Could not load the PCAN-Basic driver library.\n\n"
                "Please install PCAN-Basic from:\n"
                "https://www.peak-system.com/PCAN-Basic.239.0.html\n\n"
                "Windows: PCANBasic.dll must be in PATH or application directory\n"
                "Linux: libpcanbasic.so must be installed (e.g. via peak-linux-driver)\n\n"
                "Error: " + err);
            m_connectionWidget->setChannels({});
            statusBar()->showMessage("PCAN library not found: " + err, 10000);
            return;
        }

        auto channels = m_pcanDriver->availableChannels();
        m_connectionWidget->setChannels(channels);

        if (channels.empty()) {
            QMessageBox::information(this, "PCAN Scan",
                "PCAN-Basic library loaded successfully, but no PCAN-USB devices were detected.\n\n"
                "Please check:\n"
                "- Is the PCAN-USB adapter plugged in?\n"
                "- Are the drivers installed correctly?\n"
                "- Is another application using the channel?");
            statusBar()->showMessage("PCAN library OK, but no USB channels found", 5000);
        } else {
            statusBar()->showMessage(QString("Found %1 PCAN channel(s)").arg(channels.size()), 3000);
        }
    }
}

void MainWindow::onSimulationToggled(bool enabled)
{
    m_useSimulation = enabled;
    if (enabled) {
        // In simulation mode, channels are always available
        m_connectionWidget->setChannels(m_simInterface->availableChannels());
    } else {
        // In PCAN mode, clear channels - user must click Scan to discover
        m_connectionWidget->setChannels({});
        statusBar()->showMessage("PCAN mode: click Scan to detect PCAN-USB adapters", 5000);
    }
}

void MainWindow::onStartCharging()
{
    if (!m_module || !m_module->isRunning()) {
        statusBar()->showMessage("Connect to CAN interface first", 3000);
        return;
    }

    // Per datasheet: set EVReady, set mandatory parameters, then wait for state progression
    m_module->requestStartCharging();
    m_sessionReport->startSession();
    statusBar()->showMessage("Charging requested - waiting for CMS state machine progression", 5000);
}

void MainWindow::onStopCharging()
{
    if (!m_module) return;

    m_module->requestStopCharging();
    if (m_sessionReport->isActive()) {
        m_sessionReport->endSession();
    }
    statusBar()->showMessage("Stop charging requested", 3000);
}

void MainWindow::onEmergencyStop()
{
    if (!m_module) return;

    m_module->emergencyStop();
    if (m_sessionReport->isActive()) {
        m_sessionReport->endSession();
    }
    statusBar()->showMessage("EMERGENCY STOP ACTIVATED", 10000);
}

void MainWindow::onStateChanged(CmsState state)
{
    QString stateStr = cmsStateToString(state);
    m_statusState->setText("State: " + stateStr);

    // Auto-actions based on state transitions per datasheet
    switch (state) {
        case CmsState::PreCharge:
            // Per datasheet: TargetCurrent fixed at 2A during PreCharge
            m_module->setEvTargetCurrent(2.0);
            statusBar()->showMessage("PreCharge: Target current set to 2A per datasheet", 3000);
            break;

        case CmsState::Charge:
            statusBar()->showMessage("Charging active", 3000);
            break;

        case CmsState::Error:
            statusBar()->showMessage("Module entered ERROR state - check fault display", 10000);
            break;

        case CmsState::ShutOff:
            statusBar()->showMessage("Charging session complete - ShutOff state", 5000);
            if (m_sessionReport->isActive()) {
                m_sessionReport->endSession();
            }
            break;

        default:
            break;
    }
}

void MainWindow::onErrorCode(uint16_t code, const QString& desc)
{
    statusBar()->showMessage(QString("Error 0x%1: %2").arg(code, 4, 16, QChar('0')).arg(desc), 10000);
}

void MainWindow::onStatusUpdate()
{
    // Update frame counts
    m_statusFrames->setText(QString("RX: %1 | TX: %2").arg(m_rxFrameCount).arg(m_txFrameCount));

    // Update session info
    if (m_sessionReport->isActive()) {
        int secs = m_sessionReport->durationSeconds();
        m_statusSession->setText(QString("Session: %1:%2 | %3 Wh")
            .arg(secs / 60, 2, 10, QChar('0'))
            .arg(secs % 60, 2, 10, QChar('0'))
            .arg(m_sessionReport->energyEstimateWh(), 0, 'f', 0));
    }

    // Update CAN status
    if (m_canInterface && m_canInterface->isOpen()) {
        m_connectionWidget->setStatus(m_canInterface->status());

        // Update module firmware version
        const auto& evse = m_module->evseData();
        if (evse.swVersionMajor > 0 || evse.swVersionMinor > 0) {
            m_connectionWidget->setModuleInfo(QString("FW %1.%2.%3 (Config %4)")
                .arg(evse.swVersionMajor)
                .arg(evse.swVersionMinor)
                .arg(evse.swVersionPatch)
                .arg(evse.swVersionConfig));
        }
    }
}

} // namespace ccs
