#include "FndmParaDialog.h"
#include "core/McbController.h"
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QTimer>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include "core/IniFile.h"
#include <QCoreApplication>

using FndmParaBuilder::Ch;
using FndmParaBuilder::LastPara;

FndmParaDialog::FndmParaDialog(McbController* ctrl, LastPara* lastPara, QWidget* parent)
    : QDialog(parent), m_ctrl(ctrl), m_last(lastPara)
{
    setWindowTitle(QStringLiteral("FNDM Parameter Set && Scan Neutron Spectrum"));
    // ---- 左列：参数设置（IDD_DLG_FNDM 左半）----
    auto* paraBox = new QGroupBox(QStringLiteral("Parameter"));
    auto* form = new QFormLayout(paraBox);
    m_module    = new QSpinBox;  m_module->setRange(0, 63);
    m_row       = new QSpinBox;  m_row->setRange(0, FndmParaBuilder::kRow - 1);
    m_column    = new QSpinBox;  m_column->setRange(0, FndmParaBuilder::kColumn - 1);
    m_valBias   = new QSpinBox;  m_valBias->setRange(0, FndmParaBuilder::kDacMax);
    m_valThr    = new QSpinBox;  m_valThr->setRange(0, FndmParaBuilder::kDacMax);
    m_low = new QRadioButton(QStringLiteral("Low"));
    m_up  = new QRadioButton(QStringLiteral("Up"));
    m_bias = new QCheckBox(QStringLiteral("Bias"));
    m_thr  = new QCheckBox(QStringLiteral("Threshold"));
    m_stepB   = new QSpinBox;   m_stepB->setRange(0, FndmParaBuilder::kDacMax);
    m_stepThr = new QSpinBox;   m_stepThr->setRange(0, FndmParaBuilder::kDacMax);
    m_allChn = new QCheckBox(QStringLiteral("All Channel"));
    form->addRow(QStringLiteral("Module"), m_module);
    form->addRow(QStringLiteral("Row"), m_row);
    form->addRow(QStringLiteral("Column"), m_column);
    form->addRow(QStringLiteral("Value(Bias)"), m_valBias);
    form->addRow(QStringLiteral("Value(Threshold)"), m_valThr);
    auto* lu = new QWidget; auto* lul = new QHBoxLayout(lu); lul->setContentsMargins(0,0,0,0);
    lul->addWidget(m_low); lul->addWidget(m_up);
    form->addRow(QString(), lu);
    auto* bt = new QWidget; auto* btl = new QHBoxLayout(bt); btl->setContentsMargins(0,0,0,0);
    btl->addWidget(m_bias); btl->addWidget(m_thr);
    form->addRow(QString(), bt);
    form->addRow(QStringLiteral("Step(Bias)"), m_stepB);
    form->addRow(QStringLiteral("Step(Threshold)"), m_stepThr);
    form->addRow(QString(), m_allChn);
    m_allModule = new QCheckBox(QStringLiteral("All Module"));
    form->addRow(QString(), m_allModule);
    m_apply = new QPushButton(QStringLiteral("Apply"));
    m_add   = new QPushButton(QStringLiteral("Add"));
    m_sub   = new QPushButton(QStringLiteral("Sub"));
    auto* btns = new QWidget; auto* bl = new QHBoxLayout(btns); bl->setContentsMargins(0,0,0,0);
    bl->addWidget(m_apply); bl->addWidget(m_add); bl->addWidget(m_sub);
    form->addRow(QString(), btns);

    // ---- 右列：谱扫描 ----
    auto* scanBox = new QGroupBox(QStringLiteral("Scan Neutron Spectrum"));
    auto* sf = new QFormLayout(scanBox);
    m_scan = new QCheckBox(QStringLiteral("Scan"));
    m_valScan     = new QSpinBox; m_valScan->setRange(0, FndmParaBuilder::kDacMax);
    m_valScan2    = new QSpinBox; m_valScan2->setRange(0, FndmParaBuilder::kDacMax);
    m_valBin      = new QSpinBox; m_valBin->setRange(FndmParaBuilder::kMinBin, FndmParaBuilder::kDacMax);
    m_sampleTimes = new QSpinBox; m_sampleTimes->setRange(1, 20000);
    m_valScan->setValue(50); m_valScan2->setValue(4095);
    m_valBin->setValue(100); m_sampleTimes->setValue(100);
    sf->addRow(QString(), m_scan);
    sf->addRow(QStringLiteral("Threshold Start"), m_valScan);
    sf->addRow(QStringLiteral("Threshold Stop"), m_valScan2);
    sf->addRow(QStringLiteral("Threshold Bin"), m_valBin);
    sf->addRow(QStringLiteral("Sample Times"), m_sampleTimes);
    m_scanStart = new QPushButton(QStringLiteral("Scan Start"));
    m_scanAbort = new QPushButton(QStringLiteral("Scan Abort"));
    auto* sb = new QWidget; auto* sbl = new QHBoxLayout(sb); sbl->setContentsMargins(0,0,0,0);
    sbl->addWidget(m_scanStart); sbl->addWidget(m_scanAbort);
    sf->addRow(QString(), sb);

    auto* grid = new QGridLayout(this);
    grid->addWidget(paraBox, 0, 0);
    grid->addWidget(scanBox, 0, 1);

    // 初值：lastPara 记忆（对照 OnInitDialog）
    const DMINFO d = m_ctrl->getDmInfo(0);
    m_module->setMaximum(qMax(0, int(d.nDMNum) - 1));
    m_module->setValue(int(m_last->module));
    m_row->setValue(int(m_last->row));
    m_column->setValue(int(m_last->column));
    m_valBias->setValue(m_last->nValBias);
    m_valThr->setValue(m_last->lowup == 0 ? m_last->nValThrL : m_last->nValThrU);
    (m_last->lowup == 0 ? m_low : m_up)->setChecked(true);
    m_bias->setChecked(m_last->bBias);
    m_thr->setChecked(m_last->bThr);
    m_allChn->setChecked(m_last->bAllChn);
    m_allModule->setChecked(m_last->bAllModule);

    connect(m_apply, &QPushButton::clicked, this, &FndmParaDialog::onApply);
    connect(m_add,   &QPushButton::clicked, this, &FndmParaDialog::onAdd);
    connect(m_sub,   &QPushButton::clicked, this, &FndmParaDialog::onSub);
    connect(m_scanStart, &QPushButton::clicked, this, &FndmParaDialog::onScanStart);
    connect(m_scanAbort, &QPushButton::clicked, this, &FndmParaDialog::onScanAbort);
    // 字段切换回读（对照 OnEnKillfocus*/OnBnClickedRadio*）
    for (auto* sb2 : {m_module, m_row, m_column})
        connect(sb2, &QSpinBox::editingFinished, this, [this]{ updateVal(); });
    auto onRadio = [this]{
        syncFromUi();
        updateVal(false, true);          // 换 Low/Up 刷阈值显示
    };
    connect(m_low, &QRadioButton::toggled, this, onRadio);
    for (auto* cb : {m_bias, m_thr, m_allChn, m_allModule, m_scan})
        connect(cb, &QCheckBox::toggled, this, [this]{ updateControl(); });

    m_poll = new QTimer(this);
    m_poll->setInterval(500);            // Windows SetTimer(1, 500)
    connect(m_poll, &QTimer::timeout, this, &FndmParaDialog::onPollScan);
    updateControl();
}

