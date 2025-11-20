#ifndef APPLICATION_ALGORITHM_CONTEXT_H
#define APPLICATION_ALGORITHM_CONTEXT_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <optional>
#include "domain/algorithm/algorithm_result.h"

// ============================================================================
// 标准键名常量定义 (Standard Context Keys)
// ============================================================================
/**
 * @brief 算法上下文标准键名
 *
 * 为算法插件开发者提供类型安全的键名常量，避免字符串拼写错误。
 * 使用这些常量而非字符串字面量可以获得编译时检查和自动补全支持。
 *
 * @code
 * // 推荐使用：
 * auto curve = context->get<ThermalCurve*>(ContextKeys::ActiveCurve);
 *
 * // 避免使用：
 * auto curve = context->get<ThermalCurve*>("activeCurve");  // 容易拼写错误
 * @endcode
 */
namespace ContextKeys {
    // ========== 基础曲线数据 (Basic Curve Data) ==========
    /** 当前活动曲线 (ThermalCurve) - 算法将在此曲线上执行
     *  注意：为确保线程安全，上下文存储的是曲线副本，而非指针 */
    inline constexpr const char* ActiveCurve = "activeCurve";

    /** 输入曲线 (ThermalCurve) - 算法的数据来源曲线 */
    inline constexpr const char* InputCurve = "inputCurve";

    /** 活动曲线的基线曲线列表 (QVector<ThermalCurve*>) - 由 AlgorithmCoordinator 自动注入 */
    inline constexpr const char* BaselineCurves = "baselineCurves";

    /** 曲线管理器 (CurveManager*) - 用于查找关联曲线 */
    inline constexpr const char* CurveManager = "curveManager";

    // ========== 用户交互选点 (User Interaction Points) ==========
    /** 用户选择的点集合 (QVector<QPointF>) - 用于基线、峰面积等算法 */
    inline constexpr const char* SelectedPoints = "selectedPoints";

    // ========== 通用算法参数 (Common Algorithm Parameters) ==========
    /** 窗口大小 (int) - 用于移动平均算法 */
    inline constexpr const char* ParamWindow = "param.window";

    /** 半窗口大小 (int) - 用于微分算法 */
    inline constexpr const char* ParamHalfWin = "param.halfWin";

    /** 时间步长 (double) - 用于微分等数值计算 */
    inline constexpr const char* ParamDt = "param.dt";

    /** 启用调试输出 (bool) - 用于算法调试 */
    inline constexpr const char* ParamEnableDebug = "param.enableDebug";

    /** 阈值 (double) - 用于峰值检测等算法 */
    inline constexpr const char* ParamThreshold = "param.threshold";
}

