#include "BaselineCommand.h"
#include "ui/PlotWidget.h"
#include <QUuid>
#include <QPen>
#include <QDebug>

BaselineCommand::BaselineCommand(PlotWidget* plotWidget,
                                 const QString& curveId,
                                 const QPointF& start,
                                 const QPointF& end)
    : m_plotWidget(plotWidget),
      m_curveId(curveId),
      m_start(start),
      m_end(end)
{
    // 生成唯一的基线ID
    m_lineId = QUuid::createUuid().toString();
}

bool BaselineCommand::execute()
{
    if (!m_plotWidget) {
        qWarning() << "BaselineCommand: PlotWidget 指针为空";
        return false;
    }

    // 添加基线到图表（红色虚线）
    QPen pen(Qt::red, 2, Qt::DashLine);
    m_plotWidget->addAnnotationLine(m_lineId, m_curveId, m_start, m_end, pen);

    qDebug() << "BaselineCommand: 执行成功，基线ID:" << m_lineId;
    return true;
}

bool BaselineCommand::undo()
{
    if (!m_plotWidget) {
        qWarning() << "BaselineCommand: PlotWidget 指针为空";
        return false;
    }

    m_plotWidget->removeAnnotation(m_lineId);

    qDebug() << "BaselineCommand: 撤销成功，基线ID:" << m_lineId;
    return true;
}

QString BaselineCommand::description() const
{
    return QObject::tr("绘制基线");
}
