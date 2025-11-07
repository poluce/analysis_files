# 统一算法输出设计 - AlgorithmResult 容器

## 🎯 设计目标

创建一个**统一的结果容器类**，封装所有算法输出类型，提供结构化、类型安全的结果访问接口。

### 核心理念
```
算法输入：AlgorithmContext (拉取数据)
           ↓
      算法执行逻辑
           ↓
算法输出：AlgorithmResult (结构化结果)
```

---

## 📦 核心设计

### 1️⃣ 结果类型枚举

```cpp
// src/domain/algorithm/algorithm_result.h

/**
 * @brief 算法输出结果类型
 */
enum class ResultType {
    Curve,          // 输出曲线（如微分、积分、滤波后的曲线）
    Marker,         // 标注点（如峰值点、外推点、拐点）
    Region,         // 区域（如峰面积、积分区域）
    ScalarValue,    // 单个数值（如外推温度、峰高、斜率）
    Composite       // 混合结果（包含多种类型）
};
```

### 2️⃣ 结果容器类

```cpp
// src/domain/algorithm/algorithm_result.h

#include <QString>
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QVariantMap>
#include <QDateTime>
#include "domain/model/thermal_curve.h"

/**
 * @brief 算法执行结果的统一容器
 *
 * 封装所有类型的算法输出，提供类型安全的访问接口。
 * 支持单一输出和混合输出。
 */
class AlgorithmResult {
public:
    // ==================== 构造函数 ====================

    /**
     * @brief 默认构造函数（表示失败或空结果）
     */
    AlgorithmResult()
        : m_success(false)
        , m_type(ResultType::ScalarValue)
        , m_timestamp(QDateTime::currentDateTime())
    {}

    /**
     * @brief 创建成功的结果
     */
    static AlgorithmResult success(
        const QString& algorithmKey,
        const QString& parentCurveId,
        ResultType type)
    {
        AlgorithmResult result;
        result.m_success = true;
        result.m_algorithmKey = algorithmKey;
        result.m_parentCurveId = parentCurveId;
        result.m_type = type;
        result.m_timestamp = QDateTime::currentDateTime();
        return result;
    }

    /**
     * @brief 创建失败的结果
     */
    static AlgorithmResult failure(
        const QString& algorithmKey,
        const QString& errorMessage)
    {
        AlgorithmResult result;
        result.m_success = false;
        result.m_algorithmKey = algorithmKey;
        result.m_errorMessage = errorMessage;
        result.m_timestamp = QDateTime::currentDateTime();
        return result;
    }

    // ==================== 状态查询 ====================

    bool isSuccess() const { return m_success; }
    bool hasError() const { return !m_success; }
    QString errorMessage() const { return m_errorMessage; }

    // ==================== 基本信息 ====================

    QString algorithmKey() const { return m_algorithmKey; }
    QString parentCurveId() const { return m_parentCurveId; }
    ResultType type() const { return m_type; }
    QDateTime timestamp() const { return m_timestamp; }

    // ==================== 曲线输出 ====================

    /**
     * @brief 添加输出曲线
     */
    void addCurve(const ThermalCurve& curve) {
        m_curves.append(curve);
    }

    /**
     * @brief 设置单条输出曲线（便捷方法）
     */
    void setCurve(const ThermalCurve& curve) {
        m_curves.clear();
        m_curves.append(curve);
    }

    /**
     * @brief 获取所有输出曲线
     */
    QList<ThermalCurve> curves() const { return m_curves; }

    /**
     * @brief 获取第一条曲线（用于单曲线输出）
     */
    ThermalCurve primaryCurve() const {
        return m_curves.isEmpty() ? ThermalCurve() : m_curves.first();
    }

    bool hasCurves() const { return !m_curves.isEmpty(); }
    int curveCount() const { return m_curves.size(); }

    // ==================== 标注点输出 ====================

    void addMarker(const QPointF& point, const QString& label = QString()) {
        m_markers.append(point);
        if (!label.isEmpty()) {
            m_meta[QString("marker.%1.label").arg(m_markers.size() - 1)] = label;
        }
    }

    void setMarkers(const QList<QPointF>& markers) { m_markers = markers; }
    QList<QPointF> markers() const { return m_markers; }
    bool hasMarkers() const { return !m_markers.isEmpty(); }
    int markerCount() const { return m_markers.size(); }

    // ==================== 区域输出 ====================

    void addRegion(const QPolygonF& region, const QString& label = QString()) {
        m_regions.append(region);
        if (!label.isEmpty()) {
            m_meta[QString("region.%1.label").arg(m_regions.size() - 1)] = label;
        }
    }

    void setRegions(const QList<QPolygonF>& regions) { m_regions = regions; }
    QList<QPolygonF> regions() const { return m_regions; }
    bool hasRegions() const { return !m_regions.isEmpty(); }
    int regionCount() const { return m_regions.size(); }

    // ==================== 元数据访问 ====================

    /**
     * @brief 设置元数据（键值对）
     *
     * 常用键名：
     * - "area"              : 峰面积 (double)
     * - "peakHeight"        : 峰高 (double)
     * - "peakPosition"      : 峰位置 (QPointF)
     * - "slope"             : 斜率 (double)
     * - "intercept"         : 截距 (double)
     * - "onset"             : 起始温度 (double)
     * - "endset"            : 结束温度 (double)
     * - "unit"              : 单位 (QString)
     * - "label"             : 标签 (QString)
     * - "color"             : 显示颜色 (QColor)
     * - "signalType"        : 信号类型 (SignalType)
     */
    void setMeta(const QString& key, const QVariant& value) {
        m_meta[key] = value;
    }

    QVariant meta(const QString& key, const QVariant& defaultValue = QVariant()) const {
        return m_meta.value(key, defaultValue);
    }

    template<typename T>
    T metaValue(const QString& key, const T& defaultValue = T()) const {
        return m_meta.value(key, QVariant::fromValue(defaultValue)).template value<T>();
    }

    QVariantMap allMeta() const { return m_meta; }
    bool hasMeta(const QString& key) const { return m_meta.contains(key); }

    // ==================== 便捷方法（常用标量值）====================

    void setArea(double area, const QString& unit = "J/g") {
        m_meta["area"] = area;
        m_meta["unit"] = unit;
    }

    double area() const { return metaValue<double>("area", 0.0); }

    void setPeakPosition(const QPointF& pos) {
        m_meta["peakPosition"] = pos;
    }

    QPointF peakPosition() const {
        return metaValue<QPointF>("peakPosition", QPointF());
    }

    void setSignalType(SignalType type) {
        m_meta["signalType"] = QVariant::fromValue(type);
    }

    SignalType signalType() const {
        return metaValue<SignalType>("signalType", SignalType::Raw);
    }

    // ==================== 调试输出 ====================

    QString toString() const {
        QString str = QString("[AlgorithmResult]\n");
        str += QString("  Algorithm: %1\n").arg(m_algorithmKey);
        str += QString("  Success: %1\n").arg(m_success ? "Yes" : "No");
        if (!m_success) {
            str += QString("  Error: %1\n").arg(m_errorMessage);
        }
        str += QString("  Type: %1\n").arg(resultTypeToString(m_type));
        str += QString("  Curves: %1\n").arg(m_curves.size());
        str += QString("  Markers: %1\n").arg(m_markers.size());
        str += QString("  Regions: %1\n").arg(m_regions.size());
        str += QString("  Meta: %1 items\n").arg(m_meta.size());
        str += QString("  Timestamp: %1\n").arg(m_timestamp.toString(Qt::ISODate));
        return str;
    }

private:
    // 执行状态
    bool m_success;
    QString m_errorMessage;

    // 基本信息
    QString m_algorithmKey;         // 算法标识（如 "differentiation"）
    QString m_parentCurveId;        // 来源曲线ID
    ResultType m_type;              // 输出类型
    QDateTime m_timestamp;          // 生成时间

    // 输出数据
    QList<ThermalCurve> m_curves;   // 曲线型输出
    QList<QPointF> m_markers;       // 点标注输出（如峰值、外推点）
    QList<QPolygonF> m_regions;     // 区域输出（如峰面积）
    QVariantMap m_meta;             // 元数据（数值、单位、标签等）

    static QString resultTypeToString(ResultType type) {
        switch (type) {
        case ResultType::Curve: return "Curve";
        case ResultType::Marker: return "Marker";
        case ResultType::Region: return "Region";
        case ResultType::ScalarValue: return "ScalarValue";
        case ResultType::Composite: return "Composite";
        default: return "Unknown";
        }
    }
};
```

