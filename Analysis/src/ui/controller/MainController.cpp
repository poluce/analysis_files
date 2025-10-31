#include "ui/controller/MainController.h"
#include "application/curve/CurveManager.h"
#include "domain/model/ThermalCurve.h"
#include "ui/DataImportWidget.h"
#include "infrastructure/io/TextFileReader.h"
#include <QDebug>

#include "application/algorithm/AlgorithmService.h"
#include "domain/algorithm/IThermalAlgorithm.h"

MainController::MainController(CurveManager* curveManager, QObject *parent)
    : QObject(parent),
      m_curveManager(curveManager),
      m_dataImportWidget(new DataImportWidget()),
      m_textFileReader(new TextFileReader()),
      m_algorithmService(AlgorithmService::instance())
{
    Q_ASSERT(m_curveManager);
    Q_ASSERT(m_algorithmService);

    // 将 CurveManager 实例设置到算法服务中，以便能够创建新曲线
    m_algorithmService->setCurveManager(m_curveManager);

    // ========== 信号连接设置 ==========
    // 命令路径：DataImportWidget → MainController
    connect(m_dataImportWidget, &DataImportWidget::previewRequested,
            this, &MainController::onPreviewRequested);
    connect(m_dataImportWidget, &DataImportWidget::importRequested,
            this, &MainController::onImportTriggered);

    // 通知路径：AlgorithmService → MainController（可选的转发）
    connect(m_algorithmService, &AlgorithmService::algorithmFinished,
            this, &MainController::onAlgorithmFinished);
}

MainController::~MainController()
{
    delete m_dataImportWidget;
    delete m_textFileReader;
}

void MainController::onShowDataImport()
{
    m_dataImportWidget->show();
}

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

    // 3. 调用 m_curveManager->addCurve() 添加新曲线
    m_curveManager->addCurve(curve);

    // 4. 发射信号，通知UI更新
    emit curveAvailable(curve);

    // 5. 关闭导入窗口
    m_dataImportWidget->close();
}

// ========== 处理命令的槽函数（命令路径：UI → Controller → Service） ==========
void MainController::onAlgorithmRequested(const QString& algorithmName)
{
    qDebug() << "MainController: 接收到算法执行请求：" << algorithmName;

    // 1. 业务规则检查
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "没有可用的活动曲线来应用算法。";
        return;
    }

    // 2. 调用 Service 执行算法
    m_algorithmService->execute(algorithmName, activeCurve);

    // 3. Service 会创建新曲线并调用 CurveManager::addCurve()
    // 4. CurveManager 会发射 curveAdded 信号
    // 5. PlotWidget 监听该信号并自动显示新曲线
}

void MainController::onAlgorithmRequestedWithParams(const QString& algorithmName, const QVariantMap& params)
{
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "没有可用的活动曲线来应用算法";
        return;
    }

    IThermalAlgorithm* algo = m_algorithmService->getAlgorithm(algorithmName);
    if (!algo) {
        qWarning() << "找不到算法" << algorithmName;
        return;
    }
    // 设置参数
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        algo->setParameter(it.key(), it.value());
    }
    // 执行
    m_algorithmService->execute(algorithmName, activeCurve);
}

void MainController::onAlgorithmFinished(const QString& curveId)
{
    emit curveDataChanged(curveId);
}
