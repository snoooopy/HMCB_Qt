#pragma once
#include <QString>
#include <QVector>
#include "McbTypes.h"

// Save Image Data 三格式写盘纯函数（对照 Windows OnSavedata，MCB_TESTView.cpp:4134-4595）。
// - ORI：PtT_file_head(128B)+PTT_det_saveinfo(128B 全零) 头 + 小端 long 数据，
//   每帧每列前 16B datainfo；多列非 allCol 拆 -N.ori
// - IMG：36B 大端头 + 大端像素（16bit 右对齐 v>>(adBit-16) / 32bit 左对齐 v<<(32-adBit)），
//   bSaveExdata 前 4 行 accE/godown/Ain0/0，bOriGM 末尾 16 行 Ain0；拆 -N.img
// - CSV：SN,M{j}CH{l}-{k:02d},...,EXCH0..4
// - bSaveExdata 附加：<名>.acce / .Ain0 / .godown.csv（变化点）
// 开关读 MCB_TEST.ini [FILE]（IMGFILEPIXEL/GMCORINORI/ALLCOLUMN/SAVE_EXDATA），由调用方传入 Options。
namespace ImageDataWriter {

struct Options {
    int nAdm = 1;          // AD 模块数（m_nMCBADNum）
    int nRowNum = 8;       // 每 DM 行数
    int nColNum = 1;       // 列（笔）数
    int adBit = 20;
    bool pixel16 = true;   // IMGFILEPIXEL：16/32 bit
    bool oriGM = false;    // GMCORINORI：ORI/IMG 追加 16 行亮度校正
    bool allColumn = true; // ALLCOLUMN：全列单文件
    bool saveExdata = true;// SAVE_EXDATA：IMG 4 行 + 附加文件
    bool fnd = false;      // FND 模式：GM 行 16bit 不移位（Windows bFND 分支）
};

enum class Format { Ori, Img, Csv };

// frames：ring 快照（老→新，已由引擎环形限 kDataNum）；ex：逐帧扩展字（可空）
bool save(const QString& path, Format fmt, const QVector<QVector<uint32_t>>& frames,
          const QVector<Mcb::FrameEx>& ex, const Options& opt, QString* err = nullptr);

} // namespace ImageDataWriter
