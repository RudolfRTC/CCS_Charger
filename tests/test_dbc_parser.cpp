#include <QtTest/QtTest>
#include "dbc/dbc_parser.h"

using namespace ccs;

class TestDbcParser : public QObject {
    Q_OBJECT

private:
    DbcParser m_parser;
    bool m_parsed = false;
    uint32_t m_chargeInfoId = 0;

    /// Find a message CAN ID by its name (searches all messages)
    uint32_t findMessageIdByName(const QString& name) const {
        for (auto it = m_parser.database().messages.constBegin();
             it != m_parser.database().messages.constEnd(); ++it) {
            if (it->name == name) return it.key();
        }
        return 0xFFFFFFFF;
    }

    QString dbcPath() const {
        // Try to find the DBC file relative to the test executable
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
        QVERIFY2(!path.isEmpty(), "DBC file not found");
        m_parsed = m_parser.parse(path);
        QVERIFY2(m_parsed, qPrintable("Failed to parse DBC: " + m_parser.lastError()));
        m_chargeInfoId = findMessageIdByName("ChargeInfo");
    }

    void testParseSuccess()
    {
        QVERIFY(m_parsed);
    }

    void testDatabaseHasMessages()
    {
        const auto& db = m_parser.database();
        QVERIFY(db.messages.size() > 0);
    }

    void testDatabaseNodes()
    {
        const auto& db = m_parser.database();
        QVERIFY(db.nodes.size() >= 2);
        QVERIFY(db.nodes.contains("VCU") || db.nodes.contains("CMS"));
    }

