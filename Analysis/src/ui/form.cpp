#include "form.h"
#include "ui_form.h"  // 使用 Qt Designer 生成的 UI 头文件
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QTextStream>

Form::Form(QWidget* parent)
    : QWidget(parent), ui(new Ui::Form)
{
    ui->setupUi(this);
    setupConnections(); // 调用连接函数
}

Form::~Form()
{
    delete ui;
}

// 访问器方法:返回 QString
QString Form::lineEdit_5() const
{
    return ui->lineEdit_5->text();
}

QString Form::textEdit() const
{
    return ui->textEdit->toPlainText();
}

QString Form::comboBox_2() const
{
    return ui->comboBox_2->currentText();
}

QString Form::comboBox_6() const
{
    return ui->comboBox_6->currentText();
}

QString Form::comboBox_4() const
{
    return ui->comboBox_4->currentText();
}

QString Form::comboBox_8() const
{
    return ui->comboBox_8->currentText();
}

QString Form::lineEdit_4() const
{
    return ui->lineEdit_4->text();
}

double Form::getDoubleSpinBoxValue() const
{
    return ui->doubleSpinBox->value();
}

double Form::getDoubleSpinBoxValue_2() const
{
    return ui->doubleSpinBox_2->value();
}

void Form::on_pushButton_2_clicked()
{
    this->close(); // 关闭窗口
}

void Form::on_pushButton_3_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("选择文件"), "./",
        tr("Text Files (*.txt);;Comma-Separated Values (*.csv);;All Files (*)"));

    if (fileName.isEmpty())
        return;

    // 显示文件名
    ui->textEdit->setText(fileName);

    QFile file(fileName);

    // 优化: 添加文件大小检查
    const qint64 maxFileSize = 100 * 1024 * 1024; // 100MB
    if (file.size() > maxFileSize) {
        QMessageBox::warning(this, tr("文件过大"),
                            tr("文件大小超过 100MB,可能导致性能问题。"));
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::information(this, tr("无法打开文件"), file.errorString());
        return;
    }

    // 分隔方式:csv 用逗号,其它用空白
    const bool isCsv = fileName.endsWith(".csv", Qt::CaseInsensitive);
    auto splitLine = [&](const QString& s) -> QStringList {
        if (isCsv)
            return s.split(',', Qt::KeepEmptyParts);
        return s.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    };

    QStringList headerLines; // 表头行(字母/中文开头)
    QStringList dataLines;   // 数字数据行
    QTextStream in(&file);

    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QString t = line.trimmed();
        if (t.isEmpty())
            continue;

        QChar c = t.at(0);
        if (c.isLetter() || (c.unicode() >= 0x4e00 && c.unicode() <= 0x9fff)) {
            headerLines.append(t);
        } else {
            dataLines.append(t);
        }
    }
    file.close();

    ui->textBrowser->setText(headerLines.join("\n"));

    if (dataLines.isEmpty())
        return;

    // 用第一条数据行确定列数
    const QStringList firstDataCols = splitLine(dataLines.at(0));
    const int colCount = firstDataCols.size();
    const int rowCount = dataLines.size(); // 只放数据行

    ui->tableWidget->clear();
    ui->tableWidget->setColumnCount(colCount);
    ui->tableWidget->setRowCount(rowCount);

    // ===== 设置"序号+名字"的表头 =====
    QStringList headerCols;
    if (!headerLines.isEmpty()) {
        headerCols = splitLine(headerLines.last()); // 取最后一条表头行作为列名来源
    }
    QStringList headerLabels;
    headerLabels.reserve(colCount);
    for (int j = 0; j < colCount; ++j) {
        QString name = (j < headerCols.size() && !headerCols.at(j).trimmed().isEmpty())
            ? headerCols.at(j).trimmed()
            : tr("列%1").arg(j + 1);
        headerLabels << tr("%1:%2").arg(j + 1).arg(name); // "序号:名字"
    }

    // 优化: 批量更新时禁用UI刷新
    ui->tableWidget->setUpdatesEnabled(false);
    ui->tableWidget->setHorizontalHeaderLabels(headerLabels);

    // ===== 写入数据,从第0行开始 =====
    for (int i = 0; i < dataLines.size(); ++i) {
        const QStringList cols = splitLine(dataLines.at(i));
        for (int j = 0; j < colCount; ++j) {
            const QString v = (j < cols.size()) ? cols.at(j).trimmed() : QString();
            ui->tableWidget->setItem(i, j, new QTableWidgetItem(v));
        }
    }

    // 优化: 重新启用UI刷新
    ui->tableWidget->setUpdatesEnabled(true);

    // 组合框初始化(按列数)
    ui->comboBox->clear();
    ui->comboBox_3->clear();
    ui->comboBox_5->clear();
    ui->comboBox_8->clear();
    for (int i = 1; i <= colCount; ++i) {
        const QString s = QString::number(i);
        ui->comboBox->addItem(s);
        ui->comboBox_3->addItem(s);
        ui->comboBox_5->addItem(s);
        ui->comboBox_8->addItem(s);
    }
}

