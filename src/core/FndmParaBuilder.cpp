#include "FndmParaBuilder.h"

namespace FndmParaBuilder {

QVector<Ch> buildApply(bool bias, bool thr, bool allModule, bool allChn,
                       uint32_t module, uint32_t row, uint32_t column,
                       uint32_t lorU, int32_t valBias, int32_t valThr, int nDmNum) {
    QVector<Ch> v;
    if (bias) {
        for (int i = 0; i < nDmNum; ++i) {
            v.append(Ch(allModule ? uint32_t(i) : module, 0, 0, 0, 0, valBias));
            if (!allModule) break;
        }
    }
    if (thr) {
        for (int j = 0; j < nDmNum; ++j) {
            for (int i = 0; i < kRow * kColumn; ++i) {
                v.append(Ch(allModule ? uint32_t(j) : module,
                            lorU + 1,
                            allChn ? uint32_t(i % kRow) : row,
                            allChn ? uint32_t(i / kRow) : column,
                            0, valThr));
                if (!allChn) break;
            }
            if (!allModule) break;
        }
    }
    return v;
}

QVector<Ch> buildAdjust(bool bias, bool thr, bool allChn,
                        uint32_t module, uint32_t row, uint32_t column,
                        uint32_t lorU, int32_t stepB, int32_t stepThr, int sign) {
    QVector<Ch> v;
    if (bias)
        v.append(Ch(module, 0, 0, 0, 1, sign * stepB));
    if (thr) {
        for (int i = 0; i < kRow * kColumn; ++i) {
            v.append(Ch(module, lorU + 1,
                        allChn ? uint32_t(i % kRow) : row,
                        allChn ? uint32_t(i / kRow) : column,
                        1, sign * stepThr));
            if (!allChn) break;
        }
    }
    return v;
}

QVector<Ch> buildScanThr(int nDmNum, int32_t valThr, int32_t valBin) {
    QVector<Ch> v;
    for (int n = 0; n < nDmNum; ++n)
        for (int i = 0; i < kRow * kColumn; ++i)
            for (int j = 0; j < 2; ++j)   // 0-下阈值 1-上阈值
                v.append(Ch(uint32_t(n), uint32_t(j + 1),
                            uint32_t(i % kRow), uint32_t(i / kRow),
                            0, j == 0 ? valThr : valThr + valBin));
    return v;
}

bool extractCurrent(const QVector<Ch>& para, uint32_t module,
                    uint32_t row, uint32_t column,
                    int32_t& valBias, int32_t& valThrL, int32_t& valThrU) {
    bool found = false;
    for (const Ch& c : para) {
        if (c.fndm != module) continue;
        found = true;
        if (c.type == 0)                       // bias：不判行列
            valBias = c.value;
        else if (c.row == row && c.colnum == column) {
            if (c.type == 1) valThrL = c.value;
            else             valThrU = c.value;
        }
    }
    return found;
}

} // namespace FndmParaBuilder
