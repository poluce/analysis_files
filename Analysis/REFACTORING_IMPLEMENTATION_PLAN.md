# ChartView 重构实施计划（一步到位）

## 实施策略

由于项目处于设计阶段，采用**一步到位**策略，直接完成所有 Phase 2-5 的迁移工作。

## 实施步骤

### Step 1: 完整实现 ThermalChart ✅ (即将完成)
迁移 chart_view.cpp 中的所有数据管理和叠加物功能到 thermal_chart.cpp：

**数据管理 (~400 行)**
- addCurve() - 添加曲线到图表
- updateCurve() - 更新曲线数据
- removeCurve() - 删除曲线
- clearCurves() - 清空所有曲线
- setCurveVisible() - 设置曲线可见性（含级联子曲线）
- highlightCurve() - 高亮选中曲线
- setXAxisMode() - 横轴模式切换（温度/时间）
- rescaleAxes() - 自动缩放所有坐标轴

**叠加物管理 (~500 行)**
- FloatingLabel 管理（addFloatingLabel, removeFloatingLabel, clearFloatingLabels）
- CurveMarkers 管理（addCurveMarkers, removeCurveMarkers, clearAllMarkers）
- MassLossTool 管理（addMassLossTool, removeMassLossTool, clearAllMassLossTools）
- AnnotationLine 管理（addAnnotationLine, removeAnnotation, clearAllAnnotations）

### Step 2: 完整实现 ThermalChartView ⏳
迁移 chart_view.cpp 中的所有交互逻辑到 thermal_chart_view.cpp：

**事件处理 (~400 行)**
- mousePressEvent() - 左键点击（曲线选择/值点击）
- mouseMoveEvent() - 鼠标移动（十字线更新/悬停提示）
- wheelEvent() - Ctrl+滚轮缩放
- contextMenuEvent() - 右键菜单
- eventFilter() - 右键拖动平移

**交互辅助 (~200 行)**
- handleCurveSelectionClick() - 曲线选择逻辑
- handleValueClick() - 值点击逻辑
- findSeriesNearPoint() - 碰撞检测（点到线段距离计算）
- handleRightDrag() - 右键拖动计算

### Step 3: 简化 ChartView 为薄容器 ⏳
重写 chart_view.h/cpp：

**chart_view.h (~150 行)**
- 持有 ThermalChart* 和 ThermalChartView*
- 转发所有公共接口
- 保留算法交互状态机（InteractionState, ActiveAlgorithmInfo, m_selectedPoints）

**chart_view.cpp (~200 行)**
- 构造函数：组合 ThermalChart 和 ThermalChartView
- 转发方法：调用 m_chart 或 m_chartView 的对应方法
- 状态机实现：处理算法交互流程（startAlgorithmInteraction, onValueClicked等）

## 代码迁移清单

### ThermalChart 需要实现的方法（Phase 2-3）

#### 曲线管理
- [x] addCurve()
- [x] updateCurve()
- [x] removeCurve()
- [x] clearCurves()
- [x] setCurveVisible() - 含级联逻辑
- [x] highlightCurve()

#### 坐标轴管理
- [x] rescaleAxes()
- [x] setXAxisMode() - 含叠加物同步
- [x] ensureYAxisForCurve()
- [x] rescaleAxis()
- [x] resetAxesToDefault()

#### 浮动标签
- [x] addFloatingLabel()
- [x] addFloatingLabelHUD()
- [x] removeFloatingLabel()
- [x] clearFloatingLabels()

#### 标注点
- [x] addCurveMarkers()
- [x] removeCurveMarkers()
- [x] clearAllMarkers()

#### 测量工具
- [x] addMassLossTool()
- [x] removeMassLossTool()
- [x] clearAllMassLossTools()

#### 注释线
- [x] addAnnotationLine()
- [x] removeAnnotation()
- [x] clearAllAnnotations()

### ThermalChartView 需要实现的方法（Phase 4）

#### 事件处理
- [ ] mousePressEvent() - 完整实现
- [ ] mouseMoveEvent() - 完整实现
- [ ] mouseReleaseEvent()
- [ ] wheelEvent() - Ctrl+滚轮缩放
- [ ] contextMenuEvent() - 右键菜单
- [ ] eventFilter() - 右键拖动

#### 交互辅助
- [ ] handleCurveSelectionClick()
- [ ] handleValueClick()
- [ ] findSeriesNearPoint() - 碰撞检测
- [ ] handleRightDrag()

### ChartView 需要重写的内容（Phase 5）

#### 构造函数
- [ ] 创建 ThermalChart
- [ ] 创建 ThermalChartView
- [ ] 连接信号

#### 转发接口
- [ ] addCurve() → m_chart->addCurve()
- [ ] removeCurve() → m_chart->removeCurve()
- [ ] toggleXAxisMode() → m_chart->setXAxisMode()
- [ ] ... (所有公共接口)

#### 算法状态机
- [ ] startAlgorithmInteraction()
- [ ] cancelAlgorithmInteraction()
- [ ] onValueClicked() - 组装选点逻辑
- [ ] onCurveHit() - 转发信号
- [ ] completePointSelection()

## 测试计划

### 编译测试
- [ ] thermal_chart.cpp 编译通过
- [ ] thermal_chart_view.cpp 编译通过
- [ ] chart_view.cpp 重写后编译通过
- [ ] 整个项目编译通过

### 功能测试
- [ ] 导入曲线显示正常
- [ ] 曲线增删改查正常
- [ ] 横轴切换（温度/时间）正常
- [ ] 曲线可见性控制（含级联）正常
- [ ] 曲线高亮正常
- [ ] 右键拖动平移正常
- [ ] Ctrl+滚轮缩放正常
- [ ] 算法交互选点正常
- [ ] 浮动标签显示正常
- [ ] 测量工具功能正常
- [ ] 标注点显示正常
- [ ] 十字线显示正常

## 预期成果

### 代码行数
| 文件 | 优化前 | 优化后 | 变化 |
|------|--------|--------|------|
| chart_view.h | ~400 | ~150 | ↓ 62.5% |
| chart_view.cpp | ~1415 | ~200 | ↓ 85.9% |
| thermal_chart.h | - | ~200 | +200 |
| thermal_chart.cpp | - | ~1200 | +1200 |
| thermal_chart_view.h | - | ~110 | +110 |
| thermal_chart_view.cpp | - | ~800 | +800 |
| **总计** | ~1815 | ~2660 | +845 |

### 优势
- ✅ 代码职责清晰（数据 vs 交互 vs 容器）
- ✅ 单向依赖无循环
- ✅ 可单独测试每个模块
- ✅ 扩展无需修改核心代码
- ✅ 团队并行开发无冲突

---

*最后更新: 2025-11-09*
*策略: 一步到位完成所有迁移*
