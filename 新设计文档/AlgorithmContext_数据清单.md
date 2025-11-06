# AlgorithmContext 上下文数据清单

基于代码分析，梳理出 AlgorithmContext 需要管理的所有上下文数据。

---

## 一、数据分类体系

### 📁 1. 原始实验数据 (Raw Data)
**来源**: Form 通过 dataReady 信号传入
**生命周期**: 从数据导入开始，贯穿整个分析流程

| 键名模板 | 数据类型 | 说明 | 来源 |
|---------|---------|------|------|
| `温度{N}` | `QList<double>` | 第 N 个数据集的温度序列 | Form |
| `时间{N}` | `QList<double>` | 第 N 个数据集的时间序列（经过采样频率校正） | Form |
| `{自定义列名}{N}` | `QList<double>` | 自定义信号列（如"质量%"、"热流"） | Form |
| `速率{N}` | `QList<double>` | 升温/降温速率（可选） | Form |

**示例**:
```cpp
context.set("温度1", temperatureData, "Form/DataImport");
context.set("时间1", timeData, "Form/DataImport");
context.set("质量%1", massData, "Form/DataImport");
```

---

### 📊 2. 曲线选择与激活状态 (Curve Selection)
**来源**: 用户通过图例点击选择曲线
**用途**: 决定后续算法操作的目标曲线

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `selectedSeries` | `QString` | 当前选中的曲线名称 | UI/LegendClick |
| `selectedSeriesType` | `SeriesType` (enum) | 当前选中曲线的类型 | UI/LegendClick |
| `selectedView` | `QString` | 选中曲线所在的视图 ("View1" / "View2") | UI/LegendClick |
| `activeSeriesData` | `QVector<QPointF>` | 当前激活曲线的完整数据点 | UI/CurveData |

**示例**:
```cpp
context.set("selectedSeries", "质量%1", "UI/LegendClick");
context.set("selectedSeriesType", QVariant::fromValue(SeriesType::Raw), "UI/LegendClick");
```

---

### 🎯 3. 用户交互选点数据 (User Interaction Points)
**来源**: 用户在图表上点击选择的点
**用途**: 基线绘制、峰面积计算、温度标记等

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `selectedXPoints` | `QList<double>` | 选中点的 X 坐标列表 | UI/PointSelection |
| `selectedYPoints` | `QList<double>` | 选中点的 Y 坐标列表 | UI/PointSelection |
| `selectionMode` | `QString` | 当前选点模式 | UI/ModeSwitch |
| `lastClickedPoint` | `QPointF` | 最后一次点击的点 | UI/PointSelection |

**选点模式枚举值**:
- `"baseline"` - 基线选点
- `"ladder"` - 阶梯选点
- `"reaction"` - 反应过程选点
- `"area"` - 峰面积选点
- `"massloss"` - 质量损失选点
- `"bolihua"` - 玻璃化转变选点
- `"endwires"` - 末端选点
- `"end"` - 终点选点

**示例**:
```cpp
context.set("selectionMode", "baseline", "UI/BaselineButton");
context.set("selectedXPoints", QVariant::fromValue(xPoints), "UI/PointSelection");
context.set("selectedYPoints", QVariant::fromValue(yPoints), "UI/PointSelection");
```

---

### 📐 4. 基线数据 (Baseline Data)
**来源**: 基线计算算法 (Form1)
**用途**: 背景扣除、质量损失计算

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `baselineType` | `int` | 基线类型 (0=线性, 1=多项式) | Form1/Algorithm |
| `baselineSeries` | `QVector<QPointF>` | 基线曲线数据点 | BaselineAlgorithm |
| `baselineP1` | `QPointF` | 基线起点 | BaselineAlgorithm |
| `baselineP2` | `QPointF` | 基线终点 | BaselineAlgorithm |
| `baselineCoefficients` | `QList<double>` | 基线拟合系数 | BaselineAlgorithm |
| `adjustedBaselineSeries` | `QVector<QPointF>` | 调整后的基线 | BaselineAdjustment |

**示例**:
```cpp
context.set("baselineType", 0, "Form1/LinearBaseline");
context.set("baselineSeries", baselineData, "BaselineAlgorithm");
```

---