---

## 🔨 算法接口修改

### 修改 IThermalAlgorithm

```cpp
// src/domain/algorithm/i_thermal_algorithm.h

class IThermalAlgorithm {
public:
    // ... 其他方法 ...

    /**
     * @brief 执行算法（上下文驱动，返回结构化结果）
     *
     * ✅ **输入**：算法从上下文拉取数据
     * ✅ **输出**：返回 AlgorithmResult 结构化容器
     *
     * @param context 算法上下文，包含输入数据。
     * @return 算法执行结果（成功或失败）。
     */
    virtual AlgorithmResult executeWithContext(AlgorithmContext* context) = 0;
};
```

---

## 🎯 算法实现示例

### 示例A：简单算法（微分）

```cpp
// src/infrastructure/algorithm/differentiation_algorithm.cpp

AlgorithmResult DifferentiationAlgorithm::executeWithContext(AlgorithmContext* context) {
    // 1. 验证上下文
    if (!context) {
        return AlgorithmResult::failure("differentiation", "上下文为空");
    }

    // 2. 拉取输入
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) {
        return AlgorithmResult::failure("differentiation", "未找到活动曲线");
    }

    int windowSize = context->get<int>("param.windowSize").value_or(50);

    // 3. 获取输入数据
    ThermalCurve* inputCurve = curve.value();
    const auto& inputData = inputCurve->getProcessedData();
    if (inputData.size() < windowSize * 2) {
        return AlgorithmResult::failure("differentiation", "数据点数不足");
    }

    // 4. 执行算法逻辑
    QVector<ThermalDataPoint> outputData;
    for (int i = windowSize; i < inputData.size() - windowSize; ++i) {
        // ... 微分计算 ...
        outputData.append(point);
    }

    // 5. 创建结果对象
    AlgorithmResult result = AlgorithmResult::success(
        "differentiation",
        inputCurve->id(),
        ResultType::Curve
    );

    // 6. 创建输出曲线
    ThermalCurve outputCurve(QUuid::createUuid().toString(), "微分");
    outputCurve.setProcessedData(outputData);
    outputCurve.setInstrumentType(inputCurve->instrumentType());
    outputCurve.setSignalType(SignalType::Derivative);
    outputCurve.setParentId(inputCurve->id());

    // 7. 填充结果
    result.setCurve(outputCurve);
    result.setSignalType(SignalType::Derivative);
    result.setMeta("unit", "mg/min");
    result.setMeta("label", "DTG");
    result.setMeta("windowSize", windowSize);

    return result;
}
```

