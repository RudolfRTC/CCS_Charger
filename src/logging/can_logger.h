#pragma once

#include "can/can_frame.h"
#include "dbc/signal_codec.h"
#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <chrono>

namespace ccs {

/// Logs raw CAN frames and decoded signals to files
class CanLogger : public QObject {
    Q_OBJECT
public:
    explicit CanLogger(QObject* parent = nullptr);
    ~CanLogger() override;

    bool startRawLog(const QString& filePath);
    bool startDecodedLog(const QString& filePath);
    void stopAll();

    void setCodec(const SignalCodec* codec) { m_codec = codec; }

    bool isRawLogging() const { return m_rawFile.isOpen(); }
    bool isDecodedLogging() const { return m_decodedFile.isOpen(); }

    uint64_t rawFrameCount() const { return m_rawCount; }
    uint64_t decodedFrameCount() const { return m_decodedCount; }

public slots:
    void logFrame(const ccs::CanFrame& frame);

private:
    void writeRawFrame(const CanFrame& frame);
    void writeDecodedFrame(const CanFrame& frame);

    QFile m_rawFile;
    QFile m_decodedFile;
    QTextStream m_rawStream;
    QTextStream m_decodedStream;
    const SignalCodec* m_codec = nullptr;

    std::chrono::steady_clock::time_point m_startTime;
    uint64_t m_rawCount = 0;
    uint64_t m_decodedCount = 0;
};

} // namespace ccs
