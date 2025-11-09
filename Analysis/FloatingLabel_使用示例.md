# FloatingLabel 浮动标签使用示例

## 概述

FloatingLabel 是一个功能强大的浮动标签组件，可以在图表上显示可拖动、可缩放的文本标签。

## 核心特性

### 两种锚定模式

1. **DataAnchored（数据锚定）**：
   - 标签锚定到数据坐标（x, y）
   - 图表缩放/平移时自动跟随数据位置
   - 适用于标注特定数据点

2. **ViewAnchored（视图锚定）**：
   - 标签锚定到视图像素位置（HUD）
   - 图表缩放不影响标签位置
   - 适用于固定提示信息

### 交互功能

- **拖动移动**：鼠标左键拖拽标签
- **缩放**：Ctrl + 鼠标滚轮调整标签大小
- **关闭按钮**：点击右上角红色 "×" 按钮删除标签
- **锁定/解锁**：点击锁定按钮切换固定状态
- **多行文本**：支持 `\n` 换行

## 使用示例

### 示例 1：添加数据锚定标签

在特定数据点上添加标签（标签会跟随图表缩放）：

```cpp
// 假设你已经有一个 ChartView 实例和一个曲线 ID
QString curveId = "curve_001";
QPointF dataPos(350.0, 1.23);  // 数据坐标：温度=350°C, 值=1.23

// 添加标签
FloatingLabel* label = chartView->addFloatingLabel(
    "温度=350°C\n值=1.23 mg",  // 支持多行文本
    dataPos,                   // 数据坐标
    curveId                    // 曲线ID
);

// 可选：进一步自定义
if (label) {
    label->setBackgroundColor(QColor(255, 255, 200, 200));  // 浅黄色背景
    label->setTextColor(Qt::darkBlue);                       // 深蓝色文字
    label->setPadding(12.0);                                 // 增加内边距
}
```

### 示例 2：添加视图锚定标签（HUD）

在视图固定位置添加提示信息（不受图表缩放影响）：

```cpp
// 在图表右上角添加 HUD 标签
QPointF viewPos(60, 30);  // 相对于 plotArea 左上角的偏移（像素）

FloatingLabel* hud = chartView->addFloatingLabelHUD(
    "实验条件\nT=25°C\nP=101.3 kPa",  // 多行文本
    viewPos                             // 视图坐标
);

// 可选：锁定标签防止误操作移动
if (hud) {
    hud->setLocked(true);
}
```

### 示例 3：在算法执行后自动添加标签

在基线校正完成后，自动标注基线点：

```cpp
// 假设基线校正算法返回了两个基线点
QVector<QPointF> baselinePoints = {QPointF(300.0, 2.5), QPointF(500.0, 1.8)};
QString curveId = "baseline_corrected_curve";

// 标注起点
chartView->addFloatingLabel(
    QString("基线起点\nT=%1°C").arg(baselinePoints[0].x()),
    baselinePoints[0],
    curveId
);

// 标注终点
chartView->addFloatingLabel(
    QString("基线终点\nT=%1°C").arg(baselinePoints[1].x()),
    baselinePoints[1],
    curveId
);
```

### 示例 4：响应标签事件

监听标签状态变化：

```cpp
FloatingLabel* label = chartView->addFloatingLabel(...);

// 连接锁定状态改变信号
connect(label, &FloatingLabel::lockStateChanged, [](bool locked) {
    qDebug() << "标签锁定状态:" << (locked ? "已锁定" : "已解锁");
});

// 注意：closeRequested 信号已在 ChartView 中自动处理
// 点击关闭按钮后标签会自动删除
```

### 示例 5：批量管理标签

获取所有标签并批量操作：

```cpp
// 获取所有浮动标签
const QVector<FloatingLabel*>& labels = chartView->floatingLabels();

// 批量设置样式
for (FloatingLabel* label : labels) {
    label->setBackgroundColor(QColor(200, 255, 200, 180));  // 浅绿色
    label->setScaleFactor(1.2);  // 放大 20%
}

// 清空所有标签
chartView->clearFloatingLabels();
```

## API 参考

### ChartView 管理接口

```cpp
// 添加数据锚定标签
FloatingLabel* addFloatingLabel(
    const QString& text,      // 标签文本（支持 \n 换行）
    const QPointF& dataPos,   // 数据坐标 (x, y)
    const QString& curveId    // 曲线ID
);

// 添加视图锚定标签（HUD）
FloatingLabel* addFloatingLabelHUD(
    const QString& text,      // 标签文本
    const QPointF& viewPos    // 视图像素位置（相对于 plotArea）
);

// 移除指定标签
void removeFloatingLabel(FloatingLabel* label);

// 清空所有标签
void clearFloatingLabels();

// 获取所有标签
const QVector<FloatingLabel*>& floatingLabels() const;
```

### FloatingLabel 属性设置

```cpp
// 文本
void setText(const QString& text);
QString text() const;

// 锚定模式
void setMode(Mode mode);  // Mode::DataAnchored 或 Mode::ViewAnchored
Mode mode() const;

// 数据坐标锚点（DataAnchored 模式）
void setAnchorValue(const QPointF& value, QAbstractSeries* series);
QPointF anchorValue() const;

// 样式
void setPadding(qreal padding);                  // 内边距（像素）
void setScaleFactor(qreal factor);               // 缩放因子（0.3 ~ 5.0）
void setOffset(const QPointF& offset);           // 位置偏移
void setBackgroundColor(const QColor& color);    // 背景颜色
void setTextColor(const QColor& color);          // 文本颜色

// 锁定状态
void setLocked(bool locked);  // 锁定后禁止拖动
bool isLocked() const;

// 几何更新（通常自动调用）
void updateGeometry();
```

