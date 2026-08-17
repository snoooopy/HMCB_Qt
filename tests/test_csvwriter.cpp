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
        opt.hasVn = true; opt.hasVz = true;

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
        CsvResultWriter::Options opt; opt.csv = false; opt.hasVz = true;
        QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        CsvResultWriter::write(out, 1, vz, vn, opt);
        out.flush();
        QVERIFY2(QString::fromUtf8(bytes).contains("CH["), "txt block marker missing");
    }

    // ---- Phase 3: 条件列组 ----
    void conditionalColumns() {
        QVector<double> vz = {100.0, 200.0};
        QVector<MeasurementEngine::NoiseChan> vn;      // 空——未测 Noise
        CsvResultWriter::Options opt;
        opt.hasVz = true; opt.hasInl = true;
        opt.inl = {0.00123, 0.00456}; opt.diff = {1.02, 0.97};
        QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        CsvResultWriter::writeCsv(out, 2, vz, vn, opt);
        out.flush();
        const QString s = QString::fromUtf8(bytes);
        const QString first = s.section('\n', 0, 0);
        QCOMPARE(first, QString("Channel,Zero(LSB),INL(thousandth),DIV(percent),"));
        QVERIFY2(s.contains(QString::asprintf("%8.2f", 1.23).trimmed()), "INL permille value missing");
        QVERIFY2(s.contains(QString::asprintf("%8.1f", 2.0).trimmed()), "DIV percent value missing");
    }

    void allUntestedGivesChannelOnly() {
        CsvResultWriter::Options opt;
        QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        CsvResultWriter::writeCsv(out, 1, {}, {}, opt);
        out.flush();
        QCOMPARE(QString::fromUtf8(bytes).section('\n', 0, 0), QString("Channel,"));
    }

    void txtInlSections() {
        CsvResultWriter::Options opt;
        opt.hasInl = true;
        opt.inl = {0.002}; opt.diff = {1.05};
        QByteArray bytes; QBuffer buf(&bytes); buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        CsvResultWriter::writeTxt(out, 1, {}, {}, opt);
        out.flush();
        const QString s = QString::fromUtf8(bytes);
        QVERIFY2(s.contains("a. INL(thousandth)"), "INL txt section missing");
        QVERIFY2(s.contains("b. Diversity between Channels(percent)"), "DIV txt section missing");
        QVERIFY2(s.contains(QString::asprintf("%5.2f", 2.0).trimmed()), "INL txt value missing");
    }
};

QTEST_GUILESS_MAIN(TestCsvWriter)
#include "test_csvwriter.moc"
