#ifndef THERMAL_CHART_VIEW_H
#define THERMAL_CHART_VIEW_H

#include <QChartView>
#include <QPointF>
#include <QVector>

QT_CHARTS_USE_NAMESPACE

class ThermalChart;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;
class QAbstractSeries;
class QLineSeries;
class CurveManager;
struct ThermalDataPoint;

/**
 * @brief 定义图表的交互模式
 */
enum class InteractionMode {
    View, // 视图模式：缩放、平移
    Pick  // 拾取模式：选择数据点、测量
};

/**
 * @brief ThermalChartView - 自定义图表视图类
 *
 * 职责：
 * 1. 处理用户交互事件（鼠标、滚轮、键盘）
 * 2. 坐标换算（viewport → scene → chart → value）
 * 3. 发出基础交互信号（供 ChartView 使用）
 * 4. 交互模式管理
 *
 * 设计原则：
 * - 只处理交互和坐标换算，不包含业务逻辑
 * - 不直接操作数据，通过 ThermalChart 接口访问
 * - 发出基础信号，由 ChartView 组装为业务语义信号
 */
class ThermalChartView : public QChartView {
    Q_OBJECT

public:
    explicit ThermalChartView(ThermalChart* chart, QWidget* parent = nullptr);
    ~ThermalChartView();

    // ==================== 获取 ThermalChart（强类型）====================
    ThermalChart* thermalChart() const { return m_thermalChart; }

    // ==================== 外部依赖注入 ====================
    void setCurveManager(CurveManager* manager);

    // ==================== 交互模式 ====================
    void setInteractionMode(InteractionMode mode);
    InteractionMode interactionMode() const { return m_mode; }

    // ==================== 碰撞检测配置 ====================
    void setHitTestBasePixelThreshold(qreal px);
    qreal hitTestBasePixelThreshold() const { return m_hitTestBasePx; }
    void setHitTestIncludePenWidth(bool enabled);
    bool hitTestIncludePenWidth() const { return m_hitTestIncludePen; }

    // ==================== 测量工具 ====================
    void startMassLossTool();

signals:
    /**
     * @brief 值点击信号（用户在图表上点击）
     *
     * @param value 数据坐标值
     * @param series 点击的系列（可能为 nullptr）
     */
    void valueClicked(const QPointF& value, QAbstractSeries* series);

    /**
     * @brief 曲线命中信号（用户点击到某条曲线）
     *
     * @param curveId 被点击的曲线 ID
     */
    void curveHit(const QString& curveId);

    /**
     * @brief 悬停移动信号（鼠标在图表上移动）
     *
     * @param viewPos 视口坐标
     * @param value 数据坐标值
     */
    void hoverMoved(const QPoint& viewPos, const QPointF& value);

protected:
    // ==================== 事件处理 ====================
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // ==================== 交互辅助函数 ====================
    void handleCurveSelectionClick(const QPointF& viewportPos);
    void handleValueClick(const QPointF& viewportPos);
    QLineSeries* findSeriesNearPoint(const QPointF& viewportPos, qreal& outDistance) const;
    qreal hitThreshold() const;

    // ==================== 右键拖动 ====================
    void handleRightDrag(const QPointF& currentPos);

    // ==================== 坐标转换 ====================
    QPointF viewportToScene(const QPointF& viewportPos) const;
    QPointF sceneToChart(const QPointF& scenePos) const;
    QPointF chartToValue(const QPointF& chartPos) const;

    // ==================== 数据查询辅助函数 ====================
    ThermalDataPoint findNearestDataPoint(const QVector<ThermalDataPoint>& curveData,
                                           double xValue) const;

private:
    ThermalChart* m_thermalChart = nullptr;
    CurveManager* m_curveManager = nullptr;

    // ==================== 交互模式 ====================
    InteractionMode m_mode = InteractionMode::View;

    // ==================== 碰撞检测配置 ====================
    qreal m_hitTestBasePx = 8.0;
    bool m_hitTestIncludePen = true;

    // ==================== 右键拖动 ====================
    bool m_isRightDragging = false;
    QPointF m_rightDragStartPos;

    // ==================== 测量工具状态 ====================
    bool m_massLossToolActive = false;
};

#endif // THERMAL_CHART_VIEW_H
