#!/bin/bash
#
# ChartView 函数删除步骤 - 可执行脚本
# 
# 使用说明：
#   bash ChartView_删除步骤_可执行脚本.sh [step]
#
# 支持的步骤：
#   verify     - 验证函数未被调用
#   analyze    - 分析删除影响
#   delete     - 执行删除（需确认）
#   all        - 执行所有步骤
#

set -e

PROJECT_ROOT="/home/user/analysis_files/Analysis"
CHART_VIEW_H="${PROJECT_ROOT}/src/ui/chart_view.h"

echo "╔════════════════════════════════════════════════════════════╗"
echo "║      ChartView 函数删除 - 可执行脚本                          ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# 验证步骤：检查函数未被调用
verify_step() {
    echo "Step 1: 验证 selectedPoints() 和 selectedPointsCurveId() 未被调用"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    echo ""
    echo "搜索 selectedPoints() 调用..."
    if grep -r "\.selectedPoints()" "${PROJECT_ROOT}/src" --include="*.cpp" --include="*.h"; then
        echo "⚠️  发现调用！请勿删除"
        return 1
    else
        echo "✅ 未发现调用（除定义外）"
    fi
    
    echo ""
    echo "搜索 selectedPointsCurveId() 调用..."
    if grep -r "\.selectedPointsCurveId()" "${PROJECT_ROOT}/src" --include="*.cpp" --include="*.h"; then
        echo "⚠️  发现调用！请勿删除"
        return 1
    else
        echo "✅ 未发现调用（除定义外）"
    fi
    
    echo ""
    echo "✅ 验证通过：两个函数都未被调用"
    return 0
}

# 分析步骤：显示将要删除的行
analyze_step() {
    echo ""
    echo "Step 2: 分析将要删除的代码"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    
    echo "文件：${CHART_VIEW_H}"
    echo ""
    
    echo "行 121 - selectedPoints():"
    sed -n '121p' "${CHART_VIEW_H}"
    echo ""
    
    echo "行 126 - selectedPointsCurveId():"
    sed -n '126p' "${CHART_VIEW_H}"
    echo ""
    
    echo "共删除 2 行代码"
}

# 删除步骤：执行实际删除
delete_step() {
    echo ""
    echo "Step 3: 执行删除"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    
    read -p "确定要删除这 2 个函数吗？ (y/N): " confirm
    if [[ "${confirm}" != "y" && "${confirm}" != "Y" ]]; then
        echo "❌ 取消删除"
        return 1
    fi
    
    echo "备份原始文件..."
    cp "${CHART_VIEW_H}" "${CHART_VIEW_H}.bak"
    echo "✅ 备份到 ${CHART_VIEW_H}.bak"
    echo ""
    
    echo "删除第 121 和 126 行..."
    # 使用 sed 删除第 121 和 126 行
    sed -i '121d; 125d' "${CHART_VIEW_H}"
    echo "✅ 删除完成"
    echo ""
    
    echo "验证删除结果..."
    echo "第 121 行现在是："
    sed -n '120,122p' "${CHART_VIEW_H}"
    
    echo ""
    echo "✅ 删除步骤完成"
}

# 全部步骤
all_steps() {
    verify_step && analyze_step && delete_step
}

# 主逻辑
case "${1:-all}" in
    verify)
        verify_step
        ;;
    analyze)
        analyze_step
        ;;
    delete)
        delete_step
        ;;
    all)
        all_steps
        ;;
    *)
        echo "未知选项：$1"
        echo ""
        echo "用法：$0 [step]"
        echo ""
        echo "可用步骤："
        echo "  verify     - 验证函数未被调用"
        echo "  analyze    - 分析删除影响"
        echo "  delete     - 执行删除（需确认）"
        echo "  all        - 执行所有步骤（默认）"
        exit 1
        ;;
esac

echo ""
