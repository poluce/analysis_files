#include "algorithm_descriptor_registry.h"
#include <stdexcept>

namespace App {

AlgorithmDescriptorRegistry& AlgorithmDescriptorRegistry::instance() {
    static AlgorithmDescriptorRegistry inst;
    return inst;
}

void AlgorithmDescriptorRegistry::registerDescriptor(const AlgorithmDescriptor& desc) {
    map_[desc.name] = desc;
}

bool AlgorithmDescriptorRegistry::has(const QString& name) const {
    return map_.contains(name);
}

AlgorithmDescriptor AlgorithmDescriptorRegistry::get(const QString& name) const {
    auto it = map_.find(name);
    if (it == map_.end()) throw std::runtime_error("Descriptor not found");
    return it.value();
}

QVector<AlgorithmDescriptor> AlgorithmDescriptorRegistry::all() const {
    return map_.values().toVector();
}

} // namespace App

