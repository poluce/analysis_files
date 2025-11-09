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
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include "domain/model/thermal_data_point.h"

class ThermalCurve;
class QPainter;
class QGraphicsLineItem;
class FloatingLabel;
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
 * @brief 定义横轴显示模式
 *
 * 控制图表横轴使用的自变量：
 * - Temperature: 以温度为横轴（默认）
 * - Time: 以时间为横轴
 */
enum class XAxisMode {
    Temperature, // 温度为横轴（默认）
    Time         // 时间为横轴
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

class CurveManager;  // 前置声明

class ChartView : public QWidget {
    Q_OBJECT
public:
    explicit ChartView(QWidget* parent = nullptr);

    /**
     * @brief 设置曲线管理器（用于获取曲线数据）
     * @param manager CurveManager 指针
     */
    void setCurveManager(CurveManager* manager);

    void setHitTestBasePixelThreshold(qreal px);
    qreal hitTestBasePixelThreshold() const;

    void setHitTestIncludePenWidth(bool enabled);
    bool hitTestIncludePenWidth() const;
    void setInteractionMode(InteractionMode type);

    /**
     * @brief 获取当前横轴模式
     */
    XAxisMode xAxisMode() const { return m_xAxisMode; }

    /**
     * @brief 切换横轴模式（温度 ↔ 时间）
     */
    void toggleXAxisMode();

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

    /**
     * @brief 获取已选择的点（完整数据点，包含温度、时间、值）
     */
    const QVector<ThermalDataPoint>& selectedPoints() const { return m_selectedPoints; }

    /**
     * @brief 获取选中点所属的曲线ID
     */
    const QString& selectedPointsCurveId() const { return m_selectedPointsCurveId; }

    void addAnnotationLine(
        const QString& id, const QString& curveId, const QPointF& start, const QPointF& end, const QPen& pen = QPen(Qt::red, 2));
    void removeAnnotation(const QString& id);
    void clearAllAnnotations();

    /**
     * @brief 获取指定曲线的颜色
     * @param curveId 曲线ID
     * @return 曲线的颜色，如果找不到曲线则返回黑色
     */
    QColor getCurveColor(const QString& curveId) const;

    // ==================== 浮动标签管理 ====================
    /**
     * @brief 添加浮动标签（数据锚定模式）
     * @param text 标签文本（支持多行，使用 \n 分隔）
     * @param dataPos 数据坐标锚点 (x, y)
     * @param curveId 所属曲线ID（用于确定Y轴和自动跟随）
     * @return FloatingLabel 指针（可用于后续配置）
     */
    FloatingLabel* addFloatingLabel(const QString& text, const QPointF& dataPos, const QString& curveId);

    /**
     * @brief 添加浮动标签（视图锚定模式，HUD）
     * @param text 标签文本
     * @param viewPos 视图像素位置（相对于 plotArea）
     * @return FloatingLabel 指针
     */
    FloatingLabel* addFloatingLabelHUD(const QString& text, const QPointF& viewPos);

    /**
     * @brief 移除指定浮动标签
     * @param label 要移除的标签指针
     */
    void removeFloatingLabel(FloatingLabel* label);

    /**
     * @brief 清空所有浮动标签
     */
    void clearFloatingLabels();

    /**
     * @brief 获取所有浮动标签
     */
    const QVector<FloatingLabel*>& floatingLabels() const { return m_floatingLabels; }

    // ==================== 标注点（Markers）管理接口 ====================

    /**
     * @brief 为指定曲线添加标注点（用于显示算法生成的特征点，如基线定义点）
     * @param curveId 曲线ID
     * @param markers 标注点列表（数据坐标）
     * @param color 标注点颜色（默认红色，与用户选点时的颜色一致）
     * @param size 标注点大小（默认12，与用户选点时的大小一致）
     */
    void addCurveMarkers(const QString& curveId, const QList<QPointF>& markers,
                         const QColor& color = Qt::red, qreal size = 12.0);

    /**
     * @brief 移除指定曲线的标注点
     * @param curveId 曲线ID
     */
    void removeCurveMarkers(const QString& curveId);

    /**
     * @brief 清空所有标注点
     */
    void clearAllMarkers();

    // ==================== 测量工具管理 ====================
    /**
     * @brief 启动质量损失测量工具（进入拖动选择模式）
     */
    void startMassLossTool();

