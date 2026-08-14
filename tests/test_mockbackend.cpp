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
};

QTEST_GUILESS_MAIN(TestMockBackend)
#include "test_mockbackend.moc"
