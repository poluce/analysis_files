#include "text_file_reader.h"
#include "domain/model/thermal_curve.h"
#include <QDebug>
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUuid>

TextFileReader::TextFileReader() { qDebug() << "构造:  TextFileReader"; }

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
    // 强制按 GBK 解码文本，避免受系统本地编码影响
    in.setCodec("GBK");

    QStringList headerLines;
    QStringList dataLines;
    int lineCount = 0;

    // 读取最多30行作为预览
    while (!in.atEnd() && lineCount < 30) {
        const QString line = in.readLine();
        previewData.previewContent.append(line + "\n");

        const QString t = line.trimmed();
        if (t.isEmpty())
            continue;

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
        const QStringList headerCols
            = isCsv ? headerLines.last().split(',') : headerLines.last().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        previewData.columns.reserve(headerCols.size());
        for (int i = 0; i < headerCols.size(); ++i) {
            FilePreviewColumn column;
            column.index = i;
            column.label = headerCols.at(i).trimmed();
            previewData.columns.append(column);
        }
    } else if (!dataLines.isEmpty()) {
        // 如果没有表头，则根据第一行数据列数生成默认列名
        const bool isCsv = filePath.endsWith(".csv", Qt::CaseInsensitive);
        const int colCount
            = isCsv ? dataLines.first().count(',') + 1 : dataLines.first().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
        previewData.columns.reserve(colCount);
        for (int i = 0; i < colCount; ++i) {
            FilePreviewColumn column;
            column.index = i;
            column.label = QStringLiteral("列 %1").arg(i + 1);
            previewData.columns.append(column);
        }
    }

    file.close();
    return previewData;
}

ThermalCurve TextFileReader::read(const QString& filePath, const QVariantMap& config) const
{
    QString id = QUuid::createUuid().toString();
    // 曲线名称设置为"[源]"，项目名称使用文件名
    QFileInfo fileInfo(filePath);
    QString projectName = fileInfo.fileName();
    ThermalCurve curve(id, "[源]");
    curve.setProjectName(projectName);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件:" << filePath << file.errorString();
        return curve;
    }

    // 1. 分离表头和数据行
    QTextStream in(&file);
    // 强制按 GBK 解码文本，避免受系统本地编码影响
    in.setCodec("GBK");

    QStringList dataLines;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        QChar c = line.at(0);
        if (!c.isLetter() && c != '\0') { // 简单的判断，非字母开头的为数据行
            dataLines.append(line);
        }
    }
    file.close();

    if (dataLines.isEmpty())
        return curve;

    // 2. 获取配置
    const int timeCol = config.value("timeColumn").toInt();
    const int tempCol = config.value("tempColumn").toInt();
    const int signalCol = config.value("signalColumn").toInt();
    const bool tempIsFixed = config.value("tempIsFixed").toBool();
    const double tempFixedValue = config.value("tempFixedValue").toDouble();

    // 3. 单位转换因子
    double timeFactor = 1.0;
    if (config.value("timeUnit").toString() == "min")
        timeFactor = 60.0;
    else if (config.value("timeUnit").toString() == "h")
        timeFactor = 3600.0;
    else if (config.value("timeUnit").toString() == "ms")
        timeFactor = 0.001;

    double tempOffset = 0.0;
    if (config.value("tempUnit").toString() == "K")
        tempOffset = -273.15;

    // 4. 解析数据
    const bool isCsv = filePath.endsWith(".csv", Qt::CaseInsensitive);
    auto splitLine
        = [&](const QString& s) -> QStringList { return isCsv ? s.split(',') : s.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts); };

    QVector<ThermalDataPoint> points;
    points.reserve(dataLines.size());

    for (const QString& line : dataLines) {
        const QStringList cols = splitLine(line);
        ThermalDataPoint point;

        bool ok = false;
        double time = 0.0;
        if (timeCol >= 0 && timeCol < cols.size()) {
            time = cols.at(timeCol).toDouble(&ok) * timeFactor;
        }
        point.time = ok ? time : 0.0;

        bool tempOk = false;
        double temp = tempFixedValue;
        if (!tempIsFixed && tempCol >= 0 && tempCol < cols.size()) {
            temp = cols.at(tempCol).toDouble(&tempOk) + tempOffset;
        } else {
            tempOk = true;
        }
        point.temperature = tempOk ? temp : tempFixedValue;

        ok = false;
        double value = 0.0;
        if (signalCol >= 0 && signalCol < cols.size()) {
            value = cols.at(signalCol).toDouble(&ok);
        }
        point.value = ok ? value : 0.0;

        points.append(point);
    }

    // 5. 设置元数据
    CurveMetadata metadata;
    metadata.sampleName = config.value("signalName").toString();
    metadata.sampleMass = config.value("initialMass").toDouble();
    metadata.additional.insert("source_file", filePath);

    // 6. 设置曲线类型（仪器类型 + 信号类型）并进行质量百分比转换
    QString typeStr = config.value("curveType").toString();
    if (typeStr.isEmpty()) {
        typeStr = config.value("signalType").toString();
    }
    const QString normalizedType = typeStr.trimmed().toUpper();

    if (normalizedType == "TGA" || typeStr == "质量") {
        curve.setInstrumentType(InstrumentType::TGA);
        curve.setSignalType(SignalType::Raw);

        // 如果是质量类型且设置了初始质量，转换为质量损失百分比
        if (metadata.sampleMass > 0.0) {
            qDebug() << "将质量数据转换为百分比，初始质量:" << metadata.sampleMass;
            for (ThermalDataPoint& point : points) {
                // 质量损失百分比 = (当前质量 / 初始质量) * 100
                point.value = (point.value / metadata.sampleMass) * 100.0;
            }
        }
    } else if (normalizedType == "ARC") {
        curve.setInstrumentType(InstrumentType::ARC);
        curve.setSignalType(SignalType::Raw);
    } else if (normalizedType == "DTG") {
        // DTG 是 TGA 的微分信号
        curve.setInstrumentType(InstrumentType::TGA);
        curve.setSignalType(SignalType::Derivative);
    } else if (normalizedType == "DSCDERIV" || normalizedType == "DDSC") {
        // DSC 的微分信号（如果文件中有的话）
        curve.setInstrumentType(InstrumentType::DSC);
        curve.setSignalType(SignalType::Derivative);
    } else {
        // 默认为 DSC 原始信号
        curve.setInstrumentType(InstrumentType::DSC);
        curve.setSignalType(SignalType::Raw);
    }

    curve.setRawData(points);
    curve.setMetadata(metadata);

    qDebug() << "文件" << filePath << "已成功读取并应用配置。";
    return curve;
}
