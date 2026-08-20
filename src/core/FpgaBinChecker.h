#pragma once
#include <QString>
#include <QStringList>

// INITIAL 前预检 DB FPGA 配置 bin。
// 库在缺 bin 时 ConfigFpga 失败 → 0x0210 → CLOSE 路径段错误（无法用返回值
// 规避），应用侧必须在发 INITIAL 前拦截并提示，替代闪退。
// 解析规则复刻 MCBCtrlC.cpp:1299-1327 + ConfigFpga(:3450)：
//   nMCBNum = [MCB]MCBNUM（缺省 1）
//   文件名[i] = [ADM]MCB{i}BINFILE，缺省继承 [ADM]BINFILE（缺省 ddc232.bin）
//   实际路径 = SETCONFIGFILE 目录 + "/" + 文件名[i]（ConfigFpga 只用板级名，
//   DB 级键 MCBnDBnBINFILE 解析后不参与烧写文件打开）
class FpgaBinChecker {
public:
    static QStringList requiredFiles(const QString& configDir);   // 每板相对名
    static QStringList missingFiles(const QString& configDir);    // 缺失子集
};
