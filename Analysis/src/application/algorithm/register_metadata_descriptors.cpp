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

static void registerDifferentiation() {
    AlgorithmDescriptor d;
    d.name = "differentiation";  // 必须与 DifferentiationAlgorithm::name() 返回值一致
    d.displayName = QObject::tr("微分");

    // 微分算法有默认参数，但通常不需要用户修改，可以直接执行
    // 如果需要高级用户可配置，可以取消下面的注释

    // ParameterDescriptor halfWin;
    // halfWin.name = "halfWin";  // 对应 ContextKeys::ParamHalfWin = "param.halfWin"
    // halfWin.label = QObject::tr("半窗口大小");
    // halfWin.type = ParamType::Integer;
    // halfWin.required = false;
    // halfWin.defaultValue = 50;
    // halfWin.intConstraint = IntConstraint{1, 500, 1};
    // halfWin.description = QObject::tr("用于平滑的半窗口大小");
    //
    // ParameterDescriptor dt;
    // dt.name = "dt";  // 对应 ContextKeys::ParamDt = "param.dt"
    // dt.label = QObject::tr("时间步长");
    // dt.type = ParamType::Double;
    // dt.required = false;
    // dt.defaultValue = 0.1;
    // dt.doubleConstraint = DoubleConstraint{0.001, 10.0, 0.01, "s"};
    // dt.description = QObject::tr("虚拟时间步长");
    //
    // d.params = { halfWin, dt };

    // 无参数，直接执行
    d.params = {};
    d.meta.insert("output", "AppendCurve");

    AlgorithmDescriptorRegistry::instance().registerDescriptor(d);
}

static void registerIntegration() {
    AlgorithmDescriptor d;
    d.name = "integration";  // 必须与 IntegrationAlgorithm::name() 返回值一致
    d.displayName = QObject::tr("积分");

    // 积分算法无可配置参数，直接执行
    d.params = {};
    d.meta.insert("output", "AppendCurve");

    AlgorithmDescriptorRegistry::instance().registerDescriptor(d);
}

static void registerTemperatureExtrapolation() {
    AlgorithmDescriptor d;
    d.name = "temperature_extrapolation";  // 必须与 TemperatureExtrapolationAlgorithm::name() 返回值一致
    d.displayName = QObject::tr("温度外推");

    // 无参数，但需要选择2个点定义切线区域
    d.params = {};

    PointSelectionSpec pick;
    pick.minCount = 2;
    pick.maxCount = 2;
    pick.hint = QObject::tr("请在曲线上选择两个点定义切线区域（用于外推起始/终止温度）");
    d.pointSelection = pick;

    d.meta.insert("output", "Marker");  // 输出标注点（Onset/Endset温度）

    AlgorithmDescriptorRegistry::instance().registerDescriptor(d);
}

static void registerPeakArea() {
    AlgorithmDescriptor d;
    d.name = "peak_area";  // 必须与 PeakAreaAlgorithm::name() 返回值一致
    d.displayName = QObject::tr("峰面积");

    // 无参数，但需要选择2个点定义积分范围
    d.params = {};

    PointSelectionSpec pick;
    pick.minCount = 2;
    pick.maxCount = 2;
    pick.hint = QObject::tr("请在曲线上选择两个点定义积分范围（起点和终点）");
    d.pointSelection = pick;

    d.meta.insert("output", "Composite");  // 输出混合结果（标注点 + 面积数值）

    AlgorithmDescriptorRegistry::instance().registerDescriptor(d);
}

void registerDefaultDescriptors() {
    registerMovingAverage();
    registerBaselineCorrection();
    registerDifferentiation();
    registerIntegration();
    registerTemperatureExtrapolation();
    registerPeakArea();
}

