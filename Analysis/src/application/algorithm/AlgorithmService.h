#ifndef ALGORITHMSERVICE_H
#define ALGORITHMSERVICE_H

#include <QObject>
#include <QMap>
#include <QString>

// 前置声明
class IThermalAlgorithm;
class ThermalCurve;

class AlgorithmService : public QObject
{
    Q_OBJECT
public:
    static AlgorithmService* instance();

    void registerAlgorithm(IThermalAlgorithm* algorithm);
    IThermalAlgorithm* getAlgorithm(const QString& name);
    void execute(const QString& name, ThermalCurve* curve);

signals:
    void algorithmFinished(const QString& curveId);

private:
    explicit AlgorithmService(QObject *parent = nullptr);
    ~AlgorithmService();
    AlgorithmService(const AlgorithmService&) = delete;
    AlgorithmService& operator=(const AlgorithmService&) = delete;

    QMap<QString, IThermalAlgorithm*> m_algorithms;
};

#endif // ALGORITHMSERVICE_H
