#include "dbc/signal_codec.h"
#include <cmath>
#include <climits>

namespace ccs {

DecodedMessage SignalCodec::decode(const CanFrame& frame) const
{
    DecodedMessage result;
    result.canId = frame.id;

    if (!m_db) return result;

    const auto* msg = m_db->findMessage(frame.id);
    if (!msg) return result;

    result.messageName = msg->name;

    for (const auto& sig : msg->dbcSignals) {
        result.decodedSignals.append(decodeSignal(frame, sig));
    }

    return result;
}

DecodedSignal SignalCodec::decodeSignal(const CanFrame& frame, const DbcSignal& sig) const
{
    DecodedSignal result;
    result.name = sig.name;
    result.unit = sig.unit;

    uint64_t raw = extractBits(frame.data.data(), sig.startBit, sig.bitLength, sig.littleEndian);
    result.rawValue = raw;

    result.physicalValue = rawToPhysical(raw, sig.factor, sig.offset, sig.isSigned, sig.bitLength);

    // Check value descriptions
    int rawInt = static_cast<int>(raw);
    if (sig.valueDescriptions.contains(rawInt)) {
        result.valueDescription = sig.valueDescriptions[rawInt];
    }

    // Check if this is SNA (Signal Not Available)
    // SNA is typically the max value for the bit length, or explicitly listed
    if (result.valueDescription == "SNA") {
        result.isValid = false;
    }

    return result;
}

bool SignalCodec::encodeSignal(CanFrame& frame, const DbcSignal& sig, double physicalValue) const
{
    // Clamp to valid range
    double clamped = std::clamp(physicalValue, sig.minimum, sig.maximum);
    uint64_t raw = physicalToRaw(clamped, sig.factor, sig.offset);

    // Clamp to bit width
    uint64_t maxVal = (sig.bitLength >= 64) ? UINT64_MAX : ((1ULL << sig.bitLength) - 1);
    if (raw > maxVal) raw = maxVal;

    insertBits(frame.data.data(), sig.startBit, sig.bitLength, sig.littleEndian, raw);
    return true;
}

bool SignalCodec::encodeSignalRaw(CanFrame& frame, const DbcSignal& sig, uint64_t rawValue) const
{
    uint64_t maxVal = (sig.bitLength >= 64) ? UINT64_MAX : ((1ULL << sig.bitLength) - 1);
    if (rawValue > maxVal) rawValue = maxVal;

    insertBits(frame.data.data(), sig.startBit, sig.bitLength, sig.littleEndian, rawValue);
    return true;
}

uint64_t SignalCodec::extractBits(const uint8_t* data, uint32_t startBit,
                                   uint32_t bitLength, bool littleEndian)
{
    uint64_t result = 0;

    if (littleEndian) {
        // Intel byte order: startBit is the LSB position
        for (uint32_t i = 0; i < bitLength; ++i) {
            uint32_t bitPos = startBit + i;
            uint32_t byteIdx = bitPos / 8;
            uint32_t bitIdx = bitPos % 8;
            if (byteIdx < 8) {
                if (data[byteIdx] & (1 << bitIdx)) {
                    result |= (1ULL << i);
                }
            }
        }
    }
    else {
        // Motorola byte order: startBit is the MSB position
        // Convert Motorola start bit to bit positions
        for (uint32_t i = 0; i < bitLength; ++i) {
            uint32_t byteNum = startBit / 8;
            uint32_t bitNum = startBit % 8;

            // Calculate the actual bit position for the i-th bit (MSB first)
            int srcByte = static_cast<int>(byteNum);
            int srcBit = static_cast<int>(bitNum) - static_cast<int>(i);

            while (srcBit < 0) {
                srcByte++;
                srcBit += 8;
            }

            if (srcByte < 8 && srcByte >= 0) {
                if (data[srcByte] & (1 << srcBit)) {
                    result |= (1ULL << (bitLength - 1 - i));
                }
            }
        }
    }

    return result;
}

void SignalCodec::insertBits(uint8_t* data, uint32_t startBit,
                              uint32_t bitLength, bool littleEndian, uint64_t value)
{
    if (littleEndian) {
        for (uint32_t i = 0; i < bitLength; ++i) {
            uint32_t bitPos = startBit + i;
            uint32_t byteIdx = bitPos / 8;
            uint32_t bitIdx = bitPos % 8;
            if (byteIdx < 8) {
                if (value & (1ULL << i)) {
                    data[byteIdx] |= (1 << bitIdx);
                } else {
                    data[byteIdx] &= ~(1 << bitIdx);
                }
            }
        }
    }
    else {
        for (uint32_t i = 0; i < bitLength; ++i) {
            uint32_t byteNum = startBit / 8;
            uint32_t bitNum = startBit % 8;

            int srcByte = static_cast<int>(byteNum);
            int srcBit = static_cast<int>(bitNum) - static_cast<int>(i);

            while (srcBit < 0) {
                srcByte++;
                srcBit += 8;
            }

            if (srcByte < 8 && srcByte >= 0) {
                if (value & (1ULL << (bitLength - 1 - i))) {
                    data[srcByte] |= (1 << srcBit);
                } else {
                    data[srcByte] &= ~(1 << srcBit);
                }
            }
        }
    }
}

uint64_t SignalCodec::physicalToRaw(double physical, double factor, double offset)
{
    if (factor == 0.0) return 0;
    double raw = (physical - offset) / factor;
    return static_cast<uint64_t>(std::round(raw));
}

double SignalCodec::rawToPhysical(uint64_t raw, double factor, double offset,
                                   bool isSigned, uint32_t bitLength)
{
    if (isSigned && bitLength < 64) {
        // Sign-extend
        uint64_t signBit = 1ULL << (bitLength - 1);
        if (raw & signBit) {
            // Negative value - sign extend
            int64_t signedRaw = static_cast<int64_t>(raw | (~0ULL << bitLength));
            return static_cast<double>(signedRaw) * factor + offset;
        }
    }
    return static_cast<double>(raw) * factor + offset;
}

} // namespace ccs
