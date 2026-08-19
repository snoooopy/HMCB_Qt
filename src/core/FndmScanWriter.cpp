#include "FndmScanWriter.h"
#include <QFile>
#include <QTextStream>

namespace FndmScanWriter {

// txt（对照 769-783 行）：两段（背景/谱），每段头行 + 每 bin 一行
bool writeTxt(const QString& path, const Spec& s, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("Open File Error!");
        return false;
    }
    QTextStream out(&f);
    const int nChn = s.scanData.size();
    const int nBin = s.binCount();
    const int timeMs = s.potmtMs / 1000;
    for (int i = 0; i < 2; ++i) {                       // background and spectrum
        out << "Binwidth[mV] Binstart[mV] " << s.sampleTimes
            << " x [[time[ms]  " << nChn << " x pixelvalue]]\n";
        for (int k = 0; k < nBin; ++k) {
            out << s.valBin << '\t' << (s.valScan + k * s.valBin) << '\t' << timeMs;
            for (int j = 0; j < nChn; ++j)
                out << '\t' << s.scanData[j][k][i];
            out << '\n';
        }
    }
    return true;
}

// csv（对照 789-819 行）：两段，每段 = 头行 + Binstart 行 + Pixel 行 + 每通道一行 + 空行
bool writeCsv(const QString& path, const Spec& s, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("Open File Error!");
        return false;
    }
    QTextStream out(&f);
    const int nChn = s.scanData.size();
    const int nBin = s.binCount();
    const int timeMs = s.sampleTimes * s.potmtMs / 1000;
    for (int i = 0; i < 2; ++i) {
        out << "Binwidth[mV]:" << s.valBin << "  time[ms]" << timeMs
            << "  " << nChn << " x pixelvalue\n";
        out << "Binstart[mV]";
        for (int k = 0; k < nBin; ++k)
            out << ',' << (s.valScan + k * s.valBin);
        out << '\n' << "Pixel\n";
        for (int j = 0; j < nChn; ++j) {
            out << j;
            for (int k = 0; k < nBin; ++k)
                out << ',' << s.scanData[j][k][i];
            out << '\n';
        }
        out << '\n';
    }
    return true;
}

} // namespace FndmScanWriter
