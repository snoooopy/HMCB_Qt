#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <atomic>
#include "core/McbController.h"
#include "core/McbTypes.h"
#include "core/IniFile.h"

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

    void gainCalibrationConvergesAndWritesBack() {
        qRegisterMetaType<Mcb::McbFrame>("Mcb::McbFrame");
        McbController c; c.setBackendMode("mock"); c.startUp();
        std::atomic<bool> ready{false};
        connect(&c, &McbController::deviceReady, this, [&]{ ready = true; });
        c.initializeBackend();
        QElapsedTimer t; t.start();
        while (!ready && t.elapsed() < 2000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(ready, "no deviceReady within 2s");

        QTemporaryDir dir;
        c.setGainWritePaths(dir.path() + "/MCBCtrl.ini", dir.path() + "/DIASConfig.xml");
        MeasurementEngine::Config cfg = c.engine()->config();
        cfg.gainFrames = 5; cfg.gainDelayMs = 10;   // 加速测试
        c.engine()->applyConfig(cfg);

        std::atomic<bool> done{false};
        connect(&c, &McbController::gainCalibReady, this, [&]{ done = true; });
        c.measureGain();
        t.restart();
        while (!done && t.elapsed() < 15000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(done, "no gainCalibReady within 15s");

        IniFile ini;
        ini.setFileName(dir.path() + "/MCBCtrl.ini");
        QVERIFY(ini.readAll());
        QVERIFY2(ini.getInt("RANGE0", "MCB0DM0", -1) >= 1,
                 qPrintable(QString("DM0 gain=%1").arg(ini.getInt("RANGE0", "MCB0DM0", -1))));
        QVERIFY2(ini.getInt("RANGE0", "MCB0DM2", -1) >= 2,
                 qPrintable(QString("DM2 gain=%1").arg(ini.getInt("RANGE0", "MCB0DM2", -1))));
        QFile x(dir.path() + "/DIASConfig.xml");
        QVERIFY2(x.open(QIODevice::ReadOnly), "DIASConfig.xml missing");
        QVERIFY(QString::fromUtf8(x.readAll()).contains("MCB0_RANGE"));
        QVERIFY(c.dmGainValues(0).size() >= 25);
        c.shutdown();
    }

    void setNewIpValidates() {
        // 对照 Windows：DlgSetNewIPAddress OK → 12 字节 → MCB_CMD_SETNEWIP(0x0601)
        McbController c;
        c.setBackendMode("mock");
        c.startUp();

        QStringList msgs;
        connect(&c, &McbController::statusMessage, this,
                [&](const QString& s, int) { msgs.append(s); });

        // 非法地址：拒绝下发，给出错误消息
        QVERIFY(!c.setNewIp("999.1.1.1", "255.255.255.0", "192.111.111.1"));
        qApp->processEvents(QEventLoop::AllEvents, 20);
        QVERIFY2(!msgs.isEmpty(), "no error message for invalid IP");
        QVERIFY(msgs.first().contains("SETNEWIP", Qt::CaseInsensitive));

        // 合法地址：接受
        msgs.clear();
        QVERIFY(c.setNewIp("192.111.111.212", "255.255.255.0", "192.111.111.1"));
        c.shutdown();
    }
};

QTEST_GUILESS_MAIN(TestController)
#include "test_controller.moc"
