#ifndef DIFFERENTIATIONALGORITHM_H
#define DIFFERENTIATIONALGORITHM_H

#include "domain/algorithm/IThermalAlgorithm.h"

class DifferentiationAlgorithm : public IThermalAlgorithm
{
public:
    QVector<ThermalDataPoint> process(const QVector<ThermalDataPoint>& inputData) override;
    QString name() const override;
    QString category() const override;
    QVariantMap parameters() const override;
    void setParameter(const QString& key, const QVariant& value) override;
};

#endif // DIFFERENTIATIONALGORITHM_H
