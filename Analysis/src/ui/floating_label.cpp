#include "floating_label.h"
#include <QApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtCharts/QChart>
#include <QtMath>

FloatingLabel::FloatingLabel(QChart* chart, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_chart(chart) {
    // 设置交互标志
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);

    // 默认模式：视图锁定（像素大小稳定）
    setFlag(ItemIgnoresTransformations, m_mode == Mode::ViewAnchored);

    // 设置高 Z 值，确保显示在曲线之上
    if (m_chart) {
        setZValue(m_chart->zValue() + 20);
    }
}

// ==================== 属性设置 ====================

void FloatingLabel::setText(const QString& text) {
    if (m_text != text) {
        m_text = text;
        prepareGeometryChange();
        update();
    }
}

void FloatingLabel::setMode(Mode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        setFlag(ItemIgnoresTransformations, m_mode == Mode::ViewAnchored);
        updateGeometry();
    }
}

void FloatingLabel::setAnchorValue(const QPointF& value, QAbstractSeries* series) {
    m_anchorValue = value;
    m_series = series;
    updateGeometry();
}

void FloatingLabel::setPadding(qreal padding) {
    if (m_padding != padding) {
        m_padding = padding;
        prepareGeometryChange();
        update();
    }
}

void FloatingLabel::setScaleFactor(qreal factor) {
    // 限制缩放范围 0.3 ~ 5.0
    qreal newFactor = qBound(0.3, factor, 5.0);
    if (!qFuzzyCompare(m_scaleFactor, newFactor)) {
        m_scaleFactor = newFactor;
        setScale(m_scaleFactor);
        update();
    }
}

void FloatingLabel::setLocked(bool locked) {
    if (m_locked != locked) {
        m_locked = locked;
        setFlag(ItemIsMovable, !m_locked);
        emit lockStateChanged(m_locked);
        update();
    }
}

// ==================== 几何更新 ====================

void FloatingLabel::updateGeometry() {
    if (!m_chart) {
        return;
    }

    if (m_mode == Mode::DataAnchored && m_series) {
        // 数据锚定模式：将数据坐标转换为视图坐标
        const QPointF scenePos = m_chart->mapToPosition(m_anchorValue, m_series);
        setPos(scenePos + m_offset);
    } else {
        // 视图锚定模式：保持当前位置不变
        // 不做任何操作，位置由用户拖动确定
    }

    prepareGeometryChange();
    update();
}

// ==================== QGraphicsItem 接口 ====================

QRectF FloatingLabel::boundingRect() const {
    QFontMetrics fm(qApp->font());
    QStringList lines = m_text.split('\n');

    // 计算多行文本的尺寸
    int maxWidth = 0;
    int totalHeight = 0;
    for (const QString& line : lines) {
        QSize sz = fm.size(Qt::TextSingleLine, line);
        maxWidth = qMax(maxWidth, sz.width());
        totalHeight += sz.height();
    }

    // 添加行间距
    if (lines.size() > 1) {
        totalHeight += fm.lineSpacing() * (lines.size() - 1);
    }

    // 按钮区域的额外宽度（关闭按钮 + 锁定按钮）
    const qreal buttonWidth = 36;  // 两个按钮，每个 18px

    // 计算总宽度和高度
    qreal totalWidth = maxWidth + buttonWidth + 2 * m_padding;
    totalHeight += 2 * m_padding;

    // 中心对齐
    return QRectF(-totalWidth / 2, -totalHeight / 2, totalWidth, totalHeight);
}

void FloatingLabel::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);

    QRectF rect = boundingRect();

    // 绘制背景和边框
    QPen borderPen(m_borderColor, 1);
    if (isSelected()) {
        borderPen.setColor(Qt::blue);
        borderPen.setWidth(2);
    }

    painter->setPen(borderPen);
    painter->setBrush(m_bgColor);
    painter->drawRoundedRect(rect, 6, 6);

    // 如果锁定，绘制锁定图案（右下角小锁图标）
    if (m_locked) {
        painter->save();
        painter->setPen(QPen(Qt::darkGray, 1));
        painter->setBrush(Qt::lightGray);
        QRectF lockIcon = QRectF(rect.right() - 16, rect.bottom() - 16, 12, 12);
        painter->drawRect(lockIcon);
        painter->restore();
    }

    // 绘制关闭按钮
    paintCloseButton(painter);

    // 绘制锁定按钮
    paintLockButton(painter);

    // 绘制文本（多行支持）
    painter->setPen(m_textColor);
    QFontMetrics fm(painter->font());
    QStringList lines = m_text.split('\n');

    // 计算文本区域（减去按钮宽度）
    QRectF textRect = rect.adjusted(m_padding, m_padding, -36 - m_padding, -m_padding);

    int y = textRect.top() + fm.ascent();
    for (const QString& line : lines) {
        painter->drawText(QRectF(textRect.left(), y - fm.ascent(), textRect.width(), fm.height()), Qt::AlignLeft | Qt::AlignVCenter, line);
        y += fm.height() + fm.lineSpacing();
    }
}

// ==================== 事件处理 ====================

void FloatingLabel::wheelEvent(QGraphicsSceneWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+滚轮：缩放标签
        qreal delta = event->delta() > 0 ? 1.1 : 0.9;
        setScaleFactor(m_scaleFactor * delta);
        event->accept();
    } else {
        // 让图表处理缩放
        QGraphicsObject::wheelEvent(event);
    }
}

