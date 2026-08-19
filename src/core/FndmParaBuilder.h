#pragma once
#include <QVector>
#include <cstdint>

// FNDM（快中子模块）参数组包纯逻辑，对照 Windows DlgFNDMParaSet.cpp。
// 库结构 FNDMPARA.ChannelPara[] 的 Qt 值对象，后端负责转库结构转发
// MCB_CMD_SETFNDMPARA(0x0240)。Linux 库 MCBThread 该 case 实现被注释
// （静默忽略），应用侧按 Windows 契约组包发送。
namespace FndmParaBuilder {

struct Ch {
    uint32_t fndm = 0;     // FNDM 模块号
    uint32_t type = 0;     // 0-bias 1-下阈值 2-上阈值
    uint32_t row = 0;      // 行（bias 时忽略）
    uint32_t colnum = 0;   // 列（bias 时忽略）
    uint32_t adjMode = 0;  // 0-绝对值 1-相对值
    int32_t value = 0;
    Ch() = default;
    Ch(uint32_t f, uint32_t t, uint32_t r, uint32_t c, uint32_t adj, int32_t v)
        : fndm(f), type(t), row(r), colnum(c), adjMode(adj), value(v) {}
};
inline bool operator==(const Ch& a, const Ch& b) {
    return a.fndm == b.fndm && a.type == b.type && a.row == b.row
        && a.colnum == b.colnum && a.adjMode == b.adjMode && a.value == b.value;
}

constexpr int kRow = 8;          // FNDM_ROW
constexpr int kColumn = 6;       // FNDM_COLUMN（每模块 48 通道）
constexpr int kDacMax = 4095;    // (1<<FND_DACBIT)-1，FND_DACBIT=12
constexpr int kMinBin = 10;      // MIN_BIN

// Apply（OnBnClickedApply）：绝对值设置。bias 段全模块时每 DM 一项；
// thr 段按 allChn 展开 48 通道（row=i%8、col=i/8）或单 (row,column)
QVector<Ch> buildApply(bool bias, bool thr, bool allModule, bool allChn,
                       uint32_t module, uint32_t row, uint32_t column,
                       uint32_t lorU, int32_t valBias, int32_t valThr, int nDmNum);

// Add/Sub（OnBnClickedButtonAdd/Sub）：相对调整（adjMode=1、value=±step），
// 只对单 module（Windows 此路径不遍历 allModule）
QVector<Ch> buildAdjust(bool bias, bool thr, bool allChn,
                        uint32_t module, uint32_t row, uint32_t column,
                        uint32_t lorU, int32_t stepB, int32_t stepThr, int sign);

// 扫描阈值（SetScanThr）：每 DM×48 通道×上下两阈值，upper=lower+bin
QVector<Ch> buildScanThr(int nDmNum, int32_t valThr, int32_t valBin);

// 回读提取（UpdateCurrentFndmPara）：按 module 取 bias（不判行列）、
// 按 (row,column) 取上下阈值；模块不存在返回 false 且不改输出
bool extractCurrent(const QVector<Ch>& para, uint32_t module,
                    uint32_t row, uint32_t column,
                    int32_t& valBias, int32_t& valThrL, int32_t& valThrU);

// 上次对话框状态（Windows View::m_FndLastPara / LASTPARA）：跨打开记忆
struct LastPara {
    uint32_t module = 0, row = 0, column = 0;
    bool bAllChn = true, bAllModule = true, bBias = true, bThr = true;
    uint32_t lowup = 0;               // 0-下阈值 1-上阈值
    int32_t nValBias = 0, nValThrL = 0, nValThrU = 0;
};

} // namespace FndmParaBuilder
