#include <QtTest/QtTest>
#include "module/state_machine.h"

using namespace ccs;

class TestStateMachine : public QObject {
    Q_OBJECT

private slots:
    // ── CmsState enum tests ─────────────────────────────────

    void testCmsStateValues()
    {
        QCOMPARE(static_cast<uint8_t>(CmsState::Default), static_cast<uint8_t>(0));
        QCOMPARE(static_cast<uint8_t>(CmsState::Init), static_cast<uint8_t>(1));
        QCOMPARE(static_cast<uint8_t>(CmsState::Authentication), static_cast<uint8_t>(2));
        QCOMPARE(static_cast<uint8_t>(CmsState::Parameter), static_cast<uint8_t>(3));
        QCOMPARE(static_cast<uint8_t>(CmsState::Isolation), static_cast<uint8_t>(4));
        QCOMPARE(static_cast<uint8_t>(CmsState::PreCharge), static_cast<uint8_t>(5));
        QCOMPARE(static_cast<uint8_t>(CmsState::Charge), static_cast<uint8_t>(6));
        QCOMPARE(static_cast<uint8_t>(CmsState::Welding), static_cast<uint8_t>(7));
        QCOMPARE(static_cast<uint8_t>(CmsState::StopCharge), static_cast<uint8_t>(8));
        QCOMPARE(static_cast<uint8_t>(CmsState::SessionStop), static_cast<uint8_t>(9));
        QCOMPARE(static_cast<uint8_t>(CmsState::ShutOff), static_cast<uint8_t>(10));
        QCOMPARE(static_cast<uint8_t>(CmsState::Paused), static_cast<uint8_t>(11));
        QCOMPARE(static_cast<uint8_t>(CmsState::Error), static_cast<uint8_t>(12));
        QCOMPARE(static_cast<uint8_t>(CmsState::SNA), static_cast<uint8_t>(15));
    }

    void testCmsStateToString()
    {
        QCOMPARE(QString(cmsStateToString(CmsState::Default)), QString("Default"));
        QCOMPARE(QString(cmsStateToString(CmsState::Init)), QString("Init"));
        QCOMPARE(QString(cmsStateToString(CmsState::Authentication)), QString("Authentication"));
        QCOMPARE(QString(cmsStateToString(CmsState::Parameter)), QString("Parameter"));
        QCOMPARE(QString(cmsStateToString(CmsState::Isolation)), QString("Isolation"));
        QCOMPARE(QString(cmsStateToString(CmsState::PreCharge)), QString("PreCharge"));
        QCOMPARE(QString(cmsStateToString(CmsState::Charge)), QString("Charge"));
        QCOMPARE(QString(cmsStateToString(CmsState::Welding)), QString("Welding"));
        QCOMPARE(QString(cmsStateToString(CmsState::StopCharge)), QString("StopCharge"));
        QCOMPARE(QString(cmsStateToString(CmsState::SessionStop)), QString("SessionStop"));
        QCOMPARE(QString(cmsStateToString(CmsState::ShutOff)), QString("ShutOff"));
        QCOMPARE(QString(cmsStateToString(CmsState::Paused)), QString("Paused"));
        QCOMPARE(QString(cmsStateToString(CmsState::Error)), QString("Error"));
        QCOMPARE(QString(cmsStateToString(CmsState::SNA)), QString("SNA"));
    }

    // ── ControlPilotState tests ──────────────────────────────

    void testControlPilotStateValues()
    {
        QCOMPARE(static_cast<uint8_t>(ControlPilotState::A), static_cast<uint8_t>(0));
        QCOMPARE(static_cast<uint8_t>(ControlPilotState::B), static_cast<uint8_t>(1));
        QCOMPARE(static_cast<uint8_t>(ControlPilotState::C), static_cast<uint8_t>(2));
        QCOMPARE(static_cast<uint8_t>(ControlPilotState::D), static_cast<uint8_t>(3));
        QCOMPARE(static_cast<uint8_t>(ControlPilotState::E), static_cast<uint8_t>(4));
        QCOMPARE(static_cast<uint8_t>(ControlPilotState::F), static_cast<uint8_t>(5));
        QCOMPARE(static_cast<uint8_t>(ControlPilotState::Invalid), static_cast<uint8_t>(14));
        QCOMPARE(static_cast<uint8_t>(ControlPilotState::SNA), static_cast<uint8_t>(15));
    }

