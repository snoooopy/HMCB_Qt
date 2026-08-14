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
};

QTEST_GUILESS_MAIN(TestMcbTypes)
#include "test_mcbtypes.moc"
