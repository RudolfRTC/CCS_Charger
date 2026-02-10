#include <QtTest/QtTest>
#include "dbc/signal_codec.h"
#include "dbc/dbc_parser.h"
#include "can/can_frame.h"
#include <cstring>

using namespace ccs;

class TestSignalCodec : public QObject {
    Q_OBJECT

private:
    DbcParser m_parser;
    SignalCodec m_codec;
    bool m_parsed = false;
    uint32_t m_chargeInfoId = 0;

    uint32_t findMessageIdByName(const QString& name) const {
        for (auto it = m_parser.database().messages.constBegin();
             it != m_parser.database().messages.constEnd(); ++it) {
            if (it->name == name) return it.key();
        }
        return 0xFFFFFFFF;
    }

    QString dbcPath() const {
        QStringList paths = {
            QCoreApplication::applicationDirPath() + "/ISC_CMS_Automotive.dbc",
            QCoreApplication::applicationDirPath() + "/../ISC_CMS_Automotive.dbc",
            QCoreApplication::applicationDirPath() + "/../../ISC_CMS_Automotive.dbc",
            QString(PROJECT_SOURCE_DIR) + "/ISC_CMS_Automotive.dbc"
        };
        for (const auto& p : paths) {
            if (QFile::exists(p)) return p;
        }
        return {};
    }

private slots:
    void initTestCase()
    {
        QString path = dbcPath();
        if (!path.isEmpty()) {
            m_parsed = m_parser.parse(path);
            if (m_parsed) {
                m_codec.setDatabase(m_parser.database());
                m_chargeInfoId = findMessageIdByName("ChargeInfo");
            }
        }
    }

    // ── Static bit manipulation tests ────────────────────────

    void testExtractBits_littleEndian_singleByte()
    {
        uint8_t data[8] = {0x05, 0, 0, 0, 0, 0, 0, 0};
        // Bits 0-6 of byte 0: value 5
        uint64_t val = SignalCodec::extractBits(data, 0, 7, true);
        QCOMPARE(val, 5ULL);
    }

    void testExtractBits_littleEndian_16bit()
    {
        uint8_t data[8] = {0xD0, 0x07, 0, 0, 0, 0, 0, 0};
        // 16-bit little-endian starting at bit 0 = 0x07D0 = 2000
        uint64_t val = SignalCodec::extractBits(data, 0, 16, true);
        QCOMPARE(val, 2000ULL);
    }

    void testExtractBits_littleEndian_4bit_offset()
    {
        uint8_t data[8] = {0, 0x31, 0, 0, 0, 0, 0, 0};
        // StateMachineState: bits 8-11 in byte 1
        uint64_t val = SignalCodec::extractBits(data, 8, 4, true);
        QCOMPARE(val, 1ULL); // Init state
    }

    void testInsertBits_littleEndian_16bit()
    {
        uint8_t data[8] = {};
        SignalCodec::insertBits(data, 0, 16, true, 2000);
        QCOMPARE(data[0], static_cast<uint8_t>(0xD0));
        QCOMPARE(data[1], static_cast<uint8_t>(0x07));
    }

    void testInsertBits_littleEndian_4bit_offset()
    {
        uint8_t data[8] = {};
        // Insert state 5 (PreCharge) at bits 8-11
        SignalCodec::insertBits(data, 8, 4, true, 5);
        QCOMPARE(data[1] & 0x0F, 5);
    }

    void testInsertBits_preservesOtherBits()
    {
        uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        // Insert 0 at bits 8-11 (should clear bits 8-11 of byte 1)
        SignalCodec::insertBits(data, 8, 4, true, 0);
        QCOMPARE(data[0], static_cast<uint8_t>(0xFF)); // unchanged
        QCOMPARE(data[1] & 0x0F, 0);   // cleared
        QCOMPARE(data[1] & 0xF0, 0xF0); // preserved
    }

    void testExtractInsertRoundtrip_littleEndian()
    {
        uint8_t data[8] = {};
        uint64_t original = 12345;
        SignalCodec::insertBits(data, 4, 16, true, original);
        uint64_t extracted = SignalCodec::extractBits(data, 4, 16, true);
        QCOMPARE(extracted, original);
    }

    void testExtractInsertRoundtrip_variousBitLengths()
    {
        for (uint32_t bits = 1; bits <= 32; ++bits) {
            uint64_t maxVal = (1ULL << bits) - 1;
            uint64_t testVal = maxVal / 2; // middle value

            uint8_t data[8] = {};
            SignalCodec::insertBits(data, 0, bits, true, testVal);
            uint64_t extracted = SignalCodec::extractBits(data, 0, bits, true);
            QCOMPARE(extracted, testVal);
        }
    }

    // ── Physical <-> Raw conversion tests ────────────────────

