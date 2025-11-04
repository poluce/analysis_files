#include "ui/controller/main_controller.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include "infrastructure/io/text_file_reader.h"
#include "ui/data_import_widget.h"
#include <QDebug>

#include "application/algorithm/algorithm_manager.h"
#include "application/history/algorithm_command.h"
#include "application/history/baseline_command.h"
#include "application/history/history_manager.h"
#include "domain/algorithm/i_thermal_algorithm.h"
#include "ui/chart_view.h"
#include "ui/controller/interaction_controller.h"
#include "ui/peak_area_dialog.h"
#include <QMessageBox>
#include <memory>

MainController::MainController(CurveManager* curveManager, QObject* parent)
    : QObject(parent)
    , m_curveManager(curveManager)
    , m_dataImportWidget(new DataImportWidget())
    , m_textFileReader(new TextFileReader())
    , m_algorithmManager(AlgorithmManager::instance())
    , m_historyManager(&HistoryManager::instance())
{
    Q_ASSERT(m_curveManager);
    Q_ASSERT(m_algorithmManager);
    Q_ASSERT(m_historyManager);
    qDebug() << "构造:  MainController";

    // 将 CurveManager 实例设置到算法服务中，以便能够创建新曲线
    m_algorithmManager->setCurveManager(m_curveManager);

    // ========== 信号连接设置 ==========
    // 命令路径：DataImportWidget → MainController
    connect(m_dataImportWidget, &DataImportWidget::previewRequested, this, &MainController::onPreviewRequested);
    connect(m_dataImportWidget, &DataImportWidget::importRequested, this, &MainController::onImportTriggered);

    // 通知路径：AlgorithmManager → MainController（可选的转发）
    connect(m_algorithmManager, &AlgorithmManager::algorithmFinished, this, &MainController::onAlgorithmFinished);
}

MainController::~MainController()
{
    delete m_dataImportWidget;
    delete m_textFileReader;
    delete m_peakAreaDialog;
    delete m_interactionController;
}

void MainController::setPlotWidget(ChartView* plotWidget)
{
    m_plotWidget = plotWidget;

    // 创建 InteractionController
    if (m_plotWidget && !m_interactionController) {
        m_interactionController = new InteractionController(m_plotWidget, this);

        // 连接 InteractionController 的信号
        connect(m_interactionController, &InteractionController::pointsSelected, this, &MainController::onPointsSelected);

        connect(m_interactionController, &InteractionController::interactionCancelled, this, [this]() {
            qDebug() << "MainController: 交互被取消";
            m_currentPickingPurpose = PickingPurpose::None;
            if (m_peakAreaDialog) {
                m_peakAreaDialog->close();
            }
        });

        qDebug() << "MainController: InteractionController 已创建并连接";
    }
}

void MainController::onShowDataImport() { m_dataImportWidget->show(); }

void MainController::onPreviewRequested(const QString& filePath)
{
    qDebug() << "控制器：收到预览文件请求：" << filePath;
    FilePreviewData preview = m_textFileReader->readPreview(filePath);
    m_dataImportWidget->setPreviewData(preview);
}

void MainController::onImportTriggered()
{
    qDebug() << "控制器：收到导入请求。";

    // 1. 从 m_dataImportWidget 获取所有用户配置
    QVariantMap config = m_dataImportWidget->getImportConfig();
    QString filePath = config.value("filePath").toString();

    if (filePath.isEmpty()) {
        qWarning() << "导入失败：文件路径为空。";
        return;
    }

    // 2. 调用 m_textFileReader->read()，并传入配置
    ThermalCurve curve = m_textFileReader->read(filePath, config);

    if (curve.id().isEmpty()) {
        qWarning() << "导入失败：读取文件后曲线无效。";
        return;
    }

    // 3. 清空已有曲线，确保只保留最新导入
    m_curveManager->clearCurves();

    // 4. 调用 m_curveManager->addCurve() 添加新曲线
    m_curveManager->addCurve(curve);

    // 5. 设置新导入的曲线为活动曲线（默认选中）
    m_curveManager->setActiveCurve(curve.id());

    // 6. 发射信号，通知UI更新
    emit curveAvailable(curve);

    // 7. 关闭导入窗口
    m_dataImportWidget->close();
}

