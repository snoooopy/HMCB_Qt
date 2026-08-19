#pragma once
#include <QString>
#include <QVector>
#include <cstdint>

// 快中子谱扫描存档（对照 DlgFNDMParaSet::SaveScanSpec 760-828 行）：
// txt = 每 bin 一行（Binwidth/Binstart/时间 + 各通道计数），
// csv = 矩阵（行=Pixel、列=Binstart）。均为背景(sn=0)与谱(sn=1)两段。
namespace FndmScanWriter {

struct Spec {
    int valScan = 50;        // 起始阈值（bin 0 下阈）
    int valScan2 = 4095;     // 终止阈值
    int valBin = 100;        // bin 宽
    int sampleTimes = 100;   // 每 bin 采样次数
    int potmtMs = 4000;      // POTMT（ms），源码 m_nPOTMTtime
    // scanData[chn][bin][sn]：sn 0-背景 1-谱
    QVector<QVector<QVector<uint64_t>>> scanData;
    int binCount() const { return valBin > 0 ? (valScan2 - valScan) / valBin : 0; }
};

bool writeTxt(const QString& path, const Spec& s, QString* err = nullptr);
bool writeCsv(const QString& path, const Spec& s, QString* err = nullptr);

} // namespace FndmScanWriter
