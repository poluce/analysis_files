#ifndef IFILEREADER_H
#define IFILEREADER_H

#include "domain/model/ThermalCurve.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>

/**
 * @brief IFileReader 类为所有文件读取器实现定义了接口。
 *
 * 这允许使用策略模式，其中不同的文件格式可以由不同的具体读取器类来读取。
 */
class IFileReader
{
public:
    virtual ~IFileReader() = default;

    /**
     * @brief 检查读取器是否能处理给定的文件。
     * @param filePath 文件的路径。
     * @return 如果读取器支持此文件格式，则返回 true，否则返回 false。
     */
    virtual bool canRead(const QString& filePath) const = 0;

    /**
     * @brief 读取一个文件并返回一个 ThermalCurve 对象。
     * @param filePath 文件的路径。
     * @param config 包含用户导入配置的映射。
     * @return 一个包含文件数据的 ThermalCurve 对象。
     */
    virtual ThermalCurve read(const QString& filePath, const QVariantMap& config) const = 0;

    /**
     * @brief 获取支持的文件格式描述列表（例如，“Text Files (*.txt)”）。
     * @return 支持的格式字符串列表。
     */
    virtual QStringList supportedFormats() const = 0;
};

#endif // IFILEREADER_H
