#include "DataImportWidget.h"

DataImportWidget::DataImportWidget(QWidget* parent)
    : QWidget(parent)
{
    // 顶部文件选择行
    QLabel* fileLabel = new QLabel(tr("数据文件"), this);
    m_filePathEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(tr("..."), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &DataImportWidget::onBrowseFile);

    QHBoxLayout* fileLayout = new QHBoxLayout;
    fileLayout->addWidget(fileLabel);
    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(m_browseBtn);

    // 表头和预览区域
    QLabel* headerLabel = new QLabel(tr("表头"), this);
    m_headerPreview = new QTextEdit(this);
    m_headerPreview->setFixedHeight(80);
    m_headerPreview->setReadOnly(true);

    m_previewArea = new QPlainTextEdit(this);
    m_previewArea->setPlaceholderText(tr("文件预览..."));
    m_previewArea->setMinimumHeight(160);

    QVBoxLayout* middleLayout = new QVBoxLayout;
    middleLayout->addWidget(headerLabel);
    middleLayout->addWidget(m_headerPreview);
    middleLayout->addWidget(m_previewArea);

    // 质量（样品初始质量）
    QLabel* massLabel = new QLabel(tr("样品初始质量"), this);
    m_initialMassSpin = new QDoubleSpinBox(this);
    m_initialMassSpin->setDecimals(5);
    m_initialMassSpin->setRange(0.0, 1e6);
    m_initialMassSpin->setValue(0.0);
    m_massUnitLabel = new QLabel(tr("g"), this);
    QLabel* dataTypeLabel = new QLabel(tr("数据类型"), this);
    m_dataTypeCombo = new QComboBox(this);
    m_dataTypeCombo->addItem("TGA", QStringLiteral("TGA"));
    m_dataTypeCombo->addItem("ARC", QStringLiteral("ARC"));
    m_dataTypeCombo->addItem("DSC", QStringLiteral("DSC"));

    QHBoxLayout* massLayout = new QHBoxLayout;
    massLayout->addWidget(massLabel);
    massLayout->addWidget(m_initialMassSpin);
    massLayout->addWidget(m_massUnitLabel);
    massLayout->addSpacing(12);
    massLayout->addWidget(dataTypeLabel);
    massLayout->addWidget(m_dataTypeCombo);
    massLayout->addStretch();

    // 四个参数分组：温度、时间、信号、速率
    QWidget* tempGroup = createTemperatureGroup();
    QWidget* timeGroup = createTimeGroup();
    QWidget* signalGroup = createSignalGroup();
    QWidget* rateGroup = createRateGroup();

    QHBoxLayout* paramsLayout = new QHBoxLayout;
    paramsLayout->addWidget(tempGroup);
    paramsLayout->addWidget(timeGroup);
    paramsLayout->addWidget(signalGroup);
    paramsLayout->addWidget(rateGroup);

    // 底部按钮
    m_importBtn = new QPushButton(tr("导入"), this);
    m_closeBtn = new QPushButton(tr("关闭"), this);
    connect(m_importBtn, &QPushButton::clicked, this, &DataImportWidget::onImportClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &DataImportWidget::onCloseClicked);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(m_importBtn);
    btnLayout->addWidget(m_closeBtn);

    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(fileLayout);
    mainLayout->addLayout(middleLayout);
    mainLayout->addWidget(new QFrame(this));
    mainLayout->addLayout(massLayout);
    mainLayout->addLayout(paramsLayout);
    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
    setWindowTitle(tr("数据导入"));
}

QWidget* DataImportWidget::createTemperatureGroup()
{
    QGroupBox* box = new QGroupBox(tr("温度"), this);

    m_tempHasColumnChk = new QCheckBox(tr("列"), box);
    m_tempColumnCombo = new QComboBox(box);
    m_tempUnitCombo = new QComboBox(box);
    m_tempUnitCombo->addItems({ "°C", "K" });
    m_noTempColumnChk = new QCheckBox(tr("如果没有温度的列"), box);
    m_tempSetSpin = new QDoubleSpinBox(box);
    m_tempSetSpin->setRange(-273.15, 10000);
    m_tempSetSpin->setSuffix(tr(" °C"));

    QGridLayout* g = new QGridLayout(box);
    g->addWidget(m_tempHasColumnChk, 0, 0);
    g->addWidget(m_tempColumnCombo, 0, 1);
    g->addWidget(new QLabel(tr("单位"), box), 1, 0);
    g->addWidget(m_tempUnitCombo, 1, 1);
    g->addWidget(m_noTempColumnChk, 2, 0, 1, 2);
    g->addWidget(new QLabel(tr("温度设置为"), box), 3, 0);
    g->addWidget(m_tempSetSpin, 3, 1);

    box->setLayout(g);
    return box;
}

QWidget* DataImportWidget::createTimeGroup()
{
    QGroupBox* box = new QGroupBox(tr("时间"), this);

    m_timeColumnCombo = new QComboBox(box);
    m_timeUnitCombo = new QComboBox(box);
    m_timeUnitCombo->addItems({ "s", "ms", "min", "h" });
    m_filterPointsSpin = new QSpinBox(box);
    m_filterPointsSpin->setRange(0, 10000);
    m_filterPointsSpin->setValue(0);

    QFormLayout* f = new QFormLayout(box);
    f->addRow(tr("列"), m_timeColumnCombo);
    f->addRow(tr("单位"), m_timeUnitCombo);
    f->addRow(tr("Filter"), m_filterPointsSpin);

    box->setLayout(f);
    return box;
}

