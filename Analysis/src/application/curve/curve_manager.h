#ifndef CURVEMANAGER_H
#define CURVEMANAGER_H

#include "domain/model/thermal_curve.h"
#include "infrastructure/io/i_file_reader.h"
#include <QMap>
#include <QObject>
#include <QString>
#include <memory>
#include <vector>

// 前置声明
struct FilePreviewData;

/**
 * @brief CurveManager 管理应用中的所有热分析曲线
 *
 * 职责：
 * - 曲线的增删改查
 * - 活动曲线管理
 * - 从文件加载曲线
 * - 发射曲线状态变化信号
 */
class CurveManager : public QObject {
    Q_OBJECT

public:
    explicit CurveManager(QObject* parent = nullptr);
    ~CurveManager();

    // 管理曲线

    /**
     * @brief 添加新曲线
     * @param curve 要添加的曲线对象
     *
     * 添加后会发射 curveAdded 信号
     */
    void addCurve(const ThermalCurve& curve);

    /**
     * @brief 从文件加载曲线
     * @param filePath 文件路径
     * @return 加载成功返回 true，失败返回 false
     *
     * 自动选择合适的文件读取器进行读取
     */
    bool loadCurveFromFile(const QString& filePath);

    /**
     * @brief 读取文件预览数据
     * @param filePath 文件路径
     * @return 预览数据（如果 Reader 支持）
     *
     * 自动选择合适的 Reader 并调用其预览方法。
     * 如果找不到支持预览的 Reader，返回空预览数据。
     */
    FilePreviewData readFilePreview(const QString& filePath) const;

    /**
     * @brief 从文件加载曲线（支持用户配置）
     * @param filePath 文件路径
     * @param config 用户导入配置（列映射、单位等）
     * @return 加载成功的曲线ID，失败返回空字符串
     *
     * 自动选择合适的文件读取器进行读取，支持用户自定义配置。
     * 成功后会发射 curveAdded 信号。
     */
    QString loadCurveFromFileWithConfig(const QString& filePath, const QVariantMap& config);

    /**
     * @brief 根据ID获取曲线
     * @param curveId 曲线ID
     * @return 曲线指针，如果不存在返回 nullptr
     */
    ThermalCurve* getCurve(const QString& curveId);

    /**
     * @brief 获取所有曲线的映射表
     * @return 曲线ID到曲线对象的映射表
     */
    const QMap<QString, ThermalCurve>& getAllCurves() const;

    /**
     * @brief 清空所有曲线
     *
     * 清空后会发射 curvesCleared 信号
     */
    void clearCurves();

    /**
     * @brief 移除指定曲线
     * @param curveId 曲线ID
     * @return 移除成功返回 true，曲线不存在返回 false
     *
     * 移除后会发射 curveRemoved 信号
     *
     * 注意：此方法不会删除子曲线，如需级联删除请使用 removeCurveRecursively()
     */
    bool removeCurve(const QString& curveId);

    /**
     * @brief 递归删除曲线及其所有子曲线（级联删除）
     * @param curveId 曲线ID
     * @return 删除的曲线总数（包括子曲线）
     *
     * 删除顺序：先递归删除所有子曲线，再删除本身
     * 每删除一条曲线都会发射 curveRemoved 信号
     */
    int removeCurveRecursively(const QString& curveId);

    // 活动曲线管理

    /**
     * @brief 设置活动曲线
     * @param curveId 曲线ID
     *
     * 活动曲线通常是用户当前选中或操作的曲线。
     * 设置后会发射 activeCurveChanged 信号
     */
    void setActiveCurve(const QString& curveId);

    /**
     * @brief 获取活动曲线
     * @return 活动曲线指针，如果没有活动曲线返回 nullptr
     */
    ThermalCurve* getActiveCurve();

    /**
     * @brief 获取指定曲线的所有基线曲线
     * @param curveId 父曲线ID
     * @return 基线曲线指针列表，如果不存在返回空列表
     *
     * 查找条件：parentId == curveId && signalType == SignalType::Baseline
     *
     * 说明：返回所有基线，由算法自己决定如何使用：
     * - 使用第一条：baselines[0] 或 baselines.first()
     * - 使用最新的：baselines.last()
     * - 遍历所有：for (auto* baseline : baselines)
     * - 让用户选择：通过UI选择特定基线
     */
    QVector<ThermalCurve*> getBaselines(const QString& curveId);

    /**
     * @brief 检查指定曲线是否有子曲线
     * @param curveId 父曲线ID
     * @return 如果有至少一个子曲线返回 true，否则返回 false
     *
     * 查找条件：任何 parentId == curveId 的曲线
     */
    bool hasChildren(const QString& curveId) const;

    /**
     * @brief 获取指定曲线的所有直接子曲线
     * @param curveId 父曲线ID
     * @return 子曲线指针列表，如果不存在返回空列表
     *
     * 查找条件：parentId == curveId（所有信号类型）
     */
    QVector<ThermalCurve*> getChildren(const QString& curveId);

signals:
    /**
     * @brief 当新曲线被添加时发射
     * @param curveId 新添加的曲线ID
     */
    void curveAdded(const QString& curveId);

    /**
     * @brief 当活动曲线改变时发射
     * @param curveId 新的活动曲线ID
     */
    void activeCurveChanged(const QString& curveId);

    /**
     * @brief 当曲线数据被修改时发射
     * @param curveId 被修改的曲线ID
     */
    void curveDataChanged(const QString& curveId);

    /**
     * @brief 当所有曲线被清空时发射
     */
    void curvesCleared();

    /**
     * @brief 当曲线被移除时发射
     * @param curveId 被移除的曲线ID
     */
    void curveRemoved(const QString& curveId);

private:
    /**
     * @brief 注册默认的文件读取器
     *
     * 当前注册 TextFileReader 用于读取 .txt 和 .csv 文件
     */
    void registerDefaultReaders();

    QMap<QString, ThermalCurve> m_curves;
    std::vector<std::unique_ptr<IFileReader>> m_readers;
    QString m_activeCurveId;
};

#endif // CURVEMANAGER_H
