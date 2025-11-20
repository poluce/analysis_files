#ifndef THERMAL_CHART_H
#define THERMAL_CHART_H

#include <QChart>
#include <QColor>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QVector>
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

class QGraphicsLineItem;
class QGraphicsObject;
class ThermalCurve;
class CurveManager;

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
    void setCustomTitles(
        const QString& chartTitle, const QString& xAxisTitle = QString(), const QString& yAxisTitlePrimary = QString(),
        const QString& yAxisTitleSecondary = QString());

    /**
     * @brief 清除所有自定义标题，恢复自动生成
     */
    void clearCustomTitles();

    // 查询接口（供 ThermalChartView 和叠加物使用）
    QValueAxis* axisX() const { return m_axisX; }
    QValueAxis* primaryAxisY() const { return m_axisY_mass; }
    QValueAxis* secondaryAxisY() const { return m_axisY_diff; }
    QValueAxis* yAxisForCurveId(const QString& curveId);

    // ==================== 系列查询 ====================
    QXYSeries* seriesForCurveId(const QString& curveId) const;
    QString curveIdForSeries(QXYSeries* series) const;
    QColor getCurveColor(const QString& curveId) const;

    // ==================== 数据查询 ====================
    /**
     * @brief 查找最接近指定 X 值的数据点
     * @param curveData 曲线数据
     * @param xValue X 值（根据横轴模式，可能是 temperature 或 time）
     * @return 最接近的数据点
     *
     * 根据当前横轴模式（Temperature/Time）自动选择比较字段。
     * 供 ThermalChartView 和其他组件使用。
     */
    struct ThermalDataPoint findNearestDataPoint(const QVector<struct ThermalDataPoint>& curveData, double xValue) const;

    // ==================== 十字线管理（仅图元接口）====================
    void setCrosshairEnabled(bool vertical, bool horizontal);
    void setVerticalCrosshairEnabled(bool enabled);
    void setHorizontalCrosshairEnabled(bool enabled);
    bool verticalCrosshairEnabled() const { return m_verticalCrosshairEnabled; }
    bool horizontalCrosshairEnabled() const { return m_horizontalCrosshairEnabled; }
    void updateCrosshairAtChartPos(const QPointF& chartPos);
    void clearCrosshair();

    // ==================== 测量工具管理 ====================
    QGraphicsObject* addMassLossTool(const struct ThermalDataPoint& point1, const struct ThermalDataPoint& point2, const QString& curveId);
    void removeMassLossTool(QGraphicsObject* tool);
    void clearAllMassLossTools();

    // ==================== 峰面积工具管理 ====================
    class PeakAreaTool*
    addPeakAreaTool(const struct ThermalDataPoint& point1, const struct ThermalDataPoint& point2, const QString& curveId);
    void removePeakAreaTool(QGraphicsObject* tool);
    void clearAllPeakAreaTools();

    // ==================== 工具更新（坐标轴变化时）====================
    /**
     * @brief 更新所有工具（测量工具、峰面积工具）
     *
     * 当坐标轴范围改变时（如缩放、平移），需要调用此方法通知所有工具重绘。
     * 工具使用数据坐标存储位置，坐标系变化时需要更新场景坐标映射。
     */
    void updateAllTools();

    // ==================== 选中点管理（用于算法交互）====================
    void rebindSelectedPointsSeries(QValueAxis* targetYAxis);
    void addSelectedPoint(const QPointF& point);
    void clearSelectedPoints();

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

    /**
     * @brief 质量损失工具删除请求信号
     *
     * 当用户点击工具关闭按钮时发出，由 Controller 创建 RemoveCommand
     */
    void massLossToolRemoveRequested(QGraphicsObject* tool);

    /**
     * @brief 峰面积工具删除请求信号
     *
     * 当用户点击工具关闭按钮时发出，由 Controller 创建 RemoveCommand
     */
    void peakAreaToolRemoveRequested(QGraphicsObject* tool);

