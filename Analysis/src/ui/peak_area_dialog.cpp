#include "peak_area_dialog.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include <QGroupBox>
#include <QMessageBox>

PeakAreaDialog::PeakAreaDialog(CurveManager* curveManager, QWidget* parent)
    : QDialog(parent)
    , m_curveManager(curveManager)
{
    setWindowTitle(tr("峰面积计算设置"));
    setMinimumWidth(400);

    setupUI();
    loadCurves();
}

void PeakAreaDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ========== 计算曲线选择 ==========
    auto* curveGroup = new QGroupBox(tr("选择计算曲线"), this);
    auto* curveLayout = new QFormLayout(curveGroup);

    m_curveComboBox = new QComboBox(this);
    curveLayout->addRow(tr("计算曲线:"), m_curveComboBox);

    mainLayout->addWidget(curveGroup);

    // ========== 基线类型选择 ==========
    auto* baselineGroup = new QGroupBox(tr("选择基线类型"), this);
    auto* baselineLayout = new QFormLayout(baselineGroup);

    m_baselineTypeComboBox = new QComboBox(this);
    m_baselineTypeComboBox->addItem(tr("直线基线（两端点连线）"), static_cast<int>(BaselineType::Linear));
    m_baselineTypeComboBox->addItem(tr("参考曲线基线"), static_cast<int>(BaselineType::ReferenceCurve));
    baselineLayout->addRow(tr("基线类型:"), m_baselineTypeComboBox);

    // 参考曲线选择（初始隐藏）
    m_referenceCurveLabel = new QLabel(tr("参考曲线:"), this);
    m_referenceCurveComboBox = new QComboBox(this);
    m_referenceCurveLabel->setVisible(false);
    m_referenceCurveComboBox->setVisible(false);
    baselineLayout->addRow(m_referenceCurveLabel, m_referenceCurveComboBox);

    mainLayout->addWidget(baselineGroup);

    // ========== 提示信息 ==========
    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet("QLabel { color: gray; font-size: 11px; }");
    m_hintLabel->setText(tr("提示：选择计算曲线后，在图表上单击创建测量工具。"));
    mainLayout->addWidget(m_hintLabel);

    // ========== 按钮 ==========
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton = new QPushButton(tr("确定"), this);
    m_cancelButton = new QPushButton(tr("取消"), this);

    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    // ========== 信号连接 ==========
    connect(m_baselineTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PeakAreaDialog::onBaselineTypeChanged);
    connect(m_curveComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PeakAreaDialog::onCalculationCurveChanged);
    connect(m_okButton, &QPushButton::clicked, this, &PeakAreaDialog::onAccepted);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    m_okButton->setDefault(true);
}

void PeakAreaDialog::loadCurves()
{
    if (!m_curveManager) {
        return;
    }

    m_curveComboBox->clear();

    // 获取所有曲线（QMap<QString, ThermalCurve>）
    const auto& curvesMap = m_curveManager->getAllCurves();
    if (curvesMap.isEmpty()) {
        m_curveComboBox->addItem(tr("（无可用曲线）"), QString());
        m_okButton->setEnabled(false);
        return;
    }

    // 添加所有曲线到下拉列表
    for (auto it = curvesMap.begin(); it != curvesMap.end(); ++it) {
        const ThermalCurve& curve = it.value();
        QString displayName = curve.name();

        // 显示曲线类型信息
        if (curve.signalType() == SignalType::Derivative) {
            displayName += tr(" [微分]");
        } else if (curve.signalType() == SignalType::Baseline) {
            displayName += tr(" [基线]");
        }

        m_curveComboBox->addItem(displayName, curve.id());
    }

    // 默认选择活动曲线
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (activeCurve) {
        int index = m_curveComboBox->findData(activeCurve->id());
        if (index >= 0) {
            m_curveComboBox->setCurrentIndex(index);
        }
    }

    // 触发一次更新，加载参考曲线列表
    onCalculationCurveChanged(m_curveComboBox->currentIndex());
}

void PeakAreaDialog::loadReferenceCurves(const QString& parentCurveId)
{
    m_referenceCurveComboBox->clear();

    if (!m_curveManager || parentCurveId.isEmpty()) {
        m_referenceCurveComboBox->addItem(tr("（无可用参考曲线）"), QString());
        return;
    }

    // 查找所有子曲线，筛选出 signalType == Baseline 的曲线
    QVector<ThermalCurve*> children = m_curveManager->getChildren(parentCurveId);
    QVector<ThermalCurve*> baselineCurves;

    for (ThermalCurve* child : children) {
        if (child && child->signalType() == SignalType::Baseline) {
            baselineCurves.append(child);
        }
    }

    if (baselineCurves.isEmpty()) {
        m_referenceCurveComboBox->addItem(tr("（无可用基线曲线）"), QString());
        m_hintLabel->setText(tr("提示：当前曲线没有基线子曲线。请先使用\"基线校正\"功能创建基线。"));
        m_hintLabel->setStyleSheet("QLabel { color: orange; font-size: 11px; }");
        return;
    }

    // 添加基线曲线到下拉列表
    for (ThermalCurve* baseline : baselineCurves) {
        m_referenceCurveComboBox->addItem(baseline->name(), baseline->id());
    }

    m_hintLabel->setText(tr("提示：已找到 %1 条基线曲线可供参考。").arg(baselineCurves.size()));
    m_hintLabel->setStyleSheet("QLabel { color: green; font-size: 11px; }");
}

void PeakAreaDialog::onBaselineTypeChanged(int index)
{
    Q_UNUSED(index);

    BaselineType type = static_cast<BaselineType>(
        m_baselineTypeComboBox->currentData().toInt()
    );

    m_baselineType = type;

    // 显示或隐藏参考曲线选择
    bool showReferenceCurve = (type == BaselineType::ReferenceCurve);
    m_referenceCurveLabel->setVisible(showReferenceCurve);
    m_referenceCurveComboBox->setVisible(showReferenceCurve);

    if (showReferenceCurve) {
        // 重新加载参考曲线列表
        QString curveId = m_curveComboBox->currentData().toString();
        loadReferenceCurves(curveId);
    } else {
        // 恢复提示信息
        m_hintLabel->setText(tr("提示：选择计算曲线后，在图表上单击创建测量工具。"));
        m_hintLabel->setStyleSheet("QLabel { color: gray; font-size: 11px; }");
    }
}

void PeakAreaDialog::onCalculationCurveChanged(int index)
{
    Q_UNUSED(index);

    QString curveId = m_curveComboBox->currentData().toString();
    m_selectedCurveId = curveId;

    // 如果当前选择的是参考曲线基线，重新加载参考曲线列表
    if (m_baselineType == BaselineType::ReferenceCurve) {
        loadReferenceCurves(curveId);
    }
}

void PeakAreaDialog::onAccepted()
{
    // 验证选择
    m_selectedCurveId = m_curveComboBox->currentData().toString();
    if (m_selectedCurveId.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请选择一条计算曲线。"));
        return;
    }

    // 如果选择参考曲线基线，验证参考曲线是否有效
    if (m_baselineType == BaselineType::ReferenceCurve) {
        m_referenceCurveId = m_referenceCurveComboBox->currentData().toString();
        if (m_referenceCurveId.isEmpty()) {
            QMessageBox::warning(
                this, tr("错误"),
                tr("当前曲线没有可用的基线子曲线。\n\n"
                   "请先使用\"基线校正\"功能创建基线，或选择\"直线基线\"模式。")
            );
            return;
        }
    } else {
        m_referenceCurveId.clear();
    }

    accept();
}
