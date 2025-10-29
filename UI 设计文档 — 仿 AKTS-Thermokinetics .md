# UI 设计文档 — 仿 AKTS-Thermokinetics 主界面（用于 Qt 开发）

下面给出一个面向实现的 **详细 UI 设计文档**（中文），包含界面布局草图、组件清单、数据模型、关键交互流程、QSS 样式示例、资源清单、类与函数设计以及关键代码片段（Qt/C++ 风格，适配 Qt5/Qt6）。目标：帮你快速把界面骨架搭建起来，并给出功能映射与实现建议，便于后续迭代和代码编写。

------

# 1 总览与目标

目标实现一个桌面应用主界面，功能与截图类似，包含：

- 顶部“Ribbon-like”工具区（Zoom / Cursor / Insert / View / Chart Settings 等按钮与下拉组）
- 左侧树形数据/导入列表（Importations、Kinetic Results 等）
- 中央大图区域（绘图/拟合点/回归直线/图例/坐标轴）
- 右侧可选（可扩展）属性面板或工具窗格（可折叠）
- 底部状态栏（状态、viewer、Ready 等）
- 支持图表交互：缩放、平移、点选、标注、添加图片/注释、导出

设计原则：模块化（Model-View），易扩展、易测试、与绘图库解耦（可替换 QCustomPlot / Qwt / QtCharts）。

------

# 2 高级布局（Wireframe）

```
+--------------------------------------------------------------+
| Ribbon (Tabs: File / Thermokinetics / Math Functions / View)|
|  [Group Buttons: Zoom | Cursor | Insert | View | Chart etc]  |
+--------------------------------------------------------------+
| LeftPane (width ~ 20%) |            CentralPlot (70%)        |
|  - QTreeView / QDockWidget |  Plot area (QWidget + plot lib) |
|  - checkboxes / nested items  |  Axes, Legend, Points, Line   |
|                              |                              |
|                              |                              |
+------------------------------+-------------------------------+
| StatusBar: user/admin | version | viewer | Ready | zoom level |
+--------------------------------------------------------------+
```

建议使用 `QMainWindow` + `QDockWidget` + `QSplitter` 实现动态调整。

------

# 3 组件清单（建议的 Qt 控件 & 说明）

- 主窗体：`QMainWindow`
- 顶部 Ribbon（二选一实现）：
  - 简易实现：`QTabWidget`（隐藏标签页轮廓） + 每页内放 `QToolBar` 与 `QToolButton`、`QAction`。
  - 商业控件：QtitanRibbon（若需真实 Ribbon）。
- 左侧数据树：`QDockWidget`（容器）内 `QTreeView`（Model: `QStandardItemModel` 或自定义 model）
- 中央绘图区域：自定义 `PlotWidget`（内部用 QCustomPlot / Qwt / QtCharts）
- 图表工具栏（图上工具）：`QToolButton` 或 悬浮 `QWidget`（切换缩放、平移、游标）
- 右侧属性面板（可选）：`QDockWidget` + `QWidget`（属性表单：QFormLayout）
- 底部状态栏：`QStatusBar`
- 弹出或对话：`QFileDialog`, `QInputDialog`, `QMessageBox`
- 图例交互：图例项 `QCheckBox` 或 图例点击事件（由绘图库支持）
- 导航/缩放鼠标工具：绘图库提供接口或自行实现鼠标事件

------

# 4 数据模型（Model）

把左侧树、绘图数据和计算结果分离为三个核心模型：

1. **ImportModel**（用于左侧树）
   - 数据结构：`ImportEntry { id, name, sourceFile, channels: [HeatFlow, Temperature...], visible }`
   - 接口：`loadFromFile(path)`, `toggleVisible(id)`, `getChannelData(id, channel)`
2. **KineticModel**
   - 存放计算结果（不同速率下的点、拟合值、E(激活能) 等）
   - 接口：`computeKinetics(params)`, `getPoints(analysisMethod)`
3. **PlotDataModel**
   - 由 ImportModel 或 KineticModel 提供给 PlotWidget 的统一数据表示：`PlotSeries { id, label, x[], y[], color, marker }`

实现建议：使用信号/槽连接模型和视图，例如 `ImportModel::dataChanged()` -> `MainWindow::onImportChanged()` -> `PlotWidget::updateSeries()`。

------

# 5 关键交互流程（用例）

