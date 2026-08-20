// AppPaths：config 目录解析（构建树与 dist 包两种布局）。
// - 构建树：exe 与 config 同目录（CMake sync_config / .pro 拷贝）→ exe/config
// - dist 包：bin/HMCB_Qt + 包根 config/ → exe/../config
// 程序原先只认 exe/config——dist 布局下 bin/config 不存在，app.ini 读不到
// 静默落回 mock 缺省（用户改包根 app.ini 的 Mode=real 不生效，2026-08-20）。
#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include "core/AppPaths.h"

class TestAppPaths : public QObject {
    Q_OBJECT
private slots:
    void prefersConfigBesideExe();
    void fallsBackToPackageRoot();
    void defaultsToBesideExe();

private:
    static void touch(const QString& path) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
        f.write("x");
    }
};

// exe/config 存在：优先（构建树布局，保持旧行为）
void TestAppPaths::prefersConfigBesideExe() {
    QTemporaryDir d;
    const QString exe = d.filePath("build");
    touch(exe + "/config/app.ini");
    QCOMPARE(AppPaths::configDirFor(exe), QDir(exe + "/config").canonicalPath());
}

// exe/config 不存在、exe/../config 存在：回退包根（dist 布局）
void TestAppPaths::fallsBackToPackageRoot() {
    QTemporaryDir d;
    const QString exe = d.filePath("pkg/bin");
    touch(d.filePath("pkg/config/app.ini"));
    QCOMPARE(AppPaths::configDirFor(exe), QDir(d.filePath("pkg/config")).canonicalPath());
}

// 都不存在：返回 exe/config（缺省路径，交由调用方报"找不到配置"）
void TestAppPaths::defaultsToBesideExe() {
    QTemporaryDir d;
    const QString exe = d.filePath("anywhere");
    QCOMPARE(AppPaths::configDirFor(exe), QDir(exe + "/config").canonicalPath());
}

QTEST_GUILESS_MAIN(TestAppPaths)
#include "test_apppaths.moc"
