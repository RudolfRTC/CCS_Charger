#include <QtTest/QtTest>
#include "module/safety_monitor.h"

using namespace ccs;

class TestSafetyMonitor : public QObject {
    Q_OBJECT

private slots:
    // ── Voltage clamping tests ───────────────────────────────

    void testClampVoltage_withinRange()
    {
        SafetyMonitor monitor;
        // Default user max is 500V
        QCOMPARE(monitor.clampVoltage(400.0), 400.0);
    }

    void testClampVoltage_aboveUserMax()
    {
        SafetyMonitor monitor;
        // Default user max is 500V
        double clamped = monitor.clampVoltage(600.0);
        QCOMPARE(clamped, 500.0);
    }

    void testClampVoltage_belowMin()
    {
        SafetyMonitor monitor;
        double clamped = monitor.clampVoltage(-10.0);
        QCOMPARE(clamped, 0.0);
    }

    void testClampVoltage_zero()
    {
        SafetyMonitor monitor;
        QCOMPARE(monitor.clampVoltage(0.0), 0.0);
    }

    // ── Current clamping tests ───────────────────────────────

    void testClampCurrent_withinRange()
    {
        SafetyMonitor monitor;
        // Default user max is 200A
        QCOMPARE(monitor.clampCurrent(150.0), 150.0);
    }

    void testClampCurrent_aboveUserMax()
    {
        SafetyMonitor monitor;
        double clamped = monitor.clampCurrent(300.0);
        QCOMPARE(clamped, 200.0);
    }

    void testClampCurrent_negative()
    {
        SafetyMonitor monitor;
        // Min current is -3250A (regen)
        double clamped = monitor.clampCurrent(-100.0);
        QCOMPARE(clamped, -100.0);
    }

    void testClampCurrent_belowMin()
    {
        SafetyMonitor monitor;
        double clamped = monitor.clampCurrent(-5000.0);
        QCOMPARE(clamped, -3250.0);
    }

    // ── Power clamping tests ─────────────────────────────────

    void testClampPower_withinRange()
    {
        SafetyMonitor monitor;
        QCOMPARE(monitor.clampPower(50000.0), 50000.0);
    }

    void testClampPower_aboveUserMax()
    {
        SafetyMonitor monitor;
        // Default user max power is 100000W
        double clamped = monitor.clampPower(200000.0);
        QCOMPARE(clamped, 100000.0);
    }

    void testClampPower_negative()
    {
        SafetyMonitor monitor;
        double clamped = monitor.clampPower(-100.0);
        QCOMPARE(clamped, 0.0);
    }

    // ── Range validation tests ───────────────────────────────

    void testIsVoltageInRange()
    {
        SafetyMonitor monitor;
        QVERIFY(monitor.isVoltageInRange(400.0));
        QVERIFY(monitor.isVoltageInRange(0.0));
        QVERIFY(!monitor.isVoltageInRange(600.0));
        QVERIFY(!monitor.isVoltageInRange(-1.0));
    }

    void testIsCurrentInRange()
    {
        SafetyMonitor monitor;
        QVERIFY(monitor.isCurrentInRange(150.0));
        QVERIFY(monitor.isCurrentInRange(0.0));
        QVERIFY(monitor.isCurrentInRange(-100.0));
        QVERIFY(!monitor.isCurrentInRange(300.0));
        QVERIFY(!monitor.isCurrentInRange(-4000.0));
    }

    // ── User limits tests ────────────────────────────────────

    void testSetUserLimits()
    {
        SafetyMonitor monitor;
        monitor.setUserLimits(800.0, 300.0, 200000.0);

        QCOMPARE(monitor.limits().userMaxVoltage, 800.0);
        QCOMPARE(monitor.limits().userMaxCurrent, 300.0);
        QCOMPARE(monitor.limits().userMaxPower, 200000.0);
    }

    void testSetUserLimits_clampedToHardLimits()
    {
        SafetyMonitor monitor;
        // Try to set above hard limits (6500V, 6500A)
        monitor.setUserLimits(10000.0, 10000.0, 999999999.0);

        QCOMPARE(monitor.limits().userMaxVoltage, 6500.0);
        QCOMPARE(monitor.limits().userMaxCurrent, 6500.0);
        QCOMPARE(monitor.limits().userMaxPower, 3276700.0);
    }

