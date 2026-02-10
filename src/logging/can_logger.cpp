#include "logging/can_logger.h"
#include <QDateTime>
#include <QDebug>

namespace ccs {

CanLogger::CanLogger(QObject* parent)
    : QObject(parent)
    , m_startTime(std::chrono::steady_clock::now())
{
}

CanLogger::~CanLogger()
{
    stopAll();
}

bool CanLogger::startRawLog(const QString& filePath)
{
    if (m_rawFile.isOpen()) m_rawFile.close();

    m_rawFile.setFileName(filePath);
    if (!m_rawFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    m_rawStream.setDevice(&m_rawFile);
    m_rawStream << "Timestamp_ms,Direction,ID,Extended,DLC,Data\n";
    m_rawStream.flush();
    m_rawCount = 0;
    m_startTime = std::chrono::steady_clock::now();
    return true;
}

bool CanLogger::startDecodedLog(const QString& filePath)
{
    if (m_decodedFile.isOpen()) m_decodedFile.close();

    m_decodedFile.setFileName(filePath);
    if (!m_decodedFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    m_decodedStream.setDevice(&m_decodedFile);
    m_decodedStream << "Timestamp_ms,Message,Signal,RawValue,PhysicalValue,Unit,Description\n";
    m_decodedStream.flush();
    m_decodedCount = 0;
    return true;
}

void CanLogger::stopAll()
{
    if (m_rawFile.isOpen()) {
        m_rawStream.flush();
        m_rawFile.close();
    }
    if (m_decodedFile.isOpen()) {
        m_decodedStream.flush();
        m_decodedFile.close();
    }
}

void CanLogger::logFrame(const CanFrame& frame)
{
    if (m_rawFile.isOpen()) {
        writeRawFrame(frame);
    }
    if (m_decodedFile.isOpen() && m_codec) {
        writeDecodedFrame(frame);
    }
}

void CanLogger::writeRawFrame(const CanFrame& frame)
{
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        frame.timestamp - m_startTime).count();

    m_rawStream << elapsed << ","
                << "RX" << ","
                << frame.idString() << ","
                << (frame.extended ? "EXT" : "STD") << ","
                << frame.dlc << ","
                << frame.toHexString() << "\n";

    if (++m_rawCount % 100 == 0) {
        m_rawStream.flush();
    }
}

void CanLogger::writeDecodedFrame(const CanFrame& frame)
{
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        frame.timestamp - m_startTime).count();

    auto decoded = m_codec->decode(frame);
    if (decoded.messageName.isEmpty()) return;

    for (const auto& sig : decoded.decodedSignals) {
        m_decodedStream << elapsed << ","
                        << decoded.messageName << ","
                        << sig.name << ","
                        << sig.rawValue << ","
                        << sig.physicalValue << ","
                        << sig.unit << ","
                        << sig.valueDescription << "\n";
        m_decodedCount++;
    }

    if (m_decodedCount % 100 == 0) {
        m_decodedStream.flush();
    }
}

} // namespace ccs
