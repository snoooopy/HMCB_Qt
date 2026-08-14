#pragma once
#include <QTextStream>
#include <QVector>
#include "MeasurementEngine.h"

// 把 MeasurementEngine 的 Zero/Noise 结果写为 csv 或 txt（对照 OnFileSaveData）。
// INL/XRay/X-DIV 列留待 Phase 3/4 补。
class CsvResultWriter {
public:
    struct Options {
        int vnFormat = 0;        // 0 FWHM 1 RMS 2 SNR 3 ERB
        int vzFormat = 0;        // 0 LSB 1 mV
        int adBit = 20;
        double data2mV = 1048576.0 / 4096.0;
        bool csv = true;         // true=csv(逗号) false=txt(文本块)
    };
    static void write(QTextStream& out, int nChannels,
                      const QVector<double>& vz,
                      const QVector<MeasurementEngine::NoiseChan>& vn,
                      const Options& opt);
};
