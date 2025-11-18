#pragma once

#include <QDialog>
#include <QMap>
#include <QVariant>
#include "domain/algorithm/algorithm_descriptor.h"

class QFormLayout;

// TODO (Phase 4): 此类将被重构为动态参数对话框实现
// 当前使用领域层 AlgorithmDescriptor，但功能受限
// Phase 4 将使用 Qt 官方推荐方式（QDialog + QFormLayout）重新实现
class GenericAlgorithmDialog : public QDialog {
    Q_OBJECT
public:
    explicit GenericAlgorithmDialog(const AlgorithmDescriptor& desc, QWidget* parent = nullptr);

    // 用户点击"确定"后读取
    QMap<QString, QVariant> values() const;

private:
    void buildUi(const AlgorithmDescriptor& desc);
    QWidget* createEditor(const AlgorithmParameterDefinition& pdesc);
    QVariant readEditorValue(const QString& name) const;
    bool validateAll(QString* errorMessage) const;

private slots:
    void onAccept();

private:
    QFormLayout* form_ = nullptr;
    // name -> editor widget
    QMap<QString, QWidget*> editors_;
    // name -> descriptor（用于校验）
    QMap<QString, AlgorithmParameterDefinition> descMap_;
};

