#include <QtTest/QtTest>
#include <cmath>
#include <atomic>
#include <thread>
#include <QTemporaryDir>
#include <QFile>
#include "core/MeasurementEngine.h"
#include "core/McbTypes.h"
#include "core/PosDataWriter.h"
#include "core/OriginDataWriter.h"

class TestMeasurement : public QObject {
    Q_OBJECT
private slots:
    void data2mVFromAdBit() {
        // 对照 MCB_TESTView.cpp:461-472（GETDMADBIT 后的 m_DATA2mV switch）
        QCOMPARE(MeasurementEngine::data2mVFromAdBit(16), 65536.0 / 5000.0);
        QCOMPARE(MeasurementEngine::data2mVFromAdBit(20), 1048576.0 / 4096.0);
        QCOMPARE(MeasurementEngine::data2mVFromAdBit(24), 16777216.0 / 5000.0);
        QCOMPARE(MeasurementEngine::data2mVFromAdBit(18), 1048576.0 / 4096.0);  // default→20bit
    }
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

    // ---- Phase 4-5: X-DIF ----
    void xdifFromBaseline() {
        // 对照 VzeroCal fn==4：VzX=本轮均值−Vz 基线（净信号）；
        // X_dif=每 16 通道组 |max−min|/2/组均值（MCB_TESTView 2985 行）
        MeasurementEngine e;
        const int nChn = 64;                     // 4 组 ×16 通道
        for (int i = 0; i < 10; ++i)
            e.appendFrame(0, QVector<uint32_t>(nChn, 1000));
        QVERIFY(e.calcZero(0));                  // 基线 = 1000
        QVERIFY(!e.bXdif(0));

        QVector<uint32_t> ray(nChn);
        for (int k = 0; k < nChn; ++k) {
            const int g = k / 16, j = k % 16;
            int delta = 0;                       // 组 3：恒 0 → 分母 0 保护
            if (g == 0)      delta = j;          // 组 0：0..15 → dif=1.0
            else if (g == 1) delta = 100;        // 组 1：恒定 → dif=0
            else if (g == 2) delta = 10 + 2 * j; // 组 2：10..40 → dif=15/25=0.6
            ray[k] = uint32_t(1000 + delta);
        }
        for (int i = 0; i < 1100; ++i) e.appendFrame(0, ray);   // >1024：窗口全为新帧
        QVERIFY(e.calcXdif(0));
        QVERIFY(e.bXdif(0));
        QCOMPARE(e.vzx(0).size(), nChn);
        QCOMPARE(e.vzx(0)[0], 0.0);
        QCOMPARE(e.vzx(0)[1], 1.0);
        QCOMPARE(e.vzx(0)[16], 100.0);
        QCOMPARE(e.vzx(0)[32], 10.0);
        QCOMPARE(e.vzx(0)[63], 0.0);
        QCOMPARE(e.xdif(0).size(), nChn / 16);
        QCOMPARE(e.xdif(0)[0], 1.0);             // (15−0)/2 / 7.5
        QCOMPARE(e.xdif(0)[1], 0.0);
        QCOMPARE(e.xdif(0)[2], 0.6);             // (40−10)/2 / 25
        QCOMPARE(e.xdif(0)[3], 0.0);             // 全 0 组 → 0（Windows 会 NaN，这里钳 0）
        // 未覆盖满 16 通道的尾巴（nChn 非 16 倍数）不计组
        QVERIFY(e.calcZero(0));                  // 基线仍在，可再次 X-DIF
    }

