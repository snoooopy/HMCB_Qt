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
    if (opt.csv) writeCsv(out, nChannels, vz, vn, opt);
    else         writeTxt(out, nChannels, vz, vn, opt);
}

void CsvResultWriter::writeCsv(QTextStream& out, int nChannels,
                               const QVector<double>& vz,
                               const QVector<MeasurementEngine::NoiseChan>& vn,
                               const Options& opt) {
    QString head = QStringLiteral("Channel,");
    if (opt.hasVn)   head += QString::asprintf("Noise(%s),", vnLabel(opt.vnFormat));
    if (opt.hasVz)   head += QString::asprintf("Zero(%s),", vzLabel(opt.vzFormat));
    if (opt.hasXdif) head += QStringLiteral("XRay(LSB),XRay(uC/Gy),X-DIV(percent),");
    if (opt.hasInl)  head += QStringLiteral("INL(thousandth),DIV(percent),");
    out << head << "\n";
    for (int i = 0; i < nChannels; ++i) {
        QString row = QString::asprintf("%5u,", i);
        if (opt.hasVn) {
            const double fwhm = (i < vn.size()) ? vn[i].fwhm : 0.0;
            const double noise = MeasurementEngine::toDisplayFormat(fwhm, opt.vnFormat, opt.adBit);
            row += QString::asprintf(opt.adBit <= 16 ? "%10.1f," : "%10.0f,", noise);
        }
        if (opt.hasVz) {
            const double z = (i < vz.size()) ? vz[i] : 0.0;
            const double zv = (opt.vzFormat == 1) ? z / opt.data2mV : z;
            row += QString::asprintf(opt.vzFormat == 1 ? "%9.2f," : "%9.0f,", zv);
        }
        if (opt.hasXdif) {
            const double vx = (i < opt.vzX.size()) ? opt.vzX[i] : 0.0;
            const double uc = vx * opt.aXptos;
            const double xd = (i / 16 < opt.xDif.size()) ? opt.xDif[i / 16] : 0.0;
            row += QString::asprintf("%8.0f,%8.3f,%9.1f,", vx, uc, xd * 100.0);
        }
        if (opt.hasInl) {
            const double inl = (i < opt.inl.size()) ? opt.inl[i] : 0.0;
            const double dv  = (i < opt.diff.size()) ? opt.diff[i] : 1.0;
            row += QString::asprintf("%8.2f,%8.1f,", inl * 1000.0, (dv - 1.0) * 100.0);
        }
        out << row << "\n";
    }
}

void CsvResultWriter::writeTxt(QTextStream& out, int nChannels,
                               const QVector<double>& vz,
                               const QVector<MeasurementEngine::NoiseChan>& vn,
                               const Options& opt) {
    Q_UNUSED(vn);
    // Zero 节（Phase 2 行为：未设任何标志时仍输出 Zero 块）
    if (opt.hasVz || (!opt.hasVn && !opt.hasInl && !opt.hasXdif)) {
        for (int i = 0; i < nChannels; i += 4) {
            QString line;
            for (int j = 0; j < 4 && i + j < nChannels; ++j) {
                const int ch = i + j;
                const double z = (ch < vz.size()) ? vz[ch] : 0.0;
                line += QString::asprintf("CH[%3u]=%8.0f      ", ch, z);
            }
            out << line << "\n";
        }
    }
    if (opt.hasInl) {   // 对照 OnFileSaveData txt 分支 3907-3937
        out << "INL:\n  a. INL(thousandth):\n";
        for (int i = 0; i < nChannels; i += 4) {
            QString line;
            for (int j = 0; j < 4 && i + j < nChannels; ++j) {
                const int ch = i + j;
                const double inl = (ch < opt.inl.size()) ? opt.inl[ch] : 0.0;
                line += QString::asprintf("CH[%3u]=%5.2f      ", ch, inl * 1000.0);
            }
            out << line << "\n";
        }
        out << "  b. Diversity between Channels(percent):\n";
        for (int i = 0; i < nChannels; i += 4) {
            QString line;
            for (int j = 0; j < 4 && i + j < nChannels; ++j) {
                const int ch = i + j;
                const double dv = (ch < opt.diff.size()) ? opt.diff[ch] : 1.0;
                line += QString::asprintf("CH[%3u]=%6.1f      ", ch, (dv - 1.0) * 100.0);
            }
            out << line << "\n";
        }
    }
}