QVector<Ch> FndmParaDialog::collect(int sign) const {
    if (sign == 0)                       // Apply：绝对值
        return FndmParaBuilder::buildApply(
            m_bias->isChecked(), m_thr->isChecked(), m_allModule->isChecked(),
            m_allChn->isChecked(), uint32_t(m_module->value()), uint32_t(m_row->value()),
            uint32_t(m_column->value()), m_low->isChecked() ? 0 : 1,
            m_valBias->value(), m_valThr->value(), int(m_ctrl->getDmInfo(0).nDMNum));
    return FndmParaBuilder::buildAdjust( // Add/Sub：相对 ±step
        m_bias->isChecked(), m_thr->isChecked(), m_allChn->isChecked(),
        uint32_t(m_module->value()), uint32_t(m_row->value()), uint32_t(m_column->value()),
        m_low->isChecked() ? 0 : 1, m_stepB->value(), m_stepThr->value(), sign);
}

void FndmParaDialog::syncFromUi() const {
    m_last->module = uint32_t(m_module->value());
    m_last->row = uint32_t(m_row->value());
    m_last->column = uint32_t(m_column->value());
    m_last->lowup = m_low->isChecked() ? 0 : 1;
    m_last->bBias = m_bias->isChecked();
    m_last->bThr = m_thr->isChecked();
    m_last->bAllChn = m_allChn->isChecked();
    m_last->bAllModule = m_allModule->isChecked();
    m_last->nValBias = m_valBias->value();
    if (m_last->lowup == 0) m_last->nValThrL = m_valThr->value();
    else                    m_last->nValThrU = m_valThr->value();
}