    void xdifNeedsBaseline() {
        // Windows 使能条件 !bVzero 拦截：须先 Zero
        MeasurementEngine e;
        QVector<uint32_t> f(16, 5);
        for (int i = 0; i < 8; ++i) e.appendFrame(0, f);
        QVERIFY(!e.calcXdif(0));                 // 未做 Zero
        QVERIFY(e.calcZero(0));
        e.reset(0);                              // reset 作废基线与结果
        e.appendFrame(0, f);
        QVERIFY(!e.calcXdif(0));
        QVERIFY(!e.bXdif(0));
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

    void posRingAndWriter() {
        // 对照 OnSavePosdata：帧头 ptsm 逐帧缓存（老→新）→ txt 行格式
        MeasurementEngine e;
        e.appendPos(0, 10, 100);
        e.appendPos(0, 11, 101);
        e.appendPos(0, 12, 102);
        auto s = e.posSnapshot(0);
        QCOMPARE(s.size(), 3);
        QCOMPARE(s[0].first, 10u);  QCOMPARE(s[0].second, 100u);
        QCOMPARE(s[2].first, 12u);  QCOMPARE(s[2].second, 102u);
        // 环形：深 4，追加 6 帧丢最老
        e.setDepth(4);
        for (int i = 0; i < 6; ++i) e.appendPos(0, uint32_t(i), 0);
        s = e.posSnapshot(0);
        QCOMPARE(s.size(), 4);
        QCOMPARE(s.first().first, 2u);
        // reset 清空
        e.reset(0);
        QVERIFY(e.posSnapshot(0).isEmpty());
        // writer 输出
        QTemporaryDir dir;
        const QString p = dir.path() + "/pos.txt";
        QVERIFY(PosDataWriter::write(p, { {5, 7}, {6, 8} }));
        QFile f(p);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(f.readAll()),
                 QStringLiteral("Packet:0, AngleCode:5, ConveyorCode:7\n"
                                "Packet:1, AngleCode:6, ConveyorCode:8\n"));
    }

    void originDataWriterBytes() {
        // 对照 OnSavedatao：每帧前 SAVECHN=128 通道、int32 小端逐点
        MeasurementEngine e;
        QVector<uint32_t> fr(200);                 // >128 通道：只写前 128
        for (int i = 0; i < 200; ++i) fr[i] = uint32_t(1000 + i);
        e.appendFrame(0, fr);
        QVector<uint32_t> fr2 = fr;
        fr2[0] = 5;
        e.appendFrame(0, fr2);
        const auto snap = e.ringSnapshot(0);
        QCOMPARE(snap.size(), 2);

        QTemporaryDir dir;
        const QString p = dir.path() + "/o.oridat";
        QVERIFY(OriginDataWriter::write(p, snap));
        QFile f(p);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray raw = f.readAll();
        QCOMPARE(raw.size(), 2 * 128 * 4);         // 2 帧 ×128 通道 ×4B
        // 第 0 帧首点=1000、第 1 帧首点=5、末帧末通道=帧内 ch127=1127
        const auto rd = [&raw](int idx) {
            return qint32(static_cast<uint8_t>(raw[idx*4]))
                 | (qint32(static_cast<uint8_t>(raw[idx*4+1])) << 8)
                 | (qint32(static_cast<uint8_t>(raw[idx*4+2])) << 16)
                 | (qint32(static_cast<uint8_t>(raw[idx*4+3])) << 24);
        };
        QCOMPARE(rd(0), 1000);
        QCOMPARE(rd(128), 5);
        QCOMPARE(rd(127), 1127);
    }

    void exDataRingAndReset() {
        // Save Image Data 数据源：每帧扩展字环形（深同 m_depth），reset 清空
        MeasurementEngine e;
        e.setDepth(4);
        for (int i = 0; i < 6; ++i) {
            Mcb::FrameEx x;
            x.ex[0] = i; x.ex[1] = i * 2; x.ex[2] = i * 3; x.ex[3] = i * 4; x.ex[4] = i * 5;
            x.accE = i == 0 ? 1875 : 62;
            x.godown = i % 2;
            e.appendExData(0, x);
        }
        const auto snap = e.exSnapshot(0);
        QCOMPARE(snap.size(), 4);                    // 深度回绕，保留第 2..5 帧
        QCOMPARE(snap.first().ex[0], 2);
        QCOMPARE(snap.last().ex[4], 25);
        QCOMPARE(snap.at(1).accE, 62);
        QCOMPARE(snap.at(1).godown, 1);
        e.reset(0);
        QVERIFY(e.exSnapshot(0).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestMeasurement)
#include "test_measurement.moc"
