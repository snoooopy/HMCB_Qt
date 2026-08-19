#include "MeasurementEngine.h"
#include <QMutexLocker>
#include <algorithm>
#include <cmath>

static constexpr double kInlScaleToMv = 3355.4432;   // 2^25/10^4（Windows INL 标度）
static constexpr double kInlFsrMv = 5000.0;          // INL 满量程
static constexpr uint32_t kInlDaMin = 100;           // DAMIN

void MeasurementEngine::reset(int nMCB) {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
    m_ring[i].clear();
    m_pos[i].clear();
    m_latest[i].clear();
    m_count[i] = 0;
    m_vz[i].clear();
    m_vn[i].clear();
    m_vzx[i].clear();
    m_xdif[i].clear();
    m_bVzero[i] = m_bVnoise[i] = m_bInl[i] = m_bXdif[i] = false;  // 新测量开始作废旧标志
}

void MeasurementEngine::appendFrame(int nMCB, const QVector<uint32_t>& channels) {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
    if ((int)m_ring[i].size() >= m_depth) m_ring[i].removeFirst();
    m_ring[i].append(channels);
    m_latest[i] = channels;
    m_count[i] = std::min(m_count[i] + 1, m_depth);
}

void MeasurementEngine::appendPos(int nMCB, uint32_t ptsmL, uint32_t ptsmH) {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
    if ((int)m_pos[i].size() >= m_depth) m_pos[i].removeFirst();
    m_pos[i].append(qMakePair(ptsmL, ptsmH));
}

QVector<QPair<uint32_t, uint32_t>> MeasurementEngine::posSnapshot(int nMCB) const {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
    return m_pos[i];
}

QVector<QVector<uint32_t>> MeasurementEngine::ringSnapshot(int nMCB) const {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
    return m_ring[i];
}

int MeasurementEngine::frameCount(int nMCB) const {
    QMutexLocker lk(&m_mutex);
    return m_count[nMCB & 7];
}

QVector<uint32_t> MeasurementEngine::latestFrame(int nMCB) const {
    QMutexLocker lk(&m_mutex);
    return m_latest[nMCB & 7];
}

bool MeasurementEngine::calcZero(int nMCB) {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
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
    m_bVzero[i] = true;
    return true;
}

// 对照 VzeroCal:2901/2936（fn==4 分支）+ 2977-2987 段：VzX=本轮均值−Vz 基线，
// X_dif=每 16 通道组 |max−min|/2/(组均值)。组均值 0 时钳 0（Windows 会出 NaN）。
bool MeasurementEngine::calcXdif(int nMCB) {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
    const int nChn = m_latest[i].size();
    if (nChn == 0 || m_ring[i].isEmpty()) return false;
    if (!m_bVzero[i] || m_vz[i].size() != nChn) return false;   // 须先 Zero（!bVzero 拦截）
    const int N = std::min(1024, (int)m_ring[i].size());
    const int start = m_ring[i].size() - N;
    m_vzx[i].fill(0.0, nChn);
    for (int k = 0; k < nChn; ++k) {
        double sum = 0;
        for (int f = start; f < (int)m_ring[i].size(); ++f)
            sum += m_ring[i][f].value(k);
        m_vzx[i][k] = sum / N - m_vz[i][k];                      // 净信号
    }
    m_xdif[i].fill(0.0, nChn / 16);
    for (int g = 0; g < nChn / 16; ++g) {
        double dmax = 0, dmin = 0, sa = 0;
        for (int j = g * 16; j < (g + 1) * 16; ++j) {
            const double v = m_vzx[i][j];
            if (j == g * 16) dmax = dmin = v;
            else { dmax = std::max(dmax, v); dmin = std::min(dmin, v); }
            sa += v;
        }
        const double mean = sa / 16.0;
        m_xdif[i][g] = mean != 0.0 ? std::fabs((dmax - dmin) / 2.0 / mean) : 0.0;
    }
    m_bXdif[i] = true;
    return true;
}

bool MeasurementEngine::calcNoise(int nMCB) {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
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
    m_bVnoise[i] = true;
    return true;
}

