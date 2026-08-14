#include "CsvResultWriter.h"

static const char* vnLabel(int fmt) {
    switch (fmt) { case 1: return "RMS"; case 2: return "SNR"; case 3: return "ERB";
                   default: return "FWHM"; }
}
static const char* vzLabel(int fmt) { return fmt == 1 ? "mV" : "LSB"; }

void CsvResultWriter::write(QTextStream& out, int nChannels,
                            const QVector<double>& vz,
                            const QVector<MeasurementEngine::NoiseChan>& vn,
                            const Options& opt) {
    if (opt.csv) {
        out << "Channel," << "Noise(" << vnLabel(opt.vnFormat) << "),"
            << "Zero(" << vzLabel(opt.vzFormat) << ")\n";
        for (int i = 0; i < nChannels; ++i) {
            const double fwhm = (i < vn.size()) ? vn[i].fwhm : 0.0;
            const double noise = MeasurementEngine::toDisplayFormat(fwhm, opt.vnFormat, opt.adBit);
            const double z = (i < vz.size()) ? vz[i] : 0.0;
            const double zv = (opt.vzFormat == 1) ? z / opt.data2mV : z;
            out << QString::asprintf("%5u,", i);
            out << QString::asprintf(opt.adBit <= 16 ? "%10.1f," : "%10.0f,", noise);
            out << QString::asprintf(opt.vzFormat == 1 ? "%9.2f\n" : "%9.0f\n", zv);
        }
    } else {
        // txt 文本块，4 通道/行（对照 OnFileSaveData txt 分支）
        for (int i = 0; i < nChannels; i += 4) {
            for (int j = 0; j < 4 && i + j < nChannels; ++j) {
                const int ch = i + j;
                const double z = (ch < vz.size()) ? vz[ch] : 0.0;
                out << QString::asprintf("CH[%3u]=%8.0f      ", ch, z);
            }
            out << "\n";
        }
    }
}
