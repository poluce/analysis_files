#include "differentiation_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QDebug>
#include <QVariant>
#include <QUuid>
#include <cmath>

DifferentiationAlgorithm::DifferentiationAlgorithm()
{
    qDebug() << "构造: DifferentiationAlgorithm";
}

QString DifferentiationAlgorithm::name() const
{
    return "differentiation";
}

QString DifferentiationAlgorithm::displayName() const
{
    return "微分";
}

QString DifferentiationAlgorithm::category() const
{
    return "Analysis";
}

IThermalAlgorithm::InputType DifferentiationAlgorithm::inputType() const
{
    // A类算法：单曲线，无需用户交互
    return InputType::None;
}

IThermalAlgorithm::OutputType DifferentiationAlgorithm::outputType() const
{
    // 输出新曲线（微分曲线）
    return OutputType::Curve;
}

AlgorithmDescriptor DifferentiationAlgorithm::descriptor() const
{
    AlgorithmDescriptor desc;
    desc.name = name();
    desc.displayName = displayName();
    desc.category = category();
    desc.needsParameters = false;
    desc.needsPointSelection = false;

    // 依赖声明（工作流支持）
    desc.prerequisites.append(ContextKeys::ActiveCurve);
    desc.produces.append(ProducesKeys::Curve);

    return desc;
}

// ==================== 曲线属性声明接口实现 ====================

bool DifferentiationAlgorithm::isAuxiliaryCurve() const
{
    // 微分曲线是独立曲线，不是辅助曲线
    // 原因：微分改变了数据类型（Raw → Derivative），应该创建新的Y轴
    return false;
}

bool DifferentiationAlgorithm::isStronglyBound() const
{
    // 微分曲线是非强绑定曲线
    // 原因：微分曲线是重要的分析结果，应该在树中独立显示，可独立操作
    return false;
}

// ==================== 上下文驱动执行接口实现（两阶段） ====================

bool DifferentiationAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "DifferentiationAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curve.has_value()) {
        qWarning() << "DifferentiationAlgorithm::prepareContext - 缺少活动曲线";
        return false;  // 数据不完整，无法执行
    }

    // 注入默认参数（如果上下文中不存在）
    if (!context->contains(ContextKeys::ParamHalfWin)) {
        context->setValue(ContextKeys::ParamHalfWin, m_halfWin, "DifferentiationAlgorithm");
    }
    if (!context->contains(ContextKeys::ParamDt)) {
        context->setValue(ContextKeys::ParamDt, m_dt, "DifferentiationAlgorithm");
    }
    if (!context->contains(ContextKeys::ParamEnableDebug)) {
        context->setValue(ContextKeys::ParamEnableDebug, m_enableDebug, "DifferentiationAlgorithm");
    }

    qDebug() << "DifferentiationAlgorithm::prepareContext - 数据就绪，参数已准备";
    return true;  // 数据完整，可以执行
}

