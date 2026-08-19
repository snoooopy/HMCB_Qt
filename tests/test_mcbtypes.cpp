#include <QtTest/QtTest>
#include "core/McbTypes.h"

class TestMcbTypes : public QObject {
    Q_OBJECT
private slots:
    void decodeStatusParam() {
        // lParam = (nMCB<<16)+code
        auto r = Mcb::decodeStatusParam((LPARAM)((2u << 16) | 0x0204u));
        QCOMPARE(r.nMCB, 2);
        QVERIFY(r.code == 0x0204u);
    }
    void codeStrings() {
        QCOMPARE(Mcb::statusCodeStr(0x0204), QStringLiteral("REGSET_OK"));
        QCOMPARE(Mcb::statusCodeStr(0x0101), QStringLiteral("CONNECTOK"));
        QCOMPARE(Mcb::statusCategoryStr(MCB_DVREADY), QStringLiteral("DVREADY"));
    }
    void setNewIpCodes() {
        // MCB_param.h:110-112（对照 Windows 库）
        QCOMPARE(Mcb::statusCodeStr(0x1121), QStringLiteral("SETNEWIPRUN"));
        QCOMPARE(Mcb::statusCodeStr(0x1122), QStringLiteral("SETNEWIP_ERR"));
        QCOMPARE(Mcb::statusCodeStr(0x1123), QStringLiteral("SETNEWIP_OK"));
    }
    void parseIpv4() {
        quint8 b[4];
        QVERIFY(Mcb::parseIpv4("192.111.111.212", b));
        QCOMPARE(b[0], quint8(192)); QCOMPARE(b[1], quint8(111));
        QCOMPARE(b[2], quint8(111)); QCOMPARE(b[3], quint8(212));
        QVERIFY(Mcb::parseIpv4("0.0.0.0", b));
        QVERIFY(Mcb::parseIpv4("255.255.255.0", b));
        QCOMPARE(b[0], quint8(255)); QCOMPARE(b[3], quint8(0));
        QVERIFY(!Mcb::parseIpv4("999.1.1.1", b));      // 段超 255
        QVERIFY(!Mcb::parseIpv4("192.111.111", b));    // 段数不足
        QVERIFY(!Mcb::parseIpv4("192.111.111.1.5", b));// 段数超
        QVERIFY(!Mcb::parseIpv4("192.111.a.1", b));    // 非数字
        QVERIFY(!Mcb::parseIpv4("192.111..1", b));     // 空段
        QVERIFY(!Mcb::parseIpv4("", b));
    }
};

QTEST_GUILESS_MAIN(TestMcbTypes)
#include "test_mcbtypes.moc"
