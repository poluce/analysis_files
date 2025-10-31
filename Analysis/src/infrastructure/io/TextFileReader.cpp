#include "TextFileReader.h"
#include "domain/model/ThermalCurve.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QUuid>

TextFileReader::TextFileReader() {}

QStringList TextFileReader::supportedFormats() const
{
    return { "Text Files (*.txt)", "Comma-Separated Values (*.csv)", "All Files (*)" };
}

bool TextFileReader::canRead(const QString& filePath) const
{
    return filePath.endsWith(".txt", Qt::CaseInsensitive) || filePath.endsWith(".csv", Qt::CaseInsensitive);
}

FilePreviewData TextFileReader::readPreview(const QString& filePath) const
{
    FilePreviewData previewData;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件进行预览:" << filePath << file.errorString();
        return previewData;
    }

    QTextStream in(&file);
    QStringList headerLines;
    QStringList dataLines;
    int lineCount = 0;

    // 读取最多30行作为预览
    while (!in.atEnd() && lineCount < 30) {
        const QString line = in.readLine();
        previewData.previewContent.append(line + "\n");

        const QString t = line.trimmed();
        if (t.isEmpty()) continue;

        QChar c = t.at(0);
        if (c.isLetter() || (c.unicode() >= 0x4e00 && c.unicode() <= 0x9fff)) {
            headerLines.append(t);
        } else {
            dataLines.append(t);
        }
        lineCount++;
    }

    previewData.header = headerLines.join("\n");

    // 从最后一行表头中提取列名
    if (!headerLines.isEmpty()) {
        const bool isCsv = filePath.endsWith(".csv", Qt::CaseInsensitive);
        QStringList headerCols = isCsv ? headerLines.last().split(',') : headerLines.last().split(QRegExp("\\s+"), Qt::SkipEmptyParts);
        for (int i = 0; i < headerCols.size(); ++i) {
            previewData.columns << QString("%1:%2").arg(i + 1).arg(headerCols.at(i).trimmed());
        }
    } else if (!dataLines.isEmpty()) {
        // 如果没有表头，则根据第一行数据列数生成默认列名
        const bool isCsv = filePath.endsWith(".csv", Qt::CaseInsensitive);
        int colCount = isCsv ? dataLines.first().count(',') + 1 : dataLines.first().split(QRegExp("\\s+"), Qt::SkipEmptyParts).size();
        for (int i = 0; i < colCount; ++i) {
            previewData.columns << QString("列 %1").arg(i + 1);
        }
    }

    file.close();
    return previewData;
}

ThermalCurve TextFileReader::read(const QString& filePath, const QVariantMap& config) const
{
    QString id = QUuid::createUuid().toString();
    QString name = config.value("signalName", "Unnamed Curve").toString();
    ThermalCurve curve(id, name);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件:" << filePath << file.errorString();
        return curve;
    }

    // 1. 分离表头和数据行
    QTextStream in(&file);
    QStringList dataLines;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QChar c = line.at(0);
        if (!c.isLetter() && c != '\0') { // 简单的判断，非字母开头的为数据行
            dataLines.append(line);
        }
    }
    file.close();

    if (dataLines.isEmpty()) return curve;

    // 2. 获取配置
    const int timeCol = config.value("timeColumn").toInt();
    const int tempCol = config.value("tempColumn").toInt();
    const int signalCol = config.value("signalColumn").toInt();
    const bool tempIsFixed = config.value("tempIsFixed").toBool();
    const double tempFixedValue = config.value("tempFixedValue").toDouble();

    // 3. 单位转换因子
    double timeFactor = 1.0;
    if (config.value("timeUnit").toString() == "min") timeFactor = 60.0;
    else if (config.value("timeUnit").toString() == "h") timeFactor = 3600.0;
    else if (config.value("timeUnit").toString() == "ms") timeFactor = 0.001;

    double tempOffset = 0.0;
    if (config.value("tempUnit").toString() == "K") tempOffset = -273.15;

    // 4. 解析数据
    const bool isCsv = filePath.endsWith(".csv", Qt::CaseInsensitive);
    auto splitLine = [&](const QString& s) -> QStringList {
        return isCsv ? s.split(',') : s.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    };

    QVector<ThermalDataPoint> points;
    points.reserve(dataLines.size());

    for (const QString& line : dataLines) {
        const QStringList cols = splitLine(line);
        bool ok;
        ThermalDataPoint point;

        double time = (timeCol < cols.size()) ? cols.at(timeCol).toDouble(&ok) * timeFactor : 0.0;
        point.time = ok ? time : 0.0;

        double temp = tempFixedValue;
        if (!tempIsFixed && tempCol < cols.size()) {
            temp = cols.at(tempCol).toDouble(&ok) + tempOffset;
        }
        point.temperature = ok ? temp : 0.0;

        double value = (signalCol < cols.size()) ? cols.at(signalCol).toDouble(&ok) : 0.0;
        point.value = ok ? value : 0.0;

        points.append(point);
    }

    curve.setRawData(points);

    // 5. 设置元数据
    CurveMetadata metadata;
    metadata.sampleName = config.value("signalName").toString();
    metadata.sampleMass = config.value("initialMass").toDouble();
    metadata.additional.insert("source_file", filePath);
    curve.setMetadata(metadata);

    // 6. 设置曲线类型
    QString typeStr = config.value("signalType").toString();
    if (typeStr == "TGA" || typeStr == "质量") {
        curve.setType(CurveType::TGA);
    } else {
        curve.setType(CurveType::DSC); // 默认为 DSC
    }

    qDebug() << "文件" << filePath << "已成功读取并应用配置。";
    return curve;
}
