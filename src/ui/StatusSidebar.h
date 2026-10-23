#pragma once
#include <QWidget>

class QGroupBox;
class QRadioButton;
class QLabel;

// 右侧板状态栏：8 块 MCB 板的 单选 + READY/NOT READY 标签 + socket/pulse/diff 计数。
// 对照 Windows 版 MCB_TESTView 的右侧状态区。Phase 1 只展示单板（MCB0）通路。
class StatusSidebar : public QWidget {
    Q_OBJECT
public:
    explicit StatusSidebar(QWidget* parent = nullptr);

    // 设置第 i 块板就绪状态（绿色/粉色底）
    void setBoardReady(int i, bool ready);
    // 设置第 i 块板的 socket/pulse 计数与差值
    void setCounts(int i, qlonglong socket, qlonglong pulse);
    // 当前选中的板号
    int selectedBoard() const;

signals:
    // 单选切换（含用户点击与程序 setChecked(true)），参数=新选中的板号
    void boardSelected(int index);

public slots:
    void clearAll();   // 全部置 NOT READY、计数清零

private:
    static constexpr int kBoards = 8;
    struct BoardRow {
        QRadioButton* radio = nullptr;
        QLabel* ready = nullptr;
        QLabel* socket = nullptr;
        QLabel* pulse  = nullptr;
        QLabel* diff   = nullptr;
    };
    BoardRow m_boards[kBoards];
    QGroupBox* m_box = nullptr;
};
