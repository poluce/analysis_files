#ifndef INTERACTIONCONTROLLER_H
#define INTERACTIONCONTROLLER_H

#include <QObject>
#include <QPointF>
#include <QVector>
#include <QString>

// 前置声明
class PlotWidget;
class ThermalCurve;

/**
 * @brief InteractionController 管理用户与图表的交互
 *
 * 职责：
 * - 管理交互模式（点拾取、多点选择、双曲线选择等）
 * - 收集用户输入（点击位置、选择的曲线等）
 * - 处理临时高亮和视觉反馈
 * - 发出信号通知 MainController 交互完成
 *
 * 设计理念：
 * - 解耦算法执行和用户输入
 * - PlotWidget 只负责绘图和坐标转换
 * - InteractionController 负责交互状态管理
 * - MainController 负责协调算法执行流程
 */
class InteractionController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 交互模式枚举
     *
     * 定义不同的用户交互类型。
     */
    enum class Mode {
        None,                   // 无交互（正常浏览模式）
        PointSelection,         // 点拾取模式（选择 N 个点）
        MultiPointSelection,    // 多点选择模式（选择多组点）
        DualCurveSelection      // 双曲线选择模式（选择主曲线和参考曲线）
    };

    explicit InteractionController(PlotWidget* plotWidget, QObject* parent = nullptr);
    ~InteractionController();

    /**
     * @brief 启动点拾取模式
     *
     * @param numPoints 需要拾取的点数量
     * @param prompt 提示信息（显示给用户）
     */
    void startPointPicking(int numPoints, const QString& prompt);

    /**
     * @brief 启动多点选择模式
     *
     * @param prompt 提示信息
     */
    void startMultiPointSelection(const QString& prompt);

    /**
     * @brief 启动双曲线选择模式
     *
     * @param mainCurve 主曲线
     * @param prompt 提示信息
     */
    void startDualCurveSelection(ThermalCurve* mainCurve, const QString& prompt);

    /**
     * @brief 取消当前交互
     */
    void cancelInteraction();

    /**
     * @brief 获取当前交互模式
     */
    Mode currentMode() const { return m_currentMode; }

    /**
     * @brief 获取当前提示信息
     */
    QString currentPrompt() const { return m_currentPrompt; }

    /**
     * @brief 处理用户点击事件
     *
     * PlotWidget 应该在鼠标点击时调用此方法。
     *
     * @param scenePos 场景坐标（图表坐标系）
     */
    void handlePointClicked(const QPointF& scenePos);

signals:
    /**
     * @brief 点拾取完成信号
     *
     * @param points 拾取的点集合（图表坐标系）
     */
    void pointsSelected(const QVector<QPointF>& points);

    /**
     * @brief 双曲线选择完成信号
     *
     * @param mainCurve 主曲线
     * @param referenceCurve 参考曲线
     * @param points 拾取的点（如果有）
     */
    void dualCurveSelected(ThermalCurve* mainCurve, ThermalCurve* referenceCurve, const QVector<QPointF>& points);

    /**
     * @brief 交互取消信号
     */
    void interactionCancelled();

    /**
     * @brief 交互模式改变信号
     *
     * @param mode 新的交互模式
     * @param prompt 提示信息
     */
    void modeChanged(Mode mode, const QString& prompt);

private:
    /**
     * @brief 重置交互状态
     */
    void resetState();

    /**
     * @brief 完成点拾取
     */
    void finishPointPicking();

    PlotWidget* m_plotWidget;           // 关联的图表组件
    Mode m_currentMode;                 // 当前交互模式
    QString m_currentPrompt;            // 当前提示信息

    // 点拾取模式状态
    int m_requiredPointCount;           // 需要拾取的点数量
    QVector<QPointF> m_selectedPoints;  // 已拾取的点

    // 双曲线模式状态
    ThermalCurve* m_mainCurve;          // 主曲线
    ThermalCurve* m_referenceCurve;     // 参考曲线
};

#endif // INTERACTIONCONTROLLER_H