    void testPhysicalToRaw_simple()
    {
        // factor=0.1, offset=0 → 200A → raw=2000
        uint64_t raw = SignalCodec::physicalToRaw(200.0, 0.1, 0.0);
        QCOMPARE(raw, 2000ULL);
    }

    void testPhysicalToRaw_withOffset()
    {
        // factor=0.1, offset=-3250 → 0A → raw=(0-(-3250))/0.1=32500
        uint64_t raw = SignalCodec::physicalToRaw(0.0, 0.1, -3250.0);
        QCOMPARE(raw, 32500ULL);
    }

    void testPhysicalToRaw_zeroFactor()
    {
        uint64_t raw = SignalCodec::physicalToRaw(100.0, 0.0, 0.0);
        QCOMPARE(raw, 0ULL);
    }

    void testRawToPhysical_simple()
    {
        // raw=2000, factor=0.1, offset=0 → 200.0A
        double phys = SignalCodec::rawToPhysical(2000, 0.1, 0.0, false, 16);
        QCOMPARE(phys, 200.0);
    }

    void testRawToPhysical_withOffset()
    {
        // raw=32500, factor=0.1, offset=-3250 → 32500*0.1+(-3250) = 0.0A
        double phys = SignalCodec::rawToPhysical(32500, 0.1, -3250.0, false, 16);
        QVERIFY(qFuzzyCompare(phys, 0.0) || qAbs(phys) < 0.001);
    }

    void testRawToPhysical_signed()
    {
        // 4-bit signed, raw=0xF (=-1 in 4-bit signed), factor=1, offset=0
        double phys = SignalCodec::rawToPhysical(0xF, 1.0, 0.0, true, 4);
        QCOMPARE(phys, -1.0);
    }

    void testRawToPhysical_signedPositive()
    {
        // 4-bit signed, raw=0x7 (=7), factor=1, offset=0
        double phys = SignalCodec::rawToPhysical(0x7, 1.0, 0.0, true, 4);
        QCOMPARE(phys, 7.0);
    }

    void testPhysicalRawRoundtrip()
    {
        double original = 150.5;
        double factor = 0.1;
        double offset = 0.0;
        uint64_t raw = SignalCodec::physicalToRaw(original, factor, offset);
        double result = SignalCodec::rawToPhysical(raw, factor, offset, false, 16);
        QVERIFY(qAbs(result - original) < 0.15); // within one LSB
    }

    // ── Encode/Decode with DBC tests ─────────────────────────

    void testEncodeDecodeSignal_EVMaxCurrent()
    {
        if (!m_parsed) QSKIP("DBC file not available");

        const auto* sig = m_parser.database().findSignal(0x1300, "EVMaxCurrent");
        QVERIFY(sig != nullptr);

        CanFrame frame;
        frame.id = 0x1300;
        frame.extended = true;
        frame.dlc = 8;
        std::memset(frame.data.data(), 0, 8);

        // Encode 200A
        m_codec.encodeSignal(frame, *sig, 200.0);

        // Decode back
        auto decoded = m_codec.decodeSignal(frame, *sig);
        QVERIFY(qAbs(decoded.physicalValue - 200.0) < 0.15);
        QCOMPARE(decoded.name, QString("EVMaxCurrent"));
        QCOMPARE(decoded.unit, QString("A"));
    }

    void testEncodeDecodeSignal_EVMaxVoltage()
    {
        if (!m_parsed) QSKIP("DBC file not available");

        const auto* sig = m_parser.database().findSignal(0x1300, "EVMaxVoltage");
        QVERIFY(sig != nullptr);

        CanFrame frame;
        frame.id = 0x1300;
        frame.extended = true;
        frame.dlc = 8;
        std::memset(frame.data.data(), 0, 8);

        m_codec.encodeSignal(frame, *sig, 400.0);

        auto decoded = m_codec.decodeSignal(frame, *sig);
        QVERIFY(qAbs(decoded.physicalValue - 400.0) < 0.15);
    }

    void testEncodeSignalClampsToRange()
    {
        if (!m_parsed) QSKIP("DBC file not available");

        const auto* sig = m_parser.database().findSignal(0x1300, "EVMaxVoltage");
        QVERIFY(sig != nullptr);

        CanFrame frame;
        frame.id = 0x1300;
        frame.extended = true;
        frame.dlc = 8;
        std::memset(frame.data.data(), 0, 8);

        // Try encoding a value above max - should be clamped
        m_codec.encodeSignal(frame, *sig, 99999.0);

        auto decoded = m_codec.decodeSignal(frame, *sig);
        QVERIFY(decoded.physicalValue <= sig->maximum + 0.1);
    }

