#pragma once

#include "module/state_machine.h"
#include "module/safety_monitor.h"
#include "can/can_interface.h"
#include "can/can_frame.h"
#include "dbc/dbc_parser.h"
#include "dbc/signal_codec.h"

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <chrono>

namespace ccs {

/// Main Charge Module S controller.
/// Manages the protocol state machine, encodes/decodes CAN messages,
/// and coordinates with the CMS module per datasheet requirements.
class ChargeModule : public QObject {
    Q_OBJECT
public:
    /// EV-side parameters that we (VCU) set
    struct EvParameters {
        // EVDCMaxLimits (0x1300)
        double evMaxCurrent = 0.0;   // A
        double evMaxVoltage = 0.0;   // V
        double evMaxPower = 0.0;     // W
        double evFullSoC = 100.0;    // %
        double evBulkSoC = 80.0;     // %

        // EVDCChargeTargets (0x1301)
        double evTargetCurrent = 0.0;   // A
        double evTargetVoltage = 0.0;   // V
        double evPreChargeVoltage = 0.0; // V

        // EVStatusControl (0x1302)
        ChargeProgressIndication chargeProgress = ChargeProgressIndication::Stop;
        ChargeStopIndication chargeStop = ChargeStopIndication::NoStop;
        bool evReady = false;
        bool evWeldingDetectionEnable = false;
        uint8_t chargeProtocolPriority = 0; // 0=DIN_only
        BCBControl bcbControl = BCBControl::Stop;

        // EVStatusDisplay (0x1303)
        double evSoC = 0.0;         // %
        uint8_t evErrorCode = 0;    // NO_ERROR
        bool evChargingComplete = false;
        bool evBulkChargingComplete = false;
        bool evCabinConditioning = false;
        bool evRessConditioning = false;
        uint32_t evTimeToFullSoC = 0;  // seconds
        uint32_t evTimeToBulkSoC = 0;  // seconds

        // EVPlugStatus (0x1304) - for CAN configuration
        uint8_t evControlPilotState = 15;   // SNA
        uint8_t evControlPilotDutyCycle = 0;
        uint8_t evProximityPinState = 15;   // SNA

        // EVDCEnergyLimits (0x1305)
        double evEnergyCapacity = 0.0;  // Wh
        double evEnergyRequest = 0.0;   // Wh
    };

    /// EVSE-side data received from the CMS module
    struct EvseData {
        // ChargeInfo (0x0600)
        CmsState stateMachineState = CmsState::SNA;
        uint8_t aliveCounter = 15;
        ControlPilotState controlPilotState = ControlPilotState::SNA;
        uint8_t controlPilotDutyCycle = 0;
        ChargeProtocol actualChargeProtocol = ChargeProtocol::SNA;
        uint8_t proximityPinState = 15;
        bool swS2Close = false;
        bool voltageMatch = false;
        bool evseCompatible = false;
        bool tcpConnected = false;
        uint8_t bcbStatus = 0;

        // EVSEDCMaxLimits (0x1400)
        double evseMaxCurrent = 0.0;  // A
        double evseMaxVoltage = 0.0;  // V
        double evseMaxPower = 0.0;    // W
        double evseEnergyToBeDelivered = 0.0; // Wh

        // EVSEDCRegulationLimits (0x1401)
        double evseMinCurrent = 0.0;
        double evseMinVoltage = 0.0;
        double evsePeakCurrentRipple = 0.0;
        double evseCurrentRegulationTolerance = 0.0;

        // EVSEDCStatus (0x1402)
        double evsePresentVoltage = 0.0;  // V
        double evsePresentCurrent = 0.0;  // A
        EvseIsolationStatus evseIsolationStatus = EvseIsolationStatus::SNA;
        EvseStatusCode evseStatusCode = EvseStatusCode::SNA;
        uint8_t evseNotification = 3;  // SNA
        uint16_t evseNotificationMaxDelay = 0xFFFF; // SNA

