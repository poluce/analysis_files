#include "data_import_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
QString columnDisplayText(const FilePreviewColumn& column)
{
    const int displayIndex = column.index + 1;
    if (column.label.isEmpty()) {
        return QStringLiteral("%1").arg(displayIndex);
    }
    return QStringLiteral("%1 %2").arg(displayIndex).arg(column.label);
}
} // namespace

DataImportWidget::DataImportWidget(QWidget* parent)
    : QWidget(parent)
{
    qDebug() << "构造:  DataImportWidget";

    // 顶部文件选择行
    auto* fileLabel = new QLabel(tr("数据文件"), this);
    m_filePathEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(tr("..."), this);

    auto* fileLayout = new QHBoxLayout;
    fileLayout->addWidget(fileLabel);
    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(m_browseBtn);

    // 表头和预览区域
    auto* headerLabel = new QLabel(tr("表头"), this);
    m_headerPreview = new QTextEdit(this);
    m_headerPreview->setFixedHeight(80);
    m_headerPreview->setReadOnly(true);

    m_previewArea = new QPlainTextEdit(this);
    m_previewArea->setPlaceholderText(tr("文件预览..."));
    m_previewArea->setMinimumHeight(160);

    auto* middleLayout = new QVBoxLayout;
    middleLayout->addWidget(headerLabel);
    middleLayout->addWidget(m_headerPreview);
    middleLayout->addWidget(m_previewArea);

    // 质量（样品初始质量）
    auto* massLabel = new QLabel(tr("样品初始质量"), this);
    m_initialMassSpin = new QDoubleSpinBox(this);
    m_initialMassSpin->setDecimals(5);
    m_initialMassSpin->setRange(0.0, 1e6);
    m_initialMassSpin->setValue(0.0);
    m_massUnitLabel = new QLabel(tr("g"), this);
    auto* dataTypeLabel = new QLabel(tr("数据类型"), this);
    m_dataTypeCombo = new QComboBox(this);
    m_dataTypeCombo->addItem(QStringLiteral("TGA"), QStringLiteral("TGA"));
    m_dataTypeCombo->addItem(QStringLiteral("ARC"), QStringLiteral("ARC"));
    m_dataTypeCombo->addItem(QStringLiteral("DSC"), QStringLiteral("DSC"));

    auto* massLayout = new QHBoxLayout;
    massLayout->addWidget(massLabel);
    massLayout->addWidget(m_initialMassSpin);
    massLayout->addWidget(m_massUnitLabel);
    massLayout->addSpacing(12);
    massLayout->addWidget(dataTypeLabel);
    massLayout->addWidget(m_dataTypeCombo);
    massLayout->addStretch();

    // 参数分组
    QWidget* tempGroup = createTemperatureGroup();
    QWidget* timeGroup = createTimeGroup();
    QWidget* signalGroup = createSignalGroup();
    QWidget* rateGroup = createRateGroup();

    auto* paramsLayout = new QHBoxLayout;
    paramsLayout->setSpacing(12);
    paramsLayout->addWidget(tempGroup);
    paramsLayout->addWidget(timeGroup);
    paramsLayout->addWidget(signalGroup);
    paramsLayout->addWidget(rateGroup);

    // 底部按钮
    m_importBtn = new QPushButton(tr("导入"), this);
    m_closeBtn = new QPushButton(tr("关闭"), this);

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(m_importBtn);
    btnLayout->addWidget(m_closeBtn);

    // 主布局
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(fileLayout);
    mainLayout->addLayout(middleLayout);
    mainLayout->addWidget(createSeparator());
    mainLayout->addLayout(massLayout);
    mainLayout->addLayout(paramsLayout);
    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
    setWindowTitle(tr("数据导入"));

    // 缺省状态
    m_tempHasColumnChk->setChecked(true);
    m_noTempColumnChk->setChecked(false);
    m_rateHasColumnChk->setChecked(false);
    m_continuousChk->setChecked(true);

    setupConnections();
    updateTemperatureControls();
    updateRateControls();
}

