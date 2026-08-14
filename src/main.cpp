#include <QApplication>
#include <QMainWindow>
#include <QLabel>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QMainWindow w;
    w.setWindowTitle("HMCB Qt (build skeleton)");
    auto* lbl = new QLabel("skeleton OK — Qt + libmcbctrl linked");
    lbl->setAlignment(Qt::AlignCenter);
    w.setCentralWidget(lbl);
    w.resize(900, 600);
    w.show();
    return app.exec();
}