void Form::on_pushButton_clicked()
{
    if (ui->checkBox->isChecked() && ui->checkBox_3->isChecked()) {
        // 获取 QComboBox 中选择的列号
        int column1 = ui->comboBox->currentText().toInt() - 1;   // 转换为从0开始的列号
        int column2 = ui->comboBox_3->currentText().toInt() - 1; // 转换为从0开始的列号
        int column3 = ui->comboBox_5->currentText().toInt() - 1; // 转换为从0开始的列号
        int column4 = ui->comboBox_8->currentText().toInt() - 1; // 转换为从0开始的列号
        QVariantMap columnsData;                                 // 用于存储列名和对应的数据

        QList<QVariant> temperatureData;
        QList<QVariant> timeData;
        QList<QVariant> customData;
        QList<QVariant> velocityData;

        // 从 QTableWidget 中提取对应列的数据
        for (int i = 0; i < ui->tableWidget->rowCount(); ++i) {
            if (ui->tableWidget->item(i, column1)) {
                double tempValue = ui->tableWidget->item(i, column1)->text().toDouble();
                temperatureData << tempValue;
            } else {
                qDebug() << "No data in column1 at row" << i;
            }

            if (ui->tableWidget->item(i, column2)) {
                double timeValue = ui->tableWidget->item(i, column2)->text().toDouble();
                timeData << timeValue * 0.1;
            } else {
                qDebug() << "No data in column2 at row" << i;
            }

            if (ui->tableWidget->item(i, column3)) {
                double customValue = ui->tableWidget->item(i, column3)->text().toDouble();
                customData << customValue;
            } else {
                qDebug() << "No data in column3 at row" << i;
            }

            if (ui->checkBox_5->isChecked() && ui->tableWidget->item(i, column4)) {
                double velocityValue = ui->tableWidget->item(i, column4)->text().toDouble();
                velocityData << velocityValue;
            }
        }

        // 检查 lineEdit_5 是否有文本
        QString customColumnName = ui->lineEdit_5->text();
        if (customColumnName.isEmpty()) {
            QMessageBox::warning(this, tr("警告"), tr("信号列名字不能为空。"));
            return;
        }
        QString customColumnName1 = ui->lineEdit_4->text();
        if (customColumnName1.isEmpty()) {
            QMessageBox::warning(this, tr("警告"), tr("信号列单位不能为空。"));
            return;
        }

        // 确保键名正确
        columnsData["温度"] = QVariant(temperatureData);
        columnsData["时间"] = QVariant(timeData);
        columnsData[customColumnName] = QVariant(customData);

        if (!velocityData.isEmpty()) {
            columnsData["速率"] = QVariant(velocityData);
        }

        // 发射信号,将数据传递出去
        emit dataReady(columnsData);
        this->close(); // 关闭窗口
    } else {
        if (!ui->checkBox->isChecked()) {
            QMessageBox::warning(this, tr("警告"), tr("请选择温度列!"));
        }
        if (!ui->checkBox_3->isChecked()) {
            QMessageBox::warning(this, tr("警告"), tr("请选择信号列是否连续!"));
        }
        qDebug() << "Checkboxes are not checked.";
    }
}

void Form::setupConnections()
{
    connect(ui->checkBox_3, &QCheckBox::stateChanged, this, &Form::onCheckBox3Changed);
    connect(ui->checkBox_4, &QCheckBox::stateChanged, this, &Form::onCheckBox4Changed);
    connect(ui->checkBox, &QCheckBox::stateChanged, this, &Form::onCheckBoxChanged);
    connect(ui->checkBox_2, &QCheckBox::stateChanged, this, &Form::onCheckBox2Changed);

    // 当 comboBox_6 选为"质量"时,自动把 lineEdit_4 填入百分号
    connect(ui->comboBox_6, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (text == QStringLiteral("质量")) {
            ui->lineEdit_4->setText(QStringLiteral("%"));
        }
    });

    // 初始化时同步一次(如果默认就是"质量",直接填入"%")
    if (ui->comboBox_6->currentText() == QStringLiteral("质量")) {
        ui->lineEdit_4->setText(QStringLiteral("%"));
    }
}

void Form::onCheckBox3Changed(int state)
{
    if (state == Qt::Checked && ui->checkBox_4->isChecked()) {
        ui->checkBox_4->setChecked(false); // 取消 checkBox_4 的勾选
        QMessageBox::warning(this, tr("警告"), tr("不能同时选择!"));
    }
}

void Form::onCheckBox4Changed(int state)
{
    if (state == Qt::Checked && ui->checkBox_3->isChecked()) {
        ui->checkBox_3->setChecked(false); // 取消 checkBox_3 的勾选
        QMessageBox::warning(this, tr("警告"), tr("不能同时选择!"));
    }
}

void Form::onCheckBoxChanged(int state)
{
    if (state == Qt::Checked && ui->checkBox_2->isChecked()) {
        ui->checkBox_2->setChecked(false);
        QMessageBox::warning(this, tr("警告"), tr("不能同时选择!"));
    }
}

void Form::onCheckBox2Changed(int state)
{
    if (state == Qt::Checked && ui->checkBox->isChecked()) {
        ui->checkBox->setChecked(false);
        QMessageBox::warning(this, tr("警告"), tr("不能同时选择!"));
    }
}
