#ifndef CURVEVIEWCONTROLLER_H
#define CURVEVIEWCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QPointF>
#include <functional>

// 前置声明
class CurveManager;
class ChartView;
class ProjectTreeManager;
class ProjectExplorerView;

/**
 * @brief CurveViewController 管理曲线视图的展示逻辑
 *
 * 职责：
 * - 协调 ChartView、ProjectExplorerView、ProjectTreeManager 的更新
 * - 封装点拾取交互流程（使用回调函数）
 * - 管理曲线可见性、选择、高亮状态
 * - 响应 CurveManager 的数据变化信号
 * - 不包含业务逻辑，只负责视图协调
 */
class CurveViewController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param curveManager 曲线管理器（数据源）
     * @param plotWidget 图表显示组件
     * @param treeManager 项目树管理器
     * @param projectExplorer 项目浏览器视图
     * @param parent 父对象
     */
    explicit CurveViewController(CurveManager* curveManager,
                                 ChartView* plotWidget,
                                 ProjectTreeManager* treeManager,
                                 ProjectExplorerView* projectExplorer,
                                 QObject* parent = nullptr);

    // --- 点拾取接口 ---
    /**
     * @brief 请求点拾取（使用回调函数）
     * @param numPoints 需要拾取的点数
     * @param callback 拾取完成后的回调函数
     */
    using PointPickingCallback = std::function<void(const QString& curveId, const QVector<QPointF>& points)>;
    void requestPointPicking(int numPoints, PointPickingCallback callback);

    /**
     * @brief 取消当前的点拾取操作
     */
    void cancelPointPicking();

    // --- 视图管理接口 ---
    /**
     * @brief 设置曲线可见性
     * @param curveId 曲线ID
     * @param visible 是否可见
     */
    void setCurveVisible(const QString& curveId, bool visible);

    /**
     * @brief 高亮指定曲线
     * @param curveId 曲线ID
     */
    void highlightCurve(const QString& curveId);

    /**
     * @brief 更新所有曲线的显示
     */
    void updateAllCurves();

private slots:
    // --- 响应 CurveManager 信号 ---
    void onCurveAdded(const QString& curveId);
    void onCurveRemoved(const QString& curveId);
    void onCurveDataChanged(const QString& curveId);
    void onActiveCurveChanged(const QString& curveId);

    // --- 响应 ChartView 信号 ---
    void onCurveSelected(const QString& curveId);

    // --- 响应 ProjectTreeManager 信号 ---
    void onCurveCheckStateChanged(const QString& curveId, bool checked);

private:
    CurveManager* m_curveManager;
    ChartView* m_plotWidget;
    ProjectTreeManager* m_treeManager;
    ProjectExplorerView* m_projectExplorer;
};

#endif // CURVEVIEWCONTROLLER_H
