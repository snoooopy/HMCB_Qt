#include "OriginDataWriter.h"
#include <QFile>
#include <QDataStream>

bool OriginDataWriter::write(const QString& path,
                             const QVector<QVector<uint32_t>>& frames) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&f);
    out.setByteOrder(QDataStream::LittleEndian);   // x86/Windows 文件格式
    out.setVersion(QDataStream::Qt_6_0);
    for (const auto& fr : frames)
        for (int j = 0; j < kSaveChn && j < fr.size(); ++j)
            out << qint32(fr[j]);                  // Windows sizeof(long)=4 字节
    return true;
}