private:
    // ==================== 系列管理辅助函数 ====================
    QXYSeries* createSeriesForThermalCurve(const ThermalCurve& curve) const;
    QList<QPointF> buildSeriesPoints(const ThermalCurve& curve) const;
    void attachSeriesToAxes(QXYSeries* series, QValueAxis* axisY);
    void detachSeriesFromAxes(QXYSeries* series);
    void registerSeriesMapping(QXYSeries* series, const QString& curveId);
    void unregisterSeriesMapping(const QString& curveId);
    void updateSeriesStyle(QXYSeries* series, bool selected);

    // ==================== 坐标轴管理辅助函数 ====================
    QValueAxis* ensureYAxisForCurve(const ThermalCurve& curve);
    void updateAxisRangeForAttachedSeries(QValueAxis* axis) const;
    QList<QXYSeries*> seriesAttachedToAxis(QAbstractAxis* axis) const;

    void resetAxesToDefault();

    // ==================== 框选缩放辅助函数 ====================
    /**
     * @brief 检查系列是否绑定到指定 Y 轴
     * @param series 曲线系列
     * @param yAxis Y 轴指针
     * @return true=绑定，false=未绑定
     */
    bool isSeriesAttachedToYAxis(QXYSeries* series, QValueAxis* yAxis) const;

    /**
     * @brief 计算系列在指定 X 范围内的 Y 值范围
     * @param series 曲线系列
     * @param xMin X 轴最小值
     * @param xMax X 轴最大值
     * @param outYMin 输出：Y 最小值
     * @param outYMax 输出：Y 最大值
     * @return true=找到数据，false=未找到数据
     */
    bool calculateYRangeInXRange(QXYSeries* series, qreal xMin, qreal xMax, qreal& outYMin, qreal& outYMax) const;

    // ==================== 工具清理辅助函数 ====================
    /**
     * @brief 删除特定曲线关联的所有测量工具
     * @param curveId 曲线ID
     */
    void removeCurveMassLossTools(const QString& curveId);

    /**
     * @brief 删除特定曲线关联的所有峰面积工具
     * @param curveId 曲线ID
     */
    void removeCurvePeakAreaTools(const QString& curveId);

private:
    // ==================== 初始化状态标记 ====================
    bool m_initialized = false; // 防止"半初始化对象"的运行时错误

    // ==================== 坐标轴 ====================
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY_mass = nullptr;
    QValueAxis* m_axisY_diff = nullptr;
    XAxisMode m_xAxisMode = XAxisMode::Temperature;

    // ==================== 曲线系列管理 ====================
    QHash<QXYSeries*, QString> m_seriesToId;
    QHash<QString, QXYSeries*> m_idToSeries;
    QXYSeries* m_selectedSeries = nullptr;

    // ==================== 十字线 ====================
    QGraphicsLineItem* m_verticalCrosshairLine = nullptr;
    QGraphicsLineItem* m_horizontalCrosshairLine = nullptr;
    bool m_verticalCrosshairEnabled = false;
    bool m_horizontalCrosshairEnabled = false;

    // ==================== 选中点高亮系列（算法交互）====================
    QScatterSeries* m_selectedPointsSeries = nullptr;

    // ==================== 测量工具 ====================
    QVector<QGraphicsObject*> m_massLossTools;
    QVector<QGraphicsObject*> m_peakAreaTools;

    // ==================== 外部依赖 ====================
    CurveManager* m_curveManager = nullptr;

    // ==================== 自定义标题（空表示使用默认）====================
    QString m_customChartTitle;          // 自定义图表标题
    QString m_customXAxisTitle;          // 自定义X轴标题
    QString m_customYAxisTitlePrimary;   // 自定义主Y轴标题
    QString m_customYAxisTitleDiff; // 自定义次Y轴标题
};

#endif // THERMAL_CHART_H
