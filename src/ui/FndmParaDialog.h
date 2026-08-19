#pragma once
#include <QDialog>
#include "core/FndmParaBuilder.h"
#include "core/FndmScanWriter.h"

class QSpinBox;
class QCheckBox;
class QRadioButton;
class QPushButton;
class QTimer;
class McbController;

// FNDM 参数设置 + 快中子谱扫描（对照 Windows CDlgFNDMParaSet / IDD_DLG_FNDM）。
// lastPara 由 MainWindow 持有（Windows 为 View 成员 m_FndLastPara），跨打开记忆。
// 扫描 = 背景(sn=0)与谱(sn=1)两阶段：每 bin 发上下阈值对 + 有限帧采集，
// 轮询采集停后累加数据推进阈值（对照 OnTimer/GetScanData）。
class FndmParaDialog : public QDialog {
    Q_OBJECT
public:
    FndmParaDialog(McbController* ctrl, FndmParaBuilder::LastPara* lastPara,
                   QWidget* parent = nullptr);

protected:
    void reject() override;    // 对照 OnCancel：扫描中确认 + STOP

private slots:
    void onApply();
    void onAdd();
    void onSub();
    void onScanStart();
    void onScanAbort();
    void onPollScan();         // 500ms 轮询（对照 OnTimer）

private:
    void updateVal(bool upBias = true, bool upThr = true);   // 对照 UpdateVal
    void updateControl();                                    // 对照 UpdateContrl
    void setScanThr();                                       // 对照 SetScanThr
    void getScanData(int scantimes, int sn);                 // 对照 GetScanData
    void saveScanSpec();                                     // 对照 SaveScanSpec
    QVector<FndmParaBuilder::Ch> collect(int sign) const;    // Apply(+0)/Add(+1)/Sub(-1) 组包
    void syncFromUi() const;                                 // UI → lastPara

    McbController* m_ctrl;
    FndmParaBuilder::LastPara* m_last;

    QSpinBox   *m_module = nullptr, *m_row = nullptr, *m_column = nullptr;
    QSpinBox   *m_valBias = nullptr, *m_valThr = nullptr;
    QRadioButton *m_low = nullptr, *m_up = nullptr;
    QCheckBox  *m_bias = nullptr, *m_thr = nullptr, *m_allChn = nullptr,
               *m_allModule = nullptr, *m_scan = nullptr;
    QSpinBox   *m_stepB = nullptr, *m_stepThr = nullptr;
    QSpinBox   *m_valScan = nullptr, *m_valScan2 = nullptr,
               *m_valBin = nullptr, *m_sampleTimes = nullptr;
    QPushButton *m_apply = nullptr, *m_add = nullptr, *m_sub = nullptr,
                *m_scanStart = nullptr, *m_scanAbort = nullptr;
    QTimer* m_poll = nullptr;

    // 扫描状态（对照 m_bScanStart/m_nScanSn/m_nScantimes）
    bool m_scanning = false;
    int  m_scanSn = 0;          // 0-背景 1-谱
    int  m_scantimes = 0;       // 已完成 bin 数
    int  m_scanFrom = 0;        // 本 bin 采集起点帧序（取增量帧）
    FndmScanWriter::Spec m_spec;
};