### 示例B：交互算法（基线校正）

```cpp
// src/infrastructure/algorithm/baseline_correction_algorithm.cpp

AlgorithmResult BaselineCorrectionAlgorithm::executeWithContext(AlgorithmContext* context) {
    if (!context) {
        return AlgorithmResult::failure("baselineCorrection", "上下文为空");
    }

    // 拉取输入
    auto curve = context->get<ThermalCurve*>("activeCurve").value();
    auto points = context->get<QVector<QPointF>>("selectedPoints").value();

    if (points.size() < 2) {
        return AlgorithmResult::failure("baselineCorrection", "至少需要2个基线点");
    }

    // 执行基线校正
    QVector<ThermalDataPoint> correctedData = performCorrection(curve, points);

    // 创建结果
    AlgorithmResult result = AlgorithmResult::success(
        "baselineCorrection",
        curve->id(),
        ResultType::Composite  // 混合输出：曲线 + 标注点
    );

    // 添加输出曲线
    ThermalCurve outputCurve(QUuid::createUuid().toString(), "基线校正");
    outputCurve.setProcessedData(correctedData);
    outputCurve.setSignalType(SignalType::Baseline);
    outputCurve.setParentId(curve->id());
    result.setCurve(outputCurve);

    // 添加基线点标注
    for (const QPointF& pt : points) {
        result.addMarker(pt, "基线点");
    }

    // 添加元数据
    result.setMeta("correctionType", "Linear");
    result.setMeta("baselinePointCount", points.size());

    return result;
}
```

