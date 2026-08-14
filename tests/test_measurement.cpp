#include <QtTest/QtTest>
#include <cmath>
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
};

QTEST_GUILESS_MAIN(TestMeasurement)
#include "test_measurement.moc"
