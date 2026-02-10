#include <QtTest/QtTest>
#include "logging/can_logger.h"
#include "logging/session_report.h"
#include "dbc/dbc_parser.h"
#include "dbc/signal_codec.h"
#include <QTemporaryDir>

using namespace ccs;

class TestLogging : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

private slots:
    // ── CanLogger tests ──────────────────────────────────────

    void testCanLoggerInitialState()
    {
        CanLogger logger;
        QCOMPARE(logger.isRawLogging(), false);
        QCOMPARE(logger.isDecodedLogging(), false);
        QCOMPARE(logger.rawFrameCount(), 0ULL);
        QCOMPARE(logger.decodedFrameCount(), 0ULL);
    }

    void testStartRawLog()
    {
        CanLogger logger;
        QString path = m_tempDir.filePath("raw.csv");
        QVERIFY(logger.startRawLog(path));
        QCOMPARE(logger.isRawLogging(), true);

        logger.stopAll();
        QCOMPARE(logger.isRawLogging(), false);

        // Verify file was created with header
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString header = file.readLine();
        QVERIFY(header.contains("Timestamp_ms"));
        QVERIFY(header.contains("ID"));
        QVERIFY(header.contains("DLC"));
    }

    void testStartDecodedLog()
    {
        CanLogger logger;
        QString path = m_tempDir.filePath("decoded.csv");
        QVERIFY(logger.startDecodedLog(path));
        QCOMPARE(logger.isDecodedLogging(), true);

        logger.stopAll();
        QCOMPARE(logger.isDecodedLogging(), false);

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString header = file.readLine();
        QVERIFY(header.contains("Timestamp_ms"));
        QVERIFY(header.contains("Message"));
        QVERIFY(header.contains("Signal"));
    }

    void testLogRawFrame()
    {
        CanLogger logger;
        QString path = m_tempDir.filePath("raw_frames.csv");
        QVERIFY(logger.startRawLog(path));

        CanFrame frame;
        frame.id = 0x0600;
        frame.extended = true;
        frame.dlc = 8;
        frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        frame.timestamp = std::chrono::steady_clock::now();

        logger.logFrame(frame);
        QCOMPARE(logger.rawFrameCount(), 1ULL);

        logger.stopAll();

        // Verify the frame was written
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QStringList lines;
        while (!file.atEnd()) {
            lines.append(file.readLine().trimmed());
        }
        QVERIFY(lines.size() >= 2); // header + at least one data line
        QVERIFY(lines[1].contains("00000600")); // Extended ID
        QVERIFY(lines[1].contains("EXT"));
    }

    void testLogMultipleFrames()
    {
        CanLogger logger;
        QString path = m_tempDir.filePath("multi_raw.csv");
        QVERIFY(logger.startRawLog(path));

        for (int i = 0; i < 10; ++i) {
            CanFrame frame;
            frame.id = 0x0600 + i;
            frame.extended = true;
            frame.dlc = 4;
            frame.data[0] = i;
            frame.timestamp = std::chrono::steady_clock::now();
            logger.logFrame(frame);
        }

        QCOMPARE(logger.rawFrameCount(), 10ULL);
        logger.stopAll();
    }

    void testStartRawLog_invalidPath()
    {
        CanLogger logger;
        QVERIFY(!logger.startRawLog("/nonexistent/dir/raw.csv"));
        QCOMPARE(logger.isRawLogging(), false);
    }

    void testLogFrame_noActiveLog()
    {
        CanLogger logger;
        CanFrame frame;
        frame.id = 0x0600;
        frame.dlc = 8;
        frame.timestamp = std::chrono::steady_clock::now();

        // Should not crash when not logging
        logger.logFrame(frame);
        QCOMPARE(logger.rawFrameCount(), 0ULL);
    }

    // ── SessionReport tests ──────────────────────────────────

    void testSessionReportInitialState()
    {
        SessionReport report;
        QCOMPARE(report.isActive(), false);
        QCOMPARE(report.maxVoltage(), 0.0);
        QCOMPARE(report.maxCurrent(), 0.0);
        QCOMPARE(report.maxPower(), 0.0);
        QCOMPARE(report.energyEstimateWh(), 0.0);
    }

    void testSessionStartEnd()
    {
        SessionReport report;
        report.startSession();
        QCOMPARE(report.isActive(), true);
        QVERIFY(report.startTime().isValid());

        report.endSession();
        QCOMPARE(report.isActive(), false);
        QVERIFY(report.endTime().isValid());
    }

    void testUpdateValues_tracksMaximums()
    {
        SessionReport report;
        report.startSession();

        report.updateValues(400.0, 100.0, 50.0);
        QCOMPARE(report.maxVoltage(), 400.0);
        QCOMPARE(report.maxCurrent(), 100.0);
        QCOMPARE(report.maxPower(), 40000.0);

        report.updateValues(420.0, 80.0, 55.0);
        QCOMPARE(report.maxVoltage(), 420.0);
        QCOMPARE(report.maxCurrent(), 100.0); // Previous max
        QCOMPARE(report.maxPower(), 40000.0); // Previous max (400*100 > 420*80)

        report.endSession();
    }

    void testUpdateValues_inactiveSession()
    {
        SessionReport report;
        // Don't start session
        report.updateValues(400.0, 100.0, 50.0);
        QCOMPARE(report.maxVoltage(), 0.0); // No update when inactive
    }

    void testSoCTracking()
    {
        SessionReport report;
        report.startSession();

        report.updateValues(400.0, 100.0, 30.0);
        QCOMPARE(report.startSoC(), 30.0);

        report.updateValues(400.0, 100.0, 60.0);
        QCOMPARE(report.startSoC(), 30.0); // Should not change
        QCOMPARE(report.endSoC(), 60.0);

        report.endSession();
    }

    void testEnergyIntegration()
    {
        SessionReport report;
        report.startSession();

        // Simulate constant 400V * 100A = 40kW for ~100ms
        report.updateValues(400.0, 100.0, 50.0);
        QTest::qWait(110); // Wait slightly more than 100ms
        report.updateValues(400.0, 100.0, 50.5);

        // Energy should be approximately 40000W * 0.1s / 3600 ≈ 1.11 Wh
        QVERIFY(report.energyEstimateWh() > 0.5);
        QVERIFY(report.energyEstimateWh() < 5.0); // Reasonable range

        report.endSession();
    }

    void testDurationSeconds()
    {
        SessionReport report;
        report.startSession();

        QTest::qWait(1100); // Wait ~1 second

        report.endSession();
        QVERIFY(report.durationSeconds() >= 1);
        QVERIFY(report.durationSeconds() <= 3);
    }

    void testSaveReport()
    {
        SessionReport report;
        report.startSession();
        report.updateValues(400.0, 150.0, 30.0);
        QTest::qWait(100);
        report.updateValues(410.0, 145.0, 35.0);
        report.endSession();

        QString path = m_tempDir.filePath("report.txt");
        QVERIFY(report.saveReport(path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = file.readAll();
        QVERIFY(content.contains("CCS Charging Session Report"));
        QVERIFY(content.contains("Max Voltage"));
        QVERIFY(content.contains("Max Current"));
        QVERIFY(content.contains("Max Power"));
        QVERIFY(content.contains("Energy Delivered"));
        QVERIFY(content.contains("Start SoC"));
    }

    void testSaveReport_invalidPath()
    {
        SessionReport report;
        QVERIFY(!report.saveReport("/nonexistent/dir/report.txt"));
    }

    void testSaveReport_noSoCData()
    {
        SessionReport report;
        report.startSession();
        // Don't call updateValues, so SoC should not be available
        report.endSession();

        QString path = m_tempDir.filePath("report_nosoc.txt");
        QVERIFY(report.saveReport(path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = file.readAll();
        QVERIFY(content.contains("SoC data not available"));
    }
};

QTEST_MAIN(TestLogging)
#include "test_logging.moc"