    void testSetUserLimits_negative()
    {
        SafetyMonitor monitor;
        monitor.setUserLimits(-100.0, -100.0, -100.0);

        QCOMPARE(monitor.limits().userMaxVoltage, 0.0);
        QCOMPARE(monitor.limits().userMaxCurrent, 0.0);
        QCOMPARE(monitor.limits().userMaxPower, 0.0);
    }

    void testClampVoltageWithCustomLimits()
    {
        SafetyMonitor monitor;
        monitor.setUserLimits(800.0, 300.0, 200000.0);
        QCOMPARE(monitor.clampVoltage(750.0), 750.0);
        QCOMPARE(monitor.clampVoltage(900.0), 800.0);
    }

    // ── Heartbeat tests ──────────────────────────────────────

    void testHeartbeatInitialState()
    {
        SafetyMonitor monitor;
        QCOMPARE(monitor.isHeartbeatOk(), false);
    }

    void testHeartbeatUpdate()
    {
        SafetyMonitor monitor;

        QSignalSpy restoredSpy(&monitor, &SafetyMonitor::heartbeatRestored);

        monitor.updateAliveCounter(0);
        QCOMPARE(monitor.isHeartbeatOk(), true);
        QCOMPARE(restoredSpy.count(), 1);
    }

    void testHeartbeatIgnoresSNA()
    {
        SafetyMonitor monitor;
        monitor.updateAliveCounter(15); // SNA
        QCOMPARE(monitor.isHeartbeatOk(), false);
    }

    void testHeartbeatSameCounterNoRestore()
    {
        SafetyMonitor monitor;
        monitor.updateAliveCounter(5);
        QVERIFY(monitor.isHeartbeatOk());

        QSignalSpy restoredSpy(&monitor, &SafetyMonitor::heartbeatRestored);
        // Same counter value - no change
        monitor.updateAliveCounter(5);
        QCOMPARE(restoredSpy.count(), 0);
    }

    void testHeartbeatTimeout()
    {
        SafetyMonitor monitor;

        // First, establish heartbeat
        monitor.updateAliveCounter(0);
        QVERIFY(monitor.isHeartbeatOk());

        QSignalSpy lostSpy(&monitor, &SafetyMonitor::heartbeatLost);

        // Wait for heartbeat timeout (1500ms) + some margin
        QTRY_VERIFY_WITH_TIMEOUT(lostSpy.count() > 0, 2500);
        QCOMPARE(monitor.isHeartbeatOk(), false);
    }

    // ── Message timeout tests ────────────────────────────────

    void testMessageTimedOut_neverReceived()
    {
        SafetyMonitor monitor;
        QVERIFY(monitor.isMessageTimedOut(0x0600));
    }

    void testMessageReceived_notTimedOut()
    {
        SafetyMonitor monitor;
        monitor.messageReceived(0x0600);
        QVERIFY(!monitor.isMessageTimedOut(0x0600));
    }

    void testMessageTimeout_afterDelay()
    {
        SafetyMonitor monitor;

        QSignalSpy timeoutSpy(&monitor, &SafetyMonitor::messageTimeout);

        monitor.messageReceived(0x0600);
        QVERIFY(!monitor.isMessageTimedOut(0x0600));

        // Wait for message timeout (1000ms) + margin
        QTRY_VERIFY_WITH_TIMEOUT(timeoutSpy.count() > 0, 2000);
        QVERIFY(monitor.isMessageTimedOut(0x0600));
    }

    // ── Emergency stop tests ─────────────────────────────────

    void testEmergencyStopInitialState()
    {
        SafetyMonitor monitor;
        QCOMPARE(monitor.isEmergencyStopped(), false);
    }

    void testTriggerEmergencyStop()
    {
        SafetyMonitor monitor;

        QSignalSpy stopSpy(&monitor, &SafetyMonitor::emergencyStopTriggered);

        monitor.triggerEmergencyStop("Test reason");
        QCOMPARE(monitor.isEmergencyStopped(), true);
        QCOMPARE(stopSpy.count(), 1);
        QCOMPARE(stopSpy.at(0).at(0).toString(), QString("Test reason"));
    }

