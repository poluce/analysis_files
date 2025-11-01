#ifndef CURVEMANAGER_H
#define CURVEMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <memory>
#include <vector>
#include "domain/model/ThermalCurve.h"
#include "infrastructure/io/IFileReader.h"

class CurveManager : public QObject
{
    Q_OBJECT

public:
    explicit CurveManager(QObject *parent = nullptr);
    ~CurveManager();

    // 管理曲线
    void addCurve(const ThermalCurve& curve);
    bool loadCurveFromFile(const QString& filePath);
    void processRawData(const QString& curveId, const QMap<QString, int>& columnMapping);
    ThermalCurve* getCurve(const QString& curveId);
    const QMap<QString, ThermalCurve>& getAllCurves() const;
    void clearCurves();
    bool removeCurve(const QString& curveId);

    // 活动曲线管理
    void setActiveCurve(const QString& curveId);
    ThermalCurve* getActiveCurve();

signals:
    void curveAdded(const QString& curveId);
    void activeCurveChanged(const QString& curveId);
    void curveDataChanged(const QString& curveId);  // 曲线数据被修改时发射
    void curvesCleared();
    void curveRemoved(const QString& curveId);

private:
    void registerDefaultReaders();

    QMap<QString, ThermalCurve> m_curves;
    std::vector<std::unique_ptr<IFileReader>> m_readers;
    QString m_activeCurveId;
};

#endif // CURVEMANAGER_H
