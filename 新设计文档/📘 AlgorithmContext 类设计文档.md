 `AlgorithmContext` 类的**设计文档**（含设计目标、架构思路、接口说明、扩展方向等），

------

# 📘 AlgorithmContext 类设计文档

**文件名：** `algorithm_context.h` / `algorithm_context.cpp`
 **模块层级：** 框架层（Framework Layer）
 **设计时间：** 2025-11-06
 **作者：** po luce

------

## 一、设计目的

在算法框架中，不同算法需要共享、传递和更新运行时的数据，例如：

- 用户交互产生的选点、选曲线；
- 算法计算产生的中间结果；
- 系统配置、参数等全局信息。

直接使用 `QVariantMap` 虽然方便，但缺乏：

- 数据来源标识；
- 时间戳记录；
- 有效性验证；
- 变化通知机制（信号槽）。

因此设计 `AlgorithmContext` 类来统一管理上下文信息，
 使算法执行与用户交互之间能通过一个**有状态的、可追踪的数据容器**进行通信。

------

## 二、总体设计目标

| 目标           | 说明                                                         |
| -------------- | ------------------------------------------------------------ |
| **统一管理**   | 所有算法的数据（输入、输出、中间变量）均存储在同一上下文中。 |
| **带时间语义** | 每条数据都有时间戳，用于判断是否为最新状态。                 |
| **带来源信息** | 数据记录其来源（曲线名、算法名、操作类型）。                 |
| **变更通知**   | 数据更新时通过 `valueChanged()` 信号通知框架或 UI。          |
| **安全访问**   | 提供类型安全的模板 `get<T>()` 接口。                         |
| **可扩展性强** | 可在后续增加作用域、版本、序列化功能。                       |

------

## 三、设计思路与逻辑结构

### 1️⃣ 数据抽象：`ContextValue` 结构体

每个上下文键（key）不仅仅是一段数据，而是**一个数据项的完整描述**。

```cpp
struct ContextValue {
    QVariant value;        // 实际数据
    QDateTime timestamp;   // 更新时间
    QString source;        // 来源（如曲线名、算法名）
};
```

> ✅ 它解决了原始 `QVariantMap` 无法判断数据新旧、来源混乱的问题。

------

### 2️⃣ 主体容器：`AlgorithmContext` 类

整个类的核心是一个 `QMap<QString, QVariant>`，
 其中每个 `QVariant` 实际包装一个 `ContextValue`。

#### 存储结构：

```
m_data = {
    "selectedCurve" -> ContextValue("Curve_01", time=10:23, src="UI"),
    "pointA"        -> ContextValue(QPointF(12.3, 0.45), time=10:24, src="Curve_01"),
    "filteredData"  -> ContextValue("OK", time=10:30, src="FilterAlgorithm")
}
```

#### 主要职责：

| 功能          | 描述                                       |
| ------------- | ------------------------------------------ |
| **存储/更新** | 使用 `set()` 更新或添加新的上下文项        |
| **读取**      | 使用 `get()` 或 `getValue()` 获取值        |
| **合并**      | 使用 `merge()` 将算法输出写回上下文        |
| **删除**      | 使用 `remove()` 清除无效数据               |
| **调试**      | 使用 `print()` 输出当前上下文状态          |
| **导出**      | 使用 `toValueMap()` 获取纯值形式的 map     |
| **监听变化**  | 通过 `valueChanged()` 信号实现自动响应机制 |

------

### 3️⃣ 数据更新与信号机制

当任何值被 `set()` 或 `merge()` 更新时：

1. 自动打上时间戳；
2. 更新来源信息；
3. 发出信号：

```cpp
emit valueChanged(key, newContextValue);
```

> UI 或算法管理器可以监听此信号，实现 **“数据驱动算法”** 的自动执行机制。

------

### 4️⃣ 类型安全访问

通过模板函数 `get<T>()` 提供安全访问：

```cpp
template<typename T>
T get(const QString &key, const T &defaultValue = T()) const;
```

如果类型不匹配或不存在，则返回默认值，避免 `QVariant` 强制转换出错。

------

### 5️⃣ 可扩展接口示例

- `isValidFor(key, source, afterTime)`：检查某键是否为指定来源、且在指定时间之后更新；
- `clearBySource("Curve_01")`：清除指定来源的数据；
- `serializeToJson()`：导出为 JSON，用于算法重放或日志记录。

------

## 四、接口设计概览

