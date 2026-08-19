#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <atomic>
#include "core/MockMcbBackend.h"

class TestMockBackend : public QObject {
    Q_OBJECT
private slots:
    void initializeReachesReadyThenProducesFrame() {
        MockMcbBackend b;
        std::atomic<bool> gotReady{false}, gotFrame{false};
        b.setStatusCallback([&](WPARAM cat, int, uint32_t) {
            if (cat == MCB_DVREADY) gotReady = true;
        });
        b.setDataCallback([&](const Mcb::McbFrame&) { gotFrame = true; });

        b.initialize();
        QElapsedTimer t; t.start();
        while (!gotReady && t.elapsed() < 2000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(gotReady, "did not reach DVREADY within 2s");

        b.start(0);
        t.restart();
        while (!gotFrame && t.elapsed() < 4000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(gotFrame, "no data frame within 4s after start");

        QCOMPARE(b.getRunStatus(), true);
        b.stop();
        QCOMPARE(b.getRunStatus(), false);
        b.close();
    }

    void finiteAcquisitionStops() {
        MockMcbBackend b;
        std::atomic<int> frames{0};
        std::atomic<bool> stopok{false};
        b.setDataCallback([&](const Mcb::McbFrame&) { ++frames; });
        b.setStatusCallback([&](WPARAM cat, int, uint32_t) {
            if (cat == MCB_STOPOK) stopok = true;
        });

        b.initialize();
        QElapsedTimer t; t.start();
        while (!b.getDeviceReady() && t.elapsed() < 2000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(b.getDeviceReady(), "did not reach DVREADY within 2s");

        b.start(5);                 // 有限 5 帧
        t.restart();
        while (!stopok && t.elapsed() < 4000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(stopok, "no STOPOK within 4s after finite start");
        QVERIFY2(frames >= 5 && frames <= 7,
                 qPrintable(QString("expected ~5 frames, got %1").arg(frames.load())));
        b.close();
    }

    // ---- Phase 3: DA / 增益仿真 + STOPOK code 语义 ----
    void daLevelShiftsFrameMean() {
        MockMcbBackend b;
        std::atomic<bool> ready{false};
        b.setStatusCallback([&](WPARAM c, int, uint32_t) { if (c == MCB_DVREADY) ready = true; });
        b.initialize();
        QElapsedTimer t; t.start();
        while (!ready && t.elapsed() < 2000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(ready, "no DVREADY");

        std::atomic<long long> sum{0}; std::atomic<int> cnt{0};
        b.setDataCallback([&](const Mcb::McbFrame& f) {
            for (uint32_t v : f.channels) { sum += v; ++cnt; } });
        auto runMean = [&](uint32_t da) -> double {
            sum = 0; cnt = 0;
            b.setDa(da);
            b.start(6);
            QElapsedTimer w; w.start();
            while (b.getRunStatus() && w.elapsed() < 5000)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            return cnt > 0 ? double(sum.load()) / cnt.load() : 0.0;
        };
        const double m1 = runMean(64400u * 65537u);                    // 摆幅 0
        const double m2 = runMean((64400u << 16) + (64400u - 64000u)); // 摆幅 16000 码
        QVERIFY2(m1 > 100000.0, qPrintable(QString("baseline mean too low: %1").arg(m1)));
        QVERIFY2(m2 > m1 + 30000.0,
                 qPrintable(QString("DA swing should raise mean: m1=%1 m2=%2").arg(m1).arg(m2)));
        b.close();
    }

    void dmRangeLiftLowersMean() {
        MockMcbBackend b;
        std::atomic<bool> ready{false};
        b.setStatusCallback([&](WPARAM c, int, uint32_t) { if (c == MCB_DVREADY) ready = true; });
        b.initialize();
        QElapsedTimer t; t.start();
        while (!ready && t.elapsed() < 2000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(ready, "no DVREADY");

        std::atomic<long long> sum{0}; std::atomic<int> cnt{0};
        b.setDataCallback([&](const Mcb::McbFrame& f) {
            for (uint32_t v : f.channels) { sum += v; ++cnt; } });
        auto runMean = [&]() -> double {
            sum = 0; cnt = 0;
            b.start(6);
            QElapsedTimer w; w.start();
            while (b.getRunStatus() && w.elapsed() < 5000)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            return cnt > 0 ? double(sum.load()) / cnt.load() : 0.0;
        };
        b.setDa(0);                       // DA 模型复位
        QVector<uint8_t> r0(65, 0); b.setDmRange(r0);   // 全 0 档
        const double m1 = runMean();
        QVector<uint8_t> r1(65, 1); b.setDmRange(r1);   // 全 1 档
        const double m2 = runMean();
        QVERIFY2(m1 > 900000.0, qPrintable(QString("range0 near full scale: %1").arg(m1)));
        QVERIFY2(m2 < m1 * 0.95,
                 qPrintable(QString("lift should lower mean: m1=%1 m2=%2").arg(m1).arg(m2)));
        b.close();
    }

    void stopokCodeDistinguishesNaturalAndCommand() {
        MockMcbBackend b;
        std::atomic<int> lastCode{-1};
        b.setStatusCallback([&](WPARAM c, int, uint32_t code) {
            if (c == MCB_STOPOK) lastCode = int(code); });
        b.initialize();
        QElapsedTimer t; t.start();
        while (!b.getDeviceReady() && t.elapsed() < 2000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(b.getDeviceReady(), "no DVREADY");

        b.start(3);
        t.restart();
        while (lastCode.load() != 1 && t.elapsed() < 4000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QCOMPARE(lastCode.load(), 1);     // 采满自然停 code=1

        b.start(0);
        while (!b.getRunStatus() && t.elapsed() < 2000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        b.stop();                          // 命令停：同步 join 后立即回调
        QCOMPARE(lastCode.load(), 0);
        b.close();
    }

    void setNewIpRecordsBytes() {
        // 对照 Windows DlgSetNewIPAddress：IP4+掩码4+网关4 共 12 字节
        MockMcbBackend b;
        QVERIFY(b.lastNewIp().isEmpty());
        const quint8 addr[12] = {192,111,111,212, 255,255,255,0, 192,111,111,1};
        b.setNewIp(addr);
        const auto got = b.lastNewIp();
        QCOMPARE(got.size(), 12);
        for (int i = 0; i < 12; ++i)
            QCOMPARE(got.at(i), quint8(addr[i]));
    }

    void dumpAndTimingCommandsRecorded() {
        // 对照 Windows：DUMP*=1；SetPOTMT/Set INTT 固定发 0
        MockMcbBackend b;
        QVERIFY(!b.dumpEepromRequested());
        QVERIFY(!b.dumpEcsRequested());
        b.dumpEepromKeyData();
        b.dumpAllEcsRegisters();
        QVERIFY(b.dumpEepromRequested());
        QVERIFY(b.dumpEcsRequested());
        b.setPotmt(0);
        b.setIntt(0);
        QCOMPARE(b.lastPotmt(), 0u);
        QCOMPARE(b.lastIntt(), 0u);
    }
};

QTEST_GUILESS_MAIN(TestMockBackend)
#include "test_mockbackend.moc"