// ========== 处理命令的槽函数（命令路径：UI → Controller → Service） ==========
// TODO:目前是通过mainwindow 中直接调用，后续也改为信号槽链接
// 不带参数的算法调用
void MainController::onAlgorithmRequested(const QString& algorithmName)
{
    qDebug() << "MainController: 接收到算法执行请求：" << algorithmName;

    // 1. 业务规则检查
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "没有可用的活动曲线来应用算法。";
        return;
    }

    // 2. 获取算法
    IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(algorithmName);
    if (!algorithm) {
        qWarning() << "找不到算法：" << algorithmName;
        return;
    }

    // 3. 创建新建曲线的算法命令
    auto command = std::make_unique<AlgorithmCommand>(algorithm, activeCurve, m_curveManager, algorithmName);

    // 4. 通过历史管理器执行命令（支持撤销/重做）
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "算法执行失败：" << algorithmName;
    }
    // 注意：新曲线的添加和UI更新由 CurveManager::curveAdded 信号自动触发
}
// TODO:目前是通过mainwindow 中直接调用，后续也改为信号槽链接
// 带参数的算法调用
void MainController::onAlgorithmRequestedWithParams(const QString& algorithmName, const QVariantMap& params)
{
    qDebug() << "带参数的算法调用";
    // 获取曲线
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "没有可用的活动曲线来应用算法";
        return;
    }

    // 获取使用的算法
    IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(algorithmName);
    if (!algorithm) {
        qWarning() << "找不到算法" << algorithmName;
        return;
    }

    // 设置算法参数
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        algorithm->setParameter(it.key(), it.value());
    }

    // 创建算法命令
    auto command = std::make_unique<AlgorithmCommand>(algorithm, activeCurve, m_curveManager, algorithmName);

    // 通过历史管理器执行命令（支持撤销/重做）
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "算法执行失败：" << algorithmName;
    }

    // 注意：新曲线的添加和UI更新由 CurveManager::curveAdded 信号自动触发
}

void MainController::onAlgorithmFinished(const QString& curveId) { emit curveDataChanged(curveId); }

void MainController::onUndo()
{
    qDebug() << "MainController: 执行撤销操作";

    if (!m_historyManager->canUndo()) {
        qDebug() << "MainController: 无可撤销的操作";
        return;
    }

    if (!m_historyManager->undo()) {
        qWarning() << "MainController: 撤销操作失败";
    }
    // 注意：曲线的删除由 CurveManager::curveRemoved 信号自动触发UI更新
}

void MainController::onRedo()
{
    qDebug() << "MainController: 执行重做操作";

    if (!m_historyManager->canRedo()) {
        qDebug() << "MainController: 无可重做的操作";
        return;
    }

    if (!m_historyManager->redo()) {
        qWarning() << "MainController: 重做操作失败";
    }
    // 注意：曲线的添加由 CurveManager::curveAdded 信号自动触发UI更新
}

