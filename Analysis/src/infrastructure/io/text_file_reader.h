#ifndef TEXTFILEREADER_H
#define TEXTFILEREADER_H

#include "infrastructure/io/i_file_reader.h"
#include <QVariantMap>
#include <QVector>

/**
 * @brief 文件预览列信息
 *
 * 描述文件中一列的索引和标签
 */
struct FilePreviewColumn {
    int index = -1;      //!< 列索引（从0开始）
    QString label;       //!< 列标签（如 "温度", "时间", "质量"）
};

/**
 * @brief 文件预览数据
 *
 * 包含文件的表头、预览内容和列信息，用于在数据导入对话框中显示
 */
struct FilePreviewData {
    QString header;                      //!< 文件表头（前几行）
    QString previewContent;              //!< 预览内容（前几行数据）
    QVector<FilePreviewColumn> columns;  //!< 列信息列表
};

/**
 * @brief TextFileReader 实现了文本文件（.txt, .csv）的读取
 *
 * 支持：
 * - 智能列识别（温度、时间、质量等）
 * - 文件预览
 * - 用户自定义列映射
 */
class TextFileReader : public IFileReader {
public:
    TextFileReader();

    /**
     * @brief 检查是否可以读取指定文件
     * @param filePath 文件路径
     * @return 如果文件扩展名为 .txt 或 .csv 返回 true
     */
    bool canRead(const QString& filePath) const override;

    /**
     * @brief 读取文件并返回热分析曲线对象
     * @param filePath 文件路径
     * @param config 用户导入配置（列映射、单位等）
     * @return 解析后的 ThermalCurve 对象
     */
    ThermalCurve read(const QString& filePath, const QVariantMap& config) const override;

    /**
     * @brief 获取支持的文件格式列表
     * @return 格式列表（如 "Text Files (*.txt *.csv)"）
     */
    QStringList supportedFormats() const override;

    /**
     * @brief 读取文件预览数据
     *
     * 读取文件的前几行和列信息，用于在导入对话框中显示预览和进行列映射
     *
     * @param filePath 文件路径
     * @return 文件预览数据
     */
    FilePreviewData readPreview(const QString& filePath) const;
};

#endif // TEXTFILEREADER_H
