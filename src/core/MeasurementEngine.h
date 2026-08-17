#pragma once
#include <QVector>
#include <QMutex>
#include <cstdint>

// 纯逻辑测量引擎：维护每板帧环形缓冲，实现 Zero/Noise 计算、显示格式转换、pass/fail。
// 无 UI 依赖，可用合成数据单测。公式对照 Windows VzeroCal/FWHM_Cal。
// 线程模型：appendFrame 在库数据回调（工作线程）执行；latestFrame/rms256 由
// GUI 线程定时调用——m_ring/m_latest/m_count 以 m_mutex 互斥保护，其余结果
// 数组只在测量完成后经信号通知 GUI 读取，无并发。
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
        // ---- INL（对照 [INL] 节 / INL_Cal）----
        int inlSampleNum = 512;
        int inlDaMax = 16100;                 // 14bit DA 上限（100..16100）
        int inlPoint = 12;                    // 扫描点数（5..40）
        double inlMax = 33554432.0 / 1000.0;  // INL 限值(‰)=MAXINL/1000
        double diffMax = 33554432.0 / 100.0;  // DIV 限值(%)=MAXDIF/100
        // ---- 增益校准（Windows 硬编码，入配置便于测试）----
        int gainFrames = 512;
        int gainMaxSteps = 8;
        int gainDelayMs = 2000;
        double gainThreshold = 0.9;
    };

    void setDepth(int depth) { m_depth = depth > 0 ? depth : 2048; }
    void applyConfig(const Config& c) { m_cfg = c; }
    const Config& config() const { return m_cfg; }

    void reset(int nMCB);
    void appendFrame(int nMCB, const QVector<uint32_t>& channels);

    int frameCount(int nMCB) const;
    // 按值返回：内部缓冲被工作线程并发写，引用会在锁外失效
    QVector<uint32_t> latestFrame(int nMCB) const;

    bool calcZero(int nMCB);                 // 写 m_vz[nMCB]，成功 true
    bool calcNoise(int nMCB);                // 写 m_vn[nMCB]，帧数<200 返回 false
    const QVector<double>& vz(int nMCB) const { return m_vz[nMCB & 7]; }
    const QVector<NoiseChan>& vn(int nMCB) const { return m_vn[nMCB & 7]; }

    QVector<double> rms256(int nMCB) const;  // 逐通道最近 256 帧总体标准差（波形 RMS 用）

    // ---- INL（Phase 3，对照 GetINLData/INL_Cal）----
    uint32_t beginInl(int points);               // 整数除法步进，生成 m_inlVin，返回 step(16bit 码)
    void addInlPoint(int nMCB, int pointIdx);    // y = mean(min(1024,帧))/3355.4432 mV
    uint32_t inlDaCode(int pointIdx) const;      // AOUTDASET 32bit 码 = (基线<<16)|电平
    bool calcInl(int nMCB, int channelsPerDm);   // <5 点返回 false 且不置 bInl
    void clearResults();                         // 保存后清空全部测量结果（对照 OnChgddm）

    const QVector<double>& inlVin() const { return m_inlVin; }
    const QVector<double>& inlA(int nMCB) const { return m_inlA[nMCB & 7]; }
    const QVector<double>& inlB(int nMCB) const { return m_inlB[nMCB & 7]; }
    const QVector<double>& inlR(int nMCB) const { return m_inlR[nMCB & 7]; }
    const QVector<double>& inl(int nMCB) const  { return m_inl[nMCB & 7]; }
    const QVector<double>& inlDiff(int nMCB) const { return m_diff[nMCB & 7]; }
    bool bInl(int nMCB) const    { return m_bInl[nMCB & 7]; }
    bool bVzero(int nMCB) const  { return m_bVzero[nMCB & 7]; }
    bool bVnoise(int nMCB) const { return m_bVnoise[nMCB & 7]; }

    static double toDisplayFormat(double fwhm, int fmt, int adBit);
    static bool passFail(double value, double lo, double hi);   // lo==hi==0 视为未设=合格

    static constexpr int MAX_MCB = 8;

private:
    Config m_cfg;
    int m_depth = 2048;
    mutable QMutex m_mutex;                       // 保护 m_ring/m_latest/m_count
    QVector<QVector<uint32_t>> m_ring[MAX_MCB];   // 每板帧环形缓冲（深 m_depth）
    QVector<uint32_t> m_latest[MAX_MCB];
    int m_count[MAX_MCB]{};
    QVector<double> m_vz[MAX_MCB];
    QVector<NoiseChan> m_vn[MAX_MCB];
    // INL：x 轴全局一份；y/结果按板
    QVector<double> m_inlVin;
    QVector<QVector<double>> m_inlY[MAX_MCB];   // [板][点][通道] y(mV)
    QVector<double> m_inlA[MAX_MCB], m_inlB[MAX_MCB], m_inlR[MAX_MCB];
    QVector<double> m_inl[MAX_MCB], m_diff[MAX_MCB];
    bool m_bInl[MAX_MCB]{}, m_bVzero[MAX_MCB]{}, m_bVnoise[MAX_MCB]{};
    uint32_t m_inlStep = 0;
    int m_inlPoints = 0;

    int idx(int nMCB) const { return nMCB & 7; }
};
