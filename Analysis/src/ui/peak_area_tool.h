#ifndef PEAK_AREA_TOOL_H
#define PEAK_AREA_TOOL_H

#include <QGraphicsObject>
#include <QPen>
#include <QBrush>
#include <QString>
#include <QPointF>
#include <QPolygonF>
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include "domain/model/thermal_data_point.h"

QT_CHARTS_USE_NAMESPACE

class ThermalCurve;
class CurveManager;

/**
 * @brief 峰面积可交互工具
 *
 * 可拖动的峰面积测量工具，支持实时计算和阴影填充显示。
 *
 * 功能特性：
 * - 两个可拖动的端点（自动吸附到曲线）
 * - 阴影填充显示积分区域
 * - 支持3种基线模式：零基线、直线基线、参考曲线基线
 * - 实时计算峰面积（梯形积分法）
 * - 右键菜单切换基线模式
 * - 可以删除（右上角关闭按钮）
 *
 * 使用方式：
 * 1. 用户点击"峰面积"按钮
 * 2. 在图表上拖动鼠标选择两个点
 * 3. 工具显示阴影填充区域和面积数值
 * 4. 可以拖动端点重新调整范围
 * 5. 右键点击切换基线模式
 */
class PeakAreaTool : public QGraphicsObject {
    Q_OBJECT

public:
    // ==================== 基线模式 ====================
    /**
     * @brief 基线模式枚举
     */
    enum class BaselineMode {
        Zero,           ///< 零基线（Y=0）
        Linear,         ///< 直线基线（两端点连线）
        ReferenceCurve  ///< 参考曲线（已计算的基线曲线）
    };

    /**
     * @brief 构造函数
     * @param chart 所属图表
     * @param parent 父 QGraphicsItem
     */
    explicit PeakAreaTool(QChart* chart, QGraphicsItem* parent = nullptr);

    // ==================== Qt 必需的虚函数 ====================
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    // ==================== 属性设置 ====================
    /**
     * @brief 设置测量的两个端点（完整数据点，包含温度和时间）
     * @param point1 第一个点的完整数据
     * @param point2 第二个点的完整数据
     */
    void setMeasurePoints(const ThermalDataPoint& point1, const ThermalDataPoint& point2);

    /**
     * @brief 设置关联的曲线和坐标轴
     * @param curveId 曲线ID
     * @param xAxis X轴
     * @param yAxis Y轴
     * @param series 曲线系列（用于坐标转换）
     */
    void setAxes(const QString& curveId, QValueAxis* xAxis, QValueAxis* yAxis, QAbstractSeries* series = nullptr);

    /**
     * @brief 设置 CurveManager（用于曲线吸附和基线查询）
     */
    void setCurveManager(CurveManager* manager);

    /**
     * @brief 设置横轴模式（温度或时间）
     * @param useTimeAxis true=时间轴，false=温度轴
     */
    void setXAxisMode(bool useTimeAxis);

    /**
     * @brief 设置基线模式
     * @param mode 基线模式
     */
    void setBaselineMode(BaselineMode mode);

    /**
     * @brief 设置参考曲线（基线）
     * @param curveId 基线曲线ID
     */
    void setReferenceCurve(const QString& curveId);

    /**
     * @brief 获取第一个测量点
     */
    const ThermalDataPoint& point1() const { return m_point1; }

    /**
     * @brief 获取第二个测量点
     */
    const ThermalDataPoint& point2() const { return m_point2; }

    /**
     * @brief 获取峰面积
     */
    qreal peakArea() const { return m_cachedArea; }

    /**
     * @brief 获取格式化的面积文本
     */
    QString peakAreaText() const;

    /**
     * @brief 获取区域多边形（用于绘制）
     */
    QPolygonF areaPolygon() const { return m_cachedPolygon; }

signals:
    /**
     * @brief 工具被删除时发射
     */
    void removeRequested();

    /**
     * @brief 面积变化时发射
     * @param newArea 新的面积值
     */
    void areaChanged(qreal newArea);

public slots:
    /**
     * @brief 更新缓存的面积和多边形
     *
     * 当坐标轴范围变化（缩放/平移）或横轴模式切换时调用
     */
    void updateCache();

protected:
    // ==================== 鼠标事件 ====================
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    // ==================== 绘制函数 ====================
    /**
     * @brief 绘制阴影填充区域
     */
    void paintAreaFill(QPainter* painter);