AlgorithmResult DifferentiationAlgorithm::executeWithContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "DifferentiationAlgorithm::executeWithContext - 上下文为空！";
        return AlgorithmResult::failure("differentiation", "上下文为空");
    }

    // 从上下文拉取活动曲线（上下文存储的是副本，线程安全）
    auto curveOpt = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curveOpt.has_value()) {
        qWarning() << "DifferentiationAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("differentiation", "无法获取活动曲线");
    }

    const ThermalCurve& inputCurve = curveOpt.value();

    // 从上下文拉取参数（使用默认值作为fallback）
    int halfWin = context->get<int>(ContextKeys::ParamHalfWin).value_or(m_halfWin);
    double dt = context->get<double>(ContextKeys::ParamDt).value_or(m_dt);
    bool enableDebug = context->get<bool>(ContextKeys::ParamEnableDebug).value_or(m_enableDebug);

    // 获取输入数据
    const QVector<ThermalDataPoint>& inputData = inputCurve.getProcessedData();

    // 执行微分算法（核心逻辑）
    QVector<ThermalDataPoint> outputData;

    const int minPoints = 2 * halfWin + 1;
    if (inputData.size() < minPoints) {
        QString error = QString("数据点不足! 需要至少 %1 个点，实际只有 %2 个点")
                            .arg(minPoints).arg(inputData.size());
        qWarning() << "微分算法:" << error;
        return AlgorithmResult::failure("differentiation", error);
    }

    if (enableDebug) {
        qDebug() << "========== DTG微分算法开始（上下文驱动）==========";
        qDebug() << "输入数据点数:" << inputData.size();
        qDebug() << "半窗口大小:" << halfWin << "(从上下文获取)";
        qDebug() << "时间步长:" << dt << "(从上下文获取)";
    }

    const double windowTime = halfWin * dt;
    outputData.reserve(inputData.size() - 2 * halfWin);

    int positiveCount = 0;
    int negativeCount = 0;
    int zeroCount = 0;

    // 进度报告：计算总迭代次数
    const int totalIterations = inputData.size() - 2 * halfWin;
    int lastReportedProgress = 0;

    for (int i = halfWin; i < inputData.size() - halfWin; ++i) {
        // 检查取消标志（每100次迭代）
        if ((i - halfWin) % 100 == 0 && shouldCancel()) {
            qWarning() << "DifferentiationAlgorithm: 用户取消执行";
            return AlgorithmResult::failure("differentiation", "用户取消执行");
        }

        double sum_before = 0.0;
        double sum_after = 0.0;

        for (int j = 1; j <= halfWin; ++j) {
            sum_before += inputData[i - j].value;
            sum_after += inputData[i + j].value;
        }

        const double dy = sum_after - sum_before;
        const double derivative = dy / windowTime / halfWin;

        if (derivative > 0.0001) {
            positiveCount++;
        } else if (derivative < -0.0001) {
            negativeCount++;
        } else {
            zeroCount++;
        }

        ThermalDataPoint point;
        point.temperature = inputData[i].temperature;
        point.value = derivative;
        point.time = inputData[i].time;

        outputData.append(point);

        // 进度报告（每10%）
        int currentIteration = i - halfWin + 1;
        int currentProgress = (currentIteration * 100) / totalIterations;
        if (currentProgress >= lastReportedProgress + 10) {
            lastReportedProgress = currentProgress;
            reportProgress(currentProgress, QString("已处理 %1/%2 点").arg(currentIteration).arg(totalIterations));
        }
    }

    // 最终进度报告
    reportProgress(100, "微分计算完成");

    if (enableDebug) {
        qDebug() << "\n========== 微分统计 ==========";
        qDebug() << "输出数据点数:" << outputData.size();
        qDebug() << "正值点数:" << positiveCount << "(" << (100.0 * positiveCount / outputData.size()) << "%)";
        qDebug() << "负值点数:" << negativeCount << "(" << (100.0 * negativeCount / outputData.size()) << "%)";
        qDebug() << "接近零点数:" << zeroCount << "(" << (100.0 * zeroCount / outputData.size()) << "%)";
        qDebug() << "========== 微分算法结束 ==========\n";
    }

    // 创建结果对象
    AlgorithmResult result = AlgorithmResult::success(
        "differentiation",
        inputCurve.id(),
        ResultType::Curve
    );

    // 创建输出曲线
    ThermalCurve outputCurve(QUuid::createUuid().toString(), displayName());
    outputCurve.setProcessedData(outputData);
    outputCurve.setInstrumentType(inputCurve.instrumentType());
    // 微分算法：Raw → Derivative，其他保持不变
    SignalType outputSignalType = (inputCurve.signalType() == SignalType::Raw)
                                    ? SignalType::Derivative
                                    : inputCurve.signalType();
    outputCurve.setSignalType(outputSignalType);
    outputCurve.setParentId(inputCurve.id());
    outputCurve.setProjectName(inputCurve.projectName());
    outputCurve.setMetadata(inputCurve.getMetadata());
    outputCurve.setIsAuxiliaryCurve(this->isAuxiliaryCurve());  // 设置辅助曲线标志
    outputCurve.setIsStronglyBound(this->isStronglyBound());    // 设置强绑定标志

    // 填充结果
    result.setCurve(outputCurve);
    result.setSignalType(SignalType::Derivative);
    result.setMeta(MetaKeys::Unit, "mg/min");
    result.setMeta(MetaKeys::Label, "DTG");
    result.setMeta(MetaKeys::WindowSize, halfWin * 2 + 1);
    result.setMeta(MetaKeys::HalfWin, halfWin);
    result.setMeta(MetaKeys::Dt, dt);

    return result;
}
