#include "integration_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QDebug>
#include <QVariant>
#include <QUuid>
#include <QtGlobal>

IntegrationAlgorithm::IntegrationAlgorithm()
{
    qDebug() << "构造: IntegrationAlgorithm";
}

QString IntegrationAlgorithm::name() const
{
    return "integration";
}

QString IntegrationAlgorithm::displayName() const
{
    return "积分";
}

QString IntegrationAlgorithm::category() const
{
    return "Analysis";
}

IThermalAlgorithm::InputType IntegrationAlgorithm::inputType() const
{
    // A类算法：单曲线，无需用户交互
    return InputType::None;
}

IThermalAlgorithm::OutputType IntegrationAlgorithm::outputType() const
{
    // 输出新曲线（积分曲线）
    return OutputType::Curve;
}

AlgorithmDescriptor IntegrationAlgorithm::descriptor() const
{
    AlgorithmDescriptor desc;
    desc.name = name();
    desc.displayName = displayName();
    desc.category = category();
    desc.needsParameters = false;
    desc.needsPointSelection = false;
    // 暂无可配置参数，预留扩展（如方法/归一化等）

    // 依赖声明（工作流支持）
    desc.prerequisites.append(ContextKeys::ActiveCurve);
    desc.produces.append(ProducesKeys::Curve);

    return desc;
}

// ==================== 曲线属性声明接口实现 ====================

bool IntegrationAlgorithm::isAuxiliaryCurve() const
{
    // 积分曲线是独立曲线，不是辅助曲线
    // 原因：积分改变了数据类型（Derivative → Raw），应该创建新的Y轴
    return false;
}

bool IntegrationAlgorithm::isStronglyBound() const
{
    // 积分曲线是非强绑定曲线
    // 原因：积分曲线是重要的分析结果，应该在树中独立显示，可独立操作
    return false;
}

// ==================== 上下文驱动执行接口实现 ====================

bool IntegrationAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "IntegrationAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curve.has_value()) {
        qWarning() << "IntegrationAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 积分算法暂无可配置参数，预留扩展
    // 未来可以添加：积分方法（梯形/辛普森）、归一化选项等

    qDebug() << "IntegrationAlgorithm::prepareContext - 数据就绪";
    return true;
}

AlgorithmResult IntegrationAlgorithm::executeWithContext(AlgorithmContext* context)
{
    // 1. 验证上下文
    if (!context) {
        qWarning() << "IntegrationAlgorithm::executeWithContext - 上下文为空！";
        return AlgorithmResult::failure("integration", "上下文为空");
    }

    // 2. 拉取曲线（上下文存储的是副本，线程安全）
    auto curveOpt = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curveOpt.has_value()) {
        qWarning() << "IntegrationAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("integration", "无法获取活动曲线");
    }

    const ThermalCurve& inputCurve = curveOpt.value();

    // 3. 获取输入数据
    const QVector<ThermalDataPoint>& inputData = inputCurve.getProcessedData();

    // 4. 执行核心算法逻辑（梯形法则积分）
    QVector<ThermalDataPoint> outputData;
    const int n = inputData.size();
    if (n == 0) {
        qWarning() << "IntegrationAlgorithm::executeWithContext - 输入数据为空！";
        return AlgorithmResult::failure("integration", "输入数据为空");
    }

    outputData.resize(n);
    double cum = 0.0;

    // 第一个点的积分为0
    outputData[0] = inputData[0];
    outputData[0].value = 0.0;

    // 进度报告：计算总迭代次数
    int lastReportedProgress = 0;

    for (int i = 1; i < n; ++i) {
        // 检查取消标志（每100次迭代）
        if (i % 100 == 0 && shouldCancel()) {
            qWarning() << "IntegrationAlgorithm: 用户取消执行";
            return AlgorithmResult::failure("integration", "用户取消执行");
        }

        const auto& p0 = inputData[i - 1];
        const auto& p1 = inputData[i];
        const double dx = (p1.temperature - p0.temperature);
        if (!qFuzzyIsNull(dx)) {
            const double area = 0.5 * (p0.value + p1.value) * dx; // 梯形法则
            cum += area;
        }
        outputData[i] = p1;
        outputData[i].value = cum;

        // 进度报告（每10%）
        int currentProgress = (i * 100) / n;
        if (currentProgress >= lastReportedProgress + 10) {
            lastReportedProgress = currentProgress;
            reportProgress(currentProgress, QString("已处理 %1/%2 点").arg(i).arg(n));
        }
    }

    // 最终进度报告
    reportProgress(100, "积分计算完成");

    qDebug() << "IntegrationAlgorithm::executeWithContext - 完成，输出数据点数:" << outputData.size();

    // 5. 创建结果对象
    AlgorithmResult result = AlgorithmResult::success(
        "integration",
        inputCurve.id(),
        ResultType::Curve
    );

    // 创建输出曲线
    ThermalCurve outputCurve(QUuid::createUuid().toString(), displayName());
    outputCurve.setProcessedData(outputData);
    outputCurve.setInstrumentType(inputCurve.instrumentType());
    // 积分算法：Derivative → Raw，其他保持不变
    SignalType outputSignalType = (inputCurve.signalType() == SignalType::Derivative)
                                    ? SignalType::Raw
                                    : inputCurve.signalType();
    outputCurve.setSignalType(outputSignalType);
    outputCurve.setParentId(inputCurve.id());
    outputCurve.setProjectName(inputCurve.projectName());
    outputCurve.setMetadata(inputCurve.getMetadata());
    outputCurve.setIsAuxiliaryCurve(this->isAuxiliaryCurve());  // 设置辅助曲线标志
    outputCurve.setIsStronglyBound(this->isStronglyBound());    // 设置强绑定标志

    // 填充结果
    result.setCurve(outputCurve);
    result.setMeta("method", "Trapezoidal");
    result.setMeta("label", "积分曲线");

    return result;
}