1. 用户在左树勾选导入项 -> 发送 `seriesVisibleChanged(id,visible)` -> `PlotWidget` 显示/隐藏对应曲线/点。
2. 在 Ribbon/工具栏选择“Zoom In” -> `PlotWidget` 进入缩放模式（鼠标拖框）。
3. 选择“Adapt Chart Resolution” -> 调用 `PlotWidget::setResolution(...)`（重绘）。
4. 点击图例中某一条曲线的图标 -> 弹出样式/可见性菜单（或直接切换可见）。
5. 右键图表 -> 弹出 context menu（Export image / Add Note / Create Marker）。
6. 拖动图表中点 -> 若允许编辑，触发 `pointMoved(seriesId, index, newPos)`。
7. 选择“ASTM E698”（分析方法） -> 后端计算触发 `KineticModel` -> 返回拟合参数并在图上绘制拟合线和显示 E(kJ/mol)。

------

# 6 类与函数设计（建议）

```
MainWindow : QMainWindow
  - initRibbon()
  - initDockWidgets()
  - initPlotWidget()
  - connectSignals()

PlotWidget : QWidget
  - addSeries(PlotSeries)
  - removeSeries(id)
  - updateSeries(PlotSeries)
  - setMode(enum {Pan, Zoom, Cursor, Select})
  - exportImage(path)
  - addAnnotation(text, pos)
  - fitLine(seriesIds)
  - mouse handlers...

ImportModel : QObject
  - loadFromFile(path)
  - saveConfig(path)
  - toggleChannel(id, channel, visible)
  - signals: importChanged(), itemToggled(id)

KineticModel : QObject
  - calculateE(method, params)
  - computeActivationEnergy(points) -> returns parameters
  - signals: kineticsComputed(result)

AppResources
  - icons, fonts, translations
```

每个类内写明职责（SRP 单一职责）。

------

# 7 绘图库选择建议

- **QtCharts**（Qt 官方模块，使用方便）

------

# 8 关键信号/槽映射（示例）

- `ImportModel::itemToggled(id, visible)` -> `MainWindow::onImportToggled(...)`
- `MainWindow::toolZoomInTriggered()` -> `PlotWidget::setMode(Zoom)`
- `PlotWidget::pointClicked(seriesId, index)` -> `MainWindow::showPointInfo(...)`
- `KineticModel::kineticsComputed(result)` -> `PlotWidget::drawFitLine(result)`

------

# 9 QSS（样式）示例（片段）

```css
/* 顶部 Ribbon 风格 */
QTabBar::tab {
  padding: 6px 12px;
  border: 0px;
  font-weight: 600;
}
QToolButton {
  border-radius: 4px;
  padding: 6px;
  margin: 2px;
}
QToolButton:hover { background: rgba(0,0,0,0.04); }

/* 左侧树 */
QTreeView {
  background: #fafafa;
  border-right: 1px solid #e0e0e0;
}

/* 状态栏 */
QStatusBar { background: #f5f5f5; }
```

说明：样式可以进一步细化为深色/浅色两套主题。

------

# 10 资源清单

- 图标：zoom.svg, pan.svg, cursor.svg, add_image.svg, save.svg, export.svg, settings.svg（放 `:/icons/`）
- 字体：主 UI 字体（系统默认）+ 可选 iconfont (.ttf) 用于大图标
- 配置文件：`config.json`（保存上次打开文件、窗口大小、图表设置）
- 测试数据：若干 CSV 用于开发时快速绘图

------

# 11 文件/目录结构建议

```
/project
  /src
    main.cpp
    MainWindow.h/.cpp
    PlotWidget.h/.cpp
    ImportModel.h/.cpp
    KineticModel.h/.cpp
    resources.qrc
  /ui
    mainwindow.ui (若使用 Qt Designer)
  /assets
    icons/*.svg
    fonts/*.ttf
  /data
    sample1.csv sample2.csv
  CMakeLists.txt  (或 .pro)
```

------

# 12 关键代码片段（快速启动骨架，Qt C++，Qt5/6 均可）

下面给出 `MainWindow` 的布局骨架（不包含绘图库细节，演示如何放置控件）：

