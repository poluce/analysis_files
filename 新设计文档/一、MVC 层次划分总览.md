情景回顾

流程如下：

1. 用户点击一个算法（例如“基线校正算法”）。
2. 用户在曲线图中选择某条曲线（不同曲线可能有不同 Y 轴）。
3. 提示：“请在所选曲线上点击两个点”。
4. 用户点击第一个点 → 曲线上显示一个散点标记。
5. 用户点击第二个点 → 再显示一个散点标记。
6. 控制层收集两点坐标与曲线信息，发给后台算法计算。
7. 算法处理完成后，控制层通知视图刷新结果曲线（绘制新线条）。

------

## 一、MVC 层次划分总览

| 层                         | 职责                                                     | 参与对象示例                                                 |
| -------------------------- | -------------------------------------------------------- | ------------------------------------------------------------ |
| **Model（模型层）**        | 算法与数据：保存曲线数据、执行计算逻辑                   | `ThermalCurve`, `AlgorithmManager`, `BaselineCorrectionAlgorithm` |
| **View（视图层）**         | 界面交互：显示曲线、捕获鼠标点击、绘制散点/提示信息      | `ChartView`, `PlotWidget`, `CurveItem`                       |
| **Controller（控制器层）** | 负责协调流程：响应用户操作、收集参数、调用算法、更新视图 | `CurveViewController`, `MainController`, `AlgorithmController` |

------

## 二、分层职责详解

### 1. View 层（视图）

**主要职责：显示 + 交互捕获**

- 显示所有曲线；
- 响应用户点击事件；
- 将“点击坐标 + 当前曲线 ID”发出信号；
- 绘制散点标记；
- 绘制提示框（例如 QToolTip 或 QLabel 提示）。

#### 示例职责

```cpp
// ChartView.cpp
void ChartView::mousePressEvent(QMouseEvent* event)
{
    QPointF chartPos = mapToPlotCoordinates(event->pos());
    emit pointClicked(currentCurveId, chartPos);  // 发信号给 Controller
}
```

#### 不做的事：

- 不执行算法；
- 不负责判断点选数量；
- 不保存业务数据（只负责画）。

------

### 2. Controller 层（控制器）

**主要职责：流程与逻辑调度**

控制器是“调度员”，负责：

- 接收“算法按钮点击事件”；
- 记录当前选择的算法；
- 等待用户点击两点；
- 收集两个点坐标；
- 把曲线与参数交给模型（算法）；
- 监听模型计算完成信号；
- 通知视图刷新结果曲线。

####  示例逻辑

```cpp
// AlgorithmController.cpp
void AlgorithmController::onAlgorithmSelected(const QString& algoName)
{
    currentAlgorithm = algoName;
    view->showHint("请在曲线上点击两个点");
}

void AlgorithmController::onCurvePointClicked(const QString& curveId, const QPointF& pos)
{
    clickedPoints.push_back(pos);
    view->drawSelectionPoint(curveId, pos);

    if (clickedPoints.size() == 2) {
        emit twoPointsSelected(curveId, clickedPoints);
        clickedPoints.clear();
    }
}

void AlgorithmController::onTwoPointsSelected(const QString& curveId, const QVector<QPointF>& pts)
{
    model->runAlgorithm(currentAlgorithm, curveId, pts);
}
```

------

### 3. Model 层（模型）

**主要职责：执行算法和数据更新**

负责：

- 存储所有曲线；
- 提供算法执行接口；
- 计算新曲线数据；
- 发出“计算完成”信号。

#### 示例逻辑

```cpp
// AlgorithmManager.cpp
void AlgorithmManager::runAlgorithm(const QString& name,
                                    const QString& curveId,
                                    const QVector<QPointF>& pts)
{
    if (name == "BaselineCorrection") {
        BaselineCorrectionAlgorithm algo;
        auto newCurve = algo.execute(curveId, pts);
        emit algorithmFinished(curveId, newCurve);
    }
}
```

------

### 4.Controller 再次响应模型结果

控制器监听 `algorithmFinished()`：

```cpp
void AlgorithmController::onAlgorithmFinished(const QString& curveId, const ThermalCurve& newCurve)
{
    view->addResultCurve(curveId, newCurve);
    view->showHint("算法计算完成");
}
```

------

## 三、信号流向图（逻辑流）

```
[用户点击算法按钮]
        ↓
    View::emit algorithmSelected(algo)
        ↓
 Controller::onAlgorithmSelected() → 提示点击两个点
        ↓
[用户在曲线上点击两点]
        ↓
 View::emit pointClicked(curveId, pos)
        ↓
 Controller::onCurvePointClicked() 收集两点 → 通知 Model
        ↓
 Model::runAlgorithm() 计算新曲线
        ↓
 Model::emit algorithmFinished(newCurve)
        ↓
 Controller::onAlgorithmFinished() 通知 View 更新显示
        ↓
 View::addResultCurve() 绘制结果曲线
```

------

## 四、每层代码职责总结表

| 层             | 职责                                 | Qt 中的信号/类举例                                           |
| -------------- | ------------------------------------ | ------------------------------------------------------------ |
| **View**       | 显示曲线、捕获鼠标、绘制点           | `ChartView`, `QMouseEvent`, `emit pointClicked()`            |
| **Controller** | 收集用户行为、调度算法、协调界面更新 | `AlgorithmController`, `onCurvePointClicked()`               |
| **Model**      | 计算算法、返回结果曲线               | `AlgorithmManager`, `BaselineCorrectionAlgorithm`, `emit algorithmFinished()` |

------

## 五、可扩展

- **算法新增时** → 只改 `Model` 层；
- **UI 改版时** → 只改 `View`；
- **交互流程改变** → 只改 `Controller`。

三层解耦，维护成本低。

## 1.视图层 ：只有发信号和执行的功能 。

## 2.控制层 ：接收来自视图层和数据模型的信号，另外控制层可主动从其他模块的数据模型层获取数据，进行业务数据的收集和整理，经过处理转换成具体的业务数据和业务信息，调用视图层执行。

## 3.模型层： 对上（控制层）发送信号，监听其他数据模型或者控制层的的信号，不接收来自本模块视图层的信号。模型与模型之间只存在事件监听的关系，没有相互调用的路径。对外提供本模型的所有信息。模型层面分为数据存储管理，还有功能模块管理。并不是每个模型都有视图层，对于偏功能管理的没有视图层和控制层，只有模型层面。简单视图可与别的视图公用一个控制层。

## 注意：若是只有一个视图层，那么这个视图要提供获取本层全部数据的能力，和设置本层的能力。