// ============================================================================
// AlgorithmContext 类定义
// ============================================================================
/**
 * @brief 算法上下文 - 统一的运行时数据容器
 *
 * AlgorithmContext 是所有算法执行时的数据存储和传递中心，采用键值对方式管理：
 * - 算法输入参数（窗口大小、阈值等）
 * - 用户交互数据（选择的点、曲线引用等）
 * - 算法中间结果和输出数据
 *
 * 核心特性:
 *
 * 1. 类型安全访问: 使用模板方法 get<T>() 提供编译时类型检查
 * 2. 标准键名常量: ContextKeys 命名空间定义所有标准键名，避免拼写错误
 * 3. 数据来源追踪: 每个值都记录来源（UI/Algorithm/File）和时间戳
 * 4. 信号通知: 值变化时自动发出信号，支持响应式编程
 *
 * 算法开发者指南:
 *
 * 1. 读取上下文数据
 *
 * 算法应该从上下文中"拉取"所需数据，而不是通过函数参数传递：
 *
 * @code
 * QVariant MyAlgorithm::executeWithContext(AlgorithmContext* context) {
 *     // 1. 获取活动曲线（必需）
 *     auto curveOpt = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
 *     if (!curveOpt.has_value()) {
 *         qWarning() << "MyAlgorithm: activeCurve 未设置";
 *         return QVariant();
 *     }
 *     const ThermalCurve& curve = curveOpt.value();
 *
 *     // 2. 获取算法参数（使用 value_or 提供默认值）
 *     int windowSize = context->get<int>(ContextKeys::ParamWindowSize).value_or(50);
 *     double threshold = context->get<double>(ContextKeys::ParamThreshold).value_or(0.1);
 *
 *     // 3. 获取用户选择的点（如果算法需要）
 *     auto points = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
 *     if (points.has_value() && points.value().size() >= 2) {
 *         // 使用选点数据
 *     }
 *
 *     // 4. 获取曲线数据并执行算法
 *     const auto& inputData = curve.getProcessedData();
 *     QVector<ThermalDataPoint> result = performCalculation(inputData, windowSize);
 *
 *     return QVariant::fromValue(result);
 * }
 * @endcode
 *
 * 2. 设置默认参数（可选）
 *
 * 如果算法有默认参数，可以在 prepareContext() 中注入：
 *
 * @code
 * void MyAlgorithm::prepareContext(AlgorithmContext* context) {
 *     // 只在上下文中不存在时才设置默认值
 *     if (!context->contains(ContextKeys::ParamWindowSize)) {
 *         context->setValue(ContextKeys::ParamWindowSize, 50);
 *     }
 * }
 * @endcode
 *
 * 3. 标准数据类型速查
 *
 * - ContextKeys::ActiveCurve        - ThermalCurve             - 当前活动曲线（副本）
 * - ContextKeys::BaselineCurves     - QVector<ThermalCurve*>   - 活动曲线的所有基线
 * - ContextKeys::SelectedPoints     - QVector<ThermalDataPoint> - 用户选择的点集合
 * - ContextKeys::ParamWindow        - int                      - 窗口大小（移动平均）
 * - ContextKeys::ParamThreshold     - double                   - 阈值（峰值检测等）
 * - ContextKeys::BaselineType       - int                      - 基线类型 (0=线性, 1=多项式)
 * - ContextKeys::FilterType         - QString                  - 滤波类型 ("FFT", "MovingAverage")
 * - ContextKeys::PeakArea           - double                   - 峰面积计算结果
 * - ContextKeys::ActivationEnergy   - double                   - 活化能 (kJ/mol)
 *
 * 完整的数据清单请参考: 新设计文档/AlgorithmContext_数据清单.md
 *
 * 4. 注意事项
 *
 * - 推荐: 使用标准键名常量 (ContextKeys::*) 而非字符串字面量
 * - 推荐: 使用 value_or() 提供回退默认值，避免崩溃
 * - 推荐: 检查指针有效性 在使用 ThermalCurve* 前检查 != nullptr
 * - 推荐: 返回 QVariant 算法结果应包装为 QVariant::fromValue(result)
 * - 禁止: 不要修改输入曲线 使用 getProcessedData() 获取只读数据
 * - 禁止: 不要在上下文中存储大对象 对于大数据，存储指针或引用
 *
 * 高级用法:
 *
 * 监听上下文变化
 *
 * @code
 * connect(context, &AlgorithmContext::valueChanged, this,
 *         [](const QString& key, const QVariant& value) {
 *     qDebug() << "上下文更新:" << key << "=" << value;
 * });
 * @endcode
 *
 * ### 批量读取参数
 *
 * @code
 * // 获取所有以 "param." 开头的参数
 * QVariantMap params = context->values("param.");
 * // params = {"param.windowSize": 50, "param.threshold": 0.1, ...}
 * @endcode
 */
class AlgorithmContext : public QObject {
    Q_OBJECT

public:
    explicit AlgorithmContext(QObject* parent = nullptr);

    /**
     * @brief 检查上下文中是否包含指定键
     * @param key 键名
     * @return 如果存在返回 true，否则返回 false
     */
    bool contains(const QString& key) const;


    /**
     * @brief 获取指定键的值（类型安全的模板方法）
     *
     * 使用 std::optional 包装结果，避免在键不存在时返回默认构造的对象。
     *
     * @tparam T 目标类型
     * @param key 键名
     * @return std::optional<T> - 如果键存在且类型匹配返回值，否则返回 std::nullopt
     *
     * 示例：
     * @code
     * auto curve = context->get<ThermalCurve*>(ContextKeys::ActiveCurve);
     * if (curve.has_value()) {
     *     // 使用 curve.value()
     * }
     * @endcode
     */
    template <typename T>
    std::optional<T> get(const QString& key) const
    {
        if (!m_entries.contains(key)) {
            return std::nullopt;
        }
        return m_entries.value(key).storedValue.template value<T>();
    }

    /**
     * @brief 设置键值对
     * @param key 键名
     * @param value 值
     * @param source 数据来源（可选，用于调试和追踪）
     */
    void setValue(const QString& key, const QVariant& value, const QString& source = QString());

    /**
     * @brief 移除指定键
     * @param key 键名
     */
    void remove(const QString& key);

    /**
     * @brief 获取所有键的列表（可选按前缀过滤）
     * @param prefix 键名前缀（可选，如 "param." 获取所有参数键）
     * @return 键名列表
     */
    QStringList keys(const QString& prefix = QString()) const;