void FndmParaDialog::updateVal(bool upBias, bool upThr) {
    // 对照 UpdateVal：lastPara ← UI；GETFNDMPARA 回读提取当前值（Linux 库
    // 不回数据 → 提取不命中，保持显示值）
    syncFromUi();
    const auto cur = m_ctrl->getFndmPara();
    if (!cur.isEmpty())
        FndmParaBuilder::extractCurrent(cur, m_last->module, m_last->row, m_last->column,
                                        m_last->nValBias, m_last->nValThrL, m_last->nValThrU);
    if (upBias) m_valBias->setValue(m_last->nValBias);
    if (upThr)  m_valThr->setValue(m_last->lowup == 0 ? m_last->nValThrL : m_last->nValThrU);
}

void FndmParaDialog::updateControl() {
    // 对照 UpdateContrl：Scan 选中锁定参数区，反之锁定扫描区
    const bool scan = m_scan->isChecked();
    m_scan->setEnabled(!m_scanning);
    if (scan) {
        for (auto* w : QList<QWidget*>{m_valBias, m_valThr, m_stepB, m_stepThr, m_allChn,
             m_allModule, m_row, m_column, m_apply, m_add, m_sub, m_bias, m_thr, m_module})
            w->setEnabled(false);
        m_low->setEnabled(false); m_up->setEnabled(false);
        for (auto* w : QList<QWidget*>{m_valBin, m_valScan, m_valScan2, m_sampleTimes})
            w->setEnabled(!m_scanning);
        m_scanStart->setEnabled(!m_scanning);
        m_scanAbort->setEnabled(m_scanning);
    } else {
        m_valBias->setEnabled(m_bias->isChecked());
        m_valThr->setEnabled(m_thr->isChecked());
        m_stepB->setEnabled(m_bias->isChecked());
        m_stepThr->setEnabled(m_thr->isChecked());
        m_allChn->setEnabled(m_thr->isChecked());
        m_allModule->setEnabled(true);
        m_row->setEnabled(m_thr->isChecked() && !m_allChn->isChecked());
        m_column->setEnabled(m_thr->isChecked() && !m_allChn->isChecked());
        m_apply->setEnabled(m_bias->isChecked() || m_thr->isChecked());
        m_add->setEnabled(m_bias->isChecked() || m_thr->isChecked());
        m_sub->setEnabled(m_bias->isChecked() || m_thr->isChecked());
        m_bias->setEnabled(true); m_thr->setEnabled(true);
        m_module->setEnabled(!m_allModule->isChecked());
        m_low->setEnabled(true); m_up->setEnabled(true);
        for (auto* w : QList<QWidget*>{m_valBin, m_valScan, m_valScan2, m_sampleTimes,
             m_scanStart, m_scanAbort})
            w->setEnabled(false);
    }
}

void FndmParaDialog::onApply() { m_ctrl->setFndmPara(collect(0)); updateVal(); }
void FndmParaDialog::onAdd()   { m_ctrl->setFndmPara(collect(+1)); updateVal(); }
void FndmParaDialog::onSub()   { m_ctrl->setFndmPara(collect(-1)); updateVal(); }

void FndmParaDialog::setScanThr() {
    // 对照 SetScanThr：每 DM×48 通道×上下阈值对（upper=lower+bin）
    m_ctrl->setFndmPara(FndmParaBuilder::buildScanThr(
        int(m_ctrl->getDmInfo(0).nDMNum), m_valThr->value(), m_valBin->value()));
    m_last->nValThrL = m_valThr->value();
    updateVal();
}

