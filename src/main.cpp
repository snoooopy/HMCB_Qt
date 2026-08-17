#include <QApplication>
#include <QDir>
#include <QTimer>
#include "core/McbTypes.h"      // Q_DECLARE_METATYPE(Mcb::McbFrame)
#include "core/IniFile.h"
#include "core/McbController.h"
#include "ui/MainWindow.h"

int main(int argc, char** argv) {
    // 关闭 AT-SPI 无障碍桥（测试软件不需要；否则关窗时刷
    // qt.accessibility.atspi: Subscription/GetApplicationBusAddress 警告）
    qputenv("NO_AT_BRIDGE", "1");

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("HMCB_Qt"));

    // 跨线程 Queued 传递 Mcb::McbFrame 必须注册 metatype
    qRegisterMetaType<Mcb::McbFrame>("Mcb::McbFrame");

    // 读取 config/app.ini 决定后端模式（mock / real）
    McbController ctrl;
    IniFile ini;
    const QString iniPath = QApplication::applicationDirPath() + QDir::separator()
                            + QStringLiteral("config/app.ini");
    ini.setFileName(iniPath);
    ini.readAll();
    const QString mode = ini.getString(QStringLiteral("BACKEND"),
                                       QStringLiteral("Mode"),
                                       QStringLiteral("mock"));
    ctrl.setBackendMode(mode);
    ctrl.startUp();

    MainWindow w(&ctrl);
    w.show();

    // 启动后自动初始化（mock/real 均经 deviceReady 回调）
    ctrl.initializeBackend();

    // 无头冒烟钩子：HMCB_AUTOSTART=1 时就绪后自动开始连续采集
    if (qEnvironmentVariableIsSet("HMCB_AUTOSTART"))
        QTimer::singleShot(2500, &ctrl, [&ctrl] { ctrl.start(0); });

    return app.exec();
}
