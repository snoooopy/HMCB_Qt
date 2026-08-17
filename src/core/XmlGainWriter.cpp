#include "XmlGainWriter.h"
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QTextStream>

bool XmlGainWriter::write(const QString& path, int nMCB, const QVector<uint8_t>& dmGains) {
    if (path.isEmpty() || dmGains.isEmpty()) return false;
    QDomDocument doc;
    QFile f(path);
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const bool ok = doc.setContent(&f);
        f.close();
        if (!ok) return false;
    }
    QDomElement root = doc.firstChildElement(QStringLiteral("NucDiasConfigXML"));
    if (root.isNull()) {
        root = doc.createElement(QStringLiteral("NucDiasConfigXML"));
        doc.appendChild(root);
    }
    QDomElement ccm = root.firstChildElement(QStringLiteral("CCM"));
    if (ccm.isNull()) {
        ccm = doc.createElement(QStringLiteral("CCM"));
        root.appendChild(ccm);
    }
    const QString name = QStringLiteral("MCB%1_RANGE").arg(nMCB);
    QDomElement el = ccm.firstChildElement(name);
    if (el.isNull()) {
        el = doc.createElement(name);
        ccm.appendChild(el);
    }
    QString val;
    for (int i = 0; i < dmGains.size(); ++i) {
        if (i) val += QLatin1Char(',');
        val += QString::number(dmGains[i]);
    }
    while (!el.firstChild().isNull()) el.removeChild(el.firstChild());
    el.appendChild(doc.createTextNode(val));

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return false;
    QTextStream out(&f);
    doc.save(out, 2);
    return true;
}
