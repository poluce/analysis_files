#ifndef THERMAL_CHART_VIEW_H
#define THERMAL_CHART_VIEW_H

#include <QChartView>
#include <QPointF>
#include <QVector>
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

class ThermalChart;
class QMouseEvent;
class QWheelEvent;
class QContextMenuEvent;
class CurveManager;
class ThermalCurve;
class PeakAreaTool;
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

    /**
     * @brief 完整性校验与状态标记
     *
     * 在所有依赖注入完成后调用，集中断言所有必需依赖非空，
     * 并将视图标记为"已初始化状态"。
     *
     * 调用顺序：构造函数(ThermalChart*) → setCurveManager(CurveManager*) → initialize()
     *
     * NOTE: m_thermalChart 作为构造参数，由调用方保证有效性，此处不断言
     */
    void initialize();

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

    /**
     * @brief 启动峰面积测量工具
     * @param curveId 计算曲线ID
     * @param useLinearBaseline true=直线基线，false=参考曲线基线
     * @param referenceCurveId 参考曲线ID（仅当 useLinearBaseline=false 时有效）
     */
    void startPeakAreaTool(const QString& curveId, bool useLinearBaseline, const QString& referenceCurveId = QString());

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

    // ==================== EventFilter 事件处理辅助函数 ====================
    /**
     * @brief 处理右键按下事件
     * @param mouseEvent 鼠标事件
     * @return true=事件已处理，false=继续传递
     */
    bool handleRightButtonPress(QMouseEvent* mouseEvent);

    /**
     * @brief 处理右键释放事件
     * @param mouseEvent 鼠标事件
     * @return true=事件已处理，false=继续传递
     */
    bool handleRightButtonRelease(QMouseEvent* mouseEvent);

    /**
     * @brief 处理右键拖动事件
     * @param mouseEvent 鼠标事件
     * @return true=事件已处理，false=继续传递
     */
    bool handleRightButtonDrag(QMouseEvent* mouseEvent);

    /**
     * @brief 处理质量损失测量工具的单次点击创建
     * @param viewportPos 点击位置（视口坐标）
     * @return true=事件已处理，false=继续传递
     */
    bool handleMassLossToolClick(const QPointF& viewportPos);

    /**
     * @brief 处理峰面积测量工具的单次点击创建
     * @param viewportPos 点击位置（视口坐标）
     * @return true=事件已处理，false=继续传递
     */
    bool handlePeakAreaToolClick(const QPointF& viewportPos);

    /**
     * @brief 处理鼠标离开事件
     */
    void handleMouseLeave();

    // ==================== 质量损失工具辅助函数 ====================
    /**
     * @brief 验证质量损失工具前置条件并获取活动曲线和系列
     * @param outCurve 输出参数：活动曲线指针
     * @param outSeries 输出参数：活动系列指针
     * @return true=验证通过，false=验证失败
     */
    bool validateMassLossToolPreconditions(ThermalCurve** outCurve, QLineSeries** outSeries);

    /**
     * @brief 重置质量损失工具状态
     */
    void resetMassLossToolState();

    // ==================== 峰面积工具辅助函数 ====================
    /**
     * @brief 验证峰面积工具前置条件并获取目标曲线和系列
     * @param outCurve 输出参数：目标曲线指针
     * @param outSeries 输出参数：目标系列指针
     * @return true=验证通过，false=验证失败
     */
    bool validatePeakAreaToolPreconditions(ThermalCurve** outCurve, QLineSeries** outSeries);

    /**
     * @brief 检查点击位置是否在绘图区域内
     * @param viewportPos 视口坐标
     * @param outChartPos 输出参数：图表坐标
     * @return true=在绘图区内，false=不在绘图区内
     */
    bool isClickInsidePlotArea(const QPointF& viewportPos, QPointF* outChartPos);

    /**
     * @brief 计算动态延伸范围（基于当前 X 轴可见范围的百分比）
     * @param percentage 百分比（默认 5%）
     * @return 延伸范围值
     */
    qreal calculateDynamicRangeExtension(qreal percentage = 0.05);

    /**
     * @brief 应用峰面积工具的基线模式
     * @param tool 峰面积工具指针
     */
    void applyPeakAreaBaseline(PeakAreaTool* tool);

    /**
     * @brief 重置峰面积工具状态
     */
    void resetPeakAreaToolState();

    // ==================== 右键拖动 ====================
    void handleRightDrag(const QPointF& currentPos);

    // ==================== 框选缩放辅助函数 ====================
    /**
     * @brief 更新框选矩形显示
     */
    void updateSelectionBoxDisplay();

    /**
     * @brief 完成框选并执行缩放
     */
    void finalizeBoxSelection();

    /**
     * @brief 将视口坐标矩形转换为图表坐标矩形
     * @param viewportStart 视口起始点
     * @param viewportEnd 视口结束点
     * @return 图表坐标矩形
     */
    QRectF convertViewportRectToChartRect(const QPointF& viewportStart, const QPointF& viewportEnd) const;

    // ==================== 缩放辅助函数 ====================
    /**
     * @brief 以指定点为中心缩放 X 轴
     * @param chartPos 图表坐标系中的位置
     * @param factor 缩放因子（<1 放大，>1 缩小）
     */
    void zoomXAxisAtPoint(const QPointF& chartPos, qreal factor);

    /**
     * @brief 以指定点为中心缩放单个 Y 轴
     * @param yAxis 要缩放的 Y 轴
     * @param chartPos 图表坐标系中的位置
     * @param factor 缩放因子（<1 放大，>1 缩小）
     */
    void zoomYAxisAtPoint(QValueAxis* yAxis, const QPointF& chartPos, qreal factor);

    /**
     * @brief 以指定点为中心缩放所有 Y 轴
     * @param chartPos 图表坐标系中的位置
     * @param factor 缩放因子（<1 放大，>1 缩小）
     */
    void zoomAllYAxesAtPoint(const QPointF& chartPos, qreal factor);

    /**
     * @brief 查找与指定 Y 轴关联的第一个系列
     * @param yAxis Y 轴指针
     * @return 关联的系列指针，如果没有则返回 nullptr
     */
    QAbstractSeries* findFirstSeriesForYAxis(QValueAxis* yAxis) const;

    // ==================== 坐标转换 ====================
    QPointF viewportToScene(const QPointF& viewportPos) const;
    QPointF sceneToChart(const QPointF& scenePos) const;
    QPointF chartToValue(const QPointF& chartPos) const;

    // ==================== 数据查询辅助函数 ====================
    ThermalDataPoint findNearestDataPoint(const QVector<ThermalDataPoint>& curveData,
                                           double xValue) const;

private:
    // ==================== 初始化状态标记 ====================
    bool m_initialized = false;  // 防止"半初始化对象"的运行时错误

    // ==================== 依赖注入 ====================
    ThermalChart* m_thermalChart = nullptr;  // 构造函数注入（调用方保证非空）
    CurveManager* m_curveManager = nullptr;  // Setter 注入（initialize() 时断言非空）

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
    bool m_peakAreaToolActive = false;

    // 峰面积工具配置（用户通过对话框选择）
    QString m_peakAreaCurveId;
    bool m_peakAreaUseLinearBaseline = true;
    QString m_peakAreaReferenceCurveId;

    // ==================== 框选缩放状态 ====================
    bool m_isBoxSelecting = false;       // 是否正在框选
    QPointF m_boxSelectStart;            // 框选起始点（视口坐标）
    QPointF m_boxSelectEnd;              // 框选结束点（视口坐标）
};

#endif // THERMAL_CHART_VIEW_H