    // ── EvseStatusCode tests ─────────────────────────────────

    void testEvseStatusCodeValues()
    {
        QCOMPARE(static_cast<uint8_t>(EvseStatusCode::NotReady), static_cast<uint8_t>(0));
        QCOMPARE(static_cast<uint8_t>(EvseStatusCode::Ready), static_cast<uint8_t>(1));
        QCOMPARE(static_cast<uint8_t>(EvseStatusCode::EmergencyShutdown), static_cast<uint8_t>(5));
        QCOMPARE(static_cast<uint8_t>(EvseStatusCode::Malfunction), static_cast<uint8_t>(6));
        QCOMPARE(static_cast<uint8_t>(EvseStatusCode::SNA), static_cast<uint8_t>(15));
    }

    // ── EvseIsolationStatus tests ────────────────────────────

    void testEvseIsolationStatusValues()
    {
        QCOMPARE(static_cast<uint8_t>(EvseIsolationStatus::Invalid), static_cast<uint8_t>(0));
        QCOMPARE(static_cast<uint8_t>(EvseIsolationStatus::Valid), static_cast<uint8_t>(1));
        QCOMPARE(static_cast<uint8_t>(EvseIsolationStatus::Warning), static_cast<uint8_t>(2));
        QCOMPARE(static_cast<uint8_t>(EvseIsolationStatus::Fault), static_cast<uint8_t>(3));
        QCOMPARE(static_cast<uint8_t>(EvseIsolationStatus::SNA), static_cast<uint8_t>(7));
    }

    // ── ChargeProtocol tests ─────────────────────────────────

    void testChargeProtocolValues()
    {
        QCOMPARE(static_cast<uint8_t>(ChargeProtocol::NotDefined), static_cast<uint8_t>(0));
        QCOMPARE(static_cast<uint8_t>(ChargeProtocol::DIN70121), static_cast<uint8_t>(1));
        QCOMPARE(static_cast<uint8_t>(ChargeProtocol::ISO15118), static_cast<uint8_t>(2));
        QCOMPARE(static_cast<uint8_t>(ChargeProtocol::NotSupported), static_cast<uint8_t>(3));
        QCOMPARE(static_cast<uint8_t>(ChargeProtocol::SNA), static_cast<uint8_t>(15));
    }

    // ── ChargeProgressIndication tests ───────────────────────

    void testChargeProgressValues()
    {
        QCOMPARE(static_cast<uint8_t>(ChargeProgressIndication::Start), static_cast<uint8_t>(0));
        QCOMPARE(static_cast<uint8_t>(ChargeProgressIndication::Stop), static_cast<uint8_t>(1));
        QCOMPARE(static_cast<uint8_t>(ChargeProgressIndication::SNA), static_cast<uint8_t>(3));
    }

    // ── ChargeStopIndication tests ───────────────────────────

    void testChargeStopValues()
    {
        QCOMPARE(static_cast<uint8_t>(ChargeStopIndication::Terminate), static_cast<uint8_t>(0));
        QCOMPARE(static_cast<uint8_t>(ChargeStopIndication::NoStop), static_cast<uint8_t>(2));
        QCOMPARE(static_cast<uint8_t>(ChargeStopIndication::SNA), static_cast<uint8_t>(3));
    }

    // ── BCBControl tests ─────────────────────────────────────

    void testBCBControlValues()
    {
        QCOMPARE(static_cast<uint8_t>(BCBControl::Stop), static_cast<uint8_t>(0));
        QCOMPARE(static_cast<uint8_t>(BCBControl::Start), static_cast<uint8_t>(1));
        QCOMPARE(static_cast<uint8_t>(BCBControl::SNA), static_cast<uint8_t>(3));
    }

    // ── Casting tests (DBC interop) ──────────────────────────

    void testStateCastFromRawValue()
    {
        // Verify casting from raw DBC value to enum
        for (uint8_t i = 0; i <= 12; ++i) {
            auto state = static_cast<CmsState>(i);
            QVERIFY(QString(cmsStateToString(state)) != QString("Unknown"));
        }
        auto sna = static_cast<CmsState>(15);
        QCOMPARE(QString(cmsStateToString(sna)), QString("SNA"));
    }
};

QTEST_MAIN(TestStateMachine)
#include "test_state_machine.moc"