QWidget* DataImportWidget::createTemperatureGroup()
{
    auto* box = new QGroupBox(tr("温度"), this);

    m_tempHasColumnChk = new QCheckBox(tr("使用列数据"), box);
    m_tempColumnCombo = new QComboBox(box);
    m_tempColumnCombo->setMinimumWidth(120);
    m_tempUnitCombo = new QComboBox(box);
    m_tempUnitCombo->addItems({ QStringLiteral("°C"), QStringLiteral("K") });
    m_noTempColumnChk = new QCheckBox(tr("没有温度列，改用固定值"), box);
    m_tempSetSpin = new QDoubleSpinBox(box);
    m_tempSetSpin->setRange(-273.15, 10000.0);
    m_tempSetSpin->setDecimals(2);
    m_tempSetSpin->setSuffix(tr(" °C"));

    auto* layout = new QGridLayout(box);
    layout->addWidget(m_tempHasColumnChk, 0, 0, 1, 2);
    layout->addWidget(new QLabel(tr("列"), box), 1, 0);
    layout->addWidget(m_tempColumnCombo, 1, 1);
    layout->addWidget(new QLabel(tr("单位"), box), 2, 0);
    layout->addWidget(m_tempUnitCombo, 2, 1);
    layout->addWidget(m_noTempColumnChk, 3, 0, 1, 2);
    layout->addWidget(new QLabel(tr("固定温度"), box), 4, 0);
    layout->addWidget(m_tempSetSpin, 4, 1);

    box->setLayout(layout);
    return box;
}

QWidget* DataImportWidget::createTimeGroup()
{
    auto* box = new QGroupBox(tr("时间"), this);

    m_timeColumnCombo = new QComboBox(box);
    m_timeUnitCombo = new QComboBox(box);
    m_timeUnitCombo->addItems({ QStringLiteral("s"), QStringLiteral("ms"), QStringLiteral("min"), QStringLiteral("h") });
    m_filterPointsSpin = new QSpinBox(box);
    m_filterPointsSpin->setRange(0, 10000);
    m_filterPointsSpin->setValue(0);

    auto* form = new QFormLayout(box);
    form->addRow(tr("列"), m_timeColumnCombo);
    form->addRow(tr("单位"), m_timeUnitCombo);
    form->addRow(tr("采样点滤波"), m_filterPointsSpin);

    box->setLayout(form);
    return box;
}

QWidget* DataImportWidget::createSignalGroup()
{
    auto* box = new QGroupBox(tr("信号"), this);

    m_signalColumnCombo = new QComboBox(box);
    m_continuousChk = new QCheckBox(tr("连续采集"), box);
    m_noncontinuousChk = new QCheckBox(tr("不连续采集"), box);
    m_signalTypeCombo = new QComboBox(box);
    m_signalTypeCombo->addItems({ tr("质量"), tr("DTA") });
    m_signalUnitEdit = new QLineEdit(box);
    m_signalUnitEdit->setFixedWidth(64);
    m_signalUnitEdit->setPlaceholderText(tr("单位"));
    m_signalNameEdit = new QLineEdit(box);
    m_signalNameEdit->setPlaceholderText(tr("曲线名称"));

    auto* layout = new QGridLayout(box);
    layout->addWidget(new QLabel(tr("列"), box), 0, 0);
    layout->addWidget(m_signalColumnCombo, 0, 1);
    layout->addWidget(m_continuousChk, 1, 0, 1, 2);
    layout->addWidget(m_noncontinuousChk, 2, 0, 1, 2);
    layout->addWidget(new QLabel(tr("信号类型"), box), 3, 0);
    layout->addWidget(m_signalTypeCombo, 3, 1);
    layout->addWidget(new QLabel(tr("单位"), box), 4, 0);
    layout->addWidget(m_signalUnitEdit, 4, 1);
    layout->addWidget(new QLabel(tr("名称"), box), 5, 0);
    layout->addWidget(m_signalNameEdit, 5, 1);

    box->setLayout(layout);
    return box;
}