void FndmParaDialog::onScanStart() {
    // 对照 OnBnClickedScanStart：清谱、强制参数、提示、起步采集
    const int nBin = (m_valScan2->value() - m_valScan->value()) / m_valBin->value();
    if (nBin <= 0) return;
    m_spec = FndmScanWriter::Spec{};
    m_spec.valScan = m_valScan->value();
    m_spec.valScan2 = m_valScan2->value();
    m_spec.valBin = m_valBin->value();
    m_spec.sampleTimes = m_sampleTimes->value();
    // POTMT 读 MCBCtrl.ini [MODE0]（Windows m_nPOTMTtime，缺省 4000）
    {
        IniFile ini;
        ini.setFileName(QCoreApplication::applicationDirPath()
                        + QStringLiteral("/config/MCBCtrl.ini"));
        if (ini.readAll())
            m_spec.potmtMs = qMax(1, ini.getInt(QStringLiteral("MODE0"),
                                               QStringLiteral("POTMT"), 4000));
    }
    m_spec.scanData.resize(int(m_ctrl->getDmInfo(0).nAllChnNum));
    for (auto& ch : m_spec.scanData) ch.resize(nBin + 1);

    m_bias->setChecked(false);
    m_thr->setChecked(true);
    m_allChn->setChecked(true);
    m_low->setChecked(true);
    m_module->setValue(0);
    m_row->setValue(0);
    m_column->setValue(0);
    m_valThr->setValue(m_valScan->value());

    setScanThr();
    m_scanSn = 0;
    m_scantimes = 0;
    QMessageBox::information(this, QStringLiteral("Information"),
        QStringLiteral("Scanning Background ...\nBe sure that the neutron tube is off!"));
    m_scanning = true;
    updateControl();
    m_scanFrom = m_ctrl->engine() ? m_ctrl->engine()->frameCount(0) : 0;
    m_poll->start();
    m_ctrl->start(uint32_t(m_sampleTimes->value()));   // fn=5 有限采集
}

void FndmParaDialog::onPollScan() {
    if (!m_scanning || m_ctrl->getRunStatus()) return;   // 采集中：等
    getScanData(m_scantimes, m_scanSn);
    ++m_scantimes;
    if (m_valThr->value() + 2 * m_valBin->value() > m_valScan2->value()) {
        if (m_scanSn == 0) {                    // 背景段完成 → 谱段
            m_scanning = false;
            m_valThr->setValue(m_valScan->value());
            m_scanSn = 1;
            m_scantimes = 0;
            QMessageBox::information(this, QStringLiteral("Information"),
                QStringLiteral("Scanning Neutron Spectrum ...\nBe sure that the neutron tube is on!"));
            m_scanning = true;
            setScanThr();
            updateControl();
            m_scanFrom = m_ctrl->engine() ? m_ctrl->engine()->frameCount(0) : 0;
            m_ctrl->start(uint32_t(m_sampleTimes->value()));
        } else {                                 // 谱段完成：存档收尾
            m_scanning = false;
            m_poll->stop();
            updateControl();
            saveScanSpec();
        }
    } else {                                     // 下一 bin
        m_valThr->setValue(m_valThr->value() + m_valBin->value());
        setScanThr();
        updateControl();
        m_scanFrom = m_ctrl->engine() ? m_ctrl->engine()->frameCount(0) : 0;
        m_ctrl->start(uint32_t(m_sampleTimes->value()));
    }
}

void FndmParaDialog::getScanData(int scantimes, int sn) {
    // 对照 GetScanData：本轮采集帧逐通道累加（取 scanFrom 之后的增量帧）
    if (!m_ctrl->engine()) return;
    const auto ring = m_ctrl->engine()->ringSnapshot(0);
    const int nChn = m_spec.scanData.size();
    const int from = qBound(0, m_scanFrom, ring.size());
    for (int f = from; f < ring.size(); ++f)
        for (int k = 0; k < nChn && k < ring[f].size(); ++k)
            m_spec.scanData[k][scantimes][sn] += ring[f][k];
}

void FndmParaDialog::onScanAbort() {
    m_scanning = false;
    m_poll->stop();
    updateControl();
    if (QMessageBox::question(this, QStringLiteral("Question"),
            QStringLiteral("Save Spectrum File?")) == QMessageBox::Yes)
        saveScanSpec();
}

void FndmParaDialog::saveScanSpec() {
    // 对照 SaveScanSpec：txt（默认名 neutron_spectra_integrated）+ 同名 .csv
    const QString txt = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Spectrum File"),
        QStringLiteral("neutron_spectra_integrated.txt"),
        QStringLiteral("text file(*.txt)"));
    if (txt.isEmpty()) return;
    QString err;
    if (!FndmScanWriter::writeTxt(txt, m_spec, &err)) {
        QMessageBox::warning(this, QStringLiteral("Error"), err);
        return;
    }
    FndmScanWriter::writeCsv(txt + QStringLiteral(".csv"), m_spec);
}

void FndmParaDialog::reject() {
    // 对照 OnCancel：扫描中确认；发 STOP 收尾
    if (m_scanning) {
        if (QMessageBox::warning(this, QStringLiteral("Warning"),
                QStringLiteral("Scanning Neutron Spectrum...\nExit?"),
                QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
            return;
        m_poll->stop();
    }
    m_ctrl->stop();
    QDialog::reject();
}