### 🔬 5. 导数与滤波数据 (Derivative & Filtering)
**来源**: Form2, Form3 算法
**用途**: DTG 曲线、滤波后的平滑曲线

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `dtgSeries` | `QVector<QPointF>` | DTG（导数）曲线 | DerivativeAlgorithm |
| `filteredSeries` | `QVector<QPointF>` | 滤波后的曲线 | FilterAlgorithm |
| `filterType` | `QString` | 滤波类型 ("FFT", "MovingAverage") | Form3 |

---

### 📈 6. 拟合数据 (Fitting Data)
**来源**: FormFitting 算法
**用途**: 曲线拟合、峰拟合

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `fitSeries` | `QVector<QPointF>` | 拟合曲线数据 | FittingAlgorithm |
| `fitCoefficients` | `QList<double>` | 拟合系数 | FittingAlgorithm |
| `fitType` | `QString` | 拟合类型 ("polynomial", "linear") | FormFitting |
| `fitDegree` | `int` | 多项式拟合阶数 | FormFitting |
| `fitRSquared` | `double` | 拟合优度 R² | FittingAlgorithm |

---

### 🎯 7. 峰值分析数据 (Peak Analysis)
**来源**: Form7 峰值分析
**用途**: 峰值标记、峰面积计算

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `peakPoint` | `QPointF` | 峰值点坐标 | PeakAlgorithm |
| `peakXValues` | `QList<double>` | 多个峰值的 X 坐标 | PeakAlgorithm |
| `peakArea` | `double` | 峰面积 | AreaAlgorithm |
| `intersection1` | `QPointF` | 积分区间起点 | AreaAlgorithm |
| `intersection2` | `QPointF` | 积分区间终点 | AreaAlgorithm |

---

### 🌡️ 8. 温度与热流分析数据 (Temperature & Heat Flow)
**来源**: FormTemp
**用途**: 温度校正、热流分析

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `heatFlowSeries` | `QVector<QPointF>` | 热流曲线 | HeatFlowAlgorithm |
| `temperatureCorrected` | `QList<double>` | 温度校正后的数据 | TempAlgorithm |

---

### 🔥 9. 动力学分析数据 (Kinetic Analysis)
**来源**: Form4, Form5, Form6 (Kissinger, Friedman, E-I-O)
**用途**: 活化能计算、反应机理

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `activationEnergy` | `double` | 活化能 Ea (kJ/mol) | KineticAlgorithm |
| `preExponentialFactor` | `double` | 指前因子 A | KineticAlgorithm |
| `reactionOrder` | `double` | 反应级数 n | KineticAlgorithm |
| `allXValues` | `QList<QList<double>>` | Kissinger 法 X 值 | Form4 |
| `allLogValues` | `QList<QList<double>>` | Kissinger 法 log 值 | Form4 |
| `B1` | `QList<double>` | 拟合系数 B1 | Form4/Form5 |
| `B2` | `QList<double>` | 拟合系数 B2 | Form4/Form5 |

---

### 🧪 10. 玻璃化转变数据 (Glass Transition)
**来源**: Form8
**用途**: Tg 温度测定

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `tg_onset` | `double` | 玻璃化转变起始温度 | GlassTransitionAlgorithm |
| `tg_midpoint` | `double` | 玻璃化转变中点温度 | GlassTransitionAlgorithm |
| `tg_endpoint` | `double` | 玻璃化转变终点温度 | GlassTransitionAlgorithm |

---

### ⚗️ 11. 反应过程数据 (Reaction Process)
**来源**: Form9
**用途**: 反应阶段划分

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `reactionStages` | `QList<QVariantMap>` | 反应阶段列表（每个阶段包含温度范围、质量损失等） | ReactionAlgorithm |
| `massLoss` | `QList<double>` | 各阶段质量损失 | ReactionAlgorithm |
| `Mass` | `QList<double>` | 质量百分比数据 | MassCalculation |

---

### 🎨 12. 坐标轴与显示配置 (Axis & Display Config)
**来源**: Form, UI 操作
**用途**: 图表显示、标签设置

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `xAxisKey` | `QString` | 当前 X 轴数据键名 | UI/AxisConfig |
| `yAxisKey` | `QString` | 当前 Y 轴数据键名 | UI/AxisConfig |
| `xAxisLabel` | `QString` | X 轴标签文本 | Form |
| `yAxisLabel` | `QString` | Y 轴标签文本 | Form |
| `xAxisUnit` | `QString` | X 轴单位 | Form |
| `yAxisUnit` | `QString` | Y 轴单位 | Form |

