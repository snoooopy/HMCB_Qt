#pragma once
#include <QString>
#include <QVector>
#include <QMetaType>
#include <cstdint>
#include "MCBCtrl.h"   // UINT/LONG/WPARAM/LPARAM、MCB_DVREADY 等类别码

namespace Mcb {

// lParam 解码为 (nMCB, statusCode)
struct StatusParam { int nMCB = 0; uint32_t code = 0; };
StatusParam decodeStatusParam(LPARAM lParam);

// 一帧 GUI 值对象（与库结构 MCBData 解耦，便于跨线程 Queued 传递）
struct McbFrame {
    int nMCB = 0;
    long scounter = 0;
    long pcounter = 0;
    long accEnergy = 0;
    uint32_t ain0 = 0;
    uint32_t ain1 = 0;
    uint32_t regError_DI = 0;
    uint32_t ptsmL = 0;                 // d_head.head_use.ptsmdata_l：AngleCode 全 32bit
    uint32_t ptsmH = 0;                 // d_head.head_use.ptsmdata_h：ConveyorCode 全 32bit
    QVector<uint32_t> channels;   // 重排后的通道样本（AD模块×行×列 展平）
};

// 每帧扩展数据（Save Image Data 数据源，对照 Windows m_MCBExData/fAccEnergy/m_godown
// 填充链 MCB_TESTView.cpp:855-895）：ex[0]=ain0、[1]=ptsm_h>>16、[2]=ptsm_h&0xffff、
// [3]=ptsm_l>>16、[4]=ptsm_l&0xffff；accE=accEnergy?1875:62；godown=(DI&0x04)>>2
struct FrameEx {
    int32_t ex[5] = {0, 0, 0, 0, 0};
    int32_t accE = 0;
    int32_t godown = 0;
};

QString statusCategoryStr(WPARAM category);
QString statusCodeStr(uint32_t code);

// "a.b.c.d" → 4 字节（段数=4、纯数字、每段 0..255）；Set New IP 用
bool parseIpv4(const QString& text, quint8 out[4]);

} // namespace Mcb

Q_DECLARE_METATYPE(Mcb::McbFrame)
