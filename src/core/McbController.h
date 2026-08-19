#pragma once
#include <QObject>
#include <memory>
#include <cstdint>
#include "core/IMcbBackend.h"   // 也带来 McbTypes.h / MCBCtrl.h
#include "core/MeasurementEngine.h"

// McbController：UI 与后端之间的 Qt 粘合层（QObject）。
//
// 职责：
//   1. 按 app.ini [BACKEND] Mode 选择 Real / Mock 后端并持有。
//   2. 注册后端回调；回调在库/仿真工作线程触发，这里直接 emit Qt 信号，
//      由于本对象 affinity 为 GUI 线程，带 context 的连接会自动走 Queued，
//      把工作线程的数据安全地转发到 GUI 线程（Mcb::McbFrame 需注册 metatype）。
//   3. 暴露给 UI 的类型化命令槽（initialize/start/stop/setTrigger …）。
class McbController : public QObject {
    Q_OBJECT
public:
    explicit McbController(QObject* parent = nullptr);

    void setBackendMode(const QString& mode);   // "mock" / "real"
    void startUp();      // 构造后端、注册回调（须在 GUI 线程调用）
    void shutdown();     // 关闭后端、释放资源

public slots:
    void initializeBackend();
    void start(uint32_t sampleCount = 0);
    void stop();
    void setTrigger(int mode);
    void setRangeMode(int range);
    void setTiming(uint32_t potmt, uint32_t potmw, uint32_t intt);
    void setTestMode(bool on);        // TEST 模式（看内置测试图案）
    void setSendAllData(bool on);     // 发送全部数据模式
    void measure(int function, uint32_t sampleCount);   // 0 Zero 1 Noise 3 NoiseD
    void measureInl();     // function=2：INL 多点 DA 扫描
    void measureGain();    // function=6：增益自动校准（迭代升档 + 写回）
    bool setNewIp(const QString& ip, const QString& mask, const QString& gateway);  // Tools→Set New IP

    MeasurementEngine* engine() { return m_engine.get(); }

    void setGainWritePaths(const QString& iniPath, const QString& xmlPath);  // 测试注入临时目录
    QVector<uint8_t> dmGainValues(int nMCB) const;   // 该板各 DM 档位（去板号字节）

    // 同步查询
    DMINFO getDmInfo(int nMCB);
    uint32_t getMcbNum();
    bool getRunStatus();

signals:
    // 状态文本 + 归属板（用于消息日志/状态栏）
    void statusMessage(QString text, int nMCB);
    // 设备就绪（收到 MCB_DVREADY）
    void deviceReady();
    // 采集启停状态翻转
    void runStateChanged(bool running);
    // 一帧数据到达（nMCB, frame）
    void frameReady(int nMCB, Mcb::McbFrame frame);
    // 在线板数量变化（取数时刷新）
    void boardCountChanged(int count);
    // 测量完成（STOPOK 后算完）：function 0 Zero 1 Noise 2 INL 3 NoiseD
    void measurementReady(int function);
    // 增益自动校准收敛并写回完成
    void gainCalibReady();

private:
    std::unique_ptr<IMcbBackend> m_be;
    QString m_mode = QStringLiteral("mock");
    std::unique_ptr<MeasurementEngine> m_engine;
    int m_measFn = -1;
    bool m_running = false;
    void handleStopok(bool natural);   // STOPOK：natural=采满自停，命令停=用户收尾
    void handleInlStop(bool natural);
    void finalizeInl();
    void handleGainStop(bool natural);
    void finalizeGain();
    void applyDmRange(int nMCB);
    void scheduleGainAcquire();

    int m_inlTestPoint = 0;
    int m_chPerDm = 128;
    int m_gainStep = 0;
    QVector<uint8_t> m_dmGainBoard[MeasurementEngine::MAX_MCB];   // 65 字节/板：[0]=板号，[1+j]=DM j 档
    QString m_gainIniPath, m_gainXmlPath;
};
