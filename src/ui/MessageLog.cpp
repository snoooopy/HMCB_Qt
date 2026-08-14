#include "MessageLog.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QTime>

MessageLog::MessageLog(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget(this);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setAlternatingRowColors(true);
    m_list->setUniformItemSizes(true);
    lay->addWidget(m_list);
}

void MessageLog::appendMessage(const QString& text, int nMCB) {
    const QString stamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1][MCB%2] %3").arg(stamp).arg(nMCB).arg(text);
    m_list->addItem(line);
    // 超出上限则从头丢弃
    while (m_list->count() > kMaxEntries)
        delete m_list->item(0);
    m_list->scrollToBottom();
}

void MessageLog::clear() {
    m_list->clear();
}
