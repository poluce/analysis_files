#ifndef CURVEMANAGER_H
#define CURVEMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <memory>
#include <vector>
#include "domain/model/ThermalCurve.h"
#include "infrastructure/io/IFileReader.h"

/**
 * @brief CurveManager 类负责管理所有的 ThermalCurve 对象。
 *
 * 它处理从文件加载曲线、存储它们并提供对它们的访问。
 * 当曲线集合发生变化时，它会发出信号。
 */
class CurveManager : public QObject
{
    Q_OBJECT

public:
    explicit CurveManager(QObject *parent = nullptr);
    ~CurveManager();

    /**
     * @brief 将一个已创建的曲线对象添加到管理器中。
     * @param curve 要添加的曲线对象。
     */
    void addCurve(const ThermalCurve& curve);

    /**
     * @brief 从文件路径加载曲线。
     *
     * 它会寻找一个合适的读取器并用它来加载数据。
     * 如果成功，它会存储曲线并发出 curveAdded 信号。
     * @param filePath 文件的路径。
     * @return 如果加载成功，则返回 true，否则返回 false。
     */
    bool loadCurveFromFile(const QString& filePath);

    /**
     * @brief 根据列映射处理曲线的原始数据。
     * @param curveId 要处理的曲线的ID。
     * @param columnMapping 一个映射，其中键是数据类型（例如，“time”），值是列索引。
     */
    void processRawData(const QString& curveId, const QMap<QString, int>& columnMapping);

    /**
     * @brief 通过ID获取曲线。
     * @param curveId 曲线的唯一ID。
     * @return 指向 ThermalCurve 的指针，如果未找到则为 nullptr。
     */
    ThermalCurve* getCurve(const QString& curveId);

    /**
     * @brief 获取所有被管理的曲线。
     * @return 对曲线映射的常引用。
     */
    const QMap<QString, ThermalCurve>& getAllCurves() const;

    // --- Active Curve Management ---
    void setActiveCurve(const QString& curveId);
    ThermalCurve* getActiveCurve();

signals:
    /**
     * @brief 当新曲线成功添加到管理器时发出此信号。
     * @param curveId 新曲线的ID。
     */
    void curveAdded(const QString& curveId);

    /**
     * @brief 当曲线被移除时发出此信号。
     * @param curveId 被移除曲线的ID。
     */
    void curveRemoved(const QString& curveId);

    /**
     * @brief 当曲线数据被处理或更改时发出此信号。
     * @param curveId 被修改曲线的ID。
     */
    void curveDataChanged(const QString& curveId);

    /**
     * @brief 当活动曲线发生变化时发出此信号。
     * @param curveId 新的活动曲线的ID，如果无活动曲线则为空。
     */
    void activeCurveChanged(const QString& curveId);

private:
    void registerDefaultReaders();

    QMap<QString, ThermalCurve> m_curves; // 按ID存储加载的曲线
    std::vector<std::unique_ptr<IFileReader>> m_readers; // 拥有已注册的文件读取器
    QString m_activeCurveId; // 当前活动曲线的ID
};

#endif // CURVEMANAGER_H
