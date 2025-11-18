#pragma once

#include <QMap>
#include <QVector>
#include <QString>
#include "algorithm_descriptor.h"

namespace App {

class AlgorithmDescriptorRegistry {
public:
    static AlgorithmDescriptorRegistry& instance();

    void registerDescriptor(const AlgorithmDescriptor& desc);
    bool has(const QString& name) const;
    AlgorithmDescriptor get(const QString& name) const;
    QVector<AlgorithmDescriptor> all() const;

private:
    QMap<QString, AlgorithmDescriptor> map_;
};

} // namespace App

