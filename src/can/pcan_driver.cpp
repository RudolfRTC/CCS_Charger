#include "can/pcan_driver.h"
#include <QDebug>
#include <QCoreApplication>
#include <thread>
#include <chrono>

namespace ccs {

PcanDriver::PcanDriver(QObject* parent)
    : CanInterface(parent)
{
#ifdef PLATFORM_WINDOWS
    m_library.setFileName("PCANBasic");
#else
    m_library.setFileName("libpcanbasic");
#endif
}

PcanDriver::~PcanDriver()
{
    close();
}

bool PcanDriver::loadLibrary()
{
    if (m_libLoaded) return true;

    if (!m_library.load()) {
        m_lastError = "Failed to load PCAN-Basic library: " + m_library.errorString();
        emit errorOccurred(m_lastError);
        return false;
    }

    fn_Initialize   = reinterpret_cast<pcan::FN_CAN_Initialize>(m_library.resolve("CAN_Initialize"));
    fn_Uninitialize = reinterpret_cast<pcan::FN_CAN_Uninitialize>(m_library.resolve("CAN_Uninitialize"));
    fn_Read         = reinterpret_cast<pcan::FN_CAN_Read>(m_library.resolve("CAN_Read"));
    fn_Write        = reinterpret_cast<pcan::FN_CAN_Write>(m_library.resolve("CAN_Write"));
    fn_GetStatus    = reinterpret_cast<pcan::FN_CAN_GetStatus>(m_library.resolve("CAN_GetStatus"));
    fn_GetValue     = reinterpret_cast<pcan::FN_CAN_GetValue>(m_library.resolve("CAN_GetValue"));
    fn_SetValue     = reinterpret_cast<pcan::FN_CAN_SetValue>(m_library.resolve("CAN_SetValue"));
    fn_GetErrorText = reinterpret_cast<pcan::FN_CAN_GetErrorText>(m_library.resolve("CAN_GetErrorText"));

    if (!fn_Initialize || !fn_Uninitialize || !fn_Read || !fn_Write || !fn_GetStatus) {
        m_lastError = "PCAN-Basic library loaded but required functions not found";
        m_library.unload();
        emit errorOccurred(m_lastError);
        return false;
    }

    m_libLoaded = true;
    return true;
}

bool PcanDriver::open(uint16_t channel, uint32_t baudRate)
{
    if (!m_libLoaded && !loadLibrary()) {
        return false;
    }

    uint16_t baud = baudRateToCode(baudRate);
    uint32_t result = fn_Initialize(channel, baud, 0, 0, 0);

    if (result != pcan::PCAN_ERROR_OK) {
        m_lastError = "CAN_Initialize failed: " + pcanErrorText(result);
        emit errorOccurred(m_lastError);
        return false;
    }

    // Enable bus-off auto-reset
    if (fn_SetValue) {
        uint32_t val = 1;
        fn_SetValue(channel, pcan::PCAN_BUSOFF_AUTORESET, &val, sizeof(val));
    }

    m_channel = channel;
    m_open = true;
    m_polling = true;

    // Start polling thread
    m_pollThread = QThread::create([this]() { pollLoop(); });
    m_pollThread->start();

    emit statusChanged(CanStatus::Ok);
    return true;
}

void PcanDriver::close()
{
    m_polling = false;

    if (m_pollThread) {
        m_pollThread->quit();
        m_pollThread->wait(2000);
        delete m_pollThread;
        m_pollThread = nullptr;
    }

    if (m_open && fn_Uninitialize) {
        fn_Uninitialize(m_channel);
    }

    m_open = false;
    emit statusChanged(CanStatus::Disconnected);
}

bool PcanDriver::write(const CanFrame& frame)
{
    if (!m_open || !fn_Write) return false;

    pcan::TPCANMsg msg{};
    msg.ID = frame.id;
    msg.MSGTYPE = frame.extended ? pcan::PCAN_MESSAGE_EXTENDED : pcan::PCAN_MESSAGE_STANDARD;
    msg.LEN = frame.dlc;
    std::memcpy(msg.DATA, frame.data.data(), std::min<uint8_t>(frame.dlc, 8));

    uint32_t result = fn_Write(m_channel, &msg);
    if (result != pcan::PCAN_ERROR_OK) {
        m_lastError = "CAN_Write failed: " + pcanErrorText(result);
        return false;
    }
    return true;
}

std::vector<CanInterface::ChannelInfo> PcanDriver::availableChannels()
{
    std::vector<ChannelInfo> channels;

    if (!m_libLoaded && !loadLibrary()) {
        return channels;
    }

    // Check USB channels 1-8
    const uint16_t usbChannels[] = {
        pcan::PCAN_USBBUS1, pcan::PCAN_USBBUS2, pcan::PCAN_USBBUS3, pcan::PCAN_USBBUS4,
        pcan::PCAN_USBBUS5, pcan::PCAN_USBBUS6, pcan::PCAN_USBBUS7, pcan::PCAN_USBBUS8
    };

    for (int i = 0; i < 8; ++i) {
        if (fn_GetValue) {
            uint32_t condition = 0;
            uint32_t result = fn_GetValue(usbChannels[i], pcan::PCAN_CHANNEL_CONDITION,
                                          &condition, sizeof(condition));
            if (result == pcan::PCAN_ERROR_OK && (condition & pcan::PCAN_CHANNEL_AVAILABLE)) {
                channels.push_back({
                    .name = QString("PCAN-USB %1").arg(i + 1),
                    .handle = usbChannels[i],
                    .description = QString("PCAN USB Channel %1 (Handle 0x%2)")
                        .arg(i + 1).arg(usbChannels[i], 4, 16, QChar('0'))
                });
            }
        }
    }

    return channels;
}

CanStatus PcanDriver::status() const
{
    if (!m_open) return CanStatus::Disconnected;
    if (!fn_GetStatus) return CanStatus::Ok;

    uint32_t st = fn_GetStatus(m_channel);
    if (st == pcan::PCAN_ERROR_OK) return CanStatus::Ok;
    if (st & 0x40) return CanStatus::BusOff;
    if (st & 0x80) return CanStatus::BusPassive;
    if (st & 0x04) return CanStatus::BusWarning;
    return CanStatus::Error;
}

void PcanDriver::pollLoop()
{
    pcan::TPCANMsg msg{};
    pcan::TPCANTimestamp ts{};

    while (m_polling) {
        uint32_t result = fn_Read(m_channel, &msg, &ts);

        if (result == pcan::PCAN_ERROR_OK) {
            CanFrame frame;
            frame.id = msg.ID;
            frame.extended = (msg.MSGTYPE & pcan::PCAN_MESSAGE_EXTENDED) != 0;
            frame.dlc = msg.LEN;
            std::memcpy(frame.data.data(), msg.DATA, std::min<uint8_t>(msg.LEN, 8));
            frame.timestamp = std::chrono::steady_clock::now();

            emit frameReceived(frame);
        }
        else if (result == pcan::PCAN_ERROR_QRCVEMPTY) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        else {
            // Bus error - brief sleep to avoid spin
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            auto newStatus = status();
            if (newStatus != CanStatus::Ok) {
                emit statusChanged(newStatus);
            }
        }
    }
}

uint16_t PcanDriver::baudRateToCode(uint32_t baudRate) const
{
    switch (baudRate) {
        case 1000000: return pcan::PCAN_BAUD_1M;
        case 500000:  return pcan::PCAN_BAUD_500K;
        case 250000:  return pcan::PCAN_BAUD_250K;
        case 125000:  return pcan::PCAN_BAUD_125K;
        default:      return pcan::PCAN_BAUD_500K;
    }
}

QString PcanDriver::pcanErrorText(uint32_t err) const
{
    if (fn_GetErrorText) {
        char buf[256]{};
        fn_GetErrorText(err, 0x09, buf); // English
        return QString::fromLatin1(buf);
    }
    return QString("Error 0x%1").arg(err, 8, 16, QChar('0'));
}

} // namespace ccs
