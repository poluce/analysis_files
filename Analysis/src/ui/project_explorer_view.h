#ifndef PROJECTEXPLORERVIEW_H
#define PROJECTEXPLORERVIEW_H

#include <QModelIndex>
#include <QWidget>

class QTreeView;
class QAbstractItemModel;

/**
 * @brief ProjectExplorerView 是项目浏览器视图组件
 *
 * 以树形结构展示项目中的曲线及其衍生关系：
 * - 项目节点（文件名）
 *   - 原始曲线
 *     - 微分曲线
 *     - 积分曲线
 *     - 滤波曲线
 *       - ...（可嵌套多层）
 *
 * 职责：
 * - 显示树形视图
 * - 响应用户交互（单击、双击、右键菜单）
 * - 发射信号通知控制器
 */
class ProjectExplorerView : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit ProjectExplorerView(QWidget* parent = nullptr);

    /**
     * @brief 设置数据模型
     * @param model 树形模型（通常由 ProjectTreeManager 提供）
     */
    void setModel(QAbstractItemModel* model);

    /**
     * @brief 获取内部的 QTreeView 组件
     * @return QTreeView 指针
     */
    QTreeView* treeView() const;

signals:
    /**
     * @brief 当曲线节点被单击时发射
     * @param index 被单击的节点索引
     */
    void curveItemClicked(const QModelIndex& index);

    /**
     * @brief 当删除操作被请求时发射（右键菜单）
     */
    void deleteActionClicked();

private:
    QTreeView* m_treeView;
};

#endif // PROJECTEXPLORERVIEW_H
