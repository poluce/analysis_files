#include "peak_area_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QColor>
#include <QDebug>
#include <QPointF>
#include <QVariant>
#include <QtMath>

PeakAreaAlgorithm::PeakAreaAlgorithm()
{
    qDebug() << "构造: PeakAreaAlgorithm";
}

QString PeakAreaAlgorithm::name() const
{
    return "peak_area";
}

QString PeakAreaAlgorithm::displayName() const
{
    return "峰面积";
}

QString PeakAreaAlgorithm::category() const
{
    return "Analysis";
}

SignalType PeakAreaAlgorithm::getOutputSignalType(SignalType /*inputType*/) const
{
    // 峰面积计算输出的是 PeakArea 类型
    return SignalType::PeakArea;
}

IThermalAlgorithm::InputType PeakAreaAlgorithm::inputType() const
{
    // B类算法：需要用户选择2个点
    return InputType::PointSelection;
}

IThermalAlgorithm::OutputType PeakAreaAlgorithm::outputType() const
{
    // 输出面积值和区域图
    return OutputType::Area;
}

AlgorithmDescriptor PeakAreaAlgorithm::descriptor() const
{
    AlgorithmDescriptor desc;
    desc.name = name();
    desc.interaction = AlgorithmInteraction::PointSelection;
    desc.requiredPointCount = 2;
    desc.pointSelectionHint = "请在曲线上选择两个点定义积分范围（起点和终点）";
    return desc;
}

// ==================== 曲线属性声明接口实现 ====================

bool PeakAreaAlgorithm::isAuxiliaryCurve() const
{
    // 峰面积不生成曲线，返回 false
    return false;
}

bool PeakAreaAlgorithm::isStronglyBound() const
{
    // 峰面积不生成曲线，返回 false
    return false;
}

// ==================== 上下文驱动执行接口实现 ====================

bool PeakAreaAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "PeakAreaAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curve.has_value()) {
        qWarning() << "PeakAreaAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 阶段2：验证交互式算法的选点数据
    // 峰面积计算需要用户选择至少2个点
    auto points = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!points.has_value() || points.value().size() < 2) {
        qWarning() << "PeakAreaAlgorithm::prepareContext - 需要至少2个选点，当前"
                   << (points.has_value() ? points.value().size() : 0) << "个";
        return false;  // 数据不完整，等待用户选点
    }

    qDebug() << "PeakAreaAlgorithm::prepareContext - 数据就绪，选点数:" << points.value().size();
    return true;
}

AlgorithmResult PeakAreaAlgorithm::executeWithContext(AlgorithmContext* context)
{
    // 1. 验证上下文
    if (!context) {
        qWarning() << "PeakAreaAlgorithm::executeWithContext - 上下文为空！";
        return AlgorithmResult::failure("peak_area", "上下文为空");
    }

    // 2. 拉取曲线（上下文存储的是副本，线程安全）
    auto curveOpt = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curveOpt.has_value()) {
        qWarning() << "PeakAreaAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("peak_area", "无法获取活动曲线");
    }

    const ThermalCurve& inputCurve = curveOpt.value();

    // 3. 拉取选择的点（ThermalDataPoint 类型）
    auto pointsOpt = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!pointsOpt.has_value()) {
        qWarning() << "PeakAreaAlgorithm::executeWithContext - 无法获取选择的点！";
        return AlgorithmResult::failure("peak_area", "无法获取选择的点");
    }

    QVector<ThermalDataPoint> selectedPoints = pointsOpt.value();
    if (selectedPoints.size() < 2) {
        QString error = QString("需要至少2个点，实际只有 %1 个点").arg(selectedPoints.size());
        qWarning() << "PeakAreaAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("peak_area", error);
    }

    // 4. 获取输入数据
    const QVector<ThermalDataPoint>& curveData = inputCurve.getProcessedData();
    if (curveData.isEmpty()) {
        qWarning() << "PeakAreaAlgorithm::executeWithContext - 曲线数据为空！";
        return AlgorithmResult::failure("peak_area", "曲线数据为空");
    }

    // 5. 提取温度范围（峰面积计算始终基于温度）
    double temp1 = selectedPoints[0].temperature;
    double temp2 = selectedPoints[1].temperature;

    // 确保 temp1 < temp2
    if (temp1 > temp2) {
        std::swap(temp1, temp2);
    }

    qDebug() << "PeakAreaAlgorithm::executeWithContext - 温度范围: [" << temp1 << "," << temp2 << "]";

    // 6. 执行核心算法逻辑（计算峰面积）
    double area = calculateArea(curveData, temp1, temp2);

    // 检查是否被用户取消
    if (shouldCancel()) {
        qWarning() << "PeakAreaAlgorithm::executeWithContext - 用户取消执行！";
        return AlgorithmResult::failure("peak_area", "用户取消执行");
    }

    qDebug() << "PeakAreaAlgorithm::executeWithContext - 计算得到峰面积:" << area;

    // 7. 创建结果对象（混合输出：标注点 + 面积值）
    AlgorithmResult result = AlgorithmResult::success(
        "peak_area",
        inputCurve->id(),
        ResultType::Composite  // 混合输出
    );

    // 添加起点和终点标注
    QPointF point1(selectedPoints[0].temperature, selectedPoints[0].value);
    QPointF point2(selectedPoints[1].temperature, selectedPoints[1].value);

    result.addMarker(point1, "积分起点");
    result.addMarker(point2, "积分终点");

    // 设置峰面积数值
    QString unit;
    switch (inputCurve->instrumentType()) {
        case InstrumentType::TGA:
            unit = "mg·°C";
            break;
        case InstrumentType::DSC:
            unit = "J/g";
            break;
        case InstrumentType::ARC:
            unit = "°C²";
            break;
        default:
            unit = "";
            break;
    }

    result.setArea(area, unit);

    // 格式化显示文本
    QString areaText = formatAreaText(area, inputCurve->instrumentType());

    // 添加元数据
    result.setMeta("peakArea", area);
    result.setMeta("temperatureRange", QString("%1 - %2").arg(temp1).arg(temp2));
    result.setMeta("instrumentType", static_cast<int>(inputCurve->instrumentType()));
    result.setMeta("label", areaText);  // 用于 UI 显示的文本
    result.setMeta("markerColor", QColor(Qt::blue));

    // 标签显示位置（两点中间）
    QPointF labelPos((point1.x() + point2.x()) / 2.0,
                     (point1.y() + point2.y()) / 2.0);
    result.setMeta("labelPosition", labelPos);

    return result;
}

