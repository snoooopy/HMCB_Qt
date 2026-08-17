#pragma once
#include <QTextStream>
#include <QVector>
#include "MeasurementEngine.h"

// 把 MeasurementEngine 的测量结果写为 csv 或 txt（对照 OnFileSaveData:3593-3753）。
// Phase 3：条件列组——未测量的项整组省略；writeCsv/writeTxt 拆分供双写。
class CsvResultWriter {
public:
    struct Options {
        int vnFormat = 0;        // 0 FWHM 1 RMS 2 SNR 3 ERB
        int vzFormat = 0;        // 0 LSB 1 mV
        int adBit = 20;
        double data2mV = 1048576.0 / 4096.0;
        bool csv = true;         // write() 兼容入口按此分派
        // 条件列组：true 才输出该组（未测量整组省略）
        bool hasVn = false, hasVz = false, hasInl = false, hasXdif = false;
        QVector<double> inl, diff;      // INL(无量纲)、DIV(比值)；×1000/×100 显示
        QVector<double> vzX, xDif;      // Phase 4 XRay 列用
        double aXptos = 0;
    };
    static void write(QTextStream& out, int nChannels,
                      const QVector<double>& vz,
                      const QVector<MeasurementEngine::NoiseChan>& vn,
                      const Options& opt);          // 兼容入口：按 opt.csv 分派
    static void writeCsv(QTextStream& out, int nChannels,
                         const QVector<double>& vz,
                         const QVector<MeasurementEngine::NoiseChan>& vn,
                         const Options& opt);
    static void writeTxt(QTextStream& out, int nChannels,
                         const QVector<double>& vz,
                         const QVector<MeasurementEngine::NoiseChan>& vn,
                         const Options& opt);
};
