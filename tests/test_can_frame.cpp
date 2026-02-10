#include <QtTest/QtTest>
#include "can/can_frame.h"

using namespace ccs;

class TestCanFrame : public QObject {
    Q_OBJECT

private slots:
    void testDefaultConstruction()
    {
        CanFrame frame;
        QCOMPARE(frame.id, 0u);
        QCOMPARE(frame.extended, false);
        QCOMPARE(frame.dlc, static_cast<uint8_t>(0));
        for (int i = 0; i < 8; ++i)
            QCOMPARE(frame.data[i], static_cast<uint8_t>(0));
    }

    void testToHexString_empty()
    {
        CanFrame frame;
        frame.dlc = 0;
        QCOMPARE(frame.toHexString(), QString(""));
    }

    void testToHexString_singleByte()
    {
        CanFrame frame;
        frame.dlc = 1;
        frame.data[0] = 0xAB;
        QCOMPARE(frame.toHexString(), QString("AB"));
    }

    void testToHexString_multipleBytes()
    {
        CanFrame frame;
        frame.dlc = 4;
        frame.data[0] = 0x00;
        frame.data[1] = 0xFF;
        frame.data[2] = 0x12;
        frame.data[3] = 0x34;
        QCOMPARE(frame.toHexString(), QString("00 FF 12 34"));
    }

    void testToHexString_fullFrame()
    {
        CanFrame frame;
        frame.dlc = 8;
        frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        QCOMPARE(frame.toHexString(), QString("01 02 03 04 05 06 07 08"));
    }

    void testToHexString_clampedTo8()
    {
        CanFrame frame;
        frame.dlc = 10; // invalid DLC > 8 should be clamped
        frame.data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
        // Should only produce 8 bytes
        QCOMPARE(frame.toHexString(), QString("AA BB CC DD EE FF 11 22"));
    }

    void testIdString_standard()
    {
        CanFrame frame;
        frame.id = 0x667;
        frame.extended = false;
        QCOMPARE(frame.idString(), QString("667"));
    }

    void testIdString_standardSmall()
    {
        CanFrame frame;
        frame.id = 0x01;
        frame.extended = false;
        QCOMPARE(frame.idString(), QString("001"));
    }

    void testIdString_extended()
    {
        CanFrame frame;
        frame.id = 0x0600;
        frame.extended = true;
        QCOMPARE(frame.idString(), QString("00000600"));
    }

    void testIdString_extendedLarge()
    {
        CanFrame frame;
        frame.id = 0x1FFFFFFF;
        frame.extended = true;
        QCOMPARE(frame.idString(), QString("1FFFFFFF"));
    }

    void testCanStatusToString()
    {
        QCOMPARE(QString(canStatusToString(CanStatus::Ok)), QString("OK"));
        QCOMPARE(QString(canStatusToString(CanStatus::BusOff)), QString("Bus Off"));
        QCOMPARE(QString(canStatusToString(CanStatus::BusWarning)), QString("Bus Warning"));
        QCOMPARE(QString(canStatusToString(CanStatus::BusPassive)), QString("Bus Passive"));
        QCOMPARE(QString(canStatusToString(CanStatus::Error)), QString("Error"));
        QCOMPARE(QString(canStatusToString(CanStatus::Disconnected)), QString("Disconnected"));
    }
};

QTEST_MAIN(TestCanFrame)
#include "test_can_frame.moc"
