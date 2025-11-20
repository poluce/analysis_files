#ifndef CHARTVIEW_H
#define CHARTVIEW_H

#include <QPointF>
#include <QVector>
#include <QWidget>
#include <QColor>
#include <QAbstractSeries>
#include "domain/model/thermal_data_point.h"

QT_CHARTS_USE_NAMESPACE

class ThermalChart;
class ThermalChartView;
class ThermalCurve;
class CurveManager;
class HistoryManager;
class QGraphicsObject;
class QPen;
class PeakAreaTool;

/**
 * @brief 图表交互状态
 *
 * 描述当前图表的交互状态和进度
 */
enum class InteractionState {
    Idle,              // 空闲：无活动算法
    WaitingForPoints,  // 等待用户选点
    PointsCompleted,   // 选点完成，准备执行
    Executing          // 算法执行中
};

/**
 * @brief 活动算法信息
 *
 * 记录当前正在交互的算法的元数据
 */
struct ActiveAlgorithmInfo {
    QString name;                // 算法名称（如 "baseline_correction"）
    QString displayName;         // 显示名称（如 "基线校正"）
    int requiredPointCount = 0;  // 需要的点数
    QString hint;                // 交互提示（如 "请选择两个点定义基线范围"）
    QString targetCurveId;       // 目标曲线ID（用于获取曲线数据）

    bool isValid() const { return !name.isEmpty(); }
    void clear() {
        name.clear();
        displayName.clear();
        requiredPointCount = 0;
        hint.clear();
        targetCurveId.clear();
    }
};

/**
 * @brief ChartView - 图表容器组件
 *
 * 职责：
 * 1. 组合 ThermalChart（数据管理）和 ThermalChartView（交互处理）
 * 2. 管理算法交互状态机（选点流程、状态转换）
 * 3. 转发信号和方法调用
 *
 * 设计原则：
 * - 薄容器：大部分功能转发给 ThermalChart 和 ThermalChartView
 * - 算法状态机：唯一不转发的业务逻辑（ChartView 的核心职责）
 * - API 稳定性：对外 API 保持不变
 */
class ChartView : public QWidget {
    Q_OBJECT

public:
    explicit ChartView(QWidget* parent = nullptr);

    // ==================== 依赖注入 ====================
    /**
     * @brief 设置曲线管理器（用于获取曲线数据）
     * @param manager CurveManager 指针
     */
    void setCurveManager(CurveManager* manager);

    // ==================== 交互配置（转发给 ThermalChartView）====================
    void setInteractionMode(int type);  // 使用 int 避免枚举冲突

    // ==================== 横轴模式（转发给 ThermalChart）====================
    int xAxisMode() const;  // 返回 int (0=Temperature, 1=Time)

    // ==================== 算法交互状态机（ChartView 核心职责）====================
    /**
     * @brief 启动算法交互（进入选点模式）
     *
     * 当用户选择某个算法时调用，图表进入"等待用户交互"状态
     *
     * @param algorithmName 算法名称
     * @param displayName 显示名称
     * @param requiredPoints 需要的点数
     * @param hint 交互提示文本
     * @param curveId 目标曲线ID（用于确定选点标记应附着到哪个Y轴）
     */
    void startAlgorithmInteraction(const QString& algorithmName, const QString& displayName,
                                    int requiredPoints, const QString& hint, const QString& curveId);

    /**
     * @brief 取消当前算法交互
     *
     * 清空已选点，回到空闲状态
     */
    void cancelAlgorithmInteraction();

    /**
     * @brief 获取当前交互状态
     */
    InteractionState interactionState() const { return m_interactionState; }

    /**
     * @brief 获取当前活动算法信息
     */
    const ActiveAlgorithmInfo& activeAlgorithm() const { return m_activeAlgorithm; }

