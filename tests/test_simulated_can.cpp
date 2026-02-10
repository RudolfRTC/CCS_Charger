#include <QtTest/QtTest>
#include "can/can_interface.h"

using namespace ccs;

class TestSimulatedCan : public QObject {
    Q_OBJECT

private slots:
    void testInitialState()
    {
        SimulatedCanInterface iface;
        QCOMPARE(iface.isOpen(), false);
        QCOMPARE(iface.status(), CanStatus::Disconnected);
        QVERIFY(iface.lastError().isEmpty());
    }

    void testAvailableChannels()
    {
        SimulatedCanInterface iface;
        auto channels = iface.availableChannels();
        QCOMPARE(channels.size(), 2u);
        QCOMPARE(channels[0].name, QString("Simulated CAN 1"));
        QCOMPARE(channels[0].handle, static_cast<uint16_t>(0x0001));
        QCOMPARE(channels[1].name, QString("Simulated CAN 2"));
        QCOMPARE(channels[1].handle, static_cast<uint16_t>(0x0002));
    }

    void testOpenClose()
    {
        SimulatedCanInterface iface;

        QSignalSpy statusSpy(&iface, &CanInterface::statusChanged);

        QVERIFY(iface.open(0x0001, 500000));
        QCOMPARE(iface.isOpen(), true);
        QCOMPARE(iface.status(), CanStatus::Ok);
        QCOMPARE(statusSpy.count(), 1);
        QCOMPARE(statusSpy.at(0).at(0).value<CanStatus>(), CanStatus::Ok);

        iface.close();
        QCOMPARE(iface.isOpen(), false);
        QCOMPARE(iface.status(), CanStatus::Disconnected);
        QCOMPARE(statusSpy.count(), 2);
        QCOMPARE(statusSpy.at(1).at(0).value<CanStatus>(), CanStatus::Disconnected);
    }

    void testWriteWhenClosed()
    {
        SimulatedCanInterface iface;
        CanFrame frame;
        frame.id = 0x100;
        frame.dlc = 2;
        QCOMPARE(iface.write(frame), false);
    }

    void testWriteWhenOpen()
    {
        SimulatedCanInterface iface;
        iface.open(0x0001, 500000);

        CanFrame frame;
        frame.id = 0x1302;
        frame.extended = true;
        frame.dlc = 8;
        QCOMPARE(iface.write(frame), true);

        iface.close();
    }

    void testInjectFrame()
    {
        SimulatedCanInterface iface;

        QSignalSpy frameSpy(&iface, &CanInterface::frameReceived);

        CanFrame frame;
        frame.id = 0x0600;
        frame.extended = true;
        frame.dlc = 8;
        frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        frame.timestamp = std::chrono::steady_clock::now();

        iface.injectFrame(frame);

        QCOMPARE(frameSpy.count(), 1);
        auto receivedFrame = frameSpy.at(0).at(0).value<CanFrame>();
        QCOMPARE(receivedFrame.id, 0x0600u);
        QCOMPARE(receivedFrame.extended, true);
        QCOMPARE(receivedFrame.dlc, static_cast<uint8_t>(8));
        QCOMPARE(receivedFrame.data[0], static_cast<uint8_t>(0x01));
    }

    void testSimulationEmitsFrames()
    {
        SimulatedCanInterface iface;

        QSignalSpy frameSpy(&iface, &CanInterface::frameReceived);

        iface.open(0x0001, 500000);

        // Wait for at least one simulation tick (100ms)
        QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() > 0, 500);

        // Verify we received ChargeInfo (0x0600), EVSEDCStatus (0x1402),
        // EVSEDCMaxLimits (0x1400), and ErrorCodes (0x2002)
        QSet<uint32_t> receivedIds;
        for (int i = 0; i < frameSpy.count(); ++i) {
            auto f = frameSpy.at(i).at(0).value<CanFrame>();
            receivedIds.insert(f.id);
        }
        QVERIFY(receivedIds.contains(0x0600u));  // ChargeInfo
        QVERIFY(receivedIds.contains(0x1402u));  // EVSEDCStatus
        QVERIFY(receivedIds.contains(0x1400u));  // EVSEDCMaxLimits
        QVERIFY(receivedIds.contains(0x2002u));  // ErrorCodes

        iface.close();
    }

    void testSimulationStateAdvances()
    {
        SimulatedCanInterface iface;
        QSignalSpy frameSpy(&iface, &CanInterface::frameReceived);

        iface.open(0x0001, 500000);

        // After ~1s (10 ticks), state should advance from Default(0) to Init(1)
        QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() > 40, 2000);

        // Look for ChargeInfo frames and check state machine state
        bool foundInit = false;
        for (int i = 0; i < frameSpy.count(); ++i) {
            auto f = frameSpy.at(i).at(0).value<CanFrame>();
            if (f.id == 0x0600) {
                uint8_t state = f.data[1] & 0x0F;
                if (state == 1) { // Init
                    foundInit = true;
                    break;
                }
            }
        }
        QVERIFY(foundInit);

        iface.close();
    }
};

QTEST_MAIN(TestSimulatedCan)
#include "test_simulated_can.moc"