### FloatingLabel 信号

```cpp
// 关闭请求信号（点击关闭按钮时发出）
void closeRequested();

// 锁定状态改变信号
void lockStateChanged(bool locked);
```

## 交互指南

### 用户操作

1. **移动标签**：鼠标左键拖拽标签主体
2. **缩放标签**：按住 Ctrl 键 + 鼠标滚轮
3. **关闭标签**：点击右上角红色 "×" 按钮
4. **锁定/解锁**：点击锁定按钮切换固定状态

### 视觉反馈

- **选中状态**：边框变蓝色加粗
- **悬停按钮**：按钮高亮显示
- **锁定状态**：右下角显示小锁图标
- **光标变化**：
  - 悬停标签主体：四向箭头（可拖动）
  - 悬停按钮：手型指针（可点击）
  - 锁定状态：普通箭头（不可拖动）

## 高级用法

### 自定义标签样式

创建自定义样式的标签：

```cpp
FloatingLabel* customLabel = chartView->addFloatingLabel(
    "自定义标签",
    QPointF(400.0, 2.0),
    "curve_id"
);

// 自定义样式
customLabel->setBackgroundColor(QColor(100, 150, 255, 200));  // 蓝色半透明
customLabel->setTextColor(Qt::white);                          // 白色文字
customLabel->setPadding(15.0);                                 // 较大内边距
customLabel->setScaleFactor(1.5);                              // 放大 1.5 倍
customLabel->setOffset(QPointF(20, -30));                      // 自定义偏移
```

### 动态更新标签内容

根据数据变化更新标签：

```cpp
// 假设你有一个标签引用
FloatingLabel* dynamicLabel = /* ... */;

// 更新文本
dynamicLabel->setText(QString("实时温度: %1°C").arg(currentTemp));

// 如果数据坐标改变（DataAnchored 模式）
dynamicLabel->setAnchorValue(newDataPos, series);
```

### 与坐标轴变化同步

在 DataAnchored 模式下，标签会自动跟随坐标轴变化。如果需要手动触发更新：

```cpp
// 连接坐标轴范围改变信号
QValueAxis* axisX = /* ... */;
connect(axisX, &QValueAxis::rangeChanged, [label]() {
    label->updateGeometry();
});
```

## 注意事项

1. **坐标系统**：
   - DataAnchored 模式使用数据坐标（如温度、质量）
   - ViewAnchored 模式使用像素坐标（相对于 plotArea）

2. **性能考虑**：
   - 大量标签（> 50 个）可能影响渲染性能
   - 建议使用 `clearFloatingLabels()` 及时清理不需要的标签

3. **内存管理**：
   - `removeFloatingLabel()` 会自动调用 `deleteLater()`
   - `clearCurves()` 会自动清空所有浮动标签

4. **Z 值**：
   - 标签自动设置为 `chart->zValue() + 20`，确保显示在曲线之上

5. **边界约束**：
   - 拖动时标签会自动限制在 plotArea 内

## 集成到现有代码

在 MainController 或其他控制器中添加标签功能：

```cpp
// 假设在 MainController 中
void MainController::onShowPeakInfo(const QString& curveId, const QPointF& peakPos, qreal peakValue)
{
    QString text = QString("峰值\nT=%1°C\n值=%2 mg")
                       .arg(peakPos.x(), 0, 'f', 1)
                       .arg(peakValue, 0, 'f', 3);

    m_chartView->addFloatingLabel(text, peakPos, curveId);
}
```

## 完整示例：峰值标注

假设你有一个峰值检测算法，执行后自动标注所有峰值：

```cpp
// 峰值检测完成后
void MainController::onPeakDetectionCompleted(const QString& curveId, const QVector<QPointF>& peaks)
{
    for (int i = 0; i < peaks.size(); ++i) {
        const QPointF& peak = peaks[i];

        QString labelText = QString("峰 %1\nT=%2°C\n值=%3")
                                .arg(i + 1)
                                .arg(peak.x(), 0, 'f', 1)
                                .arg(peak.y(), 0, 'f', 3);

        FloatingLabel* label = m_chartView->addFloatingLabel(labelText, peak, curveId);

        // 可选：自定义每个峰值标签的样式
        if (label) {
            // 峰值越大，标签越大
            qreal scaleFactor = 1.0 + (peak.y() / maxPeakValue) * 0.5;
            label->setScaleFactor(scaleFactor);

            // 不同峰值使用不同颜色
            QColor color = (i % 2 == 0) ? QColor(255, 200, 200, 200) : QColor(200, 200, 255, 200);
            label->setBackgroundColor(color);
        }
    }
}
```

## 扩展建议

### 未来可能的扩展

1. **尺寸手柄**：在标签右下角添加手柄，支持拖拽调整大小
2. **富文本支持**：使用 `QTextDocument` 支持 HTML 格式文本
3. **动画效果**：添加淡入淡出、弹性动画
4. **序列化**：保存/加载标签到项目文件
5. **连接线**：从标签绘制箭头指向数据点
6. **分组管理**：支持标签分组，批量显示/隐藏

---

**版本**：v1.0.0
**作者**：Claude Code
**日期**：2025-11-08
