#pragma once
#include <QDialog>
#include <QList>
#include <QVector>
#include <cstdint>

// 增益自动校准结果展示（对照 CDlgGainCalibResultShow：纯展示，OK 关闭无后续）。
class QTableWidget;
class GainCalibResultDialog : public QDialog {
    Q_OBJECT
public:
    explicit GainCalibResultDialog(const QVector<int>& dmCounts,
                                   const QList<QVector<uint8_t>>& gains,
                                   QWidget* parent = nullptr);
};