### 示例C：多输出算法（峰面积）

```cpp
// src/infrastructure/algorithm/peak_area_algorithm.cpp

AlgorithmResult PeakAreaAlgorithm::executeWithContext(AlgorithmContext* context) {
    if (!context) {
        return AlgorithmResult::failure("peakArea", "上下文为空");
    }

    auto curve = context->get<ThermalCurve*>("activeCurve").value();
    auto points = context->get<QVector<QPointF>>("selectedPoints").value();

    if (points.size() < 2) {
        return AlgorithmResult::failure("peakArea", "至少需要2个点定义积分区域");
    }

    // 计算峰面积
    double area = calculateArea(curve, points);
    QVector<ThermalDataPoint> baselineCurve = generateBaseline(curve, points);
    QPolygonF areaRegion = createAreaPolygon(curve, points);

    // 创建混合结果
    AlgorithmResult result = AlgorithmResult::success(
        "peakArea",
        curve->id(),
        ResultType::Composite
    );

    // 1. 添加基线曲线
    ThermalCurve baseline(QUuid::createUuid().toString(), "基线");
    baseline.setProcessedData(baselineCurve);
    baseline.setSignalType(SignalType::Baseline);
    baseline.setParentId(curve->id());
    result.addCurve(baseline);

    // 2. 添加面积数值
    result.setArea(area, "J/g");

    // 3. 添加区域（用于阴影填充）
    result.addRegion(areaRegion, "峰面积");

    // 4. 添加标注点（起止点）
    result.addMarker(points.first(), "起点");
    result.addMarker(points.last(), "终点");

    // 5. 添加元数据
    result.setMeta("integrationRange",
                   QString("%1 - %2 °C").arg(points.first().x()).arg(points.last().x()));
    result.setMeta("peakHeight", findPeakHeight(curve, points));
    result.setSignalType(SignalType::PeakArea);

    return result;
}
```

---

## 🔄 AlgorithmManager 修改

