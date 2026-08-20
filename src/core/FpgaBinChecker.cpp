#include "FpgaBinChecker.h"
#include "IniFile.h"
#include <QFile>
#include <QDir>

QStringList FpgaBinChecker::requiredFiles(const QString& configDir) {
    IniFile ini;
    ini.setFileName(QDir(configDir).filePath(QStringLiteral("MCBCtrl.ini")));
    ini.readAll();   // 无 ini 时 getString/getInt 走缺省——与库 CIniFile 行为一致
                     // （仍按 MCBNUM=1 + ddc232.bin 检查）

    const int nMCB = ini.getInt(QStringLiteral("MCB"), QStringLiteral("MCBNUM"), 1);
    const QString defName = ini.getString(QStringLiteral("ADM"),
                                          QStringLiteral("BINFILE"),
                                          QStringLiteral("ddc232.bin"));
    QStringList names;
    for (int i = 0; i < nMCB && i < 8; ++i)   // MAX_MCBNUM=8
        names << ini.getString(QStringLiteral("ADM"),
                               QStringLiteral("MCB%1BINFILE").arg(i),
                               defName);
    names.removeAll(QString());
    names.removeDuplicates();
    return names;
}

QStringList FpgaBinChecker::missingFiles(const QString& configDir) {
    QStringList miss;
    for (const QString& name : requiredFiles(configDir))
        if (!QFile::exists(QDir(configDir).filePath(name)))
            miss << name;
    return miss;
}
