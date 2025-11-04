#include "InteractionController.h"
#include "domain/model/ThermalCurve.h"
#include "ui/ChartView.h"
#include <QDebug>

InteractionController::InteractionController(ChartView* plotWidget, QObject* parent)
    : QObject(parent)
    , m_plotWidget(plotWidget)
    , m_currentMode(Mode::None)
    , m_requiredPointCount(0)
    , m_mainCurve(nullptr)
    , m_referenceCurve(nullptr)
{
    qDebug() << "InteractionController 已创建";
}

InteractionController::~InteractionController() { qDebug() << "InteractionController 已销毁"; }

void InteractionController::startPointPicking(int numPoints, const QString& prompt)
{
    if (numPoints <= 0) {
        qWarning() << "点拾取模式：点数量必须大于 0";
        return;
    }

    if (!m_plotWidget) {
        qWarning() << "点拾取模式：ChartView 为空";
        return;
    }

    qDebug() << "启动点拾取模式：需要" << numPoints << "个点";
    qDebug() << "提示信息：" << prompt;

    // 重置状态
    resetState();

    // 设置新模式
    m_currentMode = Mode::PointSelection;
    m_currentPrompt = prompt;
    m_requiredPointCount = numPoints;
    m_selectedPoints.clear();
    m_selectedPoints.reserve(numPoints);

    // 通知模式改变
    emit modeChanged(m_currentMode, m_currentPrompt);

    // 使用 ChartView 的回调接口
    m_plotWidget->startPointPicking(
        numPoints, [this](const QString& curveId, const QVector<QPointF>& points) {
            qDebug() << "InteractionController: 收到点拾取回调，曲线ID =" << curveId
                     << ", 点数 =" << points.size();

            // 保存点和曲线ID
            m_selectedPoints = points;

            // 发出完成信号
            emit pointsSelected(points);

            // 重置状态
            resetState();
        });
}

void InteractionController::startMultiPointSelection(const QString& prompt)
{
    qDebug() << "启动多点选择模式";
    qDebug() << "提示信息：" << prompt;

    // 重置状态
    resetState();

    // 设置新模式
    m_currentMode = Mode::MultiPointSelection;
    m_currentPrompt = prompt;
    m_selectedPoints.clear();

    // 通知模式改变
    emit modeChanged(m_currentMode, m_currentPrompt);

    // TODO: 在 ChartView 上显示提示信息和完成按钮
}

void InteractionController::startDualCurveSelection(ThermalCurve* mainCurve, const QString& prompt)
{
    if (!mainCurve) {
        qWarning() << "双曲线选择模式：主曲线不能为空";
        return;
    }

    qDebug() << "启动双曲线选择模式";
    qDebug() << "主曲线：" << mainCurve->name();
    qDebug() << "提示信息：" << prompt;

    // 重置状态
    resetState();

    // 设置新模式
    m_currentMode = Mode::DualCurveSelection;
    m_currentPrompt = prompt;
    m_mainCurve = mainCurve;
    m_referenceCurve = nullptr;

    // 通知模式改变
    emit modeChanged(m_currentMode, m_currentPrompt);

    // TODO: 在 ChartView 上高亮主曲线
    // TODO: 显示提示信息让用户选择参考曲线
}

void InteractionController::cancelInteraction()
{
    qDebug() << "取消交互";

    Mode previousMode = m_currentMode;

    // 取消 ChartView 的点拾取
    if (m_plotWidget && previousMode == Mode::PointSelection) {
        m_plotWidget->cancelPointPicking();
    }

    resetState();

    if (previousMode != Mode::None) {
        emit interactionCancelled();
    }
}

void InteractionController::handlePointClicked(const QPointF& scenePos)
{
    qDebug() << "用户点击：" << scenePos;

    switch (m_currentMode) {
    case Mode::PointSelection: {
        // 添加点到集合
        m_selectedPoints.append(scenePos);
        qDebug() << "已选择" << m_selectedPoints.size() << "/" << m_requiredPointCount << "个点";

        // TODO: 在 ChartView 上绘制标记

        // 检查是否已收集足够的点
        if (m_selectedPoints.size() >= m_requiredPointCount) {
            finishPointPicking();
        }
        break;
    }

    case Mode::MultiPointSelection: {
        // 添加点到集合
        m_selectedPoints.append(scenePos);
        qDebug() << "已选择" << m_selectedPoints.size() << "个点（多点模式）";

        // TODO: 在 ChartView 上绘制标记
        // 用户需要手动完成选择（通过按钮或快捷键）
        break;
    }

    case Mode::DualCurveSelection: {
        // 在双曲线模式下，点击可能用于选择点或曲线
        // 目前先记录点击位置
        m_selectedPoints.append(scenePos);
        qDebug() << "双曲线模式：记录点击位置" << scenePos;

        // TODO: 实现曲线选择逻辑
        // TODO: 检测点击位置是否在某条曲线上
        break;
    }

    case Mode::None:
    default:
        // 非交互模式，忽略点击
        break;
    }
}

void InteractionController::resetState()
{
    m_currentMode = Mode::None;
    m_currentPrompt.clear();
    m_requiredPointCount = 0;
    m_selectedPoints.clear();
    m_mainCurve = nullptr;
    m_referenceCurve = nullptr;

    // TODO: 清除 ChartView 上的临时标记和高亮

    emit modeChanged(m_currentMode, QString());
}

void InteractionController::finishPointPicking()
{
    qDebug() << "点拾取完成：共" << m_selectedPoints.size() << "个点";

    // 复制点集合（避免 resetState 清空）
    QVector<QPointF> points = m_selectedPoints;

    // 重置状态
    Mode previousMode = m_currentMode;
    resetState();

    // 发出完成信号
    if (previousMode == Mode::PointSelection) {
        emit pointsSelected(points);
    }
}
