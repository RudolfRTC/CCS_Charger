#pragma once

#include "can/can_frame.h"
#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <atomic>
#include <functional>
#include <vector>

namespace ccs {

/// Abstract CAN interface. Concrete implementations: PcanDriver, SimulatedCan.
class CanInterface : public QObject {
    Q_OBJECT
public:
    struct ChannelInfo {
        QString name;
        uint16_t handle = 0;
        QString description;
    };

    explicit CanInterface(QObject* parent = nullptr) : QObject(parent) {}
    ~CanInterface() override = default;

    virtual bool open(uint16_t channel, uint32_t baudRate) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool write(const CanFrame& frame) = 0;
    virtual std::vector<ChannelInfo> availableChannels() = 0;
    virtual CanStatus status() const = 0;
    virtual QString lastError() const = 0;

signals:
    void frameReceived(const ccs::CanFrame& frame);
    void statusChanged(ccs::CanStatus status);
    void errorOccurred(const QString& message);
};

/// Background thread that polls CAN RX and emits frameReceived signals
class CanReaderThread : public QThread {
    Q_OBJECT
public:
    explicit CanReaderThread(CanInterface* iface, QObject* parent = nullptr)
        : QThread(parent), m_interface(iface) {}

    void stop() { m_running = false; }

signals:
    void frameReceived(const ccs::CanFrame& frame);

protected:
    void run() override;

private:
    CanInterface* m_interface = nullptr;
    std::atomic<bool> m_running{true};
};

/// Simulated CAN interface for development/testing without hardware
class SimulatedCanInterface : public CanInterface {
    Q_OBJECT
public:
    explicit SimulatedCanInterface(QObject* parent = nullptr);
    ~SimulatedCanInterface() override;

    bool open(uint16_t channel, uint32_t baudRate) override;
    void close() override;
    bool isOpen() const override { return m_open; }
    bool write(const CanFrame& frame) override;
    std::vector<ChannelInfo> availableChannels() override;
    CanStatus status() const override;
    QString lastError() const override { return m_lastError; }

    /// Inject a frame as if received from the bus (for simulation)
    void injectFrame(const CanFrame& frame);

private slots:
    void onSimulationTick();

private:
    bool m_open = false;
    QString m_lastError;
    QTimer* m_simTimer = nullptr;
    uint8_t m_aliveCounter = 0;
    uint8_t m_stateMachineState = 0; // Default
    std::vector<CanFrame> m_txQueue; // frames we sent (for echo-back in sim)
};

} // namespace ccs