```cpp
// MainWindow.h
#pragma once
#include <QMainWindow>
class QTreeView;
class PlotWidget;
class QDockWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow(QWidget* parent=nullptr);
private:
  void initRibbon();
  void initDockWidgets();
  void initCentral();
  QTreeView* leftTree_;
  PlotWidget* plotWidget_;
  QDockWidget* leftDock_;
};
// MainWindow.cpp
#include "MainWindow.h"
#include <QTreeView>
#include <QDockWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include "PlotWidget.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  resize(1200, 800);
  initRibbon();
  initCentral();
  initDockWidgets();
  statusBar()->showMessage("Ready");
}

void MainWindow::initRibbon() {
  // 简易 Ribbon：用 QTabWidget 每页放 QToolBar
  QTabWidget* tabs = new QTabWidget;
  QWidget* fileTab = new QWidget;
  QToolBar* tb = new QToolBar;
  tb->addAction(QIcon(":/icons/zoom.svg"), "Zoom In");
  tb->addAction(QIcon(":/icons/pan.svg"), "Pan");
  QVBoxLayout* l = new QVBoxLayout(fileTab);
  l->setContentsMargins(0,0,0,0);
  l->addWidget(tb);
  tabs->addTab(fileTab, "View");
  setMenuWidget(tabs); // 放在主窗口上方
}

void MainWindow::initCentral() {
  plotWidget_ = new PlotWidget;
  setCentralWidget(plotWidget_);
}

void MainWindow::initDockWidgets() {
  leftTree_ = new QTreeView;
  leftDock_ = new QDockWidget("Importations", this);
  leftDock_->setWidget(leftTree_);
  addDockWidget(Qt::LeftDockWidgetArea, leftDock_);
}
```

`PlotWidget` 内部你可以集成 QCustomPlot，并提供 `addSeries()`、`setMode()` 等方法。由于绘图库的代码较长，这里先给骨架，后面可以继续补充具体绘图库实现。

------

# 13 绘图与拟合（实现思路）

- 绘图库负责渲染点、线、误差条、拟合直线和图例。
- 拟合（回归）可以在 `KineticModel` 中完成（使用简单线性回归或更复杂最小二乘），计算出斜率、截距、R²，并通过信号传回 `PlotWidget`。
- 在图上显示 `E (kJ/mol) = 45.723` 这样的文本，使用绘图库的注释或在 `PlotWidget` 的 overlay 上绘制。

线性回归实现（简短）：

```cpp
struct FitResult { double slope, intercept, r2; };
FitResult linearFit(const QVector<double>& x, const QVector<double>& y) {
  // 实现最小二乘，返回 slope/intercept
}
```

------

# 14 可交互要点 & 实现建议（细节）

- **图例点击/开关**：图例项关联 `seriesId`，点击时发信号切换可见性。
- **拖动/编辑点**：若需要修改点值，开启编辑模式，捕获鼠标按下/移动/放开，变更数据并触发重绘。
- **缩放/平移**：绘图库一般有内建方法，或者实现视口变换（transform）。
- **保存/导出**：`PlotWidget::exportImage(path)` -> 使用 `QPixmap::grabWidget()`（Qt5）或 `QWidget::render()` 导出 PNG。
- **快捷键**：`Ctrl+Z` 撤销（与图表操作配合），`Ctrl+S` 保存。
- **多速率（多系列）展示**：左树中同一个导入项下列出多个速率，勾选后同时绘制不同颜色点和拟合线。

------

# 15 可扩展/后续功能（roadmap）

- 添加“分析管线”面板：选择 ASTM E698 / Ozawa / Kissinger 等方法并显示参数表格。
- 图表导出为高分辨矢量格式（SVG/PDF）。
- 保存/加载项目（包含导入文件、显示设置与拟合结果）。
- 支持多窗口/子图布局（QMdiArea 或 Tab）。

------

# 16 开发建议与注意事项（传统稳妥风格）

1. **先实现 MVP（最小可用产品）**：先完成左树 + 中心绘图 + 工具栏的最小交互（勾选显示、缩放、点显示）。再逐步加入拟合与导出。
2. **模块化**：绘图与数据计算分离，绘图只负责渲染，不做复杂数值计算。
3. **单元测试**：对拟合函数和数据转换写单元测试（例如带噪声数据的拟合精度）。
4. **配置文件**：保存用户的显示偏好，避免每次打开都重置。
5. **资源管理**：图标、字体用 `resources.qrc` 管理，便于打包。
6. **支持高 DPI**：确保图在高分辨率屏幕上渲染清晰（使用 devicePixelRatio 等）。

------

