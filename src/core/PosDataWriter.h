#pragma once
#include <QVector>
#include <QPair>
#include <cstdint>

// 把 MeasurementEngine 的 POS 缓存写为 txt（对照 OnSavePosdata:4752-4782）。
// 每帧一行 "Packet:%d, AngleCode:%d, ConveyorCode:%d"：
//   AngleCode   = ptsmdata_l（32bit 全值）
//   ConveyorCode = ptsmdata_h（32bit 全值）
class PosDataWriter {
public:
    // frames: (ptsmdata_l, ptsmdata_h) 老→新，Packet 序号=数组下标
    static bool write(const QString& path,
                      const QVector<QPair<uint32_t, uint32_t>>& frames);
};