QVector<double> MeasurementEngine::rms256(int nMCB) const {
    int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
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

// ---- INL（对照 GetINLData:3403 / INL_Cal:3050-3096）----

uint32_t MeasurementEngine::beginInl(int points) {
    points = qBound(5, points, 40);
    m_inlPoints = points;
    const uint32_t base = static_cast<uint32_t>(m_cfg.inlDaMax) * 4;   // 16bit 码基线
    m_inlStep = (base - kInlDaMin * 4) / static_cast<uint32_t>(points - 1);  // 整数除法必须保留
    m_inlVin.resize(points);
    m_inlVin[0] = 0.0;
    for (int i = 1; i < points; ++i)
        m_inlVin[i] = m_inlStep * 2500.0 / 65536.0 + m_inlVin[i - 1];
    for (int i = 0; i < MAX_MCB; ++i) { m_inlY[i].clear(); m_bInl[i] = false; }
    return m_inlStep;
}

void MeasurementEngine::addInlPoint(int nMCB, int pointIdx) {
    const int i = idx(nMCB);
    QMutexLocker lk(&m_mutex);
    const int nChn = m_latest[i].size();
    if (nChn == 0 || m_ring[i].isEmpty()) return;
    const int N = std::min(1024, static_cast<int>(m_ring[i].size()));
    const int start = m_ring[i].size() - N;
    if (m_inlY[i].size() <= pointIdx) m_inlY[i].resize(pointIdx + 1);
    QVector<double> y(nChn, 0.0);
    for (int k = 0; k < nChn; ++k) {
        double sum = 0;
        for (int f = start; f < static_cast<int>(m_ring[i].size()); ++f)
            sum += m_ring[i][f].value(k);
        y[k] = sum / N / kInlScaleToMv;
    }
    m_inlY[i][pointIdx] = y;
}

uint32_t MeasurementEngine::inlDaCode(int pointIdx) const {
    const uint32_t base = static_cast<uint32_t>(m_cfg.inlDaMax) * 4;
    uint32_t da = base;
    if (pointIdx == 0) da += da << 16;                                   // 第 1 点摆幅 0
    else               da = (da << 16) + (da - m_inlStep * static_cast<uint32_t>(pointIdx));
    return da;
}

bool MeasurementEngine::calcInl(int nMCB, int channelsPerDm) {
    const int i = idx(nMCB);
    const int n = static_cast<int>(m_inlY[i].size());
    if (n < 5 || channelsPerDm <= 0 || m_inlY[i][0].isEmpty()) return false;
    const int nChn = m_inlY[i][0].size();
    m_inlA[i].fill(0.0, nChn); m_inlB[i].fill(0.0, nChn); m_inlR[i].fill(0.0, nChn);
    m_inl[i].fill(0.0, nChn);  m_diff[i].fill(0.0, nChn);
    for (int k = 0; k < nChn; ++k) {
        double sx = 0, sy = 0, sxy = 0, sxx = 0;
        for (int p = 0; p < n; ++p) {
            const double x = m_inlVin[p], yv = m_inlY[i][p][k];
            sx += x; sy += yv; sxy += x * yv; sxx += x * x;
        }
        const double den = n * sxx - sx * sx;
        if (den == 0) continue;
        const double a = (n * sxy - sx * sy) / den;
        const double b = (sy - a * sx) / n;
        const double ybar = sy / n;
        double sres = 0, stot = 0, dmax = 0;
        for (int p = 0; p < n; ++p) {
            const double x = m_inlVin[p], yv = m_inlY[i][p][k];
            const double yhat = a * x + b;
            sres += (yhat - ybar) * (yhat - ybar);
            stot += (yv - ybar) * (yv - ybar);
            dmax = std::max(dmax, std::abs(yhat - yv));
        }
        m_inlA[i][k] = a;
        m_inlB[i][k] = b;
        m_inlR[i][k] = stot > 0 ? std::sqrt(sres / stot) : 0.0;
        m_inl[i][k] = dmax / kInlFsrMv;
    }
    for (int k = 0; k < nChn; ++k) {                 // DIV = 通道斜率 / DM 内均值
        const int dm = k / channelsPerDm;
        double sum = 0; int cnt = 0;
        for (int j = dm * channelsPerDm;
             j < std::min((dm + 1) * channelsPerDm, nChn); ++j) { sum += m_inlA[i][j]; ++cnt; }
        m_diff[i][k] = cnt > 0 ? m_inlA[i][k] / (sum / cnt) : 1.0;
    }
    m_bInl[i] = true;
    return true;
}

void MeasurementEngine::clearResults() {
    for (int i = 0; i < MAX_MCB; ++i) {
        m_vz[i].clear(); m_vn[i].clear();
        m_vzx[i].clear(); m_xdif[i].clear();
        m_inlY[i].clear(); m_inlA[i].clear(); m_inlB[i].clear();
        m_inlR[i].clear(); m_inl[i].clear(); m_diff[i].clear();
        m_bVzero[i] = m_bVnoise[i] = m_bInl[i] = m_bXdif[i] = false;
    }
}