| 函数名                        | 功能说明                               |
| ----------------------------- | -------------------------------------- |
| `set(key, value, source)`     | 插入或更新数据（自动打时间戳、发信号） |
| `hasKey(key)`                 | 判断是否存在某字段                     |
| `getValue(key)`               | 获取包含时间戳与来源的完整对象         |
| `get<T>(key, defaultValue)`   | 模板化取值                             |
| `merge(QVariantMap, source)`  | 批量合并新数据                         |
| `remove(key)`                 | 删除键                                 |
| `toValueMap()`                | 导出为纯值 map                         |
| `print()`                     | 打印调试输出                           |
| `valueChanged(key, newValue)` | 信号：数据更新通知                     |

------

## 五、典型使用场景

### ✅ 1️⃣ 用户交互数据

```cpp
context.set("selectedCurve", "Curve_01", "UI");
context.set("pointA", QPointF(12.3, 0.45), "Curve_01");
context.set("pointB", QPointF(18.7, 0.62), "Curve_01");
```

### ✅ 2️⃣ 算法输出数据回写

```cpp
QVariantMap result;
result["filteredData"] = "Processed OK";
result["coefficients"] = QVector<double>{1.2, 0.8};
context.merge(result, "FilterAlgorithm");
```

### ✅ 3️⃣ 动态监听机制

```cpp
connect(&context, &AlgorithmContext::valueChanged,
        this, [](const QString &key, const ContextValue &cv) {
    qDebug() << "Context updated:" << key << cv.value;
});
```

------

## 六、类示意图（UML 简化）

```
+---------------------------+
|        AlgorithmContext   |
+---------------------------+
| - m_data: QMap<QString,QVariant> |
+---------------------------+
| + set(key, value, source) |
| + hasKey(key): bool       |
| + getValue(key): ContextValue |
| + get<T>(key): T          |
| + merge(map, source)      |
| + remove(key)             |
| + print()                 |
| + toValueMap(): QVariantMap |
| + valueChanged(key, val)  |
+---------------------------+

+----------------+
| ContextValue   |
+----------------+
| + value        |
| + timestamp    |
| + source       |
+----------------+
```

------

## 七、设计优点总结

| 优点           | 描述                                       |
| -------------- | ------------------------------------------ |
| 🕒 数据带时间戳 | 能判断新旧，避免算法使用过期输入           |
| 🏷️ 数据带来源   | 明确数据从何而来（UI / 算法）              |
| 🔁 自动更新通知 | 便于框架实现响应式数据流                   |
| 🧩 可统一存储   | 支持算法间共享输入输出                     |
| 🧠 易于扩展     | 后续可添加验证、作用域、版本、序列化等功能 |
| 🔍 调试友好     | `print()` 一目了然查看状态                 |

------

## 八、未来扩展方向

| 扩展功能            | 实现方式                                     |
| ------------------- | -------------------------------------------- |
| **作用域（Scope）** | 添加任务级 / 全局级 / 会话级上下文           |
| **版本号机制**      | 每次更新生成版本 ID，支持回滚                |
| **JSON 序列化**     | `toJson()` / `fromJson()`                    |
| **自动校验函数**    | `isValidFor(key, expectedSource, afterTime)` |
| **上下文快照**      | 保存执行前后的数据状态                       |
| **信号过滤机制**    | 仅当重要字段更新时才触发信号                 |

------

## 九、使用示例摘要

```cpp
AlgorithmContext ctx;

// 用户选择曲线
ctx.set("selectedCurve", "Curve_A", "UI");

// 用户选点
ctx.set("pointA", QPointF(10.2, 0.34), "Curve_A");
ctx.set("pointB", QPointF(15.8, 0.47), "Curve_A");

// 执行算法后结果写回
QVariantMap result;
result["fitCurve"] = "poly_fit_result";
ctx.merge(result, "CurveFitAlgorithm");

// 打印当前上下文
ctx.print();
```

------

## 十、结论

`AlgorithmContext` 是整个算法执行框架的**数据中枢**，
 它把算法逻辑层、UI 交互层、执行调度层有机地连接在一起。

> ✅ 它不是单纯的“数据字典”，
>  而是一个**带时间、带来源、可监听、可合并的运行时状态容器**。

这样的设计确保了框架的可维护性、可扩展性和可观察性，
 为后续的算法插件化、任务链自动化、交互响应式提供了坚实基础。

------