// ========== 峰面积计算功能 ==========
void MainController::onPeakAreaRequested()
{
    qDebug() << "MainController::onPeakAreaRequested - 开始峰面积计算流程";

    // 1. 检查是否有活动曲线
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "  错误: 没有活动曲线";
        QMessageBox::warning(nullptr, tr("错误"), tr("请先选择一条曲线"));
        return;
    }

    qDebug() << "  活动曲线:" << activeCurve->name() << ", ID:" << activeCurve->id();

    // 2. 检查 InteractionController
    if (!m_interactionController) {
        qWarning() << "  错误: InteractionController 未初始化";
        QMessageBox::critical(nullptr, tr("错误"), tr("交互控制器未初始化"));
        return;
    }

    // 3. 创建并显示对话框
    if (!m_peakAreaDialog) {
        qDebug() << "  创建 PeakAreaDialog";
        m_peakAreaDialog = new PeakAreaDialog();

        // 连接取消信号
        connect(m_peakAreaDialog, &PeakAreaDialog::cancelled, this, [this]() {
            qDebug() << "MainController: 用户取消了峰面积计算";
            if (m_interactionController) {
                m_interactionController->cancelInteraction();
            }
            m_currentPickingPurpose = PickingPurpose::None;
        });
    }

    qDebug() << "  显示 PeakAreaDialog 并设置提示";
    m_peakAreaDialog->showPickingPrompt();
    m_peakAreaDialog->show();

    // 4. 使用 InteractionController 启动点拾取模式
    m_currentPickingPurpose = PickingPurpose::PeakArea;
    qDebug() << "  调用 InteractionController->startPointPicking(2)";
    m_interactionController->startPointPicking(2, tr("请在曲线上选择两个点来计算峰面积"));
    qDebug() << "  峰面积计算流程初始化完成";
}

// ========== 统一的点拾取完成处理 ==========
void MainController::onPointsSelected(const QVector<QPointF>& points)
{
    qDebug() << "MainController::onPointsSelected - 收到点拾取完成信号";
    qDebug() << "  当前拾取目的:" << (int)m_currentPickingPurpose;
    qDebug() << "  点数:" << points.size();

    if (points.size() < 2) {
        qWarning() << "  错误: 点数不足，需要至少2个点";
        m_currentPickingPurpose = PickingPurpose::None;
        return;
    }

    // 根据目的执行相应的操作
    switch (m_currentPickingPurpose) {
    case PickingPurpose::PeakArea:
        handlePeakAreaCalculation(points);
        break;

    case PickingPurpose::Baseline:
        handleBaselineDrawing(points);
        break;

    case PickingPurpose::None:
    default:
        qWarning() << "  警告: 没有指定拾取目的";
        break;
    }

    // 重置状态
    m_currentPickingPurpose = PickingPurpose::None;
}

void MainController::handlePeakAreaCalculation(const QVector<QPointF>& points)
{
    qDebug() << "MainController::handlePeakAreaCalculation";

    // 1. 获取活动曲线
    ThermalCurve* curve = m_curveManager->getActiveCurve();
    if (!curve) {
        qWarning() << "  错误: 没有活动曲线";
        QMessageBox::critical(nullptr, tr("错误"), tr("曲线不存在"));
        return;
    }

    qDebug() << "  曲线名称:" << curve->name();
    const auto& data = curve->getProcessedData();
    qDebug() << "  曲线数据点数:" << data.size();

    // 2. 找到两个点对应的X坐标
    double x1 = points[0].x();
    double x2 = points[1].x();

    if (x1 > x2) {
        std::swap(x1, x2);
    }

    qDebug() << "  计算峰面积，范围: X1=" << x1 << ", X2=" << x2;

    // 3. 使用梯形积分法计算峰面积
    qDebug() << "  开始梯形积分计算...";
    double area = 0.0;
    int segmentCount = 0;
    for (int i = 0; i < data.size() - 1; ++i) {
        double xa = data[i].temperature;
        double xb = data[i + 1].temperature;

        // 只计算在选定范围内的部分
        if (xb < x1 || xa > x2)
            continue;

        // 处理边界情况：线段部分在范围内
        double xStart = (xa < x1) ? x1 : xa;
        double xEnd = (xb > x2) ? x2 : xb;

        if (xStart >= xEnd)
            continue;

        // 线性插值获取边界点的Y值
        double ya = data[i].value;
        double yb = data[i + 1].value;

        double yStart, yEnd;
        if (xa == xb) {
            yStart = ya;
            yEnd = yb;
        } else {
            yStart = ya + (yb - ya) * (xStart - xa) / (xb - xa);
            yEnd = ya + (yb - ya) * (xEnd - xa) / (xb - xa);
        }

        // 梯形面积 = (yStart + yEnd) / 2 * (xEnd - xStart)
        double segmentArea = (yStart + yEnd) / 2.0 * (xEnd - xStart);
        area += segmentArea;
        segmentCount++;
    }

    qDebug() << "  积分完成: 计算了" << segmentCount << "个梯形段，总面积 =" << area;

    // 4. 显示结果
    QString unit = curve->getYAxisLabel() + QString::fromUtf8(" · °C");
    qDebug() << "  单位:" << unit << ", 绝对值面积:" << qAbs(area);

    if (m_peakAreaDialog) {
        qDebug() << "  调用 m_peakAreaDialog->showResult()";
        m_peakAreaDialog->showResult(qAbs(area), unit);
    } else {
        qWarning() << "  警告: m_peakAreaDialog 为空，无法显示结果";
    }

    // 5. 记录日志
    qInfo() << QString("峰面积计算完成: %1 到 %2, 面积 = %3").arg(x1).arg(x2).arg(area);
}