---

### 🗂️ 13. 文件与数据集管理 (File & Dataset Management)
**来源**: MainWindow 数据管理
**用途**: 多文件数据缓存

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `fileDataMap` | `QMap<QString, QList<QList<double>>>` | 文件名 -> 数据列表映射 | MainWindow |
| `dataList` | `QList<QVariantMap>` | 所有导入的数据集列表 | MainWindow |
| `currentDatasetIndex` | `int` | 当前操作的数据集索引 | MainWindow |
| `currentFileName` | `QString` | 当前文件名 | Form |

---

### 🎛️ 14. 图表交互状态 (Chart Interaction State)
**来源**: UI 交互
**用途**: 控制图表行为

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `chartInteractionMode` | `QString` | 图表交互模式 | UI |
| `lastActiveView` | `QString` | 最后活动的视图 | UI |
| `zoomLevel` | `double` | 当前缩放级别 | UI |

**交互模式值**:
- `"RubberBand"` - 框选缩放
- `"Drag"` - 拖动平移
- `"Disabled"` - 禁用交互

---

### 🔧 15. 算法中间结果 (Algorithm Intermediate Results)
**来源**: 各类算法计算过程
**用途**: 算法链式调用、结果复用

| 键名 | 数据类型 | 说明 | 来源 |
|------|---------|------|------|
| `pointInBetween` | `QPointF` | 中间点坐标 | Algorithm |
| `baselinePoint` | `QPointF` | 基线参考点 | Algorithm |
| `Baseline_fit` | `QVector<QPointF>` | 基线拟合点集 | BaselineAlgorithm |

---

## 二、数据使用场景举例

### 场景 1: 用户导入数据
```cpp
// Form 发出数据
QVariantMap rawData;
rawData["温度"] = temperatureList;
rawData["时间"] = timeList;
rawData["质量%"] = massList;
emit dataReady(rawData);

// MainWindow 接收并存入 Context
void MainWindow::onDataReady(const QVariantMap& data) {
    int index = dataList.size() + 1;
    context.set(QString("温度%1").arg(index), data["温度"], "Form/DataImport");
    context.set(QString("时间%1").arg(index), data["时间"], "Form/DataImport");
    context.set(QString("质量%%1").arg(index), data["质量%"], "Form/DataImport");
}
```

### 场景 2: 用户选择曲线并选点
```cpp
// 1. 图例点击
void MainWindow::onLegendMarkerClicked(...) {
    context.set("selectedSeries", "质量%1", "UI/LegendClick");
    context.set("selectedSeriesType", QVariant::fromValue(SeriesType::Raw), "UI");
}

// 2. 进入基线模式
void MainWindow::on_toolButton_baseline_clicked() {
    context.set("selectionMode", "baseline", "UI/BaselineButton");
    selectingBaselinePoints = true;
}

// 3. 用户点击图表
void MainWindow::handlePointSelected(QPointF point) {
    QString mode = context.get<QString>("selectionMode");
    if (mode == "baseline") {
        // 累积选点
        selectedXPoints.append(point.x());
        selectedYPoints.append(point.y());

        context.set("selectedXPoints", QVariant::fromValue(selectedXPoints), "UI/PointSelection");
        context.set("selectedYPoints", QVariant::fromValue(selectedYPoints), "UI/PointSelection");

        // 满两点后执行基线计算
        if (selectedXPoints.size() >= 2) {
            calculateBaseline();
        }
    }
}
```

### 场景 3: 基线计算并保存结果
```cpp
void MainWindow::calculateBaseline() {
    // 从 Context 获取输入
    QString curveName = context.get<QString>("selectedSeries");
    QList<double> xPoints = context.get<QList<double>>("selectedXPoints");
    QList<double> yPoints = context.get<QList<double>>("selectedYPoints");

    // 获取原始曲线数据
    QVector<QPointF> curveData = context.get<QVector<QPointF>>("activeSeriesData");

    // 计算基线
    QVector<QPointF> baseline = computeLinearBaseline(xPoints[0], yPoints[0],
                                                       xPoints[1], yPoints[1],
                                                       curveData);

    // 结果写回 Context
    context.set("baselineType", 0, "BaselineAlgorithm/Linear");
    context.set("baselineSeries", QVariant::fromValue(baseline), "BaselineAlgorithm");
    context.set("baselineP1", QPointF(xPoints[0], yPoints[0]), "BaselineAlgorithm");
    context.set("baselineP2", QPointF(xPoints[1], yPoints[1]), "BaselineAlgorithm");

    // 绘制基线到图表
    drawBaseline();
}
```

