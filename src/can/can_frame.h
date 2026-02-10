#pragma once

#include <cstdint>
#include <array>
#include <chrono>
#include <cstdio>
#include <QString>

namespace ccs {

struct CanFrame {
    uint32_t id = 0;
    bool extended = false;
    uint8_t dlc = 0;
    std::array<uint8_t, 8> data{};
    std::chrono::steady_clock::time_point timestamp;

    QString toHexString() const {
        QString result;
        for (uint8_t i = 0; i < dlc && i < 8; ++i) {
            if (i > 0) result += ' ';
            result += QString("%1").arg(data[i], 2, 16, QChar('0')).toUpper();
        }
        return result;
    }

    QString idString() const {
        if (extended)
            return QString("%1").arg(id, 8, 16, QChar('0')).toUpper();
        else
            return QString("%1").arg(id, 3, 16, QChar('0')).toUpper();
    }
};

enum class CanStatus {
    Ok,
    BusOff,
    BusWarning,
    BusPassive,
    Error,
    Disconnected
};

inline const char* canStatusToString(CanStatus s) {
    switch (s) {
        case CanStatus::Ok:           return "OK";
        case CanStatus::BusOff:       return "Bus Off";
        case CanStatus::BusWarning:   return "Bus Warning";
        case CanStatus::BusPassive:   return "Bus Passive";
        case CanStatus::Error:        return "Error";
        case CanStatus::Disconnected: return "Disconnected";
    }
    return "Unknown";
}

} // namespace ccs