void MainController::handleBaselineDrawing(const QVector<QPointF>& points)
{
    qDebug() << "MainController::handleBaselineDrawing";

    if (!m_plotWidget) {
        qWarning() << "  错误: ChartView 指针为空，无法绘制基线";
        return;
    }

    // 获取活动曲线ID
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "  错误: 没有活动曲线";
        return;
    }

    QString curveId = activeCurve->id();
    qDebug() << "  曲线ID:" << curveId << ", 点数:" << points.size();
    for (int i = 0; i < points.size(); ++i) {
        qDebug() << "    点" << i << ":" << points[i];
    }

    // 创建 BaselineCommand
    qDebug() << "  创建 BaselineCommand";
    auto command = std::make_unique<BaselineCommand>(m_plotWidget, curveId, points[0], points[1]);

    // 通过历史管理器执行命令（支持撤销/重做）
    qDebug() << "  执行 BaselineCommand";
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "  错误: 基线绘制命令执行失败";
        QMessageBox::critical(nullptr, tr("错误"), tr("基线绘制失败"));
    } else {
        qInfo() << "  基线绘制成功";
    }
}

// ========== 基线绘制功能 ==========
void MainController::onBaselineRequested()
{
    qDebug() << "MainController::onBaselineRequested - 开始基线绘制流程";

    // 1. 检查是否有活动曲线
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "  错误: 没有活动曲线";
        QMessageBox::warning(nullptr, tr("错误"), tr("请先选择一条曲线"));
        return;
    }

    qDebug() << "  活动曲线:" << activeCurve->name() << ", ID:" << activeCurve->id();

    // 2. 检查 InteractionController
    if (!m_interactionController) {
        qWarning() << "  错误: InteractionController 未初始化";
        QMessageBox::critical(nullptr, tr("错误"), tr("交互控制器未初始化"));
        return;
    }

    // 3. 使用 InteractionController 启动点拾取模式
    m_currentPickingPurpose = PickingPurpose::Baseline;
    qDebug() << "  调用 InteractionController->startPointPicking(2)";
    m_interactionController->startPointPicking(2, tr("请在曲线上选择两个点来绘制基线"));

    // TODO: 可以创建一个类似的对话框提示用户
    QMessageBox::information(nullptr, tr("基线绘制"), tr("请在曲线上选择两个点来绘制基线"));
    qDebug() << "  基线绘制流程初始化完成";
}

// ========== 响应UI的曲线删除请求 ==========
void MainController::onCurveDeleteRequested(const QString& curveId)
{
    qDebug() << "MainController::onCurveDeleteRequested - 曲线ID:" << curveId;
    m_curveManager->removeCurve(curveId);
}

// ========== 响应UI的进度更新请求 ==========
void MainController::onPointPickingProgress(int picked, int total)
{
    qDebug() << "MainController::onPointPickingProgress - 进度:" << picked << "/" << total;
    if (m_peakAreaDialog) {
        m_peakAreaDialog->updateProgress(picked, total);
    }
}
