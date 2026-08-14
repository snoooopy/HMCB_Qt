#include "IniFile.h"
#include <QFile>
#include <QTextStream>

void IniFile::rebuildIndex() {
    m_index.clear();
    QString cur;
    for (int i = 0; i < m_lines.size(); ++i) {
        QString line = m_lines[i].trimmed();
        if (line.startsWith('[') && line.endsWith(']'))
            cur = line.mid(1, line.size() - 2).trimmed();
        else if (!cur.isEmpty() && line.contains('=')
                 && !line.startsWith(';') && !line.startsWith('#')) {
            QString item = line.section('=', 0, 0).trimmed();
            if (!m_index.contains(key(cur, item)))
                m_index.insert(key(cur, item), i);
        }
    }
}

bool IniFile::readAll() {
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream s(&f);
    m_lines.clear();
    while (!s.atEnd()) m_lines << s.readLine();
    rebuildIndex();
    return true;
}

bool IniFile::writeAll() {
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return false;
    QTextStream s(&f);
    for (const QString& l : m_lines) s << l << '\n';
    return true;
}

QString IniFile::getString(const QString& sec, const QString& item, const QString& def) const {
    auto it = m_index.constFind(key(sec, item));
    if (it == m_index.constEnd()) return def;
    return m_lines.at(it.value()).section('=', 1).trimmed();
}

int IniFile::getInt(const QString& s, const QString& i, int d) const {
    QString v = getString(s, i, QString());
    bool ok = false;
    int r = v.toInt(&ok);
    return ok ? r : d;
}

double IniFile::getDouble(const QString& s, const QString& i, double d) const {
    QString v = getString(s, i, QString());
    bool ok = false;
    double r = v.toDouble(&ok);
    return ok ? r : d;
}

int IniFile::findSection(const QString& sec) const {
    QString tag = QString("[%1]").arg(sec);
    for (int i = 0; i < m_lines.size(); ++i)
        if (m_lines[i].trimmed() == tag) return i;
    return -1;
}

void IniFile::setString(const QString& sec, const QString& item, const QString& val) {
    QString newline = item + '=' + val;

    auto it = m_index.find(key(sec, item));
    if (it != m_index.end()) {                 // 已存在 -> 原地更新
        m_lines[it.value()] = newline;
        return;
    }

    int secStart = findSection(sec);
    if (secStart < 0) {                         // section 不存在 -> 末尾追加
        if (!m_lines.isEmpty() && !m_lines.last().trimmed().isEmpty())
            m_lines << QString();
        m_lines << QString("[%1]").arg(sec);
        m_lines << newline;
    } else {                                    // section 存在 -> 在其末尾插入（下一个 section 之前）
        int insertAt = m_lines.size();
        for (int i = secStart + 1; i < m_lines.size(); ++i) {
            QString t = m_lines[i].trimmed();
            if (t.startsWith('[') && t.endsWith(']')) { insertAt = i; break; }
        }
        m_lines.insert(insertAt, newline);
    }
    rebuildIndex();   // 插入会改变行号，重建索引
}

void IniFile::setInt(const QString& s, const QString& i, int v) { setString(s, i, QString::number(v)); }
