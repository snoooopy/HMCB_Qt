#pragma once
#include <QVector>
#include <cstdint>

// 纯逻辑测量引擎：维护每板帧环形缓冲，实现 Zero/Noise 计算、显示格式转换、pass/fail。
// 无 UI 依赖，可用合成数据单测。公式对照 Windows VzeroCal/FWHM_Cal。
class MeasurementEngine {
public:
    struct NoiseChan { double fwhm = 0, mean = 0, max = 0, min = 0; };
    struct Config {
        int vzSampleNum = 512;
        int vnSampleNum = 2048;
        double vzMax = 0, vzMin = 0;        // 0 = 未设阈值（不判定）
        double vnMax = 0, vnMin = 0;
        double vndMax = 0, vndMin = 0;      // NoiseD 阈值
        int vnFormat = 0;                    // 0 FWHM 1 RMS 2 SNR 3 ERB
        int vzFormat = 0;                    // 0 LSB 1 mV
        int adBit = 20;
        double data2mV = 1048576.0 / 4096.0; // 20bit 默认
    };

    void setDepth(int depth) { m_depth = depth > 0 ? depth : 2048; }
    void applyConfig(const Config& c) { m_cfg = c; }
    const Config& config() const { return m_cfg; }

    void reset(int nMCB);
    void appendFrame(int nMCB, const QVector<uint32_t>& channels);

    int frameCount(int nMCB) const { return m_count[nMCB & 7]; }
    const QVector<uint32_t>& latestFrame(int nMCB) const { return m_latest[nMCB & 7]; }

    bool calcZero(int nMCB);                 // 写 m_vz[nMCB]，成功 true
    bool calcNoise(int nMCB);                // 写 m_vn[nMCB]，帧数<200 返回 false
    const QVector<double>& vz(int nMCB) const { return m_vz[nMCB & 7]; }
    const QVector<NoiseChan>& vn(int nMCB) const { return m_vn[nMCB & 7]; }

    QVector<double> rms256(int nMCB) const;  // 逐通道最近 256 帧总体标准差（波形 RMS 用）

    static double toDisplayFormat(double fwhm, int fmt, int adBit);
    static bool passFail(double value, double lo, double hi);   // lo==hi==0 视为未设=合格

    static constexpr int MAX_MCB = 8;

private:
    Config m_cfg;
    int m_depth = 2048;
    QVector<QVector<uint32_t>> m_ring[MAX_MCB];   // 每板帧环形缓冲（深 m_depth）
    QVector<uint32_t> m_latest[MAX_MCB];
    int m_count[MAX_MCB]{};
    QVector<double> m_vz[MAX_MCB];
    QVector<NoiseChan> m_vn[MAX_MCB];

    int idx(int nMCB) const { return nMCB & 7; }
};