QWidget* DataImportWidget::createSignalGroup()
{
    QGroupBox* box = new QGroupBox(tr("信号"), this);

    m_signalColumnCombo = new QComboBox(box);
    m_continuousChk = new QCheckBox(tr("连续数据收集"), box);
    m_noncontinuousChk = new QCheckBox(tr("不连续的数据收集"), box);
    m_signalTypeCombo = new QComboBox(box);
    m_signalTypeCombo->addItems({ tr("质量"), tr("DTA") });
    m_signalUnitEdit = new QLineEdit(box);
    m_signalUnitEdit->setFixedWidth(40);
    m_signalNameEdit = new QLineEdit(box);

    QGridLayout* g = new QGridLayout(box);
    g->addWidget(new QLabel(tr("列"), box), 0, 0);
    g->addWidget(m_signalColumnCombo, 0, 1);
    g->addWidget(m_continuousChk, 1, 0, 1, 2);
    g->addWidget(m_noncontinuousChk, 2, 0, 1, 2);
    g->addWidget(new QLabel(tr("信号类型"), box), 3, 0);
    g->addWidget(m_signalTypeCombo, 3, 1);
    g->addWidget(new QLabel(tr("单位"), box), 4, 0);
    g->addWidget(m_signalUnitEdit, 4, 1);
    g->addWidget(new QLabel(tr("名字"), box), 5, 0);
    g->addWidget(m_signalNameEdit, 5, 1);

    box->setLayout(g);
    return box;
}

QWidget* DataImportWidget::createRateGroup()
{
    QGroupBox* box = new QGroupBox(tr("速率"), this);

    m_rateHasColumnChk = new QCheckBox(tr("列"), box);
    m_rateColumnCombo = new QComboBox(box);
    m_rateUnitCombo = new QComboBox(box);
    m_rateUnitCombo->addItems({ "°C/min", "°C/s", "%/min" });
    m_dynamicRateSpin = new QSpinBox(box);
    m_dynamicRateSpin->setRange(0, 10000);

    QFormLayout* f = new QFormLayout(box);
    f->addRow(m_rateHasColumnChk, m_rateColumnCombo);
    f->addRow(tr("单位"), m_rateUnitCombo);
    f->addRow(tr("动力学速率"), m_dynamicRateSpin);

    box->setLayout(f);
    return box;
}

void DataImportWidget::onBrowseFile()
{
    QString file = QFileDialog::getOpenFileName(this, tr("选择数据文件"), QString(), tr("所有文件 (*)"));

    if(file.isEmpty())
    {
        return;
    }
    m_filePathEdit->setText(file);

    // 发射信号，请求预览
    emit previewRequested(file);
}

void DataImportWidget::setPreviewData(const FilePreviewData& previewData)
{
    m_headerPreview->setPlainText(previewData.header);
    m_previewArea->setPlainText(previewData.previewContent);

    // 清空并填充列下拉框
    m_tempColumnCombo->clear();
    m_timeColumnCombo->clear();
    m_signalColumnCombo->clear();
    m_rateColumnCombo->clear();

    m_tempColumnCombo->addItems(previewData.columns);
    m_timeColumnCombo->addItems(previewData.columns);
    m_signalColumnCombo->addItems(previewData.columns);
    m_rateColumnCombo->addItems(previewData.columns);
}

void DataImportWidget::onImportClicked()
{
    emit importRequested();
}

void DataImportWidget::onCloseClicked()
{
    this->close();
}

QVariantMap DataImportWidget::getImportConfig() const
{
    QVariantMap config;

    config.insert("filePath", m_filePathEdit->text());
    config.insert("initialMass", m_initialMassSpin->value());
    QString curveType = m_dataTypeCombo->currentData().toString();
    if (curveType.isEmpty()) {
        curveType = m_dataTypeCombo->currentText();
    }
    config.insert("curveType", curveType);

    // 温度配置
    config.insert("tempFromColumn", m_tempHasColumnChk->isChecked());
    config.insert("tempColumn", m_tempColumnCombo->currentText().split(':').first().toInt() - 1);
    config.insert("tempUnit", m_tempUnitCombo->currentText());
    config.insert("tempIsFixed", m_noTempColumnChk->isChecked());
    config.insert("tempFixedValue", m_tempSetSpin->value());

    // 时间配置
    config.insert("timeColumn", m_timeColumnCombo->currentText().split(':').first().toInt() - 1);
    config.insert("timeUnit", m_timeUnitCombo->currentText());
    config.insert("filterPoints", m_filterPointsSpin->value());

    // 信号配置
    config.insert("signalColumn", m_signalColumnCombo->currentText().split(':').first().toInt() - 1);
    config.insert("isContinuous", m_continuousChk->isChecked());
    config.insert("signalType", m_signalTypeCombo->currentText());
    config.insert("signalUnit", m_signalUnitEdit->text());
    config.insert("signalName", m_signalNameEdit->text());

    // 速率配置
    config.insert("rateFromColumn", m_rateHasColumnChk->isChecked());
    config.insert("rateColumn", m_rateColumnCombo->currentText().split(':').first().toInt() - 1);
    config.insert("rateUnit", m_rateUnitCombo->currentText());
    config.insert("dynamicRate", m_dynamicRateSpin->value());

    return config;
}
