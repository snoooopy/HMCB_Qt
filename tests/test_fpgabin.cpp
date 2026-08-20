// FpgaBinChecker：INITIAL 前预检 DB FPGA 配置 bin。
// 背景：库 ConfigFpga 打不开 bin → 失败路径 0x0210 → ConnectClose → Linux 库
// 段错误（2026-08-19 真机闪退根因），应用侧必须在发 INITIAL 前拦截。
// 解析规则复刻 MCBCtrlC.cpp:1299-1327 + ConfigFpga(:3450)：
//   nMCBNum = [MCB]MCBNUM（缺省 1）
//   文件名[i] = [ADM]MCB{i}BINFILE，缺省继承 [ADM]BINFILE（缺省 ddc232.bin）
//   实际路径 = SETCONFIGFILE 目录 + "/" + 文件名[i]
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/FpgaBinChecker.h"

class TestFpgaBin : public QObject {
    Q_OBJECT
private slots:
    void defaultsToDdc232();
    void perBoardInheritance();
    void missingDetected();

private:
    bool writeText(const QString& path, const char* content) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        f.write(content);
        return true;
    }
};

// 无 ini：单板、缺省 ddc232.bin（库 GetInt/GetString 缺省值）
void TestFpgaBin::defaultsToDdc232() {
    QTemporaryDir dir;
    const QStringList req = FpgaBinChecker::requiredFiles(dir.path());
    QCOMPARE(req, QStringList{ QStringLiteral("ddc232.bin") });
    QCOMPARE(FpgaBinChecker::missingFiles(dir.path()), req);   // 目录里没有 → 全缺
    QVERIFY(writeText(dir.filePath("ddc232.bin"), "x"));
    QVERIFY(FpgaBinChecker::missingFiles(dir.path()).isEmpty());
}

// 板级覆盖 + 继承：MCB0 用全局 BINFILE，MCB1 覆盖为专属文件
void TestFpgaBin::perBoardInheritance() {
    QTemporaryDir dir;
    writeText(dir.filePath("MCBCtrl.ini"),
        "[MCB]\nMCBNUM=2\n[ADM]\nBINFILE=common.bin\nMCB1BINFILE=board1.bin\n");
    const QStringList req = FpgaBinChecker::requiredFiles(dir.path());
    QCOMPARE(req, QStringList({ QStringLiteral("common.bin"),
                                QStringLiteral("board1.bin") }));
    writeText(dir.filePath("common.bin"), "x");
    QCOMPARE(FpgaBinChecker::missingFiles(dir.path()),
             QStringList{ QStringLiteral("board1.bin") });   // 只缺覆盖文件
}

// 缺失即报：MCBNUM=1 只配 BINFILE，文件不存在 → missing 恰为该文件
void TestFpgaBin::missingDetected() {
    QTemporaryDir dir;
    writeText(dir.filePath("MCBCtrl.ini"),
        "[MCB]\nMCBNUM=1\n[ADM]\nBINFILE=HMCB_CX1664DE_DAC17.bin\n");
    QCOMPARE(FpgaBinChecker::missingFiles(dir.path()),
             QStringList{ QStringLiteral("HMCB_CX1664DE_DAC17.bin") });
}

QTEST_GUILESS_MAIN(TestFpgaBin)
#include "test_fpgabin.moc"
