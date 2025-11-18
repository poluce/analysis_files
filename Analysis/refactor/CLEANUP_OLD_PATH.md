# 清理旧算法链路（遗留代码）

## 背景

所有常规算法已迁移到元数据驱动架构（方案B）：
- ✅ moving_average（移动平均）
- ✅ baseline_correction（基线校正）
- ✅ differentiation（微分）
- ✅ integration（积分）
- ✅ temperature_extrapolation（温度外推）

仅保留峰面积等工具类使用旧链路（点选功能）。

## 可以删除的代码

### 1. 参数对话框相关（已被 GenericAlgorithmDialog 替代）
- `AlgorithmCoordinator::requestParameterDialog` 信号
- `AlgorithmCoordinator::handleParameterSubmission` 方法
- `MainController::onRequestParameterDialog` 槽函数
- `MainController` 中对 `requestParameterDialog` 的连接

### 2. handleAlgorithmTriggered 中的参数分支
- `AlgorithmInteraction::ParameterDialog` 分支
- `AlgorithmInteraction::ParameterThenPoint` 分支

## 需要保留的代码（为峰面积等工具类）

### 1. 点选功能
- `AlgorithmCoordinator::handleAlgorithmTriggered` 方法（简化版）
- `AlgorithmCoordinator::handlePointSelectionResult` 方法
- `AlgorithmCoordinator::requestPointSelection` 信号
- `m_pending`（PendingRequest）用于存储点选状态

### 2. 执行功能
- `AlgorithmCoordinator::executeAlgorithm` 方法
- `AlgorithmCoordinator::resetState` 方法

## 清理后的架构

```
用户触发算法
     ↓
MainController::onAlgorithmRequested
     ↓
判断：registry.has(algorithmName)?
     ↓
✅ YES                          ❌ NO
     ↓                               ↓
runByName()                   handleAlgorithmTriggered()
(元数据驱动)                   (遗留路径，仅峰面积等工具类)
     ↓                               ↓
GenericAlgorithmDialog        requestPointSelection
                                     ↓
                              handlePointSelectionResult
```

## 实施步骤

1. 删除 `requestParameterDialog` 信号定义
2. 删除 `handleParameterSubmission` 方法定义和实现
3. 简化 `handleAlgorithmTriggered`，只保留 `None` 和 `PointSelection` 分支
4. 删除 `MainController` 中的相关连接和槽函数
5. 添加注释说明遗留代码用途
6. 提交并测试