QWidget* DataImportWidget::createRateGroup()
{
    auto* box = new QGroupBox(tr("速率"), this);

    m_rateHasColumnChk = new QCheckBox(tr("使用列数据"), box);
    m_rateColumnCombo = new QComboBox(box);
    m_rateUnitCombo = new QComboBox(box);
    m_rateUnitCombo->addItems({ QStringLiteral("°C/min"), QStringLiteral("°C/s"), QStringLiteral("%/min") });
    m_dynamicRateSpin = new QSpinBox(box);
    m_dynamicRateSpin->setRange(0, 10000);
    m_dynamicRateSpin->setSuffix(tr(" 设定"));

    auto* form = new QFormLayout(box);
    form->addRow(m_rateHasColumnChk, m_rateColumnCombo);
    form->addRow(tr("单位"), m_rateUnitCombo);
    form->addRow(tr("固定速率"), m_dynamicRateSpin);

    box->setLayout(form);
    return box;
}

QFrame* DataImportWidget::createSeparator()
{
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setFixedHeight(1);
    return line;
}

void DataImportWidget::setupConnections()
{
    connect(m_browseBtn, &QPushButton::clicked, this, &DataImportWidget::onBrowseFile);
    connect(m_importBtn, &QPushButton::clicked, this, &DataImportWidget::onImportClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &DataImportWidget::onCloseClicked);

    connect(m_tempHasColumnChk, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked) {
            QSignalBlocker blocker(m_noTempColumnChk);
            m_noTempColumnChk->setChecked(true);
        }
        updateTemperatureControls();
    });

    connect(m_noTempColumnChk, &QCheckBox::toggled, this, [this](bool /*checked*/) { updateTemperatureControls(); });

    connect(m_rateHasColumnChk, &QCheckBox::toggled, this, [this](bool /*checked*/) { updateRateControls(); });

    auto ensureExclusive = [this](QCheckBox* box) {
        connect(box, &QCheckBox::toggled, this, [this, box](bool checked) {
            if (checked) {
                handleSignalModeToggle(box);
            }
        });
    };
    ensureExclusive(m_continuousChk);
    ensureExclusive(m_noncontinuousChk);
}

void DataImportWidget::populateColumnCombos(const QVector<FilePreviewColumn>& columns)
{
    auto populate = [&](QComboBox* combo) {
        if (!combo) {
            return;
        }
        QSignalBlocker blocker(combo);
        combo->clear();
        for (const auto& column : columns) {
            combo->addItem(columnDisplayText(column), column.index);
        }
        if (!columns.isEmpty()) {
            combo->setCurrentIndex(0);
        }
    };

    populate(m_tempColumnCombo);
    populate(m_timeColumnCombo);
    populate(m_signalColumnCombo);
    populate(m_rateColumnCombo);
}

int DataImportWidget::extractColumnIndex(const QComboBox* combo) const
{
    if (!combo || combo->currentIndex() < 0) {
        return -1;
    }
    bool ok = false;
    const int value = combo->currentData().toInt(&ok);
    return ok ? value : -1;
}

void DataImportWidget::updateTemperatureControls()
{
    const bool useColumn = m_tempHasColumnChk->isChecked();

    m_tempColumnCombo->setEnabled(useColumn);
    m_tempUnitCombo->setEnabled(useColumn);

    {
        QSignalBlocker blocker(m_noTempColumnChk);
        if (!useColumn) {
            m_noTempColumnChk->setChecked(true);
        }
        m_noTempColumnChk->setEnabled(useColumn);
    }

    const bool fixedValueEnabled = !useColumn || m_noTempColumnChk->isChecked();
    m_tempSetSpin->setEnabled(fixedValueEnabled);
}