    void testTriggerEmergencyStop_onlyOnce()
    {
        SafetyMonitor monitor;

        QSignalSpy stopSpy(&monitor, &SafetyMonitor::emergencyStopTriggered);

        monitor.triggerEmergencyStop("First");
        monitor.triggerEmergencyStop("Second");
        // Should only emit once
        QCOMPARE(stopSpy.count(), 1);
    }

    void testClearEmergencyStop()
    {
        SafetyMonitor monitor;

        QSignalSpy clearedSpy(&monitor, &SafetyMonitor::emergencyStopCleared);

        monitor.triggerEmergencyStop("Test");
        QVERIFY(monitor.isEmergencyStopped());

        monitor.clearEmergencyStop();
        QCOMPARE(monitor.isEmergencyStopped(), false);
        QCOMPARE(clearedSpy.count(), 1);
    }

    void testClearEmergencyStop_whenNotStopped()
    {
        SafetyMonitor monitor;

        QSignalSpy clearedSpy(&monitor, &SafetyMonitor::emergencyStopCleared);

        monitor.clearEmergencyStop(); // Nothing to clear
        QCOMPARE(clearedSpy.count(), 0);
    }

    // ── Error code tests ─────────────────────────────────────

    void testErrorCodeDescription_unplugged()
    {
        QString desc = SafetyMonitor::errorCodeDescription(0);
        QVERIFY(desc.contains("UNPLUGGED"));
    }

    void testErrorCodeDescription_statusOk()
    {
        QString desc = SafetyMonitor::errorCodeDescription(1);
        QVERIFY(desc.contains("STATUS_OK"));
    }

    void testErrorCodeDescription_limitsTimeout()
    {
        QString desc = SafetyMonitor::errorCodeDescription(162);
        QVERIFY(desc.contains("LIMITS_MSG_TIMEOUT"));
    }

    void testErrorCodeDescription_evseEmergency()
    {
        QString desc = SafetyMonitor::errorCodeDescription(161);
        QVERIFY(desc.contains("EVSE_EMERGENCY"));
    }

    void testErrorCodeDescription_prechargeTimeout()
    {
        QString desc = SafetyMonitor::errorCodeDescription(218);
        QVERIFY(desc.contains("PRECHARGETIMER_TIMEOUT"));
    }

    void testErrorCodeDescription_snaErrors()
    {
        QVERIFY(SafetyMonitor::errorCodeDescription(240).contains("EV_ERROR_CODE_SNA"));
        QVERIFY(SafetyMonitor::errorCodeDescription(241).contains("EV_READY_SNA"));
        QVERIFY(SafetyMonitor::errorCodeDescription(242).contains("EV_SOC_SNA"));
        QVERIFY(SafetyMonitor::errorCodeDescription(246).contains("EV_MAX_VOLT_SNA"));
        QVERIFY(SafetyMonitor::errorCodeDescription(247).contains("EV_MAX_CUR_SNA"));
    }

    void testErrorCodeAction_noAction()
    {
        QCOMPARE(SafetyMonitor::errorCodeAction(0), QString("No action needed"));
        QCOMPARE(SafetyMonitor::errorCodeAction(1), QString("No action needed"));
    }

    void testErrorCodeAction_canTimeout()
    {
        QString action = SafetyMonitor::errorCodeAction(162);
        QVERIFY(action.contains("CAN timeout"));
    }

    void testErrorCodeAction_mandatorySignals()
    {
        QString action = SafetyMonitor::errorCodeAction(240);
        QVERIFY(action.contains("mandatory"));
    }

    void testErrorCodeAction_emergencyStop()
    {
        QString action = SafetyMonitor::errorCodeAction(249);
        QVERIFY(action.contains("Emergency stop"));
    }

    // ── Hard limits verification ─────────────────────────────

    void testHardLimitsFromDatasheet()
    {
        SafetyMonitor monitor;
        const auto& limits = monitor.limits();
        QCOMPARE(limits.maxVoltage, 6500.0);
        QCOMPARE(limits.maxCurrent, 6500.0);
        QCOMPARE(limits.minVoltage, 0.0);
        QCOMPARE(limits.minCurrent, -3250.0);
    }
};

QTEST_MAIN(TestSafetyMonitor)
#include "test_safety_monitor.moc"
