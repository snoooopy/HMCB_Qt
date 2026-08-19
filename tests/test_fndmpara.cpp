#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/FndmParaBuilder.h"
#include "core/FndmScanWriter.h"

// 对照 Windows DlgFNDMParaSet.cpp OnBnClickedApply/OnBnClickedButtonAdd/Sub/
// SetScanThr/UpdateCurrentFndmPara 的组包与提取规则
class TestFndmPara : public QObject {
    Q_OBJECT
private slots:
    void applyBiasAndThreshold() {
        // 单模块 bias+thr 单通道：2 项，bias 在前
        const auto v = FndmParaBuilder::buildApply(
            true, true, false, false, /*module*/2, /*row*/3, /*column*/4,
            /*lorU*/0, /*valBias*/100, /*valThr*/200, /*nDmNum*/8);
        QCOMPARE(v.size(), 2);
        QCOMPARE(v[0], FndmParaBuilder::Ch(2, 0, 0, 0, 0, 100));    // bias
        QCOMPARE(v[1], FndmParaBuilder::Ch(2, 1, 3, 4, 0, 200));    // lower
        // 上阈值 lorU=1 → type=2
        const auto u = FndmParaBuilder::buildApply(
            false, true, false, false, 2, 3, 4, 1, 100, 200, 8);
        QCOMPARE(u.size(), 1);
        QCOMPARE(u[0], FndmParaBuilder::Ch(2, 2, 3, 4, 0, 200));
    }

    void applyAllModuleAllChannel() {
        // 全模块 bias：每 DM 一项（i 作 nFNDM）
        const auto v = FndmParaBuilder::buildApply(
            true, false, true, false, 0, 0, 0, 0, 50, 0, 3);
        QCOMPARE(v.size(), 3);
        QCOMPARE(v[0], FndmParaBuilder::Ch(0, 0, 0, 0, 0, 50));
        QCOMPARE(v[2], FndmParaBuilder::Ch(2, 0, 0, 0, 0, 50));
        // 全通道 thr 单模块：8×6=48 项，row=i%8、col=i/8
        const auto w = FndmParaBuilder::buildApply(
            false, true, false, true, 1, 0, 0, 0, 0, 300, 8);
        QCOMPARE(w.size(), 48);
        QCOMPARE(w[0],  FndmParaBuilder::Ch(1, 1, 0, 0, 0, 300));   // i=0
        QCOMPARE(w[7],  FndmParaBuilder::Ch(1, 1, 7, 0, 0, 300));   // i=7
        QCOMPARE(w[8],  FndmParaBuilder::Ch(1, 1, 0, 1, 0, 300));   // i=8
        QCOMPARE(w[47], FndmParaBuilder::Ch(1, 1, 7, 5, 0, 300));   // i=47
        // 全模块+全通道：3×48=144 项，nFNDM 随 DM
        const auto x = FndmParaBuilder::buildApply(
            false, true, true, true, 0, 0, 0, 0, 0, 300, 3);
        QCOMPARE(x.size(), 144);
        QCOMPARE(x[48], FndmParaBuilder::Ch(1, 1, 0, 0, 0, 300));
        QCOMPARE(x[143], FndmParaBuilder::Ch(2, 1, 7, 5, 0, 300));
    }

    void adjustRelativeSingleModule() {
        // Add/Sub：adjMode=1，bias/thr 都只对单 module（不遍历 allModule）
        const auto add = FndmParaBuilder::buildAdjust(
            true, true, false, 2, 3, 4, 0, 10, 20, +1);
        QCOMPARE(add.size(), 2);
        QCOMPARE(add[0], FndmParaBuilder::Ch(2, 0, 0, 0, 1, 10));
        QCOMPARE(add[1], FndmParaBuilder::Ch(2, 1, 3, 4, 1, 20));
        // Sub：value 取负
        const auto sub = FndmParaBuilder::buildAdjust(
            true, true, false, 2, 3, 4, 0, 10, 20, -1);
        QCOMPARE(sub[0], FndmParaBuilder::Ch(2, 0, 0, 0, 1, -10));
        QCOMPARE(sub[1], FndmParaBuilder::Ch(2, 1, 3, 4, 1, -20));
        // allChn 时 thr 展开 48 通道（相对模式同展开）
        const auto all = FndmParaBuilder::buildAdjust(
            false, true, true, 2, 0, 0, 0, 0, 5, +1);
        QCOMPARE(all.size(), 48);
        QCOMPARE(all[47], FndmParaBuilder::Ch(2, 1, 7, 5, 1, 5));
    }

