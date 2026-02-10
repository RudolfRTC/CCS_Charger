#pragma once

#include "can/can_interface.h"
#include <QString>
#include <QLibrary>
#include <QMutex>
#include <atomic>

namespace ccs {

/// PCAN-Basic API function types (Windows: PCANBasic.dll, Linux: libpcanbasic.so)
/// These match the PCAN-Basic API signatures.
namespace pcan {

// PCAN channel handles
constexpr uint16_t PCAN_USBBUS1  = 0x0051;
constexpr uint16_t PCAN_USBBUS2  = 0x0052;
constexpr uint16_t PCAN_USBBUS3  = 0x0053;
constexpr uint16_t PCAN_USBBUS4  = 0x0054;
constexpr uint16_t PCAN_USBBUS5  = 0x0055;
constexpr uint16_t PCAN_USBBUS6  = 0x0056;
constexpr uint16_t PCAN_USBBUS7  = 0x0057;
constexpr uint16_t PCAN_USBBUS8  = 0x0058;

// Baud rates
constexpr uint16_t PCAN_BAUD_500K = 0x001C;
constexpr uint16_t PCAN_BAUD_250K = 0x011C;
constexpr uint16_t PCAN_BAUD_125K = 0x031C;
constexpr uint16_t PCAN_BAUD_1M   = 0x0014;

// Message types
constexpr uint8_t PCAN_MESSAGE_STANDARD = 0x00;
constexpr uint8_t PCAN_MESSAGE_EXTENDED = 0x02;

// Error codes
constexpr uint32_t PCAN_ERROR_OK       = 0x00000;
constexpr uint32_t PCAN_ERROR_QRCVEMPTY = 0x00020;

// Parameters
constexpr uint8_t PCAN_CHANNEL_CONDITION = 0x02;
constexpr uint8_t PCAN_CHANNEL_AVAILABLE = 0x01;
constexpr uint8_t PCAN_BUSOFF_AUTORESET  = 0x07;

#pragma pack(push, 1)
struct TPCANMsg {
    uint32_t ID;
    uint8_t  MSGTYPE;
    uint8_t  LEN;
    uint8_t  DATA[8];
};

struct TPCANTimestamp {
    uint32_t millis;
    uint16_t millis_overflow;
    uint16_t micros;
};
#pragma pack(pop)

using FN_CAN_Initialize    = uint32_t(*)(uint16_t, uint16_t, uint8_t, uint8_t, uint32_t);
using FN_CAN_Uninitialize  = uint32_t(*)(uint16_t);
using FN_CAN_Read          = uint32_t(*)(uint16_t, TPCANMsg*, TPCANTimestamp*);
using FN_CAN_Write         = uint32_t(*)(uint16_t, TPCANMsg*);
using FN_CAN_GetStatus     = uint32_t(*)(uint16_t);
using FN_CAN_GetValue      = uint32_t(*)(uint16_t, uint8_t, void*, uint32_t);
using FN_CAN_SetValue      = uint32_t(*)(uint16_t, uint8_t, void*, uint32_t);
using FN_CAN_GetErrorText  = uint32_t(*)(uint32_t, uint16_t, char*);

} // namespace pcan

/// PCAN-Basic driver implementation
class PcanDriver : public CanInterface {
    Q_OBJECT
public:
    explicit PcanDriver(QObject* parent = nullptr);
    ~PcanDriver() override;

    bool open(uint16_t channel, uint32_t baudRate) override;
    void close() override;
    bool isOpen() const override { return m_open; }
    bool write(const CanFrame& frame) override;
    std::vector<ChannelInfo> availableChannels() override;
    CanStatus status() const override;
    QString lastError() const override { return m_lastError; }

    bool loadLibrary();
    bool isLibraryLoaded() const { return m_libLoaded; }

private:
    void pollLoop();
    uint16_t baudRateToCode(uint32_t baudRate) const;
    QString pcanErrorText(uint32_t err) const;

    QLibrary m_library;
    bool m_libLoaded = false;
    bool m_open = false;
    uint16_t m_channel = 0;
    QString m_lastError;
    std::atomic<bool> m_polling{false};
    QThread* m_pollThread = nullptr;

    // Function pointers
    pcan::FN_CAN_Initialize   fn_Initialize = nullptr;
    pcan::FN_CAN_Uninitialize fn_Uninitialize = nullptr;
    pcan::FN_CAN_Read         fn_Read = nullptr;
    pcan::FN_CAN_Write        fn_Write = nullptr;
    pcan::FN_CAN_GetStatus    fn_GetStatus = nullptr;
    pcan::FN_CAN_GetValue     fn_GetValue = nullptr;
    pcan::FN_CAN_SetValue     fn_SetValue = nullptr;
    pcan::FN_CAN_GetErrorText fn_GetErrorText = nullptr;
};

} // namespace ccs