        // Limit flags
        bool evseCurrentLimitAchieved = false;
        bool evseVoltageLimitAchieved = false;
        bool evsePowerLimitAchieved = false;

        // ErrorCodes (0x2002)
        uint16_t errorCode0 = 0;
        uint16_t errorCode1 = 0;
        uint16_t errorCode2 = 0;
        uint16_t errorCode3 = 0;

        // SoftwareInfo (0x2001)
        uint8_t swVersionMajor = 0;
        uint8_t swVersionMinor = 0;
        uint8_t swVersionPatch = 0;
        uint8_t swVersionConfig = 0;

        // SLACInfo (0x2003)
        uint8_t slacState = 7;     // SNA
        uint8_t linkStatus = 3;    // SNA
        double measuredAttenuation = 0.0; // dB
    };

    explicit ChargeModule(QObject* parent = nullptr);

    void setCanInterface(CanInterface* iface);
    void loadDbc(const QString& dbcPath);
    void start();  // Start cyclic TX
    void stop();   // Stop cyclic TX, send safe defaults

    // Parameter setters (validated by safety monitor)
    void setEvMaxVoltage(double v);
    void setEvMaxCurrent(double i);
    void setEvMaxPower(double p);
    void setEvTargetVoltage(double v);
    void setEvTargetCurrent(double i);
    void setEvPreChargeVoltage(double v);
    void setEvSoC(double soc);
    void setEvReady(bool ready);
    void setChargeProgressIndication(ChargeProgressIndication ind);
    void setChargeStopIndication(ChargeStopIndication ind);
    void setWeldingDetectionEnable(bool enable);
    void setEvErrorCode(uint8_t code);
    void setEvFullSoC(double soc);
    void setEvBulkSoC(double soc);
    void setEvEnergyCapacity(double wh);
    void setEvEnergyRequest(double wh);
    void setChargeProtocolPriority(uint8_t prio);

    // Start/stop charging helpers
    void requestStartCharging();
    void requestStopCharging();
    void emergencyStop();
    void resetModule();

    const EvParameters& evParams() const { return m_evParams; }
    const EvseData& evseData() const { return m_evseData; }
    SafetyMonitor* safetyMonitor() { return &m_safety; }
    const DbcDatabase& dbcDatabase() const { return m_dbc; }
    const SignalCodec& codec() const { return m_codec; }

    bool isRunning() const { return m_running; }

signals:
    void evseDataUpdated();
    void stateChanged(ccs::CmsState newState);
    void errorCodeReceived(uint16_t code, const QString& description);
    void rawFrameReceived(const ccs::CanFrame& frame);
    void rawFrameSent(const ccs::CanFrame& frame);

public slots:
    void onFrameReceived(const ccs::CanFrame& frame);

private slots:
    void onCyclicTx();

private:
    void decodeChargeInfo(const CanFrame& frame);
    void decodeEvseMaxLimits(const CanFrame& frame);
    void decodeEvseRegulationLimits(const CanFrame& frame);
    void decodeEvseDCStatus(const CanFrame& frame);
    void decodeErrorCodes(const CanFrame& frame);
    void decodeSoftwareInfo(const CanFrame& frame);
    void decodeSLACInfo(const CanFrame& frame);

    void sendEvDCMaxLimits();
    void sendEvDCChargeTargets();
    void sendEvStatusControl();
    void sendEvStatusDisplay();
    void sendEvPlugStatus();
    void sendEvDCEnergyLimits();

    void sendFrame(const CanFrame& frame);

    CanInterface* m_can = nullptr;
    DbcDatabase m_dbc;
    SignalCodec m_codec;
    SafetyMonitor m_safety;

    EvParameters m_evParams;
    EvseData m_evseData;

    QTimer* m_cyclicTimer = nullptr;
    bool m_running = false;
    CmsState m_lastState = CmsState::SNA;
    mutable QMutex m_mutex;
};

} // namespace ccs
