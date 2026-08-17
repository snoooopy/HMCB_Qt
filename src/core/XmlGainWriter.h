#pragma once
#include <QString>
#include <QVector>
#include <cstdint>

// 增益校准结果写回 DIASConfig.xml（对照 Windows WriteGainValueToConfigureFile:2829）。
// 与 Windows 的差异（用户决策）：路径参数化；/NucDiasConfigXML/CCM/MCB{n}_RANGE
// 缺失时自动创建（Windows MSXML 只改预存节点）。其余节点内容不破坏。
class XmlGainWriter {
public:
    static bool write(const QString& path, int nMCB, const QVector<uint8_t>& dmGains);
};
