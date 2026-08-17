#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/XmlGainWriter.h"

class TestXmlWriter : public QObject {
    Q_OBJECT
private slots:
    void createsFileWhenMissing() {
        QTemporaryDir dir;
        const QString p = dir.path() + "/DIASConfig.xml";
        QVERIFY(XmlGainWriter::write(p, 0, {0, 2, 3, 7}));
        QFile f(p);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString s = QString::fromUtf8(f.readAll());
        QVERIFY2(s.contains("NucDiasConfigXML"), qPrintable(s));
        QVERIFY(s.contains("CCM"));
        QVERIFY(s.contains("MCB0_RANGE"));
        QVERIFY(s.contains("0,2,3,7"));
    }

    void updatesExistingWithoutDestroying() {
        QTemporaryDir dir;
        const QString p = dir.path() + "/DIASConfig.xml";
        {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("<NucDiasConfigXML><CCM><OTHER>x</OTHER>"
                    "<MCB0_RANGE>9,9</MCB0_RANGE></CCM></NucDiasConfigXML>");
        }
        QVERIFY(XmlGainWriter::write(p, 0, {1}));
        QVERIFY(XmlGainWriter::write(p, 1, {1}));
        QFile f(p);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString s = QString::fromUtf8(f.readAll());
        QVERIFY2(s.contains("OTHER"), qPrintable(s));     // 其他节点保留
        QVERIFY(s.contains(">x<"));
        QVERIFY(!s.contains("9,9"));                      // 旧值被覆盖
        QVERIFY(s.contains("MCB0_RANGE"));
        QVERIFY(s.contains("MCB1_RANGE"));                // 第二块板节点新建
    }
};
QTEST_GUILESS_MAIN(TestXmlWriter)
#include "test_xmlwriter.moc"