    void testEncodeDecodeRaw()
    {
        if (!m_parsed) QSKIP("DBC file not available");

        const auto* sig = m_parser.database().findSignal(0x1302, "EVReady");
        QVERIFY(sig != nullptr);

        CanFrame frame;
        frame.id = 0x1302;
        frame.extended = true;
        frame.dlc = 8;
        std::memset(frame.data.data(), 0, 8);

        m_codec.encodeSignalRaw(frame, *sig, 1); // EVReady = True

        auto decoded = m_codec.decodeSignal(frame, *sig);
        QCOMPARE(decoded.rawValue, 1ULL);
    }

    void testDecodeFullMessage()
    {
        if (!m_parsed) QSKIP("DBC file not available");
        QVERIFY(m_chargeInfoId != 0xFFFFFFFF);

        // Build a ChargeInfo frame using actual DBC CAN ID
        CanFrame frame;
        frame.id = m_chargeInfoId;
        frame.extended = true;
        frame.dlc = 8;
        std::memset(frame.data.data(), 0, 8);
        frame.data[0] = 5;          // ControlPilotDutyCycle = 5
        frame.data[1] = 0x15;       // StateMachineState=5 (PreCharge), ControlPilotState=1 (B)

        auto decoded = m_codec.decode(frame);
        QCOMPARE(decoded.canId, m_chargeInfoId);
        QCOMPARE(decoded.messageName, QString("ChargeInfo"));
        QVERIFY(decoded.decodedSignals.size() > 0);
    }

    void testDecodeUnknownMessage()
    {
        CanFrame frame;
        frame.id = 0xFFFF;
        frame.dlc = 8;

        auto decoded = m_codec.decode(frame);
        QVERIFY(decoded.messageName.isEmpty());
        QCOMPARE(decoded.decodedSignals.size(), 0);
    }

    void testDecodeWithValueDescription()
    {
        if (!m_parsed) QSKIP("DBC file not available");

        const auto* sig = m_parser.database().findSignal(m_chargeInfoId, "StateMachineState");
        QVERIFY(sig != nullptr);

        CanFrame frame;
        frame.id = m_chargeInfoId;
        frame.extended = true;
        frame.dlc = 8;
        std::memset(frame.data.data(), 0, 8);
        // Set StateMachineState to Init(1)
        SignalCodec::insertBits(frame.data.data(), sig->startBit, sig->bitLength, sig->littleEndian, 1);

        auto decoded = m_codec.decodeSignal(frame, *sig);
        QCOMPARE(decoded.rawValue, 1ULL);
        QCOMPARE(decoded.valueDescription, QString("Init"));
    }

    void testDecodeSNA()
    {
        if (!m_parsed) QSKIP("DBC file not available");

        const auto* sig = m_parser.database().findSignal(m_chargeInfoId, "StateMachineState");
        QVERIFY(sig != nullptr);

        CanFrame frame;
        frame.id = m_chargeInfoId;
        frame.extended = true;
        frame.dlc = 8;
        std::memset(frame.data.data(), 0xFF, 8); // All 1s - SNA for 4-bit field = 15

        auto decoded = m_codec.decodeSignal(frame, *sig);
        QCOMPARE(decoded.rawValue, 15ULL);
        QCOMPARE(decoded.valueDescription, QString("SNA"));
        QCOMPARE(decoded.isValid, false);
    }

    void testCodecWithoutDatabase()
    {
        SignalCodec emptyCodec;
        CanFrame frame;
        frame.id = 0x0600;
        frame.dlc = 8;

        auto decoded = emptyCodec.decode(frame);
        QVERIFY(decoded.messageName.isEmpty());
        QCOMPARE(decoded.decodedSignals.size(), 0);
    }

    void testMultipleSignalsInOneFrame()
    {
        if (!m_parsed) QSKIP("DBC file not available");

        const auto* msg = m_parser.database().findMessage(0x1300);
        QVERIFY(msg != nullptr);

        CanFrame frame;
        frame.id = 0x1300;
        frame.extended = true;
        frame.dlc = 8;
        std::memset(frame.data.data(), 0, 8);

        // Encode multiple signals
        for (const auto& sig : msg->dbcSignals) {
            if (sig.name == "EVMaxCurrent")
                m_codec.encodeSignal(frame, sig, 150.0);
            else if (sig.name == "EVMaxVoltage")
                m_codec.encodeSignal(frame, sig, 400.0);
        }

        // Decode and verify both
        auto decoded = m_codec.decode(frame);
        bool foundCurrent = false, foundVoltage = false;
        for (const auto& s : decoded.decodedSignals) {
            if (s.name == "EVMaxCurrent") {
                QVERIFY(qAbs(s.physicalValue - 150.0) < 0.15);
                foundCurrent = true;
            }
            if (s.name == "EVMaxVoltage") {
                QVERIFY(qAbs(s.physicalValue - 400.0) < 0.15);
                foundVoltage = true;
            }
        }
        QVERIFY(foundCurrent);
        QVERIFY(foundVoltage);
    }
};

QTEST_MAIN(TestSignalCodec)
#include "test_signal_codec.moc"
