#include <QApplication>
#include <QDir>
#include <QTimer>
#include "core/McbTypes.h"      // Q_DECLARE_METATYPE(Mcb::McbFrame)
#include "core/IniFile.h"
#include "core/McbController.h"
#include "core/AppPaths.h"
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
    const QString iniPath = AppPaths::configDir() + QStringLiteral("/app.ini");
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

    // 无头冒烟钩子：HMCB_AUTOSTART=1 时就绪后自动开始连续采集。
    // 必须等 deviceReady 再发——真实设备 INITIAL（FPGA 烧写）约 19s，
    // 固定短延时会在 Initialing 中发 Start 被库拒（0x1111）。
    if (qEnvironmentVariableIsSet("HMCB_AUTOSTART"))
        QObject::connect(&ctrl, &McbController::deviceReady, &ctrl, [&ctrl] {
            QTimer::singleShot(800, &ctrl, [&ctrl] { ctrl.start(0); });
        });

    // 无头截图钩子：HMCB_SNAPSHOT=<path> 时就绪后保存主窗口截图（UI 对齐核对用）；
    // HMCB_SNAPSHOT_DELAY_MS 可调延时（默认 3500；真机 INITIAL 约 19s 时需加大）
    if (const auto snap = qEnvironmentVariable("HMCB_SNAPSHOT"); !snap.isEmpty()) {
        const int delay = qEnvironmentVariableIntValue("HMCB_SNAPSHOT_DELAY_MS");
        QTimer::singleShot(delay > 0 ? delay : 3500, &w, [&w, snap] { w.grab().save(snap); });
    }

    return app.exec();
}