    void testChargeInfoMessage()
    {
        QVERIFY(m_chargeInfoId != 0xFFFFFFFF);
        const auto* msg = m_parser.database().findMessage(m_chargeInfoId);
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->name, QString("ChargeInfo"));
        QCOMPARE(msg->dlc, static_cast<uint8_t>(8));
        QVERIFY(msg->extended);
        QVERIFY(msg->dbcSignals.size() > 0);
    }

    void testChargeInfoSignals()
    {
        const auto* sig = m_parser.database().findSignal(m_chargeInfoId, "StateMachineState");
        QVERIFY(sig != nullptr);
        QCOMPARE(sig->bitLength, 4u);

        const auto* alive = m_parser.database().findSignal(m_chargeInfoId, "AliveCounter");
        QVERIFY(alive != nullptr);
        QCOMPARE(alive->bitLength, 4u);
    }

    void testEVDCMaxLimitsMessage()
    {
        const auto* msg = m_parser.database().findMessage(0x1300);
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->name, QString("EVDCMaxLimits"));
        QVERIFY(msg->extended);
    }

    void testEVDCMaxLimitsSignals()
    {
        const auto* evMaxCurrent = m_parser.database().findSignal(0x1300, "EVMaxCurrent");
        QVERIFY(evMaxCurrent != nullptr);
        QCOMPARE(evMaxCurrent->bitLength, 16u);
        QCOMPARE(evMaxCurrent->factor, 0.1);
        QCOMPARE(evMaxCurrent->offset, 0.0);
        QCOMPARE(evMaxCurrent->unit, QString("A"));
        QVERIFY(evMaxCurrent->maximum >= 6500.0);
    }

    void testEVSEDCStatusMessage()
    {
        const auto* msg = m_parser.database().findMessage(0x1402);
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->name, QString("EVSEDCStatus"));
    }

    void testEVStatusControlMessage()
    {
        const auto* msg = m_parser.database().findMessage(0x1302);
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->name, QString("EVStatusControl"));

        // Verify key signals exist
        const auto* evReady = m_parser.database().findSignal(0x1302, "EVReady");
        QVERIFY(evReady != nullptr);

        const auto* chargeProgress = m_parser.database().findSignal(0x1302, "ChargeProgressIndication");
        QVERIFY(chargeProgress != nullptr);

        const auto* chargeStop = m_parser.database().findSignal(0x1302, "ChargeStopIndication");
        QVERIFY(chargeStop != nullptr);
    }

    void testValueDescriptions()
    {
        // StateMachineState should have value descriptions
        const auto* sig = m_parser.database().findSignal(m_chargeInfoId, "StateMachineState");
        QVERIFY(sig != nullptr);
        QVERIFY(sig->valueDescriptions.size() > 0);
        // Check if SNA is described
        QVERIFY(sig->valueDescriptions.contains(15));
        QCOMPARE(sig->valueDescriptions.value(15), QString("SNA"));
    }

    void testErrorCodesMessage()
    {
        const auto* msg = m_parser.database().findMessage(0x2002);
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->name, QString("ErrorCodes"));
    }

    void testSoftwareInfoMessage()
    {
        const auto* msg = m_parser.database().findMessage(0x2001);
        QVERIFY(msg != nullptr);
        QCOMPARE(msg->name, QString("SoftwareInfo"));
    }

    void testCycleTimeAttribute()
    {
        const auto* msg = m_parser.database().findMessage(m_chargeInfoId);
        QVERIFY(msg != nullptr);
        // ChargeInfo should have 100ms cycle time
        QCOMPARE(msg->cycleTimeMs, 100);
    }

    void testAllVcuMessagesExist()
    {
        // VCU → CMS messages
        QVERIFY(m_parser.database().findMessage(0x1300) != nullptr); // EVDCMaxLimits
        QVERIFY(m_parser.database().findMessage(0x1301) != nullptr); // EVDCChargeTargets
        QVERIFY(m_parser.database().findMessage(0x1302) != nullptr); // EVStatusControl
        QVERIFY(m_parser.database().findMessage(0x1303) != nullptr); // EVStatusDisplay
        QVERIFY(m_parser.database().findMessage(0x1304) != nullptr); // EVPlugStatus
        QVERIFY(m_parser.database().findMessage(0x1305) != nullptr); // EVDCEnergyLimits
    }

    void testAllCmsMessagesExist()
    {
        // CMS → VCU messages (ChargeInfo has dynamic ID from DBC)
        QVERIFY(m_parser.database().findMessage(m_chargeInfoId) != nullptr); // ChargeInfo
        QVERIFY(m_parser.database().findMessage(0x1400) != nullptr); // EVSEDCMaxLimits
        QVERIFY(m_parser.database().findMessage(0x1401) != nullptr); // EVSEDCRegulationLimits
        QVERIFY(m_parser.database().findMessage(0x1402) != nullptr); // EVSEDCStatus
        QVERIFY(m_parser.database().findMessage(0x2001) != nullptr); // SoftwareInfo
        QVERIFY(m_parser.database().findMessage(0x2002) != nullptr); // ErrorCodes
    }

    void testParseNonexistentFile()
    {
        DbcParser parser;
        QVERIFY(!parser.parse("/nonexistent/path/file.dbc"));
        QVERIFY(!parser.lastError().isEmpty());
    }

    void testFindSignalNonexistent()
    {
        QVERIFY(m_parser.database().findSignal(m_chargeInfoId, "NonexistentSignal") == nullptr);
        QVERIFY(m_parser.database().findSignal(0xFFFF, "AliveCounter") == nullptr);
    }

    void testSignalEndianness()
    {
        // Most CMS signals are little-endian (Intel)
        const auto* sig = m_parser.database().findSignal(0x1300, "EVMaxCurrent");
        QVERIFY(sig != nullptr);
        QCOMPARE(sig->littleEndian, true);
    }

    void testSignalRange()
    {
        const auto* sig = m_parser.database().findSignal(0x1300, "EVMaxVoltage");
        QVERIFY(sig != nullptr);
        QVERIFY(sig->minimum >= 0.0);
        QVERIFY(sig->maximum >= 6500.0);
    }
};

QTEST_MAIN(TestDbcParser)
#include "test_dbc_parser.moc"
