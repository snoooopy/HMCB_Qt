#include "SetIpDialog.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

SetIpDialog::SetIpDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Set new IP address"));   // IDD_DIALOG_SETNEWIP CAPTION

    auto* lay = new QFormLayout(this);
    m_ip      = makeField(); lay->addRow(QStringLiteral("Input new IP Address"), m_ip);
    m_mask    = makeField(); lay->addRow(QStringLiteral("Input new Subnet Mask"), m_mask);
    m_gateway = makeField(); lay->addRow(QStringLiteral("Input new Gateway IP"), m_gateway);

    auto* btns = new QWidget(this);
    auto* row  = new QHBoxLayout(btns);
    row->setContentsMargins(0, 0, 0, 0);
    row->addStretch(1);
    auto* setBtn    = new QPushButton(QStringLiteral("Set"), btns);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), btns);
    row->addWidget(setBtn);
    row->addWidget(cancelBtn);
    lay->addRow(btns);

    // MFC CIPAddressCtrl 控件本身保证合法 IPv4；这里 Set 前逐栏校验等价约束
    connect(setBtn, &QPushButton::clicked, this, [this] {
        const QLineEdit* fields[] = { m_ip, m_mask, m_gateway };
        for (const QLineEdit* e : fields)
            if (!e->hasAcceptableInput()) return;   // 非法输入：不关闭
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QLineEdit* SetIpDialog::makeField() {
    // 全匹配点分四段、每段 0..255（不允许前导 0 的歧义写法按 Windows 控件同样拒绝）
    static const QRegularExpression re(QStringLiteral(
        "^(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)"
        "(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}$"));
    auto* e = new QLineEdit(this);
    e->setValidator(new QRegularExpressionValidator(re, e));
    return e;
}

void SetIpDialog::setAddresses(const QString& ip, const QString& mask, const QString& gateway) {
    m_ip->setText(ip);
    m_mask->setText(mask);
    m_gateway->setText(gateway);
}

QString SetIpDialog::ipAddress() const  { return m_ip->text(); }
QString SetIpDialog::subnetMask() const { return m_mask->text(); }
QString SetIpDialog::gateway() const    { return m_gateway->text(); }
