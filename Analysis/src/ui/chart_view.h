#ifndef CHARTVIEW_H
#define CHARTVIEW_H

#include <QHash>
#include <QMouseEvent>
#include <QPen>
#include <QPointF>
#include <QVector>
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

class ThermalCurve;
class QPainter;
class QGraphicsLineItem;
QT_CHARTS_USE_NAMESPACE

/**
 * @brief 定义图表的交互模式
 *
 * 控制用户当前与图表的交互方式：
 * - View: 普通视图操作（缩放、平移、区域选择）
 * - Pick: 拾取或测量模式（点击、标注、分析）
 */
enum class InteractionMode {
    View, // 视图模式：缩放、平移、放大镜、框选
    Pick  // 拾取模式：选择数据点、测量、显示坐标
};

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

    bool isValid() const { return !name.isEmpty(); }
    void clear() {
        name.clear();
        displayName.clear();
        requiredPointCount = 0;
        hint.clear();
    }
};

class ChartView : public QWidget {
    Q_OBJECT
public:
    explicit ChartView(QWidget* parent = nullptr);

    void setHitTestBasePixelThreshold(qreal px);
    qreal hitTestBasePixelThreshold() const;

    void setHitTestIncludePenWidth(bool enabled);
    bool hitTestIncludePenWidth() const;
    void setInteractionMode(InteractionMode type);

    // ==================== 交互状态管理 ====================
    /**
     * @brief 启动算法交互（进入选点模式）
     *
     * 当用户选择某个算法时调用，图表进入"等待用户交互"状态
     *
     * @param algorithmName 算法名称
     * @param displayName 显示名称
     * @param requiredPoints 需要的点数
     * @param hint 交互提示文本
     */
    void startAlgorithmInteraction(const QString& algorithmName, const QString& displayName,
                                    int requiredPoints, const QString& hint);

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

    /**
     * @brief 获取已选择的点
     */
    const QVector<QPointF>& selectedPoints() const { return m_selectedPoints; }

    void addAnnotationLine(
        const QString& id, const QString& curveId, const QPointF& start, const QPointF& end, const QPen& pen = QPen(Qt::red, 2));
    void removeAnnotation(const QString& id);
    void clearAllAnnotations();

public slots:
    void addCurve(const ThermalCurve& curve);
    void updateCurve(const ThermalCurve& curve);
    void removeCurve(const QString& curveId);
    void clearCurves();
    void setVerticalCrosshairEnabled(bool enabled);
    void setHorizontalCrosshairEnabled(bool enabled);
    void rescaleAxes();
    void setCurveVisible(const QString& curveId, bool visible);

public:
    bool verticalCrosshairEnabled() const { return m_verticalCrosshairEnabled; }
    bool horizontalCrosshairEnabled() const { return m_horizontalCrosshairEnabled; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void curveSelected(const QString& curveId);

    /**
     * @brief 算法交互完成信号
     *
     * 当用户完成所有必需的交互步骤后发出，包含算法名称和选择的点
     *
     * @param algorithmName 算法名称
     * @param points 用户选择的点
     */
    void algorithmInteractionCompleted(const QString& algorithmName, const QVector<QPointF>& points);

    /**
     * @brief 交互状态改变信号
     *
     * @param newState 新的交互状态（int类型，对应InteractionState枚举值）
     *        0=Idle, 1=WaitingForPoints, 2=PointsCompleted, 3=Executing
     */
    void interactionStateChanged(int newState);

private:
    void updateSeriesStyle(QLineSeries* series, bool selected);
    QValueAxis* ensureYAxisForCurve(const ThermalCurve& curve);

    void drawAnnotationLines(QPainter& painter);
    void handleCurveSelectionClick(const QPointF& chartViewPos);
    void handlePointSelectionClick(const QPointF& chartViewPos);
    QPointF mapToChartCoordinates(const QPoint& widgetPos) const;
    QLineSeries* findSeriesNearPoint(const QPointF& chartPos, qreal& outDistance) const;
    // --- 曲线系列管理：封装 QLineSeries 的创建、数据填充与映射维护 ---
    QLineSeries* seriesForCurve(const QString& curveId) const;
    // 根据 ThermalCurve 构建新的系列实例并写入基础属性
    QLineSeries* createSeriesForCurve(const ThermalCurve& curve) const;
    // 将 ThermalCurve 数据点批量写入指定系列
    void populateSeriesWithCurveData(QLineSeries* series, const ThermalCurve& curve) const;
    // 将系列附着到 X 轴与目标 Y 轴
    void attachSeriesToAxes(QLineSeries* series, QValueAxis* axisY);
    // 将系列从所有已附着的坐标轴解绑
    void detachSeriesFromAxes(QLineSeries* series);
    // 记录系列与曲线 ID 的双向映射关系
    void registerSeriesMapping(QLineSeries* series, const QString& curveId);
    // 移除系列与曲线 ID 的双向映射关系
    void unregisterSeriesMapping(const QString& curveId);
    // 同步图例项的可见性
    void updateLegendVisibility(QLineSeries* series, bool visible) const;
    // --- 坐标轴管理：根据现有数据动态调整尺度并在清空时恢复默认 ---
    void rescaleAxis(QValueAxis* axis) const;
    void resetAxesToDefault();
    // --- 十字线管理：维护鼠标位置与十字线可见性 ---
    void updateCrosshairVisibility();
    void updateCrosshairPosition(const QPointF& viewportPos);
    void clearCrosshair();

private:
    QChartView* m_chartView;
    QLineSeries* m_selectedSeries;
    QHash<QLineSeries*, QString> m_seriesToId;
    QHash<QString, QLineSeries*> m_idToSeries;

    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY_primary = nullptr;
    QValueAxis* m_axisY_secondary = nullptr;

    qreal m_hitTestBasePx = 8.0;
    bool m_hitTestIncludePen = true;
    bool m_verticalCrosshairEnabled = false;
    bool m_horizontalCrosshairEnabled = false;
    bool m_hasMousePosition = false;
    QGraphicsLineItem* m_verticalCrosshairLine = nullptr;
    QGraphicsLineItem* m_horizontalCrosshairLine = nullptr;

    struct AnnotationLine {
        QString id;
        QString curveId;
        QPointF start;
        QPointF end;
        QPen pen;
    };
    InteractionMode m_mode = InteractionMode::View; // 切换视图和拾取点的模式

    // ==================== 交互状态管理 ====================
    InteractionState m_interactionState = InteractionState::Idle;  // 当前交互状态
    ActiveAlgorithmInfo m_activeAlgorithm;                        // 当前活动算法信息
    QVector<QPointF> m_selectedPoints;                            // 用户已选择的点

    QVector<AnnotationLine> m_annotations;
};

#endif // CHARTVIEW_H
