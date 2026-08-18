#pragma once
#include <QWidget>

class QListWidget;

// 右栏消息日志（对照 Windows IDD_MCB_TEST_FORM 的 "MESSAGE:"+列表）：
// 带时间戳、归属板号的只读列表，限制最大条数防止内存膨胀。
class MessageLog : public QWidget {
    Q_OBJECT
public:
    explicit MessageLog(QWidget* parent = nullptr);

public slots:
    // 追加一条：前缀 [HH:mm:ss.zzz][MCBn] text
    void appendMessage(const QString& text, int nMCB);
    void clear();

private:
    QListWidget* m_list = nullptr;
    static constexpr int kMaxEntries = 2000;
};
