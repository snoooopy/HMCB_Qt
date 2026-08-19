#include "PosDataWriter.h"
#include <QFile>
#include <QTextStream>

bool PosDataWriter::write(const QString& path,
                          const QVector<QPair<uint32_t, uint32_t>>& frames) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    for (int i = 0; i < frames.size(); ++i) {
        // 对照 Windows："%d" 有符号打印——32bit 值按 long 输出（同 CStdioFile 行为）
        out << QStringLiteral("Packet:%1, AngleCode:%2, ConveyorCode:%3\n")
               .arg(i)
               .arg(static_cast<qlonglong>(frames[i].first))
               .arg(static_cast<qlonglong>(frames[i].second));
    }
    return true;
}