    /**
     * @brief 绘制边界线
     */
    void paintBoundary(QPainter* painter);

    /**
     * @brief 绘制拖动手柄
     */
    void paintHandles(QPainter* painter);

    /**
     * @brief 绘制面积文本
     */
    void paintMeasureText(QPainter* painter);

    /**
     * @brief 绘制关闭按钮
     */
    void paintCloseButton(QPainter* painter);

    /**
     * @brief 绘制单个手柄
     */
    void paintHandle(QPainter* painter, const QPointF& scenePos, bool isHovered);

    // ==================== 计算函数 ====================
    /**
     * @brief 计算峰面积（梯形积分法）
     * @return 峰面积值
     */
    qreal calculateArea();

    /**
     * @brief 构建阴影区域多边形
     * @return 区域多边形（场景坐标）
     */
    QPolygonF buildAreaPolygon();

    /**
     * @brief 获取基线在指定X位置的Y值
     * @param xValue X坐标值（数据坐标）
     * @return 基线的Y值
     */
    qreal getBaselineValue(qreal xValue);

    // ==================== 辅助函数 ====================
    /**
     * @brief 将数据坐标转换为场景坐标
     */
    QPointF dataToScene(const ThermalDataPoint& dataPoint) const;

    /**
     * @brief 将场景坐标转换为数据坐标
     */
    QPointF sceneToData(const QPointF& scenePoint) const;

    /**
     * @brief 查找曲线上最接近指定X坐标的点
     * @param xValue X坐标值
     * @return 最接近的完整数据点
     */
    ThermalDataPoint findNearestPointOnCurve(qreal xValue);

    /**
     * @brief 检查鼠标是否在关闭按钮上
     */
    bool isPointInCloseButton(const QPointF& pos) const;

    /**
     * @brief 检查鼠标是否在某个拖动手柄上
     * @return 0=不在手柄上，1=在第一个手柄上，2=在第二个手柄上
     */
    int getHandleAtPosition(const QPointF& pos) const;

    /**
     * @brief 标记数据已改变，需要重新计算缓存
     */
    void markDirty();

    // ==================== 成员变量 ====================
    QChart* m_chart;                    ///< 所属图表
    CurveManager* m_curveManager;       ///< 曲线管理器（用于吸附和基线查询）
    QString m_curveId;                  ///< 关联的曲线ID
    QString m_baselineCurveId;          ///< 基线曲线ID（BaselineMode::ReferenceCurve时使用）
    QValueAxis* m_xAxis;                ///< X轴
    QValueAxis* m_yAxis;                ///< Y轴
    QAbstractSeries* m_series;          ///< 曲线系列（用于坐标转换）
    bool m_useTimeAxis;                 ///< 是否使用时间轴（true=时间，false=温度）

    ThermalDataPoint m_point1;          ///< 第一个测量点（完整数据，包含温度和时间）
    ThermalDataPoint m_point2;          ///< 第二个测量点（完整数据，包含温度和时间）

    BaselineMode m_baselineMode;        ///< 基线模式
    qreal m_cachedArea;                 ///< 缓存的面积值
    QPolygonF m_cachedPolygon;          ///< 缓存的区域多边形（场景坐标）
    bool m_isDirty;                     ///< 脏标记：数据已改变，需要重新计算缓存

    // 交互状态
    enum DragState {
        None,            ///< 无拖动
        DraggingHandle1, ///< 拖动第一个手柄
        DraggingHandle2  ///< 拖动第二个手柄
    };
    DragState m_dragState;              ///< 当前拖动状态
    int m_hoveredHandle;                ///< 鼠标悬停的手柄（0=无，1=第一个，2=第二个）
    bool m_closeButtonHovered;          ///< 关闭按钮是否被悬停

    // 样式设置
    QPen m_boundaryPen;                 ///< 边界线画笔
    QBrush m_fillBrush;                 ///< 填充画刷（半透明）
    qreal m_handleRadius;               ///< 拖动手柄半径
    QRectF m_closeButtonRect;           ///< 关闭按钮矩形
};

#endif // PEAK_AREA_TOOL_H
