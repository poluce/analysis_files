#ifndef CURVETREEMODEL_H
#define CURVETREEMODEL_H

#include <QAbstractItemModel>
#include <QMap>
#include <QString>
#include <QSet>

class CurveManager;
class ThermalCurve;

/**
 * @brief TreeItem 表示树形结构中的一个节点
 * 可以是实际曲线节点（有curveId）或虚拟项目节点（有projectName）
 */
class TreeItem
{
public:
    explicit TreeItem(const QString& curveId, TreeItem* parent = nullptr);
    explicit TreeItem(const QString& projectName, bool isProject, TreeItem* parent = nullptr);
    ~TreeItem();

    void appendChild(TreeItem* child);
    void removeChild(TreeItem* child);
    TreeItem* child(int row);
    int childCount() const;
    int row() const;
    TreeItem* parentItem();
    QString curveId() const;
    QString projectName() const;
    bool isProjectNode() const;

private:
    QList<TreeItem*> m_childItems;
    TreeItem* m_parentItem;
    QString m_curveId;
    QString m_projectName;
    bool m_isProjectNode;
};

/**
 * @brief CurveTreeModel 提供曲线的树形视图模型
 *
 * 支持父子关系展示（原始曲线和算法生成的曲线）
 * 支持 checkbox 用于选择/取消选择曲线
 */
class CurveTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit CurveTreeModel(CurveManager* curveManager, QObject* parent = nullptr);
    ~CurveTreeModel() override;

    // QAbstractItemModel 接口
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    // 获取曲线ID
    QString getCurveId(const QModelIndex& index) const;

    // 获取选中的曲线ID列表
    QStringList getCheckedCurveIds() const;

    // 设置曲线的选中状态
    void setCurveChecked(const QString& curveId, bool checked);

public slots:
    void onCurveAdded(const QString& curveId);
    void onCurveRemoved(const QString& curveId);
    void onCurvesCleared();
    void refresh();

signals:
    void curveCheckStateChanged(const QString& curveId, bool checked);

private:
    void buildTree();
    TreeItem* findItem(const QString& curveId, TreeItem* parent = nullptr) const;

    CurveManager* m_curveManager;
    TreeItem* m_rootItem;
    QSet<QString> m_checkedCurves;  // 存储选中的曲线ID
};

#endif // CURVETREEMODEL_H
