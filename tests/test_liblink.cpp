#include <QtTest/QtTest>
#include "MCBCtrl.h"     // 触发 replenish_type.hpp（UINT/LONG/WPARAM/LPARAM 等）
#include "MCB_param.h"   // MCBData / MAX_* / 状态码
#include "core/RealMcbBackend.h"

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

    // 真实连接卡顿修复（2026-08-20）：帧通道数必须按实际几何拷贝。
    // 旧实现无条件拷满 65536 通道（64*128*8 上限），CX1664DE 实机仅 128 通道，
    // 250fps 下每秒 64MB 拷贝洪水 + rms256 变 1677 万次循环（引擎锁内），
    // GUI 主线程 100% 饱和（gdb 采样 6/6 命中 GraphView::refresh→rms256）。
    void trampolineCopiesActualChannelCount() {
        RealMcbBackend be;
        Mcb::McbFrame got;
        be.setDataCallback([&](const Mcb::McbFrame& f) { got = f; });

        MCBData d{};
        for (int i = 0; i < 512; i++) d.addata[i].ul = uint(i);

        // 未注入几何（旧设备/异常路径）：保持全量拷贝上限，行为不回退
        RealMcbBackend::trampoline(MCB_MSG_DATA, reinterpret_cast<WPARAM>(&d), 0);
        QCOMPARE(got.channels.size(), int(MAX_MCBADNUM * MAX_DMROWNUM * MAX_DMCOLNUM));

        // DVREADY 后注入 nAllChnNum=128（CX1664DE: 1DM×64×2）：只拷 128 个
        be.setChannelCount(128);
        RealMcbBackend::trampoline(MCB_MSG_DATA, reinterpret_cast<WPARAM>(&d), 0);
        QCOMPARE(got.channels.size(), 128);
        QCOMPARE(got.channels.first(), 0u);
        QCOMPARE(got.channels.last(), 127u);

        // 注入值钳位：超过结构上限仍安全
        be.setChannelCount(1 << 20);
        RealMcbBackend::trampoline(MCB_MSG_DATA, reinterpret_cast<WPARAM>(&d), 0);
        QCOMPARE(got.channels.size(), int(MAX_MCBADNUM * MAX_DMROWNUM * MAX_DMCOLNUM));
    }
};

QTEST_GUILESS_MAIN(TestLibLink)
#include "test_liblink.moc"
