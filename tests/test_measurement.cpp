#include <QtTest/QtTest>
#include <cmath>
#include <atomic>
#include <thread>
#include "core/MeasurementEngine.h"

class TestMeasurement : public QObject {
    Q_OBJECT
private slots:
    void zeroConstant() {
        MeasurementEngine e;
        QVector<uint32_t> f(4, 1000);          // 4 通道，恒值 1000
        for (int i = 0; i < 600; ++i) e.appendFrame(0, f);
        QVERIFY(e.calcZero(0));
        QCOMPARE(e.frameCount(0), 600);
        for (int k = 0; k < 4; ++k)
            QCOMPARE(e.vz(0)[k], 1000.0);
    }
    void noiseAlternating() {
        // 0/1 交替：均值 0.5，方差 0.25，σ=0.5，FWHM=2.355*0.5=1.1775
        MeasurementEngine e;
        QVector<uint32_t> a(2, 0), b(2, 1);
        for (int i = 0; i < 1024; ++i) e.appendFrame(0, (i % 2) ? a : b);
        QVERIFY(e.calcNoise(0));
        for (int k = 0; k < 2; ++k) {
            QCOMPARE(e.vn(0)[k].mean, 0.5);
            QCOMPARE(e.vn(0)[k].fwhm, 2.355 * 0.5);
            QCOMPARE(e.vn(0)[k].max, 1.0);
            QCOMPARE(e.vn(0)[k].min, 0.0);
        }
    }
    void noiseTooFewFrames() {
        MeasurementEngine e;
        QVector<uint32_t> f(2, 5);
        for (int i = 0; i < 100; ++i) e.appendFrame(0, f);   // <200
        QVERIFY(!e.calcNoise(0));
    }
    void displayFormats() {
        // fwhm=2.355 → RMS=1.0；adBit=20 → SNR=2^20/1.0；ERB=log2(SNR)=20
        QCOMPARE(MeasurementEngine::toDisplayFormat(2.355, 0, 20), 2.355);          // FWHM
        QCOMPARE(MeasurementEngine::toDisplayFormat(2.355, 1, 20), 1.0);            // RMS
        QCOMPARE(MeasurementEngine::toDisplayFormat(2.355, 2, 20), std::pow(2,20)); // SNR
        QCOMPARE(MeasurementEngine::toDisplayFormat(2.355, 3, 20), 20.0);           // ERB
    }
    void passFailRule() {
        QVERIFY(MeasurementEngine::passFail(5, 0, 0));     // 未设阈值=合格
        QVERIFY(MeasurementEngine::passFail(5, 2, 8));      // 区间内
        QVERIFY(!MeasurementEngine::passFail(1, 2, 8));     // 低于下限
        QVERIFY(!MeasurementEngine::passFail(9, 2, 8));     // 高于上限
    }

    // ---- Phase 3: INL ----
    void inlLinearFitGivesTinyInlAndUnitR() {
        MeasurementEngine e;
        e.beginInl(6);
        const int nChn = 4;
        for (int p = 0; p < 6; ++p) {
            const double x = e.inlVin()[p];            // y = 2 + 0.1·x mV
            QVector<uint32_t> f(nChn);
            for (int k = 0; k < nChn; ++k)
                f[k] = static_cast<uint32_t>((2.0 + 0.1 * x) * 3355.4432);
            e.reset(0);
            e.appendFrame(0, f);
            e.addInlPoint(0, p);
        }
        QVERIFY(e.calcInl(0, 2));                       // 每 DM 2 通道
        QVERIFY(e.bInl(0));
        QCOMPARE(e.inl(0).size(), 4);
        QVERIFY2(e.inl(0)[0] * 1000.0 < 1.0, "linear data should give INL < 1 permille");
        QVERIFY(e.inlR(0)[0] > 0.9999);
        QVERIFY(std::fabs(e.inlA(0)[0] - 0.1) < 1e-9);
        QVERIFY(std::fabs(e.inlDiff(0)[1] - 1.0) < 1e-9);   // 同斜率 → DIV=1
    }

