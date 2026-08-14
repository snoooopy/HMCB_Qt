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
};

QTEST_GUILESS_MAIN(TestMockBackend)
#include "test_mockbackend.moc"
