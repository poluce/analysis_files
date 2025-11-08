#ifndef FLOATING_LABEL_H
#define FLOATING_LABEL_H

#include <QGraphicsObject>
#include <QPen>
#include <QString>
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QChart>

QT_CHARTS_USE_NAMESPACE

/**
 * @brief 浮动标签组件
 *
 * 可拖动、可缩放、悬浮在图表上的文本标签，支持两种锚定模式：
 * - DataAnchored：锚定到数据坐标，随图表缩放移动
 * - ViewAnchored：锚定到视图像素位置（HUD），不受图表缩放影响
 *
 * 核心功能：
 * - 拖动移动（鼠标左键拖拽）
 * - 缩放（Ctrl+滚轮）
 * - 关闭按钮（右上角）
 * - 固定/解锁（锁定后禁止拖动）
 * - 自动跟随坐标轴变化（DataAnchored 模式）
 *
 * 使用示例：
 * @code
 * auto* label = new FloatingLabel(chart);
 * label->setText("温度=350°C\n值=1.23");
 * label->setMode(FloatingLabel::Mode::DataAnchored);
 * label->setAnchorValue(QPointF(350.0, 1.23), series);
 * chart->scene()->addItem(label);
 * @endcode
 */
class FloatingLabel : public QGraphicsObject {
    Q_OBJECT

public:
    /**
     * @brief 锚定模式
     */
    enum class Mode {
        DataAnchored, ///< 锚定到数据坐标，随图表缩放移动
        ViewAnchored  ///< 锚定到视图像素位置（HUD），不受图表缩放影响
    };

    /**
     * @brief 构造函数
     * @param chart 所属图表
     * @param parent 父 QGraphicsItem
     */
    explicit FloatingLabel(QChart* chart, QGraphicsItem* parent = nullptr);

    // ==================== 属性设置 ====================
    /**
     * @brief 设置标签文本
     * @param text 文本内容（支持多行，使用 \n 分隔）
     */
    void setText(const QString& text);

    /**
     * @brief 获取标签文本
     */
    QString text() const { return m_text; }

    /**
     * @brief 设置锚定模式
     * @param mode 锚定模式
     */
    void setMode(Mode mode);

    /**
     * @brief 获取锚定模式
     */
    Mode mode() const { return m_mode; }

    /**
     * @brief 设置数据坐标锚点（仅对 DataAnchored 模式有效）
     * @param value 数据坐标（x, y）
     * @param series 所属曲线系列
     */
    void setAnchorValue(const QPointF& value, QAbstractSeries* series = nullptr);

    /**
     * @brief 获取数据坐标锚点
     */
    QPointF anchorValue() const { return m_anchorValue; }

    /**
     * @brief 设置内边距
     * @param padding 边距（像素）
     */
    void setPadding(qreal padding);

    /**
     * @brief 设置缩放因子
     * @param factor 缩放因子（范围 0.3 ~ 5.0）
     */
    void setScaleFactor(qreal factor);

    /**
     * @brief 设置位置偏移（相对于锚点）
     * @param offset 偏移量（像素）
     */
    void setOffset(const QPointF& offset) { m_offset = offset; updateGeometry(); }

    /**
     * @brief 设置固定状态（锁定后禁止拖动）
     * @param locked 是否锁定
     */
    void setLocked(bool locked);

    /**
     * @brief 获取固定状态
     */
    bool isLocked() const { return m_locked; }

    /**
     * @brief 设置背景颜色
     * @param color 背景颜色
     */
    void setBackgroundColor(const QColor& color) { m_bgColor = color; update(); }

    /**
     * @brief 设置文本颜色
     * @param color 文本颜色
     */
    void setTextColor(const QColor& color) { m_textColor = color; update(); }

    // ==================== 几何更新 ====================
    /**
     * @brief 更新标签几何位置（用于跟随坐标轴变化）
     *
     * DataAnchored 模式下，当坐标轴范围改变时应调用此方法
     */
    void updateGeometry();

    // ==================== QGraphicsItem 接口 ====================
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
    /**
     * @brief 关闭请求信号（点击关闭按钮时发出）
     */
    void closeRequested();

    /**
     * @brief 固定状态改变信号
     * @param locked 新的固定状态
     */
    void lockStateChanged(bool locked);

protected:
    // ==================== 事件处理 ====================
    void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    // ==================== 辅助函数 ====================
    /**
     * @brief 获取关闭按钮矩形区域
     */
    QRectF closeButtonRect() const;

    /**
     * @brief 获取锁定按钮矩形区域
     */
    QRectF lockButtonRect() const;

    /**
     * @brief 检查点是否在关闭按钮区域内
     */
    bool isInCloseButton(const QPointF& pos) const;

    /**
     * @brief 检查点是否在锁定按钮区域内
     */
    bool isInLockButton(const QPointF& pos) const;

    /**
     * @brief 绘制关闭按钮
     */
    void paintCloseButton(QPainter* painter) const;

    /**
     * @brief 绘制锁定按钮
     */
    void paintLockButton(QPainter* painter) const;

private:
    QChart* m_chart = nullptr;                      ///< 所属图表
    QAbstractSeries* m_series = nullptr;            ///< 所属曲线系列（DataAnchored 模式）
    Mode m_mode = Mode::DataAnchored;               ///< 锚定模式
    QString m_text;                                 ///< 标签文本
    QPointF m_anchorValue;                          ///< 数据坐标锚点
    QPointF m_offset{8, -8};                        ///< 位置偏移
    qreal m_padding = 8.0;                          ///< 内边距
    qreal m_scaleFactor = 1.0;                      ///< 缩放因子
    bool m_locked = false;                          ///< 固定状态

    QColor m_bgColor{255, 255, 240, 230};           ///< 背景颜色（浅黄色半透明）
    QColor m_textColor{Qt::black};                  ///< 文本颜色
    QColor m_borderColor{Qt::black};                ///< 边框颜色

    QPointF m_dragStartPos;                         ///< 拖动起始位置
    bool m_isDragging = false;                      ///< 是否正在拖动

    bool m_hoverCloseButton = false;                ///< 鼠标是否悬停在关闭按钮上
    bool m_hoverLockButton = false;                 ///< 鼠标是否悬停在锁定按钮上
};

#endif // FLOATING_LABEL_H