### 场景 4: Kissinger 动力学分析
```cpp
void MainWindow::handleForm4ButtonClick() {
    // 从 Context 获取所需数据
    QString seriesName = context.get<QString>("selectedSeries");
    QVector<QPointF> curveData = context.get<QVector<QPointF>>("activeSeriesData");
    QList<double> xPoints = context.get<QList<double>>("selectedXPoints");

    // 执行 Kissinger 计算
    QList<QList<double>> xValues, logValues;
    QList<double> B2Coeffs;
    // ... 计算过程 ...

    // 结果存入 Context
    context.set("allXValues", QVariant::fromValue(xValues), "Form4/Kissinger");
    context.set("allLogValues", QVariant::fromValue(logValues), "Form4/Kissinger");
    context.set("B2", QVariant::fromValue(B2Coeffs), "Form4/Kissinger");
    context.set("activationEnergy", Ea, "Form4/Kissinger");
}
```

---

## 三、AlgorithmContext 类的键名命名规范

### 规范建议

1. **原始数据**: 使用 `{列名}{数据集索引}` 格式
   - 例: `温度1`, `质量%2`, `时间3`

2. **曲线选择**: 使用 `selected` 前缀
   - 例: `selectedSeries`, `selectedView`, `selectedSeriesType`

3. **用户交互**: 使用描述性名称
   - 例: `selectedXPoints`, `selectionMode`, `lastClickedPoint`

4. **算法结果**: 使用算法类型前缀
   - 例: `baselineSeries`, `fitCoefficients`, `dtgSeries`

5. **配置参数**: 使用 `Config` 或描述性后缀
   - 例: `xAxisLabel`, `filterType`, `fitDegree`

---

## 四、建议的 AlgorithmContext 扩展接口

基于实际数据需求，建议添加以下接口：

```cpp
class AlgorithmContext : public QObject {
    Q_OBJECT

public:
    // 批量获取数据集
    QVariantMap getDataset(int index) const;

    // 获取所有选中点
    QVector<QPointF> getSelectedPoints() const;

    // 清除特定模式的临时数据
    void clearSelectionData();

    // 获取最新的算法结果
    template<typename T>
    T getLatestResult(const QString& keyPrefix) const;

    // 导出当前状态为快照
    QVariantMap exportSnapshot() const;

    // 从快照恢复
    void restoreFromSnapshot(const QVariantMap& snapshot);

signals:
    // 特定类型数据更新信号
    void curveSelectionChanged(const QString& seriesName);
    void selectionModeChanged(const QString& mode);
    void algorithmResultReady(const QString& algorithmName, const QVariantMap& result);
};
```

---

## 五、总结

根据代码分析，AlgorithmContext 需要管理 **15 大类、约 60+ 个数据项**，涵盖：

- ✅ 原始实验数据（温度、时间、质量、热流等）
- ✅ 用户交互数据（选点、曲线选择、模式切换）
- ✅ 算法计算结果（基线、拟合、峰值、动力学参数）
- ✅ 中间计算数据（导数、滤波、质量计算）
- ✅ 显示配置（坐标轴、标签、单位）
- ✅ 状态管理（交互模式、当前视图、数据集索引）

这些数据当前分散在 MainWindow 的 **50+ 个成员变量** 中，引入 AlgorithmContext 后可以：

1. **统一管理** - 所有数据集中存储和访问
2. **追踪来源** - 明确数据从哪个模块产生
3. **时间语义** - 自动记录数据更新时间
4. **响应式更新** - 通过信号驱动算法自动执行
5. **降低耦合** - Form 和 MainWindow 通过 Context 解耦

---

**文档版本**: v1.0
**更新时间**: 2025-11-06
**作者**: Claude Code Analysis
