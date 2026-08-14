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
    QVector<uint32_t> channels;   // 重排后的通道样本（AD模块×行×列 展平）
};

QString statusCategoryStr(WPARAM category);
QString statusCodeStr(uint32_t code);

} // namespace Mcb

Q_DECLARE_METATYPE(Mcb::McbFrame)
