#ifndef PEAK_AREA_DIALOG_H
#define PEAK_AREA_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QString>

class CurveManager;
class ThermalCurve;

/**
 * @brief 峰面积计算参数对话框
 *
 * 让用户选择：
 * 1. 计算曲线（用哪条曲线计算峰面积）
 * 2. 基线类型（直线基线 或 参考曲线基线）
 * 3. 参考曲线（如果选择参考基线，从子曲线中选择）
 */
class PeakAreaDialog : public QDialog
{
    Q_OBJECT

public:
    enum class BaselineType {
        Linear,         // 直线基线（两端点连线）
        ReferenceCurve  // 参考曲线基线
    };

    explicit PeakAreaDialog(CurveManager* curveManager, QWidget* parent = nullptr);

    // 获取用户选择的结果
    QString selectedCurveId() const { return m_selectedCurveId; }
    BaselineType baselineType() const { return m_baselineType; }
    QString referenceCurveId() const { return m_referenceCurveId; }

private slots:
    void onBaselineTypeChanged(int index);
    void onCalculationCurveChanged(int index);
    void onAccepted();

private:
    void setupUI();
    void loadCurves();
    void loadReferenceCurves(const QString& parentCurveId);

    CurveManager* m_curveManager;

    // UI 组件
    QComboBox* m_curveComboBox;
    QComboBox* m_baselineTypeComboBox;
    QComboBox* m_referenceCurveComboBox;
    QLabel* m_referenceCurveLabel;
    QLabel* m_hintLabel;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

    // 用户选择的结果
    QString m_selectedCurveId;
    BaselineType m_baselineType = BaselineType::Linear;
    QString m_referenceCurveId;
};

#endif // PEAK_AREA_DIALOG_H