    /**
     * @brief 添加质量损失测量工具到图表
     * @param point1 第一个测量点（完整数据点）
     * @param point2 第二个测量点（完整数据点）
     * @param curveId 曲线ID（用于确定坐标轴）
     */
    void addMassLossTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId);

    /**
     * @brief 移除指定的测量工具
     * @param tool 要移除的工具指针
     */
    void removeMassLossTool(QGraphicsObject* tool);

    /**
     * @brief 清空所有测量工具
     */
    void clearAllMassLossTools();

public slots:
    void addCurve(const ThermalCurve& curve);
    void updateCurve(const ThermalCurve& curve);
    void removeCurve(const QString& curveId);
    void clearCurves();
    void setVerticalCrosshairEnabled(bool enabled);
    void setHorizontalCrosshairEnabled(bool enabled);
    void rescaleAxes();
    void setCurveVisible(const QString& curveId, bool visible);
    void highlightCurve(const QString& curveId);

public:
    bool verticalCrosshairEnabled() const { return m_verticalCrosshairEnabled; }
    bool horizontalCrosshairEnabled() const { return m_horizontalCrosshairEnabled; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
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

private:
    void updateSeriesStyle(QLineSeries* series, bool selected);
    QValueAxis* ensureYAxisForCurve(const ThermalCurve& curve);

    void drawAnnotationLines(QPainter& painter);
    void handleCurveSelectionClick(const QPointF& chartViewPos);
    void handlePointSelectionClick(const QPointF& chartViewPos);
    QPointF mapToChartCoordinates(const QPoint& widgetPos) const;
    QLineSeries* findSeriesNearPoint(const QPointF& chartPos, qreal& outDistance) const;

    // --- 算法交互辅助函数 ---
    void clearInteractionState();
    QValueAxis* findYAxisForCurve(const QString& curveId);
    void setupSelectedPointsSeries(QValueAxis* targetYAxis);
    void transitionToState(InteractionState newState);
    QPointF convertToValueCoordinates(const QPointF& chartViewPos);
    void completePointSelection();
    void updateSelectedPointsDisplay();  // 更新选中点的显示位置（用于切换横轴模式）

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
    // --- 标注点辅助函数：根据温度查找最接近的数据点 ---
    ThermalDataPoint findNearestDataPoint(const QVector<ThermalDataPoint>& curveData, double temperature) const;

private:
    QChartView* m_chartView;
    QLineSeries* m_selectedSeries;
    QHash<QLineSeries*, QString> m_seriesToId;
    QHash<QString, QLineSeries*> m_idToSeries;

    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY_primary = nullptr;
    QValueAxis* m_axisY_secondary = nullptr;

    CurveManager* m_curveManager = nullptr;  // 曲线管理器（用于获取曲线数据）

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
    XAxisMode m_xAxisMode = XAxisMode::Temperature;  // 横轴模式（默认温度）

    // ==================== 交互状态管理 ====================
    InteractionState m_interactionState = InteractionState::Idle;  // 当前交互状态
    ActiveAlgorithmInfo m_activeAlgorithm;                        // 当前活动算法信息
    QVector<ThermalDataPoint> m_selectedPoints;                   // 用户已选择的点（完整数据，包含温度、时间、值）
    QString m_selectedPointsCurveId;                              // 选中点所属的曲线ID
    QScatterSeries* m_selectedPointsSeries = nullptr;             // 显示选中点的散点图系列（红色高亮）

    QVector<AnnotationLine> m_annotations;

    // ==================== 浮动标签管理 ====================
    QVector<FloatingLabel*> m_floatingLabels;                     // 浮动标签列表

    // ==================== 标注点（Markers）管理 ====================
    struct CurveMarkerData {
        QScatterSeries* series;                      // 散点图系列
        QVector<ThermalDataPoint> dataPoints;        // 原始数据点（包含温度和时间）
    };
    QMap<QString, CurveMarkerData> m_curveMarkers;   // 曲线ID → 标注点数据

    // ==================== 测量工具管理 ====================
    QVector<QGraphicsObject*> m_massLossTools;       // 质量损失测量工具列表
    bool m_massLossToolActive = false;               // 是否正在创建新的测量工具
};

#endif // CHARTVIEW_H
