#pragma once
#include <QDialog>

class QLineEdit;

// 对照 Windows IDD_DIALOG_SETNEWIP / DlgSetNewIPAddress：
// 三栏 IPv4 输入（新 IP / 子网掩码 / 网关）+ Set/Cancel 按钮。
// OK 后由调用方取三段地址拼 12 字节下发 MCB_CMD_SETNEWIP。
class SetIpDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetIpDialog(QWidget* parent = nullptr);

    void setAddresses(const QString& ip, const QString& mask, const QString& gateway);
    QString ipAddress() const;
    QString subnetMask() const;
    QString gateway() const;

private:
    QLineEdit* makeField();
    QLineEdit* m_ip = nullptr;
    QLineEdit* m_mask = nullptr;
    QLineEdit* m_gateway = nullptr;
};
