#pragma once
#include <QString>
#include <QStringList>
#include <QMap>

// 自写 INI 读写，兼容库 CIniFile 格式（[Section] + Item=Value）。
// m_lines 为权威内容；m_index 为行号缓存，修改后重建以避免插入导致的行号漂移。
class IniFile {
public:
    void setFileName(const QString& p) { m_path = p; }
    QString fileName() const { return m_path; }

    bool readAll();
    bool writeAll();

    QString getString(const QString& sec, const QString& item, const QString& def) const;
    int getInt(const QString& sec, const QString& item, int def) const;
    double getDouble(const QString& sec, const QString& item, double def) const;

    void setString(const QString& sec, const QString& item, const QString& val);
    void setInt(const QString& sec, const QString& item, int val);

private:
    QString m_path;
    QStringList m_lines;            // 原始行（含注释/空行）
    QMap<QString, int> m_index;     // "Section\tItem" -> 行号

    static QString key(const QString& s, const QString& i) { return s + '\t' + i; }
    void rebuildIndex();
    int findSection(const QString& sec) const;
};
