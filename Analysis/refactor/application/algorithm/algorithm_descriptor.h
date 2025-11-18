#pragma once

#include <QString>
#include <QVariant>
#include <QVector>
#include <QMap>
#include <optional>
#include <limits>

namespace App {

enum class ParamType {
    Integer,
    Double,
    Boolean,
    String,
    Enum,
    DoubleRange,   // [min,max] 双精度区间
    PointsOnChart  // 非编辑控件，占位用于流程控制
};

struct EnumOption {
    QString value;  // 内部值
    QString label;  // 显示文本
};

struct IntConstraint {
    int min = std::numeric_limits<int>::min();
    int max = std::numeric_limits<int>::max();
    int step = 1;
};

struct DoubleConstraint {
    double min = -std::numeric_limits<double>::infinity();
    double max =  std::numeric_limits<double>::infinity();
    double step = 0.1;
    QString unit; // 可选单位显示
};

struct PointSelectionSpec {
    int minCount = 1;
    int maxCount = 1;     // -1 表示无限制
    QString hint;         // 交互提示
};

struct ParameterDescriptor {
    QString name;                 // 唯一键
    QString label;                // 显示名（可走 i18n）
    ParamType type = ParamType::String;
    QVariant defaultValue;        // 默认值
    bool required = false;        // 是否必填

    // 约束（按类型选填）
    std::optional<IntConstraint>    intConstraint;
    std::optional<DoubleConstraint> doubleConstraint;
    QVector<EnumOption>             enumOptions; // 仅 type=Enum 使用

    // 可选说明文本
    QString description;
};

struct AlgorithmDescriptor {
    QString name;                  // 唯一算法名（与 AlgorithmManager 注册名一致）
    QString displayName;           // UI 显示名
    QVector<ParameterDescriptor> params;
    std::optional<PointSelectionSpec> pointSelection; // 需要选点则提供

    // 预留：输出写回键、默认行为等
    QMap<QString, QVariant> meta;  // e.g., { "output", "AppendCurve" }
};

} // namespace App

