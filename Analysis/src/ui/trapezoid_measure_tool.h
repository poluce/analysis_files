#ifndef TRAPEZOID_MEASURE_TOOL_H
#define TRAPEZOID_MEASURE_TOOL_H

#include <QGraphicsObject>
#include <QPen>
#include <QString>
#include <QPointF>
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include "domain/model/thermal_data_point.h"

QT_CHARTS_USE_NAMESPACE

class ThermalCurve;
class CurveManager;

/**
 * @brief 直角梯形测量工具
 *
 * 可拖动的测量工具，用于测量曲线上两点之间的垂直高度差（质量损失）。
 *
 * 功能特性：
 * - 两个可拖动的端点（自动吸附到曲线）
 * - 绘制直角梯形（两条水平线 + 一条竖直线）
 * - 实时显示垂直距离测量值
 * - 可以删除（右上角关闭按钮）
 *
 * 使用方式：
 * 1. 用户点击"质量损失"按钮
 * 2. 在图表上拖动鼠标选择两个点
 * 3. 工具自动绘制并显示测量值
 * 4. 可以拖动端点重新调整测量范围
 */
class TrapezoidMeasureTool : public QGraphicsObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param chart 所属图表
     * @param parent 父 QGraphicsItem
     */
    explicit TrapezoidMeasureTool(QChart* chart, QGraphicsItem* parent = nullptr);

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
     * @brief 设置 CurveManager（用于曲线吸附）
     */
    void setCurveManager(CurveManager* manager);

    /**
     * @brief 设置横轴模式（温度或时间）
     * @param useTimeAxis true=时间轴，false=温度轴
     */
    void setXAxisMode(bool useTimeAxis);

    /**
     * @brief 获取测量值（垂直距离）
     */
    qreal measureValue() const;

    /**
     * @brief 获取关联的曲线ID
     */
    QString curveId() const { return m_curveId; }

    /**
     * @brief 获取第一个测量点（用于 RemoveCommand 保存状态）
     */
    ThermalDataPoint point1() const { return m_point1; }

    /**
     * @brief 获取第二个测量点（用于 RemoveCommand 保存状态）
     */
    ThermalDataPoint point2() const { return m_point2; }

signals:
    /**
     * @brief 工具被删除时发射
     */
    void removeRequested();

protected:
    // ==================== 鼠标事件 ====================
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
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
     * @brief 绘制关闭按钮
     */
    void paintCloseButton(QPainter* painter);

    /**
     * @brief 绘制拖动手柄
     */
    void paintHandle(QPainter* painter, const QPointF& scenePos, bool isHovered);

    /**
     * @brief 绘制测量值文本
     */
    void paintMeasureText(QPainter* painter);

    /**
     * @brief 检查鼠标是否在关闭按钮上
     */
    bool isPointInCloseButton(const QPointF& pos) const;

    /**
     * @brief 检查鼠标是否在某个拖动手柄上
     * @return 0=不在手柄上，1=在第一个手柄上，2=在第二个手柄上
     */
    int getHandleAtPosition(const QPointF& pos) const;

    // ==================== 成员变量 ====================
    QChart* m_chart;                    ///< 所属图表
    CurveManager* m_curveManager;       ///< 曲线管理器（用于吸附）
    QString m_curveId;                  ///< 关联的曲线ID
    QValueAxis* m_xAxis;                ///< X轴
    QValueAxis* m_yAxis;                ///< Y轴
    QAbstractSeries* m_series;          ///< 曲线系列（用于坐标转换）
    bool m_useTimeAxis;                 ///< 是否使用时间轴（true=时间，false=温度）

    ThermalDataPoint m_point1;          ///< 第一个测量点（完整数据，包含温度和时间）
    ThermalDataPoint m_point2;          ///< 第二个测量点（完整数据，包含温度和时间）

    // 交互状态
    enum DragState {
        None,          ///< 无拖动
        DraggingHandle1, ///< 拖动第一个手柄
        DraggingHandle2  ///< 拖动第二个手柄
    };
    DragState m_dragState;              ///< 当前拖动状态
    int m_hoveredHandle;                ///< 鼠标悬停的手柄（0=无，1=第一个，2=第二个）
    bool m_closeButtonHovered;          ///< 关闭按钮是否被悬停

    // 样式设置
    QPen m_linePen;                     ///< 线条画笔
    qreal m_handleRadius;               ///< 拖动手柄半径
    QRectF m_closeButtonRect;           ///< 关闭按钮矩形
};

#endif // TRAPEZOID_MEASURE_TOOL_H