void DataImportWidget::updateRateControls()
{
    const bool useColumn = m_rateHasColumnChk->isChecked();
    m_rateColumnCombo->setEnabled(useColumn);
    m_rateUnitCombo->setEnabled(useColumn);
    m_dynamicRateSpin->setEnabled(!useColumn);
}

void DataImportWidget::handleSignalModeToggle(QCheckBox* source)
{
    if (!source) {
        return;
    }
    if (source == m_continuousChk) {
        QSignalBlocker blocker(m_noncontinuousChk);
        m_noncontinuousChk->setChecked(false);
    } else if (source == m_noncontinuousChk) {
        QSignalBlocker blocker(m_continuousChk);
        m_continuousChk->setChecked(false);
    }
}

void DataImportWidget::onBrowseFile()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("选择数据文件"), QString(), tr("所有文件 (*)"));
    if (file.isEmpty()) {
        return;
    }
    m_filePathEdit->setText(file);
    emit previewRequested(file);
}

void DataImportWidget::setPreviewData(const FilePreviewData& previewData)
{
    m_headerPreview->setPlainText(previewData.header);
    m_previewArea->setPlainText(previewData.previewContent);
    populateColumnCombos(previewData.columns);
    updateTemperatureControls();
    updateRateControls();
}

void DataImportWidget::onImportClicked() { emit importRequested(); }

void DataImportWidget::onCloseClicked() { close(); }

QVariantMap DataImportWidget::getImportConfig() const
{
    QVariantMap config;

    config.insert(QStringLiteral("filePath"), m_filePathEdit->text());
    config.insert(QStringLiteral("initialMass"), m_initialMassSpin->value());

    QString curveType = m_dataTypeCombo->currentData().toString();
    if (curveType.isEmpty()) {
        curveType = m_dataTypeCombo->currentText();
    }
    config.insert(QStringLiteral("curveType"), curveType);

    const bool tempFromColumn = m_tempHasColumnChk->isChecked();
    const bool tempFixed = !tempFromColumn || m_noTempColumnChk->isChecked();

    config.insert(QStringLiteral("tempFromColumn"), tempFromColumn);
    config.insert(QStringLiteral("tempColumn"), tempFromColumn ? extractColumnIndex(m_tempColumnCombo) : -1);
    config.insert(QStringLiteral("tempUnit"), m_tempUnitCombo->currentText());
    config.insert(QStringLiteral("tempIsFixed"), tempFixed);
    config.insert(QStringLiteral("tempFixedValue"), m_tempSetSpin->value());

    config.insert(QStringLiteral("timeColumn"), extractColumnIndex(m_timeColumnCombo));
    config.insert(QStringLiteral("timeUnit"), m_timeUnitCombo->currentText());
    config.insert(QStringLiteral("filterPoints"), m_filterPointsSpin->value());

    config.insert(QStringLiteral("signalColumn"), extractColumnIndex(m_signalColumnCombo));
    config.insert(QStringLiteral("isContinuous"), m_continuousChk->isChecked());
    config.insert(QStringLiteral("signalType"), m_signalTypeCombo->currentText());
    config.insert(QStringLiteral("signalUnit"), m_signalUnitEdit->text());
    config.insert(QStringLiteral("signalName"), m_signalNameEdit->text());

    const bool rateFromColumn = m_rateHasColumnChk->isChecked();
    config.insert(QStringLiteral("rateFromColumn"), rateFromColumn);
    config.insert(QStringLiteral("rateColumn"), rateFromColumn ? extractColumnIndex(m_rateColumnCombo) : -1);
    config.insert(QStringLiteral("rateUnit"), m_rateUnitCombo->currentText());
    config.insert(QStringLiteral("dynamicRate"), m_dynamicRateSpin->value());

    return config;
}
