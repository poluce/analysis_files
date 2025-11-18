#include "generic_algorithm_dialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "domain/algorithm/algorithm_descriptor.h"

// TODO (Phase 4): 此实现将被重构，使用增强后的领域层 AlgorithmDescriptor
// 当前简化版本仅支持基础参数类型

GenericAlgorithmDialog::GenericAlgorithmDialog(const AlgorithmDescriptor& desc, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(desc.name);  // 领域层使用 name 而非 displayName
    buildUi(desc);
}

void GenericAlgorithmDialog::buildUi(const AlgorithmDescriptor& desc) {
    auto* vbox = new QVBoxLayout(this);
    form_ = new QFormLayout();
    vbox->addLayout(form_);

    // 领域层使用 parameters 而非 params，使用 key 而非 name
    for (const auto& p : desc.parameters) {
        descMap_[p.key] = p;
        QWidget* editor = createEditor(p);
        editors_[p.key] = editor;

        form_->addRow(p.label, editor);
    }

    // 领域层使用 requiredPointCount 和 pointSelectionHint
    if (desc.requiredPointCount > 0) {
        auto* hint = new QLabel(tr("需要选点：%1 个。%2")
            .arg(desc.requiredPointCount)
            .arg(desc.pointSelectionHint), this);
        hint->setStyleSheet("color: gray;");
        form_->addRow(hint);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &GenericAlgorithmDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &GenericAlgorithmDialog::reject);
    vbox->addWidget(buttons);
}

QWidget* GenericAlgorithmDialog::createEditor(const AlgorithmParameterDefinition& p) {
    // 领域层使用 QVariant::Type 和通用 constraints QVariantMap
    switch (p.valueType) {
    case QVariant::Int: {
        auto* w = new QSpinBox(this);
        if (p.constraints.contains("min")) {
            w->setMinimum(p.constraints.value("min").toInt());
        }
        if (p.constraints.contains("max")) {
            w->setMaximum(p.constraints.value("max").toInt());
        }
        if (p.constraints.contains("step")) {
            w->setSingleStep(p.constraints.value("step").toInt());
        }
        w->setValue(p.defaultValue.isValid() ? p.defaultValue.toInt() : 0);
        return w;
    }
    case QVariant::Double: {
        auto* w = new QDoubleSpinBox(this);
        w->setDecimals(6);
        if (p.constraints.contains("min")) {
            w->setMinimum(p.constraints.value("min").toDouble());
        }
        if (p.constraints.contains("max")) {
            w->setMaximum(p.constraints.value("max").toDouble());
        }
        if (p.constraints.contains("step")) {
            w->setSingleStep(p.constraints.value("step").toDouble());
        }
        if (p.constraints.contains("unit")) {
            w->setSuffix(" " + p.constraints.value("unit").toString());
        }
        w->setValue(p.defaultValue.isValid() ? p.defaultValue.toDouble() : 0.0);
        return w;
    }
    case QVariant::Bool: {
        auto* w = new QCheckBox(this);
        w->setChecked(p.defaultValue.isValid() ? p.defaultValue.toBool() : false);
        return w;
    }
    case QVariant::String: {
        auto* w = new QLineEdit(this);
        w->setText(p.defaultValue.toString());
        return w;
    }
    case QVariant::StringList: {
        // 用于枚举类型（选项列表）
        auto* w = new QComboBox(this);
        if (p.constraints.contains("options")) {
            QStringList options = p.constraints.value("options").toStringList();
            w->addItems(options);
        }
        if (p.defaultValue.isValid()) {
            w->setCurrentText(p.defaultValue.toString());
        }
        return w;
    }
    default:
        return new QLabel(tr("不支持的参数类型"), this);
    }
}

QMap<QString, QVariant> GenericAlgorithmDialog::values() const {
    QMap<QString, QVariant> out;
    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
        out.insert(it.key(), readEditorValue(it.key()));
    }
    return out;
}

QVariant GenericAlgorithmDialog::readEditorValue(const QString& name) const {
    const auto p = descMap_.value(name);
    QWidget* w = editors_.value(name);

    switch (p.valueType) {
    case QVariant::Int:
        return qobject_cast<QSpinBox*>(w)->value();
    case QVariant::Double:
        return qobject_cast<QDoubleSpinBox*>(w)->value();
    case QVariant::Bool:
        return qobject_cast<QCheckBox*>(w)->isChecked();
    case QVariant::String:
        return qobject_cast<QLineEdit*>(w)->text();
    case QVariant::StringList: {
        auto* cb = qobject_cast<QComboBox*>(w);
        return cb->currentText();
    }
    default:
        return QVariant();
    }
}

bool GenericAlgorithmDialog::validateAll(QString* msg) const {
    for (auto it = descMap_.cbegin(); it != descMap_.cend(); ++it) {
        const auto& p = it.value();
        if (p.required) {
            const auto v = readEditorValue(p.key);
            if (!v.isValid() || (p.valueType == QVariant::String && v.toString().trimmed().isEmpty())) {
                if (msg) *msg = tr("参数 [%1] 为必填").arg(p.label);
                return false;
            }
        }
    }
    return true;
}

void GenericAlgorithmDialog::onAccept() {
    QString err;
    if (!validateAll(&err)) {
        setWindowTitle(windowTitle() + " - " + err);
        return;
    }
    accept();
}

