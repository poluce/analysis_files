#ifndef THERMAL_CHART_H
#define THERMAL_CHART_H

#include <QChart>
#include <QHash>
#include <QMap>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QAbstractSeries>

QT_CHARTS_USE_NAMESPACE

class QGraphicsLineItem;
class QGraphicsObject;
class ThermalCurve;
class CurveManager;
class FloatingLabel;

/**
 * @brief 定义横轴显示模式
 */
enum class XAxisMode {
    Temperature, // 温度为横轴（默认）
    Time         // 时间为横轴
};

/**
 * @brief ThermalChart - 自定义图表类
 *
 * 职责：
 * 1. 管理曲线系列（增删改查）
 * 2. 管理坐标轴（X/Y 轴，缩放）
 * 3. 管理叠加物（浮动标签、标注点、测量工具、注释线）
 * 4. 提供数据查询接口
 *
 * 设计原则：
 * - 只管理数据和图元，不处理用户交互
 * - 不依赖 QChartView，所有操作基于 QChart API
 * - 通过信号通知状态变化，让关注者自行响应
 */
class ThermalChart : public QChart {
    Q_OBJECT

public:
    explicit ThermalChart(QGraphicsItem* parent = nullptr, Qt::WindowFlags wFlags = Qt::WindowFlags());
    ~ThermalChart();

    // ==================== 外部依赖注入 ====================
    void setCurveManager(CurveManager* manager);

    /**
     * @brief 完整性校验与状态标记
     *
     * 在所有依赖注入完成后调用，集中断言所有必需依赖非空，
     * 并将图表标记为"已初始化状态"。
     *
     * 调用顺序：构造函数() → setCurveManager(CurveManager*) → initialize()
     */
    void initialize();

    // ==================== 曲线管理 ====================
    void addCurve(const ThermalCurve& curve);
    void updateCurve(const ThermalCurve& curve);
    void removeCurve(const QString& curveId);
    void clearCurves();
    void setCurveVisible(const QString& curveId, bool visible);
    void highlightCurve(const QString& curveId);

    // ==================== 坐标轴管理 ====================
    void rescaleAxes();
    void setXAxisMode(XAxisMode mode);
    void toggleXAxisMode();  // 切换横轴模式（温度 ↔ 时间）
    XAxisMode xAxisMode() const { return m_xAxisMode; }

    // ==================== 标题配置（自定义标题）====================
    /**
     * @brief 设置图表主标题
     * @param title 自定义标题（空字符串恢复默认 "热分析曲线"）
     */
    void setCustomChartTitle(const QString& title);

    /**
     * @brief 设置X轴标题
     * @param title 自定义标题（空字符串恢复默认温度/时间自动切换）
     */
    void setCustomXAxisTitle(const QString& title);

    /**
     * @brief 设置主Y轴标题
     * @param title 自定义标题（空字符串恢复默认：根据曲线类型自动生成）
     */
    void setCustomYAxisTitlePrimary(const QString& title);

    /**
     * @brief 设置次Y轴标题
     * @param title 自定义标题（空字符串恢复默认：根据曲线类型自动生成）
     */
    void setCustomYAxisTitleSecondary(const QString& title);

    /**
     * @brief 批量设置所有标题（便捷方法）
     */
    void setCustomTitles(const QString& chartTitle,
                        const QString& xAxisTitle = QString(),
                        const QString& yAxisTitlePrimary = QString(),
                        const QString& yAxisTitleSecondary = QString());

    /**
     * @brief 清除所有自定义标题，恢复自动生成
     */
    void clearCustomTitles();

    // 查询接口（供 ThermalChartView 和叠加物使用）
    QValueAxis* axisX() const { return m_axisX; }
    QValueAxis* primaryAxisY() const { return m_axisY_primary; }
    QValueAxis* secondaryAxisY() const { return m_axisY_secondary; }
    QValueAxis* yAxisForCurve(const QString& curveId);

    // ==================== 系列查询 ====================
    QLineSeries* seriesForCurve(const QString& curveId) const;
    QString curveIdForSeries(QLineSeries* series) const;
    QColor getCurveColor(const QString& curveId) const;

    // ==================== 十字线管理（仅图元接口）====================
    void setCrosshairEnabled(bool vertical, bool horizontal);
    void setVerticalCrosshairEnabled(bool enabled);
    void setHorizontalCrosshairEnabled(bool enabled);
    bool verticalCrosshairEnabled() const { return m_verticalCrosshairEnabled; }
    bool horizontalCrosshairEnabled() const { return m_horizontalCrosshairEnabled; }
    void updateCrosshairAtChartPos(const QPointF& chartPos);
    void clearCrosshair();

    // ==================== 浮动标签管理 ====================
    FloatingLabel* addFloatingLabel(const QString& text, const QPointF& dataPos, const QString& curveId);
    FloatingLabel* addFloatingLabelHUD(const QString& text, const QPointF& viewPos);
    void removeFloatingLabel(FloatingLabel* label);
    void clearFloatingLabels();
    const QVector<FloatingLabel*>& floatingLabels() const { return m_floatingLabels; }

    // ==================== 标注点（Markers）管理 ====================
    void addCurveMarkers(const QString& curveId, const QList<QPointF>& markers,
                         const QColor& color = Qt::red, qreal size = 12.0);
    void removeCurveMarkers(const QString& curveId);
    void clearAllMarkers();

    // ==================== 测量工具管理 ====================
    void addMassLossTool(const struct ThermalDataPoint& point1,
                         const struct ThermalDataPoint& point2,
                         const QString& curveId);
    void removeMassLossTool(QGraphicsObject* tool);
    void clearAllMassLossTools();

