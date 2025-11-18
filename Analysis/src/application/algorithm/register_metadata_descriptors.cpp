#include "metadata_descriptor_registry.h"
#include <QObject>

using namespace App;

static void registerMovingAverage() {
    AlgorithmDescriptor d;
    d.name = "moving_average";  // 必须与 MovingAverageFilterAlgorithm::name() 返回值一致
    d.displayName = QObject::tr("移动平均滤波");

    // 参数1：窗口大小（必须与算法期望的键名一致：param.window）
    ParameterDescriptor win;
    win.name = "window";  // 对应 ContextKeys::ParamWindow = "param.window"
    win.label = QObject::tr("窗口尺寸");
    win.type = ParamType::Integer;
    win.required = true;
    win.defaultValue = 5;
    win.intConstraint = IntConstraint{1, 999, 2};
    // 移除描述字段，避免对话框中显示额外文本

    // 参数2：迭代次数（可选）
    ParameterDescriptor passes;
    passes.name = "passes";
    passes.label = QObject::tr("迭代次数");
    passes.type = ParamType::Integer;
    passes.required = false;
    passes.defaultValue = 1;
    passes.intConstraint = IntConstraint{1, 999, 1};  // 修正最大值为999

    d.params = { win, passes };
    d.meta.insert("output", "AppendCurve");

    AlgorithmDescriptorRegistry::instance().registerDescriptor(d);
}

static void registerBaselineCorrection() {
    AlgorithmDescriptor d;
    d.name = "baseline_correction";  // 必须与 BaselineCorrectionAlgorithm::name() 返回值一致
    d.displayName = QObject::tr("基线校正");

    ParameterDescriptor method;
    method.name = "method";
    method.label = QObject::tr("方法");
    method.type = ParamType::Enum;
    method.required = true;
    method.defaultValue = "Linear";
    method.enumOptions = {
        { "Linear", QObject::tr("线性") },
        { "Polynomial", QObject::tr("多项式") }
    };

    ParameterDescriptor order;
    order.name = "order";
    order.label = QObject::tr("多项式阶数");
    order.type = ParamType::Integer;
    order.required = false;
    order.defaultValue = 2;
    order.intConstraint = IntConstraint{1, 6, 1};
    order.description = QObject::tr("仅在方法选择“多项式”时生效");

    d.params = { method, order };

    PointSelectionSpec pick;
    pick.minCount = 2;
    pick.maxCount = 2;
    pick.hint = QObject::tr("请在主曲线上选择两个基线参考点");
    d.pointSelection = pick;

    d.meta.insert("output", "ReplaceCurve");

    AlgorithmDescriptorRegistry::instance().registerDescriptor(d);
}

void registerDefaultDescriptors() {
    registerMovingAverage();
    registerBaselineCorrection();
}

