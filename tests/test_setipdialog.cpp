// SetIpDialog：对照 Windows IDD_DIALOG_SETNEWIP / DlgSetNewIPAddress
// 契约：标题、三栏地址（IP/掩码/网关）回读、IPv4 全匹配验证器（MFC CIPAddressCtrl 等价约束）
#include <QtTest>
#include <QApplication>
#include <QLineEdit>
#include "ui/SetIpDialog.h"

class TestSetIpDialog : public QObject {
    Q_OBJECT
private slots:
    void titleAndRoundTrip() {
        SetIpDialog d;
        QCOMPARE(d.windowTitle(), QStringLiteral("Set new IP address"));
        d.setAddresses("192.111.111.212", "255.255.255.0", "192.111.111.1");
        QCOMPARE(d.ipAddress(), QStringLiteral("192.111.111.212"));
        QCOMPARE(d.subnetMask(), QStringLiteral("255.255.255.0"));
        QCOMPARE(d.gateway(), QStringLiteral("192.111.111.1"));
    }
    void validatorAcceptsLegal() {
        SetIpDialog d;
        d.setAddresses("0.0.0.0", "255.255.255.255", "192.111.111.1");
        for (QLineEdit* e : d.findChildren<QLineEdit*>())
            QVERIFY2(e->hasAcceptableInput(), qPrintable(e->text()));
    }
    void validatorRejectsIllegal() {
        SetIpDialog d;
        for (const char* bad : { "999.1.1.1", "192.168.1", "192.168.1.1.5",
                                 "192.168.a.1", "192.168..1", "", " 192.168.1.1" }) {
            d.setAddresses(bad, "255.255.255.0", "192.111.111.1");
            QVERIFY2(!d.findChild<QLineEdit*>()->hasAcceptableInput(),
                     qPrintable(QString("should reject: %1").arg(bad)));
        }
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);            // QWidget 需要 QApplication
    TestSetIpDialog t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_setipdialog.moc"
