#ifndef PROJECTTREEMANAGER_H
#define PROJECTTREEMANAGER_H

#include <QObject>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QMap>
#include <QString>
#include <QStringList>

class CurveManager;
class ThermalCurve;

/**
 * @brief ProjectTreeManager 管理曲线的树形视图
 * 
 * 使用 QStandardItemModel 简化实现,提供:
 * - 按项目分组的树形结构
 * - 支持父子曲线关系(多层嵌套)
 * - Checkbox 选择/取消选择曲线
 * - 自动响应 CurveManager 的变化
 */
class ProjectTreeManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectTreeManager(CurveManager* curveManager, QObject* parent = nullptr);
    ~ProjectTreeManager() = default;

    /**
     * @brief 获取内部的 QStandardItemModel
     * @return 可直接用于 QTreeView::setModel()
     */
    QStandardItemModel* model() { return m_model; }

    /**
     * @brief 获取所有被勾选的曲线ID列表
     */
    QStringList getCheckedCurveIds() const;

    /**
     * @brief 设置指定曲线的勾选状态
     * @param curveId 曲线ID
     * @param checked 是否勾选
     */
    void setCurveChecked(const QString& curveId, bool checked);

    /**
     * @brief 获取指定索引对应的曲线ID
     * @param index 模型索引
     * @return 曲线ID,如果是项目节点则返回空字符串
     */
    QString getCurveId(const QModelIndex& index) const;

public slots:
    /**
     * @brief 响应 CurveManager 的曲线添加信号
     */
    void onCurveAdded(const QString& curveId);

    /**
     * @brief 响应 CurveManager 的曲线移除信号
     */
    void onCurveRemoved(const QString& curveId);

    /**
     * @brief 响应 CurveManager 的清空信号
     */
    void onCurvesCleared();

    /**
     * @brief 完全重建树形结构
     */
    void refresh();

signals:
    /**
     * @brief 曲线勾选状态改变时发射
     * @param curveId 曲线ID
     * @param checked 新的勾选状态
     */
    void curveCheckStateChanged(const QString& curveId, bool checked);

private slots:
    /**
     * @brief 监听 QStandardItem 变化(主要是 checkbox 状态)
     */
    void onItemChanged(QStandardItem* item);

private:
    /**
     * @brief 构建完整的树形结构
     */
    void buildTree();

    /**
     * @brief 查找或创建项目节点
     * @param projectName 项目名称
     * @return 项目节点
     */
    QStandardItem* findOrCreateProjectItem(const QString& projectName);

    /**
     * @brief 递归查找指定曲线ID的 QStandardItem
     * @param curveId 曲线ID
     * @param parent 从哪个节点开始查找,nullptr 表示从根节点
     * @return 找到的 item,未找到返回 nullptr
     */
    QStandardItem* findCurveItem(const QString& curveId, QStandardItem* parent = nullptr) const;

    /**
     * @brief 递归收集所有被勾选的曲线ID
     * @param parent 从哪个节点开始收集
     * @param result 输出参数,收集的曲线ID列表
     */
    void collectCheckedItems(QStandardItem* parent, QStringList& result) const;

    /**
     * @brief 创建一个曲线节点
     * @param curve 曲线对象
     * @param checked 是否默认勾选
     * @return 创建的 QStandardItem
     */
    QStandardItem* createCurveItem(const ThermalCurve& curve, bool checked = false);

    CurveManager* m_curveManager;      // 曲线管理器
    QStandardItemModel* m_model;        // Qt 标准模型
};

#endif // PROJECTTREEMANAGER_H