    // ==================== 叠加物管理（转发给 ThermalChart）====================
    // 测量工具
    void startMassLossTool();
    QGraphicsObject* addMassLossTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId);
    void removeMassLossTool(QGraphicsObject* tool);
    void clearAllMassLossTools();

    // 峰面积工具
    void startPeakAreaTool(const QString& curveId, bool useLinearBaseline, const QString& referenceCurveId = QString());
    PeakAreaTool* addPeakAreaTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId);
    void removePeakAreaTool(QGraphicsObject* tool);
    void clearAllPeakAreaTools();

    // ==================== 曲线查询（转发给 ThermalChart）====================
    QColor getCurveColor(const QString& curveId) const;

public slots:
    // ==================== 曲线管理（转发给 ThermalChart）====================
    void addCurve(const ThermalCurve& curve);
    void updateCurve(const ThermalCurve& curve);
    void removeCurve(const QString& curveId);
    void clearCurves();
    void setCurveVisible(const QString& curveId, bool visible);
    void highlightCurve(const QString& curveId);

    // ==================== 坐标轴管理（转发给 ThermalChart）====================
    void rescaleAxes();

    // ==================== 内部组件访问（用于信号连接）====================
    /**
     * @brief 获取内部 ThermalChart 实例
     * @return ThermalChart 指针（保证非空，构造时创建）
     */
    ThermalChart* chart() const { return m_chart; }

    // ==================== 十字线控制（转发给 ThermalChart）====================
    void setVerticalCrosshairEnabled(bool enabled);
    void setHorizontalCrosshairEnabled(bool enabled);

signals:
    /**
     * @brief 曲线选中信号（来自 ThermalChartView）
     */
    void curveSelected(const QString& curveId);

    /**
     * @brief 算法交互完成信号
     *
     * 当用户完成所有必需的交互步骤后发出，包含算法名称和选择的点
     *
     * @param algorithmName 算法名称
     * @param points 用户选择的点（完整数据点，包含温度、时间、值）
     */
    void algorithmInteractionCompleted(const QString& algorithmName, const QVector<ThermalDataPoint>& points);

    /**
     * @brief 交互状态改变信号
     *
     * @param newState 新的交互状态（int类型，对应InteractionState枚举值）
     *        0=Idle, 1=WaitingForPoints, 2=PointsCompleted, 3=Executing
     */
    void interactionStateChanged(int newState);

    /**
     * @brief 质量损失工具创建请求信号（转发自 ThermalChartView）
     */
    void massLossToolCreateRequested(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId);

    /**
     * @brief 峰面积工具创建请求信号（转发自 ThermalChartView）
     */
    void peakAreaToolCreateRequested(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId,
                                     bool useLinearBaseline, const QString& referenceCurveId);

private slots:
    // ==================== 算法状态机内部信号处理 ====================
    void onValueClicked(const QPointF& value, QAbstractSeries* series);
    void onCurveHit(const QString& curveId);

private:
    // ==================== 算法状态机辅助函数 ====================
    void handlePointSelection(const QPointF& value);
    void transitionToState(InteractionState newState);
    void completePointSelection();

private:
    // ==================== 组合的核心组件 ====================
    /**
     * @brief ThermalChart - 数据管理层（拥有所有权）
     *
     * 生命周期：在构造函数中创建，与 ChartView 绑定，不可能为 nullptr
     * 无需在使用前进行空指针检查
     */
    ThermalChart* m_chart = nullptr;

    /**
     * @brief ThermalChartView - 交互处理层（拥有所有权）
     *
     * 生命周期：在构造函数中创建，与 ChartView 绑定，不可能为 nullptr
     * 无需在使用前进行空指针检查
     */
    ThermalChartView* m_chartView = nullptr;

    /**
     * @brief CurveManager - 曲线管理器（外部依赖，通过 setter 注入）
     *
     * 可能为 nullptr，使用前需要检查
     */
    CurveManager* m_curveManager = nullptr;

    // ==================== 算法交互状态机（ChartView 核心职责）====================
    InteractionState m_interactionState = InteractionState::Idle;  // 当前交互状态
    ActiveAlgorithmInfo m_activeAlgorithm;                         // 当前活动算法信息
    QVector<ThermalDataPoint> m_selectedPoints;                    // 用户已选择的点
    QString m_selectedPointsCurveId;                               // 选中点所属的曲线ID
};

#endif // CHARTVIEW_H
