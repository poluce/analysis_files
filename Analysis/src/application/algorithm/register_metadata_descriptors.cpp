#include "metadata_descriptor_registry.h"
#include <QObject>

using namespace App;

static void registerMovingAverage() {
    AlgorithmDescriptor d;
    d.name = "MovingAverageFilter";
    d.displayName = QObject::tr("移动平均滤波");

    ParameterDescriptor win;
    win.name = "windowSize";
    win.label = QObject::tr("窗口尺寸");
    win.type = ParamType::Integer;
    win.required = true;
    win.defaultValue = 5;
    win.intConstraint = IntConstraint{1, 999, 2};
    win.description = QObject::tr("建议使用奇数窗口以获得中心对齐效果");

    ParameterDescriptor passes;
    passes.name = "passes";
    passes.label = QObject::tr("迭代次数");
    passes.type = ParamType::Integer;
    passes.required = false;
    passes.defaultValue = 1;
    passes.intConstraint = IntConstraint{1, 10, 1};

    d.params = { win, passes };
    d.meta.insert("output", "AppendCurve");

    AlgorithmDescriptorRegistry::instance().registerDescriptor(d);
}

static void registerBaselineCorrection() {
    AlgorithmDescriptor d;
    d.name = "BaselineCorrection";
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