void FloatingLabel::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF localPos = event->pos();

        // 检查是否点击关闭按钮
        if (isInCloseButton(localPos)) {
            event->accept();
            emit closeRequested();
            return;
        }

        // 检查是否点击锁定按钮
        if (isInLockButton(localPos)) {
            event->accept();
            setLocked(!m_locked);
            return;
        }

        // 开始拖动
        if (!m_locked) {
            m_dragStartPos = pos();
            m_isDragging = true;
        }
    }

    QGraphicsObject::mousePressEvent(event);
}

void FloatingLabel::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (!m_locked && m_isDragging) {
        QGraphicsObject::mouseMoveEvent(event);

        // 数据锚定模式：更新锚点
        if (m_mode == Mode::DataAnchored && m_series && m_chart) {
            QPointF scenePos = pos() - m_offset;
            QPointF dataValue = m_chart->mapToValue(scenePos, m_series);
            m_anchorValue = dataValue;
        }

        // 限制在 plotArea 内
        if (m_chart) {
            QRectF plotArea = m_chart->plotArea();
            QPointF p = pos();
            p.setX(qBound(plotArea.left(), p.x(), plotArea.right()));
            p.setY(qBound(plotArea.top(), p.y(), plotArea.bottom()));
            setPos(p);
        }
    }
}

void FloatingLabel::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    m_isDragging = false;
    QGraphicsObject::mouseReleaseEvent(event);
}

void FloatingLabel::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    setCursor(Qt::ArrowCursor);
    QGraphicsObject::hoverEnterEvent(event);
}

void FloatingLabel::hoverMoveEvent(QGraphicsSceneHoverEvent* event) {
    QPointF localPos = event->pos();

    bool wasHoverClose = m_hoverCloseButton;
    bool wasHoverLock = m_hoverLockButton;

    m_hoverCloseButton = isInCloseButton(localPos);
    m_hoverLockButton = isInLockButton(localPos);

    // 更新光标
    if (m_hoverCloseButton || m_hoverLockButton) {
        setCursor(Qt::PointingHandCursor);
    } else if (!m_locked) {
        setCursor(Qt::SizeAllCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }

    // 如果悬停状态改变，重绘
    if (wasHoverClose != m_hoverCloseButton || wasHoverLock != m_hoverLockButton) {
        update();
    }

    QGraphicsObject::hoverMoveEvent(event);
}

void FloatingLabel::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    m_hoverCloseButton = false;
    m_hoverLockButton = false;
    setCursor(Qt::ArrowCursor);
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

// ==================== 辅助函数 ====================

QRectF FloatingLabel::closeButtonRect() const {
    QRectF rect = boundingRect();
    qreal buttonSize = 16;
    return QRectF(rect.right() - buttonSize - 4, rect.top() + 4, buttonSize, buttonSize);
}

QRectF FloatingLabel::lockButtonRect() const {
    QRectF rect = boundingRect();
    qreal buttonSize = 16;
    return QRectF(rect.right() - 2 * buttonSize - 8, rect.top() + 4, buttonSize, buttonSize);
}

bool FloatingLabel::isInCloseButton(const QPointF& pos) const {
    return closeButtonRect().contains(pos);
}

bool FloatingLabel::isInLockButton(const QPointF& pos) const {
    return lockButtonRect().contains(pos);
}

void FloatingLabel::paintCloseButton(QPainter* painter) const {
    QRectF btnRect = closeButtonRect();

    painter->save();

    // 背景
    if (m_hoverCloseButton) {
        painter->setBrush(QColor(255, 100, 100, 180));
    } else {
        painter->setBrush(QColor(200, 200, 200, 100));
    }
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(btnRect);

    // 绘制 "×" 符号
    painter->setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap));
    qreal margin = 4;
    painter->drawLine(btnRect.left() + margin, btnRect.top() + margin, btnRect.right() - margin, btnRect.bottom() - margin);
    painter->drawLine(btnRect.right() - margin, btnRect.top() + margin, btnRect.left() + margin, btnRect.bottom() - margin);

    painter->restore();
}

void FloatingLabel::paintLockButton(QPainter* painter) const {
    QRectF btnRect = lockButtonRect();

    painter->save();

    // 背景
    if (m_hoverLockButton) {
        painter->setBrush(QColor(100, 150, 255, 180));
    } else {
        painter->setBrush(QColor(200, 200, 200, 100));
    }
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(btnRect);

    // 绘制锁图标
    painter->setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap));

    qreal cx = btnRect.center().x();
    qreal cy = btnRect.center().y();

    if (m_locked) {
        // 锁定状态：闭合的锁
        // 锁身（矩形）
        QRectF lockBody(cx - 3, cy, 6, 5);
        painter->drawRect(lockBody);

        // 锁扣（半圆弧）
        QPainterPath lockShackle;
        lockShackle.moveTo(cx - 3, cy);
        lockShackle.arcTo(QRectF(cx - 3, cy - 6, 6, 6), 180, 180);
        painter->drawPath(lockShackle);
    } else {
        // 解锁状态：打开的锁
        // 锁身（矩形）
        QRectF lockBody(cx - 3, cy, 6, 5);
        painter->drawRect(lockBody);

        // 锁扣（半圆弧，右侧打开）
        QPainterPath lockShackle;
        lockShackle.moveTo(cx - 3, cy);
        lockShackle.arcTo(QRectF(cx - 3, cy - 6, 6, 6), 180, 90);
        painter->drawPath(lockShackle);
    }

    painter->restore();
}
