#include "StatusSidebar.h"
#include <QGroupBox>
#include <QRadioButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>

StatusSidebar::StatusSidebar(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_box = new QGroupBox(QStringLiteral("MCB STATUS"), this);
    auto* grid = new QGridLayout(m_box);
    grid->setSpacing(4);

    // 表头
    grid->addWidget(new QLabel(QStringLiteral("BOARD"), m_box), 0, 0);
    grid->addWidget(new QLabel(QStringLiteral("STATUS"), m_box), 0, 1);
    grid->addWidget(new QLabel(QStringLiteral("SOCKET"), m_box), 0, 2);
    grid->addWidget(new QLabel(QStringLiteral("PULSE"), m_box), 0, 3);
    grid->addWidget(new QLabel(QStringLiteral("DIFF"), m_box), 0, 4);

    for (int i = 0; i < kBoards; ++i) {
        auto& r = m_boards[i];
        r.radio = new QRadioButton(QString::number(i), m_box);
        r.radio->setAutoExclusive(true);
        r.ready = new QLabel(QStringLiteral("NOT READY"), m_box);
        r.ready->setAlignment(Qt::AlignCenter);
        r.socket = new QLabel(QStringLiteral("0"), m_box);
        r.pulse  = new QLabel(QStringLiteral("0"), m_box);
        r.diff   = new QLabel(QStringLiteral("0"), m_box);
        grid->addWidget(r.radio,  i + 1, 0);
        grid->addWidget(r.ready,  i + 1, 1);
        grid->addWidget(r.socket, i + 1, 2);
        grid->addWidget(r.pulse,  i + 1, 3);
        grid->addWidget(r.diff,   i + 1, 4);
        setBoardReady(i, false);
        // 单选切换 → boardSelected（含下方默认 MCB0 的 setChecked(true) 那次）
        connect(r.radio, &QRadioButton::toggled, this, [this, i](bool on) {
            if (on) emit boardSelected(i);
        });
    }
    m_boards[0].radio->setChecked(true);   // 默认选中 MCB0

    outer->addWidget(m_box);
    outer->addStretch(1);
}

void StatusSidebar::setBoardReady(int i, bool ready) {
    if (i < 0 || i >= kBoards) return;
    auto* lbl = m_boards[i].ready;
    lbl->setText(ready ? QStringLiteral("READY") : QStringLiteral("NOT READY"));
    // 绿底=就绪，粉底=未就绪
    lbl->setStyleSheet(ready
        ? QStringLiteral("background:#3ccb3c;color:white;border-radius:3px;")
        : QStringLiteral("background:#f0a0a0;color:black;border-radius:3px;"));
}

void StatusSidebar::setCounts(int i, qlonglong socket, qlonglong pulse) {
    if (i < 0 || i >= kBoards) return;
    m_boards[i].socket->setText(QString::number(socket));
    m_boards[i].pulse->setText(QString::number(pulse));
    m_boards[i].diff->setText(QString::number(socket - pulse));
}

int StatusSidebar::selectedBoard() const {
    for (int i = 0; i < kBoards; ++i)
        if (m_boards[i].radio->isChecked()) return i;
    return 0;
}

void StatusSidebar::clearAll() {
    for (int i = 0; i < kBoards; ++i) {
        setBoardReady(i, false);
        setCounts(i, 0, 0);
    }
}