double PeakAreaAlgorithm::calculateArea(const QVector<ThermalDataPoint>& curveData,
                                        double temp1, double temp2) const
{
    if (curveData.isEmpty()) {
        return 0.0;
    }

    // 使用梯形积分法计算面积
    // A = Σ [(y[i] + y[i+1]) / 2] * (x[i+1] - x[i])

    double area = 0.0;

    // 进度报告：计算总迭代次数
    const int totalIterations = curveData.size() - 1;
    int lastReportedProgress = 0;

    for (int i = 0; i < curveData.size() - 1; ++i) {
        // 检查取消标志（每100次迭代）
        if (i % 100 == 0 && shouldCancel()) {
            qWarning() << "PeakAreaAlgorithm: 用户取消执行";
            return 0.0;  // 返回0表示取消
        }

        double x1 = curveData[i].temperature;
        double x2 = curveData[i + 1].temperature;
        double y1 = curveData[i].value;
        double y2 = curveData[i + 1].value;

        // 检查数据点是否在积分范围内
        if (x2 < temp1 || x1 > temp2) {
            continue;  // 完全在范围外
        }

        // 裁剪到积分范围
        double effectiveX1 = qMax(x1, temp1);
        double effectiveX2 = qMin(x2, temp2);

        // 如果需要插值
        double effectiveY1 = y1;
        double effectiveY2 = y2;

        if (x1 < temp1 && x2 > temp1) {
            // 左边界需要插值
            double ratio = (temp1 - x1) / (x2 - x1);
            effectiveY1 = y1 + ratio * (y2 - y1);
        }

        if (x1 < temp2 && x2 > temp2) {
            // 右边界需要插值
            double ratio = (temp2 - x1) / (x2 - x1);
            effectiveY2 = y1 + ratio * (y2 - y1);
        }

        // 计算梯形面积
        double dx = effectiveX2 - effectiveX1;
        double avgY = (effectiveY1 + effectiveY2) / 2.0;
        area += avgY * dx;

        // 进度报告（每10%）
        int currentProgress = (i * 100) / totalIterations;
        if (currentProgress >= lastReportedProgress + 10) {
            lastReportedProgress = currentProgress;
            reportProgress(currentProgress, QString("计算峰面积 %1/%2").arg(i + 1).arg(totalIterations));
        }
    }

    // 最终进度报告
    reportProgress(100, "峰面积计算完成");

    return area;
}

QString PeakAreaAlgorithm::formatAreaText(double area, InstrumentType instrumentType) const
{
    QString unit;

    // 根据仪器类型确定单位
    switch (instrumentType) {
        case InstrumentType::TGA:
            unit = "mg·°C";  // 质量 × 温度
            break;
        case InstrumentType::DSC:
            unit = "J/g";    // 焓变单位
            break;
        case InstrumentType::ARC:
            unit = "°C²";    // 温度平方
            break;
        default:
            unit = "";       // 无单位
            break;
    }

    // 格式化数值，保留3位小数
    QString valueStr = QString::number(qAbs(area), 'f', 3);

    return QString("峰面积 = %1 %2").arg(valueStr, unit);
}
