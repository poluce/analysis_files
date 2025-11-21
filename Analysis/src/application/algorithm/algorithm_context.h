#ifndef APPLICATION_ALGORITHM_CONTEXT_H
#define APPLICATION_ALGORITHM_CONTEXT_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <optional>
#include "domain/algorithm/algorithm_result.h"

/**
 * @brief 算法上下文标准键名常量
 */
namespace ContextKeys {
    // 基础曲线数据
    inline constexpr const char* ActiveCurve = "activeCurve";           // ThermalCurve
    inline constexpr const char* InputCurve = "inputCurve";             // ThermalCurve
    inline constexpr const char* BaselineCurves = "baselineCurves";     // QVector<ThermalCurve*>
    inline constexpr const char* CurveManager = "curveManager";         // CurveManager*

    // 用户交互
    inline constexpr const char* SelectedPoints = "selectedPoints";     // QVector<QPointF>

    // 算法参数
    inline constexpr const char* ParamWindow = "param.window";          // int
    inline constexpr const char* ParamHalfWin = "param.halfWin";        // int
    inline constexpr const char* ParamDt = "param.dt";                  // double
    inline constexpr const char* ParamEnableDebug = "param.enableDebug"; // bool
    inline constexpr const char* ParamThreshold = "param.threshold";    // double
}

/**
 * @brief 算法上下文 - 统一的运行时数据容器
 *
 * 采用键值对方式管理算法输入参数、用户交互数据和算法结果。
 * 使用 get<T>() 模板方法获取类型安全的数据访问。
 *
 * 标准数据类型速查:
 * - ActiveCurve      - ThermalCurve              - 当前活动曲线（副本）
 * - InputCurve       - ThermalCurve              - 输入曲线
 * - BaselineCurves   - QVector<ThermalCurve*>    - 活动曲线的所有基线
 * - CurveManager     - CurveManager*             - 曲线管理器
 * - SelectedPoints   - QVector<ThermalDataPoint> - 用户选择的点集合
 * - ParamWindow      - int                       - 窗口大小（移动平均）
 * - ParamHalfWin     - int                       - 半窗口大小（微分）
 * - ParamDt          - double                    - 时间步长
 * - ParamEnableDebug - bool                      - 启用调试输出
 * - ParamThreshold   - double                    - 阈值（峰值检测）
 *
 * 注意事项:
 * - 使用 ContextKeys::* 常量而非字符串字面量
 * - 使用 value_or() 提供回退默认值
 * - 检查指针有效性后再使用
 * - 不要修改输入曲线，使用 getProcessedData() 获取只读数据
 */
class AlgorithmContext : public QObject {
    Q_OBJECT

public:
    explicit AlgorithmContext(QObject* parent = nullptr);

    /** 检查键是否存在 */
    bool contains(const QString& key) const;

    /** 获取值（类型安全） */
    template <typename T>
    std::optional<T> get(const QString& key) const
    {
        if (!m_entries.contains(key)) {
            return std::nullopt;
        }
        return m_entries.value(key).storedValue.template value<T>();
    }

    /** 设置键值对 */
    void setValue(const QString& key, const QVariant& value, const QString& source = QString());

    /** 移除键 */
    void remove(const QString& key);

    /** 获取键列表（可按前缀过滤） */
    QStringList keys(const QString& prefix = QString()) const;

    /** 创建深拷贝（用于异步任务） */
    AlgorithmContext* clone() const;

    // ========== 算法结果 API ==========

    /** 保存算法结果 */
    void saveResult(const QString& taskId,
                   const QString& algorithm,
                   const QString& parentCurveId,
                   const AlgorithmResult& result);

    /** 获取最新结果 */
    std::optional<AlgorithmResult> latestResult(const QString& algorithm,
                                                const QString& curveId) const;

    /** 获取历史结果列表 */
    QVector<AlgorithmResult> historyResults(const QString& algorithm,
                                           const QString& curveId,
                                           int limit = 10) const;

    /** 设置历史深度 */
    void setHistoryDepth(int depth);

    /** 获取历史深度 */
    int historyDepth() const;

signals:
    void valueChanged(const QString& key, const QVariant& value);
    void valueRemoved(const QString& key);

private:
    struct Entry {
        QVariant storedValue;
        QString source;
    };

    QHash<QString, Entry> m_entries;
    int m_historyDepth = 20;
};

#endif // APPLICATION_ALGORITHM_CONTEXT_H