    // ==================== 峰面积工具管理 ====================
    class PeakAreaTool* addPeakAreaTool(const struct ThermalDataPoint& point1,
                                         const struct ThermalDataPoint& point2,
                                         const QString& curveId);
    void removePeakAreaTool(QGraphicsObject* tool);
    void clearAllPeakAreaTools();

    // ==================== 选中点管理（用于算法交互）====================
    void setupSelectedPointsSeries(QValueAxis* targetYAxis);
    void addSelectedPoint(const QPointF& point);
    void clearSelectedPoints();

    // ==================== 框选缩放管理 ====================
    /**
     * @brief 显示框选矩形
     * @param rect 矩形区域（图表坐标系）
     */
    void showSelectionBox(const QRectF& rect);

    /**
     * @brief 隐藏框选矩形
     */
    void hideSelectionBox();

    /**
     * @brief 缩放到指定矩形区域
     * @param rect 矩形区域（图表坐标系）
     *
     * 实现方案 A：
     * - X轴：精确缩放到矩形的 X 范围
     * - Y轴：自适应到该 X 范围内的数据实际范围
     */
    void zoomToRect(const QRectF& rect);

signals:
    /**
     * @brief 横轴模式改变信号
     *
     * 当横轴模式切换时发出，通知叠加物更新显示位置
     */
    void xAxisModeChanged(XAxisMode mode);

    /**
     * @brief 曲线可见性改变信号
     */
    void curveVisibilityChanged(const QString& curveId, bool visible);

private:
    // ==================== 系列管理辅助函数 ====================
    QLineSeries* createSeriesForCurve(const ThermalCurve& curve) const;
    void populateSeriesWithCurveData(QLineSeries* series, const ThermalCurve& curve) const;
    void attachSeriesToAxes(QLineSeries* series, QValueAxis* axisY);
    void detachSeriesFromAxes(QLineSeries* series);
    void registerSeriesMapping(QLineSeries* series, const QString& curveId);
    void unregisterSeriesMapping(const QString& curveId);
    void updateSeriesStyle(QLineSeries* series, bool selected);
    void updateLegendVisibility(QLineSeries* series, bool visible) const;

    // ==================== 坐标轴管理辅助函数 ====================
    QValueAxis* ensureYAxisForCurve(const ThermalCurve& curve);
    void rescaleAxis(QValueAxis* axis) const;
    void resetAxesToDefault();

    // ==================== 数据查询辅助函数 ====================
    struct ThermalDataPoint findNearestDataPoint(const QVector<struct ThermalDataPoint>& curveData,
                                                   double temperature) const;

    // ==================== 框选缩放辅助函数 ====================
    /**
     * @brief 自适应 Y 轴到指定 X 范围内的数据
     * @param yAxis Y 轴指针
     * @param xMin X 轴最小值
     * @param xMax X 轴最大值
     */
    void rescaleYAxisForXRange(QValueAxis* yAxis, qreal xMin, qreal xMax);

    /**
     * @brief 检查系列是否绑定到指定 Y 轴
     * @param series 曲线系列
     * @param yAxis Y 轴指针
     * @return true=绑定，false=未绑定
     */
    bool isSeriesAttachedToYAxis(QLineSeries* series, QValueAxis* yAxis) const;

    /**
     * @brief 计算系列在指定 X 范围内的 Y 值范围
     * @param series 曲线系列
     * @param xMin X 轴最小值
     * @param xMax X 轴最大值
     * @param outYMin 输出：Y 最小值
     * @param outYMax 输出：Y 最大值
     * @return true=找到数据，false=未找到数据
     */
    bool calculateYRangeInXRange(QLineSeries* series, qreal xMin, qreal xMax,
                                  qreal& outYMin, qreal& outYMax) const;

private:
    // ==================== 初始化状态标记 ====================
    bool m_initialized = false;  // 防止"半初始化对象"的运行时错误

    // ==================== 坐标轴 ====================
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY_primary = nullptr;
    QValueAxis* m_axisY_secondary = nullptr;
    XAxisMode m_xAxisMode = XAxisMode::Temperature;

    // ==================== 曲线系列管理 ====================
    QHash<QLineSeries*, QString> m_seriesToId;
    QHash<QString, QLineSeries*> m_idToSeries;
    QLineSeries* m_selectedSeries = nullptr;

    // ==================== 十字线 ====================
    QGraphicsLineItem* m_verticalCrosshairLine = nullptr;
    QGraphicsLineItem* m_horizontalCrosshairLine = nullptr;
    bool m_verticalCrosshairEnabled = false;
    bool m_horizontalCrosshairEnabled = false;

    // ==================== 选中点高亮系列（算法交互）====================
    QScatterSeries* m_selectedPointsSeries = nullptr;

    // ==================== 标注点（Markers）====================
    struct CurveMarkerData {
        QScatterSeries* series;
        QVector<struct ThermalDataPoint> dataPoints;
    };
    QMap<QString, CurveMarkerData> m_curveMarkers;

    // ==================== 浮动标签 ====================
    QVector<FloatingLabel*> m_floatingLabels;

    // ==================== 测量工具 ====================
    QVector<QGraphicsObject*> m_massLossTools;
    QVector<QGraphicsObject*> m_peakAreaTools;

    // ==================== 框选矩形 ====================
    QGraphicsRectItem* m_selectionBox = nullptr;

    // ==================== 外部依赖 ====================
    CurveManager* m_curveManager = nullptr;

    // ==================== 自定义标题（空表示使用默认）====================
    QString m_customChartTitle;              // 自定义图表标题
    QString m_customXAxisTitle;              // 自定义X轴标题
    QString m_customYAxisTitlePrimary;       // 自定义主Y轴标题
    QString m_customYAxisTitleSecondary;     // 自定义次Y轴标题
};

#endif // THERMAL_CHART_H
