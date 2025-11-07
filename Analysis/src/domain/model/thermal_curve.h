#ifndef THERMALCURVE_H
#define THERMALCURVE_H

#include "thermal_data_point.h"
#include <QString>
#include <QVariantMap>
#include <QVector>

/**
 * @brief 定义热分析仪器类型
 *
 * 表示数据来源的仪器类型，不同仪器测量不同的物理量：
 * - TGA: 测量质量 vs 温度
 * - DSC: 测量热流 vs 温度
 * - ARC: 测量压力 vs 温度
 */
enum class InstrumentType {
    TGA, // 热重分析仪
    DSC, // 差示扫描量热仪
    ARC, // 加速量热仪
};

/**
 * @brief 定义信号处理类型
 *
 * 表示数据的处理状态：
 * - Raw: 原始信号（直接从仪器获得）
 * - Derivative: 微分信号（通过算法派生）
 * - Baseline: 基线（用于峰面积计算的参考线）
 * - PeakArea: 峰面积（原始信号与基线之间的区域）
 */
enum class SignalType {
    Raw,        // 原始信号
    Derivative, // 微分信号
    Baseline,   // 基线
    PeakArea,   // 峰面积
};

/**
 * @brief 存储与实验或曲线相关的元数据。
 */
struct CurveMetadata {
    QString device;          // 实验设备
    QString sampleName;      // 样品名称
    double sampleMass = 0.0; // 样品质量
    QVariantMap additional;  // 其他自定义参数
};

/**
 * @brief 定义曲线的绘制类型
 *
 * 表示该曲线在图表中应如何呈现：
 * - Line: 折线
 * - Scatter: 散点
 * - Area: 面积（用于两条曲线间填充）
 */
enum class PlotStyle {
    Line,    // 折线图
    Scatter, // 散点图
    Area     // 面积图（通常需要两条曲线）
};

/**
 * @brief ThermalCurve 类代表一个完整的热分析数据曲线。
 *
 * 它持有从文件加载的原始、不可变的数据，以及一份可由算法处理的数据副本。
 * 这样可以方便地实现撤销/重做和重置功能。
 */
class ThermalCurve {
public:
    explicit ThermalCurve(QString id, QString name);

    // --- 获取器 ---
    QString id() const;
    QString name() const;
    QString projectName() const;
    InstrumentType instrumentType() const;
    SignalType signalType() const;
    const QVector<ThermalDataPoint>& getRawData() const;
    const QVector<ThermalDataPoint>& getProcessedData() const;
    const CurveMetadata& getMetadata() const;
    QString parentId() const;
    PlotStyle plotStyle() const;
    bool isAuxiliaryCurve() const;

    // --- 设置器 ---
    void setName(const QString& name);
    void setProjectName(const QString& projectName);
    void setInstrumentType(InstrumentType type);
    void setSignalType(SignalType type);
    void setRawData(const QVector<ThermalDataPoint>& data);
    void setProcessedData(const QVector<ThermalDataPoint>& data);
    void setMetadata(const CurveMetadata& metadata);
    void setParentId(const QString& parentId);
    void setPlotStyle(PlotStyle style);
    void setIsAuxiliaryCurve(bool isAuxiliary);

    // --- 辅助方法 ---
    /**
     * @brief 获取 Y 轴标签（包含物理量名称和单位）
     * @return Y 轴标签字符串，例如 "质量 (%)" 或 "热流 (W/g)"
     */
    QString getYAxisLabel() const;

    /**
     * @brief 获取物理量名称（不含单位）
     * @return 物理量名称，例如 "质量" 或 "热流"
     */
    QString getPhysicalQuantityName() const;

    /**
     * @brief 将处理后的数据重置为原始数据。
     */
    void resetToRaw();

private:
    QString m_id;                            // 唯一标识
    QString m_name;                          // 曲线名称
    QString m_projectName;                   // 项目名称（文件名，用于树形结构的根节点）
    InstrumentType m_instrumentType;         // 仪器类型
    SignalType m_signalType;                 // 信号处理类型
    QString m_parentId;                      // 父曲线ID（用于算法生成的曲线）
    bool m_isAuxiliaryCurve;                 // 判断是否是辅助曲线
    PlotStyle m_plotStyle = PlotStyle::Line; // 默认折线

    QVector<ThermalDataPoint> m_rawData;       // 原始数据 (只读)
    QVector<ThermalDataPoint> m_processedData; // 处理后数据

    CurveMetadata m_metadata; // 实验参数
};

// 注册类型到 Qt 元对象系统，用于 QVariant
Q_DECLARE_METATYPE(ThermalCurve*)
Q_DECLARE_METATYPE(SignalType)

#endif // THERMALCURVE_H