    /**
     * @brief 创建上下文的深拷贝（用于异步任务快照）
     *
     * **语义说明**：
     * - 深拷贝键值对结构（QHash 和 Entry）
     * - 对于值类型（int, double, QString, ThermalCurve 等），QVariant 自动深拷贝
     * - ThermalCurve 存储为值类型，克隆时会完整复制所有数据
     *
     * **线程安全**：
     * - 每个工作线程获得独立的曲线数据副本
     * - 工作线程可以安全地读取曲线数据，不会受主线程修改影响
     * - 避免了指针失效和数据竞争问题
     *
     * **使用场景**：
     * - 创建任务快照时调用，避免主线程修改影响任务执行
     * - 每个任务独占一个上下文快照，互不干扰
     *
     * @return 上下文的深拷贝（调用者负责管理生命周期）
     *
     * @code
     * // 在主线程创建快照
     * AlgorithmContext* snapshot = context->clone();
     *
     * // 将快照传递给任务（任务独占所有权）
     * auto task = AlgorithmTaskPtr::create(algorithmName, snapshot);
     * @endcode
     */
    AlgorithmContext* clone() const;

    // ========== 算法结果专用 API ==========

    /**
     * @brief 保存算法执行结果（支持并发和历史管理）
     *
     * 此方法使用混合索引方案存储结果：
     * - 主存储（byTask）：按 taskId 存储，不可覆盖，支持并发
     * - 最新指针（latestTaskId）：指向该曲线+算法的最新任务ID
     * - 历史队列（historyTaskIds）：记录最近 N 次执行的 taskId 列表
     *
     * LRU 裁剪策略：
     * - 当历史队列超出 m_historyDepth 时，自动裁剪最旧的记录
     * - 被裁剪的任务的主存储也会被删除（可选）
     *
     * @param taskId 任务ID（全局唯一，格式：{algorithm}-{timestamp}-{uuid}）
     * @param algorithm 算法名称
     * @param parentCurveId 来源曲线ID
     * @param result 算法执行结果
     *
     * 使用示例:
     * @code
     * AlgorithmResult result = AlgorithmResult::success("differentiation", "curve-001", ResultType::Curve);
     * context->saveResult("task-001", "differentiation", "curve-001", result);
     * @endcode
     */
    void saveResult(const QString& taskId,
                   const QString& algorithm,
                   const QString& parentCurveId,
                   const AlgorithmResult& result);

    /**
     * @brief 获取某条曲线某算法的最新执行结果
     *
     * 读取逻辑：
     * 1. 读取 latestTaskId 指针
     * 2. 展开 taskId，读取主存储（byTask）
     *
     * @param algorithm 算法名称
     * @param curveId 曲线ID
     * @return std::optional<AlgorithmResult> - 如果存在返回结果，否则返回 std::nullopt
     *
     * 使用示例:
     * @code
     * auto result = context->latestResult("differentiation", "curve-001");
     * if (result.has_value()) {
     *     qDebug() << "最新微分结果:" << result->timestamp();
     * }
     * @endcode
     */
    std::optional<AlgorithmResult> latestResult(const QString& algorithm,
                                                const QString& curveId) const;

    /**
     * @brief 获取某条曲线某算法的历史执行结果列表（按时间倒序）
     *
     * 读取逻辑：
     * 1. 读取 historyTaskIds 列表
     * 2. 展开每个 taskId，读取主存储（byTask）
     *
     * @param algorithm 算法名称
     * @param curveId 曲线ID
     * @param limit 限制返回数量（默认10，0表示不限制）
     * @return QVector<AlgorithmResult> - 历史结果列表（最新的在前）
     *
     * 使用示例:
     * @code
     * auto history = context->historyResults("differentiation", "curve-001", 5);
     * qDebug() << "最近5次微分执行:" << history.size();
     * @endcode
     */
    QVector<AlgorithmResult> historyResults(const QString& algorithm,
                                           const QString& curveId,
                                           int limit = 10) const;

    /**
     * @brief 设置历史记录深度（默认20）
     * @param depth 历史深度（必须 > 0）
     */
    void setHistoryDepth(int depth);

    /**
     * @brief 获取当前历史记录深度
     * @return 历史深度
     */
    int historyDepth() const;

signals:
    /**
     * @brief 当键值发生改变时发射此信号
     * @param key 改变的键名
     * @param value 新值
     */
    void valueChanged(const QString& key, const QVariant& value);

    /**
     * @brief 当键被移除时发射此信号
     * @param key 被移除的键名
     */
    void valueRemoved(const QString& key);

private:
    struct Entry {
        QVariant storedValue;
        QString source;
    };

    QHash<QString, Entry> m_entries;
    int m_historyDepth = 20;  // 历史记录深度（默认保留最近20次执行）
};

#endif // APPLICATION_ALGORITHM_CONTEXT_H