    void scanThresholdPairs() {
        // 每 DM×48 通道×上下两阈值；upper=lower+bin
        const auto v = FndmParaBuilder::buildScanThr(2, 100, 10);
        QCOMPARE(v.size(), 2 * 48 * 2);
        QCOMPARE(v[0], FndmParaBuilder::Ch(0, 1, 0, 0, 0, 100));    // DM0 ch0 lower
        QCOMPARE(v[1], FndmParaBuilder::Ch(0, 2, 0, 0, 0, 110));    // DM0 ch0 upper
        QCOMPARE(v[2], FndmParaBuilder::Ch(0, 1, 1, 0, 0, 100));
        QCOMPARE(v[95], FndmParaBuilder::Ch(0, 2, 7, 5, 0, 110));
        QCOMPARE(v[96], FndmParaBuilder::Ch(1, 1, 0, 0, 0, 100));   // DM1 开始
        QCOMPARE(v[191], FndmParaBuilder::Ch(1, 2, 7, 5, 0, 110));
    }

    void extractCurrentValues() {
        // 对照 UpdateCurrentFndmPara：bias 不判行列；阈值按 (row,column) 提取
        QVector<FndmParaBuilder::Ch> para = {
            FndmParaBuilder::Ch(2, 0, 0, 0, 0, 111),   // bias of module 2
            FndmParaBuilder::Ch(2, 1, 3, 4, 0, 222),   // lower (3,4)
            FndmParaBuilder::Ch(2, 2, 3, 4, 0, 333),   // upper (3,4)
            FndmParaBuilder::Ch(2, 1, 5, 5, 0, 999),   // 别的通道，不取
            FndmParaBuilder::Ch(3, 0, 0, 0, 0, 777),   // 别的模块，不取
        };
        int bias = 0, thrL = 0, thrU = 0;
        QVERIFY(FndmParaBuilder::extractCurrent(para, 2, 3, 4, bias, thrL, thrU));
        QCOMPARE(bias, 111);
        QCOMPARE(thrL, 222);
        QCOMPARE(thrU, 333);
        // 模块不存在 → false，输出保持 0
        int b2 = 5;
        QVERIFY(!FndmParaBuilder::extractCurrent(para, 7, 0, 0, b2, thrL, thrU));
        QCOMPARE(b2, 5);
    }

    void scanWriterTxtCsvFormats() {
        // 对照 SaveScanSpec：2 通道 × 3 bin，scanData[chn][bin][sn]
        FndmScanWriter::Spec s;
        s.valScan = 100; s.valScan2 = 400; s.valBin = 100;   // bins=(400-100)/100=3
        s.sampleTimes = 7; s.potmtMs = 4000;
        s.scanData.resize(2);
        for (int ch = 0; ch < 2; ++ch) {
            s.scanData[ch].resize(3);
            for (int b = 0; b < 3; ++b)
                for (int sn = 0; sn < 2; ++sn)
                    s.scanData[ch][b].append(uint64_t(ch * 30 + b * 10 + sn));
        }
        QCOMPARE(s.binCount(), 3);
        QTemporaryDir dir;
        const QString txt = dir.path() + "/spec.txt";
        const QString csv = dir.path() + "/spec.csv";
        QVERIFY(FndmScanWriter::writeTxt(txt, s));
        QVERIFY(FndmScanWriter::writeCsv(csv, s));
        QFile f(txt);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto lines = QString::fromUtf8(f.readAll()).split('\n');
        // 段头（背景段）+ 3 bin 行，随后谱段重复
        QCOMPARE(lines[0], QStringLiteral("Binwidth[mV] Binstart[mV] 7 x [[time[ms]  2 x pixelvalue]]"));
        QCOMPARE(lines[1], QStringLiteral("100\t100\t4\t0\t30"));
        QCOMPARE(lines[2], QStringLiteral("100\t200\t4\t10\t40"));
        QCOMPARE(lines[4], lines[0]);                          // 谱段头
        QCOMPARE(lines[5], QStringLiteral("100\t100\t4\t1\t31"));
        QFile g(csv);
        QVERIFY(g.open(QIODevice::ReadOnly));
        const auto cl = QString::fromUtf8(g.readAll()).split('\n');
        QCOMPARE(cl[0], QStringLiteral("Binwidth[mV]:100  time[ms]28  2 x pixelvalue"));
        QCOMPARE(cl[1], QStringLiteral("Binstart[mV],100,200,300"));
        QCOMPARE(cl[2], QStringLiteral("Pixel"));
        QCOMPARE(cl[3], QStringLiteral("0,0,10,20"));          // 通道 0 背景段
        QCOMPARE(cl[4], QStringLiteral("1,30,40,50"));
        QCOMPARE(cl[5], QString());                            // 段间空行
        QCOMPARE(cl[6], cl[0]);                                // 谱段头
        QCOMPARE(cl[9], QStringLiteral("0,1,11,21"));          // 通道 0 谱段
    }
};

QTEST_GUILESS_MAIN(TestFndmPara)
#include "test_fndmpara.moc"
