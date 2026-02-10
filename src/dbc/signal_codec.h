#pragma once

#include "dbc/dbc_parser.h"
#include "can/can_frame.h"
#include <QMap>
#include <QString>
#include <cstdint>

namespace ccs {

/// Decoded signal with physical value and metadata
struct DecodedSignal {
    QString name;
    double physicalValue = 0.0;
    uint64_t rawValue = 0;
    QString unit;
    QString valueDescription; // From VAL_ table (e.g., "Init", "Charge")
    bool isValid = true;      // false if SNA
};

/// Result of decoding a full CAN message
struct DecodedMessage {
    uint32_t canId = 0;
    QString messageName;
    QVector<DecodedSignal> decodedSignals;
};

class SignalCodec {
public:
    SignalCodec() = default;
    explicit SignalCodec(const DbcDatabase& db) : m_db(&db) {}

    void setDatabase(const DbcDatabase& db) { m_db = &db; }

    /// Decode all signals from a CAN frame using the DBC
    DecodedMessage decode(const CanFrame& frame) const;

    /// Decode a single signal from a CAN frame
    DecodedSignal decodeSignal(const CanFrame& frame, const DbcSignal& sig) const;

    /// Encode a physical value into a CAN frame's data bytes
    /// Returns true if successful
    bool encodeSignal(CanFrame& frame, const DbcSignal& sig, double physicalValue) const;

    /// Encode a raw value directly into a CAN frame
    bool encodeSignalRaw(CanFrame& frame, const DbcSignal& sig, uint64_t rawValue) const;

    /// Extract raw bits from CAN data
    static uint64_t extractBits(const uint8_t* data, uint32_t startBit,
                                uint32_t bitLength, bool littleEndian);

    /// Insert raw bits into CAN data
    static void insertBits(uint8_t* data, uint32_t startBit,
                           uint32_t bitLength, bool littleEndian, uint64_t value);

    /// Convert physical value to raw value
    static uint64_t physicalToRaw(double physical, double factor, double offset);

    /// Convert raw value to physical value
    static double rawToPhysical(uint64_t raw, double factor, double offset, bool isSigned, uint32_t bitLength);

private:
    const DbcDatabase* m_db = nullptr;
};

} // namespace ccs
