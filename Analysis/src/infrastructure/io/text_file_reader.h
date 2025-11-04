#ifndef TEXTFILEREADER_H
#define TEXTFILEREADER_H

#include "infrastructure/io/i_file_reader.h"
#include <QVariantMap>

// 定义一个简单的结构体来存储预览数据
struct FilePreviewData {
    QString header;
    QString previewContent;
    QStringList columns;
};

class TextFileReader : public IFileReader {
public:
    TextFileReader();

    bool canRead(const QString& filePath) const override;
    // 更新 read 方法以匹配接口
    ThermalCurve read(const QString& filePath, const QVariantMap& config) const override;
    QStringList supportedFormats() const override;

    // 新增一个只读取预览数据的方法
    FilePreviewData readPreview(const QString& filePath) const;
};

#endif // TEXTFILEREADER_H
