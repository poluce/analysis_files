#pragma once

#include <QDialog>
#include <QMap>
#include <QVariant>
#include "application/algorithm/metadata_descriptor.h"

class QFormLayout;

class GenericAlgorithmDialog : public QDialog {
    Q_OBJECT
public:
    explicit GenericAlgorithmDialog(const App::AlgorithmDescriptor& desc, QWidget* parent = nullptr);

    // 用户点击“确定”后读取
    QMap<QString, QVariant> values() const;

private:
    void buildUi(const App::AlgorithmDescriptor& desc);
    QWidget* createEditor(const App::ParameterDescriptor& pdesc);
    QVariant readEditorValue(const QString& name) const;
    bool validateAll(QString* errorMessage) const;

private slots:
    void onAccept();

private:
    QFormLayout* form_ = nullptr;
    // name -> editor widget
    QMap<QString, QWidget*> editors_;
    // name -> descriptor（用于校验）
    QMap<QString, App::ParameterDescriptor> descMap_;
};

