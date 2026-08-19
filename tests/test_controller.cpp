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

    void measureXdifNeedsZeroThenProduces() {
        // 对照 OnXDif：未 Zero 前不采集；Zero 后 X-DIF 完成并给出 VzX/X_dif
        qRegisterMetaType<Mcb::McbFrame>("Mcb::McbFrame");
        McbController c; c.setBackendMode("mock"); c.startUp();
        std::atomic<bool> ready{false};
        connect(&c, &McbController::deviceReady, this, [&]{ ready = true; });
        c.initializeBackend();
        QElapsedTimer t; t.start();
        while (!ready && t.elapsed() < 2000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(ready, "no deviceReady within 2s");

        MeasurementEngine::Config cfg = c.engine()->config();
        cfg.vzSampleNum = 5;
        c.engine()->applyConfig(cfg);

        c.measureXdif();                 // 未做 Zero：只提示、不进入采集
        qApp->processEvents(QEventLoop::AllEvents, 50);
        QVERIFY(!c.engine()->bXdif(0));

        int lastFn = -1; std::atomic<bool> measured{false};
        connect(&c, &McbController::measurementReady, this,
                [&](int fn) { lastFn = fn; measured = true; });
        c.measure(0, 300);               // 先 Zero
        t.restart();
        while (!measured && t.elapsed() < 6000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(measured, "no measurementReady(0) within 6s");

        measured = false;
        c.measureXdif();                 // 再 X-DIF
        t.restart();
        while (!measured && t.elapsed() < 6000) qApp->processEvents(QEventLoop::AllEvents, 5);
        QVERIFY2(measured, "no measurementReady(4) within 6s");
        QCOMPARE(lastFn, 4);
        QVERIFY2(c.engine()->bXdif(0), "bXdif not set after fn=4");
        QVERIFY2(!c.engine()->vzx(0).isEmpty(), "VzX empty after fn=4");
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

    void dmRangeFromIniRoundTrip() {
        // 对照 Windows OnSetdmrange：读 MCBCtrl.ini [RANGE0] MCB{i}DM{j}（0..7）
        // 组 65 字节（[0]=板号）发 SETRANGE
        QTemporaryDir dir;
        QFile f(dir.path() + "/MCBCtrl.ini");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("[RANGE0]\nMCB0DM0=3\nMCB0DM1=5\n");    // 其余缺省=沿上一值(初始 7)
        f.close();

        McbController c;
        c.setBackendMode("mock");
        c.startUp();
        c.setGainWritePaths(dir.path() + "/MCBCtrl.ini", dir.path() + "/DIASConfig.xml");
        c.setDmRangeFromIni();
        const auto r = c.dmGainValues(0);                // 去板号的 64 档
        QVERIFY(r.size() >= 25);
        QCOMPARE(r.at(0), uint8_t(3));
        QCOMPARE(r.at(1), uint8_t(5));
        QCOMPARE(r.at(2), uint8_t(5));                   // 未配置沿上一 DM 的值（Windows GetInt 缺省=当前 range）
        c.shutdown();
    }

    void dm2TypeRangeFromIniRoundTrip() {
        // 对照 Windows OnSetdm2range：读 MCB_TEST.ini [SETRANGE] MCB{i}DM_0/DM_1，
        // 组 [板号][类型0×nDMNum][类型1×nDMNum] 发 SET2TYPERANGE
        QTemporaryDir dir;
        QFile f(dir.path() + "/MCB_TEST.ini");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("[SETRANGE]\nMCB0DM_0=1\nMCB0DM_1=2\n");
        f.close();

        McbController c;
        c.setBackendMode("mock");
        c.startUp();
        c.setDm2TypeRangeFromIni(dir.path() + "/MCB_TEST.ini");
        const auto r = c.dm2TypeValues(0);               // 2×nDMNum 档
        QCOMPARE(r.size(), 2 * 25);                       // Mock 几何 nDMNum=25
        QCOMPARE(r.at(0), uint8_t(1));
        QCOMPARE(r.at(24), uint8_t(1));
        QCOMPARE(r.at(25), uint8_t(2));
        QCOMPARE(r.at(49), uint8_t(2));
        c.shutdown();
    }
};

QTEST_GUILESS_MAIN(TestController)
#include "test_controller.moc"
