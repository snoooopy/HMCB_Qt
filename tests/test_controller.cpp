#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <atomic>
#include "core/McbController.h"
#include "core/McbTypes.h"

class TestController : public QObject {
    Q_OBJECT
private slots:
    void relaysReadyAndFrame() {
        qRegisterMetaType<Mcb::McbFrame>("Mcb::McbFrame");
        McbController c;
        c.setBackendMode("mock");
        c.startUp();

        std::atomic<bool> ready{false}, frame{false};
        // 用 this 作为 context → 信号在工作线程 emit 时走 Queued 到 GUI 线程。
        connect(&c, &McbController::deviceReady, this, [&]{ ready = true; });
        connect(&c, &McbController::frameReady, this,
                [&](int, const Mcb::McbFrame&) { frame = true; });

        c.initializeBackend();
        QElapsedTimer t; t.start();
        while (!ready && t.elapsed() < 2000)
            qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(ready, "no deviceReady within 2s");

        c.start(0);
        t.restart();
        while (!frame && t.elapsed() < 4000)
            qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(frame, "no frame within 4s after start");

        c.stop();
        c.shutdown();
    }

    void measureZeroProducesResults() {
        qRegisterMetaType<Mcb::McbFrame>("Mcb::McbFrame");
        McbController c; c.setBackendMode("mock"); c.startUp();
        std::atomic<bool> ready{false};
        connect(&c, &McbController::deviceReady, this, [&]{ ready = true; });
        c.initializeBackend();
        QElapsedTimer t; t.start();
        while (!ready && t.elapsed() < 2000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(ready, "no deviceReady within 2s");

        std::atomic<bool> measured{false};
        connect(&c, &McbController::measurementReady, this,
                [&](int) { measured = true; });
        // 调小采样数以加速测试（默认 512 帧×100ms 太慢）
        MeasurementEngine::Config cfg = c.engine()->config();
        cfg.vzSampleNum = 5;
        c.engine()->applyConfig(cfg);
        c.measure(0, 300);           // Zero，5 帧即自停
        t.restart();
        while (!measured && t.elapsed() < 6000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(measured, "no measurementReady within 6s");
        QVERIFY2(c.engine()->vz(0).size() > 0,
                 "Zero result vector empty after measurement");
        c.shutdown();
    }

    void measureInlFullFlow() {
        qRegisterMetaType<Mcb::McbFrame>("Mcb::McbFrame");
        McbController c; c.setBackendMode("mock"); c.startUp();
        std::atomic<bool> ready{false};
        connect(&c, &McbController::deviceReady, this, [&]{ ready = true; });
        c.initializeBackend();
        QElapsedTimer t; t.start();
        while (!ready && t.elapsed() < 2000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(ready, "no deviceReady within 2s");

        MeasurementEngine::Config cfg = c.engine()->config();
        cfg.inlPoint = 5; cfg.inlSampleNum = 3;   // 5 点 × 3 帧，加速测试
        c.engine()->applyConfig(cfg);

        std::atomic<bool> done{false}; std::atomic<int> fnGot{-1};
        connect(&c, &McbController::measurementReady, this,
                [&](int fn) { fnGot = fn; done = true; });
        c.measureInl();
        t.restart();
        while (!done && t.elapsed() < 10000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(done, "no measurementReady(2) within 10s");
        QCOMPARE(fnGot.load(), 2);
        QVERIFY(c.engine()->bInl(0));
        QVERIFY2(c.engine()->inl(0).size() > 0, "INL result empty");
        QVERIFY(c.engine()->inlR(0)[0] > 0.9);    // Mock 近线性 → r 高
        c.shutdown();
    }
};

QTEST_GUILESS_MAIN(TestController)
#include "test_controller.moc"
