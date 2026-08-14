#include "MeasurementEngine.h"
#include <algorithm>
#include <cmath>

void MeasurementEngine::reset(int nMCB) {
    int i = idx(nMCB);
    m_ring[i].clear();
    m_latest[i].clear();
    m_count[i] = 0;
    m_vz[i].clear();
    m_vn[i].clear();
}

void MeasurementEngine::appendFrame(int nMCB, const QVector<uint32_t>& channels) {
    int i = idx(nMCB);
    if ((int)m_ring[i].size() >= m_depth) m_ring[i].removeFirst();
    m_ring[i].append(channels);
    m_latest[i] = channels;
    m_count[i] = std::min(m_count[i] + 1, m_depth);
}

bool MeasurementEngine::calcZero(int nMCB) {
    int i = idx(nMCB);
    const int nChn = m_latest[i].size();
    if (nChn == 0 || m_ring[i].isEmpty()) return false;
    const int N = std::min(1024, (int)m_ring[i].size());
    const int start = m_ring[i].size() - N;
    m_vz[i].fill(0.0, nChn);
    for (int k = 0; k < nChn; ++k) {
        double sum = 0;
        for (int f = start; f < (int)m_ring[i].size(); ++f)
            sum += m_ring[i][f].value(k);
        m_vz[i][k] = sum / N;
    }
    return true;
}

bool MeasurementEngine::calcNoise(int nMCB) {
    int i = idx(nMCB);
    const int nChn = m_latest[i].size();
    if (nChn == 0 || m_count[i] < 200) return false;
    const int N = std::min(2048, (int)m_ring[i].size());
    const int start = m_ring[i].size() - N;
    m_vn[i].fill(NoiseChan{}, nChn);
    for (int k = 0; k < nChn; ++k) {
        double sum = 0, sq = 0; long amax = 0, amin = 0x7FFFFFFF;
        for (int f = start; f < (int)m_ring[i].size(); ++f) {
            long v = (long)m_ring[i][f].value(k);
            sum += v; sq += (double)v * v;
            if (v > amax) amax = v;
            if (v < amin) amin = v;
        }
        double var = sq / N - (sum * sum) / ((double)N * N);
        if (var < 0) var = 0;
        m_vn[i][k].fwhm = 2.355 * std::sqrt(var);
        m_vn[i][k].mean = sum / N;
        m_vn[i][k].max = amax;
        m_vn[i][k].min = amin;
    }
    return true;
}

QVector<double> MeasurementEngine::rms256(int nMCB) const {
    int i = idx(nMCB);
    QVector<double> out(m_latest[i].size(), 0.0);
    if (m_ring[i].isEmpty()) return out;
    const int N = std::min(256, (int)m_ring[i].size());
    const int start = m_ring[i].size() - N;
    for (int k = 0; k < out.size(); ++k) {
        double sum = 0, sq = 0;
        for (int f = start; f < (int)m_ring[i].size(); ++f) {
            long v = (long)m_ring[i][f].value(k);
            sum += v; sq += (double)v * v;
        }
        double var = sq / N - (sum * sum) / ((double)N * N);
        out[k] = var > 0 ? std::sqrt(var) : 0.0;
    }
    return out;
}

double MeasurementEngine::toDisplayFormat(double fwhm, int fmt, int adBit) {
    switch (fmt) {
    case 1: return fwhm / 2.355;                                                          // RMS
    case 2: { const double s = fwhm / 2.355; return s > 0 ? std::pow(2.0, adBit) / s : 0.0; } // SNR
    case 3: { const double s = fwhm / 2.355; return s > 0 ? std::log2(std::pow(2.0, adBit) / s) : 0.0; } // ERB
    case 0:
    default: return fwhm;                                                                 // FWHM
    }
}

bool MeasurementEngine::passFail(double value, double lo, double hi) {
    if (lo == 0.0 && hi == 0.0) return true;       // 未设阈值
    return value >= lo && value <= hi;
}
