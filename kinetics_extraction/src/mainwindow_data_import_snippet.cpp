// ============================================================================
// 从 mainwindow.cpp 中提取的数据导入相关代码片段
// ============================================================================

// 1. 打开数据导入窗口 (on_toolButtonOpen_clicked)
// 位置: mainwindow.cpp 第2097-2107行
void MainWindow::on_toolButtonOpen_clicked()
{
    if (!form) {
        form = new Form;
        connect(form, &Form::dataReady, this, &MainWindow::onDataReady);
    }
    form->show();
}

// 2. 接收Form窗口传递的数据 (onDataReady)
// 位置: mainwindow.cpp 第2032-2096行
void MainWindow::onDataReady(const QVariantMap& data)
{
    int index = dataList.size() + 1; // 获取当前数据集的索引

    QVariantMap newData;
    temperatureKey = QString("温度%1").arg(index);
    timeKey = QString("时间%1").arg(index);
    customColumnKey = QString("%1%2").arg(form->lineEdit_5()).arg(index);
    velocityKey = QString("速率%1").arg(index);

    // 确保键名正确并直接存储数据
    QList<double> temperatureData;
    QList<double> timeData;
    QList<double> customData;
    QList<double> velocityData;

    // 将 QVariantList 转换为 QList<double>
    foreach (const QVariant& v, data.value("温度").toList()) {
        temperatureData << v.toDouble();
    }
    foreach (const QVariant& v, data.value("时间").toList()) {
        timeData << v.toDouble();
    }
    foreach (const QVariant& v, data.value(form->lineEdit_5()).toList()) {
        customData << v.toDouble();
    }
    if (data.contains("速率")) {
        foreach (const QVariant& v, data.value("速率").toList()) {
            velocityData << v.toDouble();
        }
    }

    // 存储转换后的数据
    newData[temperatureKey] = QVariant::fromValue(temperatureData);
    newData[timeKey] = QVariant::fromValue(timeData);
    newData[customColumnKey] = QVariant::fromValue(customData);
    if (!velocityData.isEmpty()) {
        newData[velocityKey] = QVariant::fromValue(velocityData);
    }

    this->dataList.append(newData); // 将新数据添加到 dataList 中

    QString textFromLineEdit = form->textEdit();

    // 更新树状控件显示（在UI中显示导入的数据列表）
    QTreeWidgetItem* textEditItem = new QTreeWidgetItem(ui->treeWidget);
    textEditItem->setText(0, textFromLineEdit);
    textEditItem->setToolTip(0, textFromLineEdit);
    textEditItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTreeWidgetItem* temperatureItem = new QTreeWidgetItem(textEditItem);
    temperatureItem->setText(0, temperatureKey);
    temperatureItem->setFlags(temperatureItem->flags() | Qt::ItemIsUserCheckable);
    temperatureItem->setCheckState(0, Qt::Unchecked);

    QTreeWidgetItem* timeItem = new QTreeWidgetItem(textEditItem);
    timeItem->setText(0, timeKey);
    timeItem->setFlags(timeItem->flags() | Qt::ItemIsUserCheckable);
    timeItem->setCheckState(0, Qt::Unchecked);

    QTreeWidgetItem* customColumnItem = new QTreeWidgetItem(textEditItem);
    customColumnItem->setText(0, customColumnKey);
    customColumnItem->setFlags(customColumnItem->flags() | Qt::ItemIsUserCheckable);
    customColumnItem->setCheckState(0, Qt::Unchecked);
}

// ============================================================================
// MainWindow 中需要的成员变量声明 (来自 mainwindow.h)
// ============================================================================

// 在 mainwindow.h 的 private: 部分：
/*
    Form* form;                              // 数据导入窗口指针
    QList<QVariantMap> dataList;             // 存储所有导入的数据
    QString temperatureKey;                  // 温度数据的键名
    QString timeKey;                         // 时间数据的键名
    QString customColumnKey;                 // 自定义列的键名
    QString velocityKey;                     // 速率数据的键名
*/

// ============================================================================
// 信号槽连接说明
// ============================================================================

/*
Form窗口 --> MainWindow

信号: Form::dataReady(const QVariantMap &data)
槽函数: MainWindow::onDataReady(const QVariantMap& data)

连接代码:
    connect(form, &Form::dataReady, this, &MainWindow::onDataReady);

数据流向:
    1. 用户在Form窗口选择文件并配置列映射
    2. Form解析文件，构建QVariantMap数据
    3. Form发射dataReady信号
    4. MainWindow的onDataReady槽函数接收数据
    5. 数据被添加到dataList中
    6. UI树状控件更新，显示导入的数据
*/
