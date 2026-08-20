#include "AppPaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

QString AppPaths::configDir() {
    return configDirFor(QCoreApplication::applicationDirPath());
}

QString AppPaths::configDirFor(const QString& exeDir) {
    // 注：不能用 "exe/../config" 直接 exists()——中间目录不存在时内核按路径
    // 逐级解析会 ENOENT；QFileInfo::absolutePath() 是纯字符串取父目录
    const QDir beside(exeDir + QStringLiteral("/config"));
    if (beside.exists())
        return beside.canonicalPath();          // 构建树布局（旧行为优先）
    const QDir pkgRoot(QFileInfo(exeDir).absolutePath() + QStringLiteral("/config"));
    if (pkgRoot.exists())
        return pkgRoot.canonicalPath();         // dist 包布局（bin 同级 config）
    return beside.canonicalPath();              // 都不在：缺省路径（由调用方报缺）
}