    void inlCubicNonlinearLiftsInl() {
        MeasurementEngine e;
        e.beginInl(7);
        const int nChn = 2;
        for (int p = 0; p < 7; ++p) {
            const double x = e.inlVin()[p];
            const double y = 40.0 + 0.105 * x + 40.0 * std::pow(x / 5000.0, 3);
            QVector<uint32_t> f(nChn);
            for (int k = 0; k < nChn; ++k)
                f[k] = static_cast<uint32_t>(y * 3355.4432);
            e.reset(0); e.appendFrame(0, f); e.addInlPoint(0, p);
        }
        QVERIFY(e.calcInl(0, 2));
        QVERIFY2(e.inl(0)[0] * 1000.0 > 0.1, "cubic term should lift INL above 0.1 permille");
        QVERIFY(e.inlR(0)[0] < 1.0 && e.inlR(0)[0] > 0.9);
    }

    void inlNeedsFivePoints() {
        MeasurementEngine e;
        e.beginInl(5);
        for (int p = 0; p < 4; ++p) {                  // 只喂 4 点
            e.reset(0);
            QVector<uint32_t> f(2, 100000);
            e.appendFrame(0, f);
            e.addInlPoint(0, p);
        }
        QVERIFY(!e.calcInl(0, 1));
        QVERIFY(!e.bInl(0));
    }

    void beginInlIntegerStepTruncates() {
        MeasurementEngine e;
        MeasurementEngine::Config cfg; cfg.inlDaMax = 100;   // (100-100)*4 = 0
        e.applyConfig(cfg);
        QCOMPARE(e.beginInl(5), 0u);                   // 整数除法截断 → step 0
        QCOMPARE(e.inlVin().size(), 5);
        QCOMPARE(e.inlVin()[4], 0.0);
        QCOMPARE(e.beginInl(3), e.beginInl(5));        // 点数钳位 5..40（Windows 行为）
    }

    void inlDaCodeAndClearResults() {
        MeasurementEngine e;                            // 默认 DAMAX=16100 → base=64400
        const uint32_t step = e.beginInl(12);
        QCOMPARE(step, (64400u - 400u) / 11u);
        QCOMPARE(e.inlDaCode(0), 64400u * 65537u);               // da += da<<16
        QCOMPARE(e.inlDaCode(2), (64400u << 16) + (64400u - 2u * step));
        // calcZero 置 bVzero；clearResults 清全部
        e.appendFrame(0, QVector<uint32_t>(4, 500000));
        QVERIFY(e.calcZero(0));
        QVERIFY(e.bVzero(0));
        e.clearResults();
        QVERIFY(e.vz(0).isEmpty() && e.vn(0).isEmpty() && e.inl(0).isEmpty());
        QVERIFY(!e.bVzero(0) && !e.bVnoise(0) && !e.bInl(0));
    }

    void concurrentAppendRead() {
        // 回归保护：appendFrame 在工作线程跑、latestFrame/rms256 在 GUI 线程读
        // （GraphView 400ms 定时器）。帧内所有通道填同一标记值，读到的任一帧
        // 必须整帧一致——无锁并发下 QVector 重分配会出现撕裂/崩溃。
        MeasurementEngine e;
        e.setDepth(64);
        e.reset(0);
        const int nCh = 512;
        std::atomic<bool> stop{false};
        std::thread writer([&] {
            QVector<uint32_t> ch(nCh);
            for (uint32_t f = 1; f <= 3000 && !stop.load(); ++f) {
                ch.fill(f);                       // 帧号做整帧标记
                e.appendFrame(0, ch);
            }
        });
        bool consistent = true;
        for (int i = 0; i < 4000; ++i) {
            const auto fr = e.latestFrame(0);
            if (!fr.isEmpty())
                for (uint32_t v : fr)
                    if (v != fr.first()) { consistent = false; break; }
            if (i % 8 == 0) (void)e.rms256(0);
        }
        stop.store(true);
        writer.join();
        QVERIFY2(consistent, "latestFrame 读到撕裂帧：append 与读并发无同步");
        QVERIFY(e.frameCount(0) > 0);
    }
};

QTEST_GUILESS_MAIN(TestMeasurement)
#include "test_measurement.moc"
