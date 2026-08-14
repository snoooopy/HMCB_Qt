#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "core/IniFile.h"

class TestIniFile : public QObject {
    Q_OBJECT
private slots:
    void roundTrip() {
        QTemporaryFile f;
        QVERIFY(f.open());
        QString path = f.fileName();
        f.close();

        IniFile ini;
        ini.setFileName(path);
        ini.setString("MODE0", "POTMT", "4000");
        ini.setInt("MODE0", "INTT", 400);
        QVERIFY(ini.writeAll());

        IniFile r;
        r.setFileName(path);
        QVERIFY(r.readAll());
        QCOMPARE(r.getString("MODE0", "POTMT", ""), QString("4000"));
        QCOMPARE(r.getInt("MODE0", "INTT", 0), 400);
        QCOMPARE(r.getString("MODE0", "MISSING", "d"), QString("d"));
    }
    void preservesExistingSections() {
        QTemporaryFile f;
        QVERIFY(f.open());
        QString path = f.fileName();
        f.close();

        // 预置一个含多 section 的文件
        {
            QFile pf(path);
            pf.open(QIODevice::WriteOnly | QIODevice::Text);
            QTextStream s(&pf);
            s << "[CONNECTION]\nIPADDR=192.111.111.211\n\n[MODE0]\nPOTMT=4000\n";
        }
        IniFile ini;
        ini.setFileName(path);
        QVERIFY(ini.readAll());
        ini.setInt("MODE0", "INTT", 400);      // 在 MODE0 末尾追加，不应破坏 CONNECTION
        QVERIFY(ini.writeAll());

        IniFile r;
        r.setFileName(path);
        QVERIFY(r.readAll());
        QCOMPARE(r.getString("CONNECTION", "IPADDR", ""), QString("192.111.111.211"));
        QCOMPARE(r.getInt("MODE0", "POTMT", 0), 4000);
        QCOMPARE(r.getInt("MODE0", "INTT", 0), 400);
    }
};

QTEST_GUILESS_MAIN(TestIniFile)
#include "test_inifile.moc"
