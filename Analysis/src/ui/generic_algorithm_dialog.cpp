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
#include "application/algorithm/metadata_descriptor.h"

using namespace App;

GenericAlgorithmDialog::GenericAlgorithmDialog(const AlgorithmDescriptor& desc, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(desc.displayName);
    buildUi(desc);
}

void GenericAlgorithmDialog::buildUi(const AlgorithmDescriptor& desc) {
    auto* vbox = new QVBoxLayout(this);
    form_ = new QFormLayout();
    vbox->addLayout(form_);

    for (const auto& p : desc.params) {
        descMap_[p.name] = p;
        QWidget* editor = createEditor(p);
        editors_[p.name] = editor;
        form_->addRow(p.label, editor);
        if (!p.description.isEmpty()) {
            form_->addRow(new QLabel(p.description, this));
        }
    }

    if (desc.pointSelection.has_value()) {
        const auto& spec = desc.pointSelection.value();
        auto* hint = new QLabel(tr("需要选点：%1-%2 个。%3")
            .arg(spec.minCount)
            .arg(spec.maxCount < 0 ? tr("∞") : QString::number(spec.maxCount))
            .arg(spec.hint), this);
        hint->setStyleSheet("color: gray;");
        form_->addRow(hint);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &GenericAlgorithmDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &GenericAlgorithmDialog::reject);
    vbox->addWidget(buttons);
}

QWidget* GenericAlgorithmDialog::createEditor(const ParameterDescriptor& p) {
    switch (p.type) {
    case ParamType::Integer: {
        auto* w = new QSpinBox(this);
        if (p.intConstraint.has_value()) {
            w->setMinimum(p.intConstraint->min);
            w->setMaximum(p.intConstraint->max);
            w->setSingleStep(p.intConstraint->step);
        }
        w->setValue(p.defaultValue.isValid() ? p.defaultValue.toInt() : 0);
        return w;
    }
    case ParamType::Double: {
        auto* w = new QDoubleSpinBox(this);
        w->setDecimals(6);
        if (p.doubleConstraint.has_value()) {
            w->setMinimum(p.doubleConstraint->min);
            w->setMaximum(p.doubleConstraint->max);
            w->setSingleStep(p.doubleConstraint->step);
            if (!p.doubleConstraint->unit.isEmpty())
                w->setSuffix(" " + p.doubleConstraint->unit);
        }
        w->setValue(p.defaultValue.isValid() ? p.defaultValue.toDouble() : 0.0);
        return w;
    }
    case ParamType::Boolean: {
        auto* w = new QCheckBox(this);
        w->setChecked(p.defaultValue.isValid() ? p.defaultValue.toBool() : false);
        return w;
    }
    case ParamType::String: {
        auto* w = new QLineEdit(this);
        w->setText(p.defaultValue.toString());
        return w;
    }
    case ParamType::Enum: {
        auto* w = new QComboBox(this);
        int idx = 0, defIdx = -1;
        for (const auto& opt : p.enumOptions) {
            w->addItem(opt.label, opt.value);
            if (defIdx < 0 && p.defaultValue.isValid() && p.defaultValue.toString() == opt.value) {
                defIdx = idx;
            }
            ++idx;
        }
        w->setCurrentIndex(defIdx >= 0 ? defIdx : 0);
        return w;
    }
    case ParamType::DoubleRange: {
        auto* container = new QWidget(this);
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0,0,0,0);
        auto* a = new QDoubleSpinBox(container);
        auto* b = new QDoubleSpinBox(container);
        a->setDecimals(6); b->setDecimals(6);
        if (p.doubleConstraint.has_value()) {
            for (auto* w : {a,b}) {
                w->setMinimum(p.doubleConstraint->min);
                w->setMaximum(p.doubleConstraint->max);
                w->setSingleStep(p.doubleConstraint->step);
                if (!p.doubleConstraint->unit.isEmpty())
                    w->setSuffix(" " + p.doubleConstraint->unit);
            }
        }
        if (p.defaultValue.canConvert<QVariantList>()) {
            auto lst = p.defaultValue.toList();
            if (lst.size() >= 2) { a->setValue(lst[0].toDouble()); b->setValue(lst[1].toDouble()); }
        }
        layout->addWidget(a); layout->addWidget(new QLabel(" ~ ", container)); layout->addWidget(b);
        return container;
    }
    case ParamType::PointsOnChart:
        return new QLabel(tr("执行前将进入选点模式"), this);
    }
    return new QLabel(tr("未知参数类型"), this);
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

    switch (p.type) {
    case ParamType::Integer: return qobject_cast<QSpinBox*>(w)->value();
    case ParamType::Double:  return qobject_cast<QDoubleSpinBox*>(w)->value();
    case ParamType::Boolean: return qobject_cast<QCheckBox*>(w)->isChecked();
    case ParamType::String:  return qobject_cast<QLineEdit*>(w)->text();
    case ParamType::Enum: {
        auto* cb = qobject_cast<QComboBox*>(w);
        return cb->currentData();
    }
    case ParamType::DoubleRange: {
        auto* container = w;
        auto spins = container->findChildren<QDoubleSpinBox*>();
        if (spins.size() >= 2) {
            return QVariantList{ spins[0]->value(), spins[1]->value() };
        }
        return QVariantList{};
    }
    case ParamType::PointsOnChart:
        return QVariant();
    }
    return QVariant();
}

bool GenericAlgorithmDialog::validateAll(QString* msg) const {
    for (auto it = descMap_.cbegin(); it != descMap_.cend(); ++it) {
        const auto& p = it.value();
        if (p.required) {
            const auto v = readEditorValue(p.name);
            if (!v.isValid() || (p.type == ParamType::String && v.toString().trimmed().isEmpty())) {
                if (msg) *msg = tr("参数 [%1] 为必填").arg(p.label);
                return false;
            }
        }
        if (p.type == ParamType::DoubleRange) {
            auto lst = readEditorValue(p.name).toList();
            if (lst.size() >= 2 && lst[0].toDouble() > lst[1].toDouble()) {
                if (msg) *msg = tr("参数 [%1] 的最小值应不大于最大值").arg(p.label);
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

