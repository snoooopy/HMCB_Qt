#pragma once
#include <QVector>
#include <cstdint>

// 把 MeasurementEngine 的帧缓存写为 .oridat 二进制（对照 OnSavedatao:4623-4664）。
// 每帧只写前 SAVECHN=128 个通道样本，逐点 int32 小端（Windows long=4B）。
class OriginDataWriter {
public:
    static constexpr int kSaveChn = 128;   // Windows #define SAVECHN
    // frames: 老→新（engine ringSnapshot）
    static bool write(const QString& path, const QVector<QVector<uint32_t>>& frames);
};
