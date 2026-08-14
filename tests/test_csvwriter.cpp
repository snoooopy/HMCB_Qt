#include <QtTest/QtTest>
#include <QBuffer>
#include <QTextStream>
#include "core/CsvResultWriter.h"
#include "core/MeasurementEngine.h"

class TestCsvWriter : public QObject {
    Q_OBJECT
private slots:
    void csvLayout() {
        using NC = MeasurementEngine::NoiseChan;
        QVector<double> vz = {1000.0, 2000.0};
        QVector<NC> vn = {NC{2.355, 1000, 1001, 999}, NC{4.71, 2000, 2002, 1998}};
        CsvResultWriter::Options opt;
        opt.vnFormat = 1;   // RMS → 2.355/2.355=1.0, 4.71/2.355=2.0
        opt.vzFormat = 0;   // LSB
        opt.adBit = 16;     // <=16 触发 %10.1f，RMS 带小数 1.0/2.0
        opt.csv = true;

        QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        CsvResultWriter::write(out, 2, vz, vn, opt);
        out.flush();
        const QString s = QString::fromUtf8(bytes);

        QVERIFY2(s.contains("Channel"), qPrintable("header missing: " + s));
        QVERIFY2(s.contains("1000"), qPrintable("zero value missing: " + s));
        QVERIFY2(s.contains("1.0"), qPrintable("noise RMS value missing: " + s));
    }
    void txtLayout() {
        using NC = MeasurementEngine::NoiseChan;
        QVector<double> vz = {1000.0};
        QVector<NC> vn = {NC{2.355, 1000, 1001, 999}};
        CsvResultWriter::Options opt; opt.csv = false;
        QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        CsvResultWriter::write(out, 1, vz, vn, opt);
        out.flush();
        QVERIFY2(QString::fromUtf8(bytes).contains("CH["), "txt block marker missing");
    }
};

QTEST_GUILESS_MAIN(TestCsvWriter)
#include "test_csvwriter.moc"
