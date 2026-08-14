#include <QtTest/QtTest>
#include "MCBCtrl.h"     // 触发 replenish_type.hpp（UINT/LONG/WPARAM/LPARAM 等）
#include "MCB_param.h"   // MCBData / MAX_* / 状态码

// 验证：能包含库头编译，且 SetMCBCommand 符号可被链接器解析（取地址即强制 NEEDED）。
class TestLibLink : public QObject {
    Q_OBJECT
private slots:
    void symbolsResolve() {
        auto p = &SetMCBCommand;
        QVERIFY(p != nullptr);
        QVERIFY(sizeof(MCBData) > 0);
        QVERIFY(MCB_MSG_DATA == 0x0600);    // WM_USER(0x0400)+0x200
        QVERIFY(MCB_MSG_STATUS == 0x0500);  // WM_USER(0x0400)+0x100
        QVERIFY(MAX_MCBADNUM * MAX_DMROWNUM * MAX_DMCOLNUM == 64 * 128 * 8);
    }
};

QTEST_GUILESS_MAIN(TestLibLink)
#include "test_liblink.moc"