```cpp
// src/application/algorithm/algorithm_manager.cpp

void AlgorithmManager::executeWithContext(const QString& name, AlgorithmContext* context) {
    // ... 前置检查 ...

    // 执行算法
    AlgorithmResult result = algorithm->executeWithContext(context);

    // 检查执行状态
    if (result.hasError()) {
        qWarning() << "算法执行失败:" << name << "-" << result.errorMessage();
        emit algorithmExecutionFailed(name, result.errorMessage());
        return;
    }

    // 处理结果
    handleAlgorithmResult(result);

    // 发出成功信号
    emit algorithmResultReady(name, result);
}

void AlgorithmManager::handleAlgorithmResult(const AlgorithmResult& result) {
    if (!result.isSuccess()) {
        return;
    }

    qDebug() << result.toString();

    // 根据结果类型分发处理
    switch (result.type()) {
    case ResultType::Curve:
        handleCurveResult(result);
        break;

    case ResultType::Marker:
        handleMarkerResult(result);
        break;

    case ResultType::Region:
        handleRegionResult(result);
        break;

    case ResultType::ScalarValue:
        handleScalarResult(result);
        break;

    case ResultType::Composite:
        // 混合结果：依次处理所有输出
        if (result.hasCurves()) {
            handleCurveResult(result);
        }
        if (result.hasMarkers()) {
            handleMarkerResult(result);
        }
        if (result.hasRegions()) {
            handleRegionResult(result);
        }
        if (result.hasMeta("area")) {
            qDebug() << "面积:" << result.area() << result.meta("unit").toString();
        }
        break;
    }
}

void AlgorithmManager::handleCurveResult(const AlgorithmResult& result) {
    // 添加所有输出曲线
    for (const ThermalCurve& curve : result.curves()) {
        // 使用历史管理添加曲线
        if (m_historyManager) {
            auto command = new AddCurveCommand(m_curveManager, curve);
            m_historyManager->executeCommand(command);
        } else {
            m_curveManager->addCurve(curve);
        }

        qDebug() << "添加输出曲线:" << curve.name() << "ID:" << curve.id();
    }
}

void AlgorithmManager::handleMarkerResult(const AlgorithmResult& result) {
    // 发送标注点到 ChartView
    emit markersReady(result.algorithmKey(), result.markers());
}

void AlgorithmManager::handleRegionResult(const AlgorithmResult& result) {
    // 发送区域到 ChartView（用于阴影填充）
    emit regionsReady(result.algorithmKey(), result.regions());
}

void AlgorithmManager::handleScalarResult(const AlgorithmResult& result) {
    // 显示标量值（如面积、温度）
    qDebug() << "算法" << result.algorithmKey() << "结果:";
    for (auto it = result.allMeta().constBegin(); it != result.allMeta().constEnd(); ++it) {
        qDebug() << "  " << it.key() << ":" << it.value();
    }
}
```

---

## 🎯 核心优势

| 优势 | 说明 |
|------|------|
| ✅ **结构化** | 所有输出封装在单一对象中，清晰明确 |
| ✅ **类型安全** | 强类型字段，编译时检查 |
| ✅ **多输出支持** | `Composite` 类型支持同时返回多种结果 |
| ✅ **易于解析** | 专门的处理方法针对不同结果类型 |
| ✅ **可扩展** | 通过 `meta` 字段灵活扩展 |
| ✅ **错误处理** | 内置成功/失败状态和错误信息 |
| ✅ **可追溯** | 包含时间戳和来源信息 |
| ✅ **调试友好** | `toString()` 方法方便日志输出 |

---

## 📊 对比分析

| 维度 | QVariant返回 | 输出到上下文 | AlgorithmResult |
|------|-------------|--------------|-----------------|
| **结构化** | ❌ 弱 | ⚠️ 中等 | ✅ 强 |
| **类型安全** | ❌ 弱 | ⚠️ 中等 | ✅ 强 |
| **多输出** | ❌ 不支持 | ✅ 支持 | ✅ 完整支持 |
| **错误处理** | ❌ 无 | ⚠️ 需手动 | ✅ 内置 |
| **易用性** | ⚠️ 中等 | ⚠️ 中等 | ✅ 高 |
| **扩展性** | ❌ 弱 | ✅ 强 | ✅ 强 |
| **可读性** | ❌ 差 | ⚠️ 中等 | ✅ 优秀 |

---

## 🚀 实施计划

### 阶段1：创建基础设施
1. ✅ 定义 `AlgorithmResult` 类
2. ✅ 修改 `IThermalAlgorithm` 接口签名
3. ✅ 更新 `AlgorithmManager` 处理逻辑

### 阶段2：迁移算法
1. ✅ 微分算法（单曲线输出）
2. ✅ 积分算法（单曲线输出）
3. ✅ 移动平均（单曲线输出）
4. ✅ 基线校正（混合输出：曲线+标注）

### 阶段3：新功能
1. ✅ 峰面积（混合输出：数值+曲线+区域）
2. ✅ 峰检测（标注点输出）
3. ✅ 外推法（标注点+数值）

---

## 📚 相关文档
- `📘 AlgorithmContext 类设计文档.md` - 输入上下文设计
- `CLAUDE.md` - 主架构文档

---

**版本历史**
- v1.0 (2025-01-XX): AlgorithmResult 统一输出容器设计
