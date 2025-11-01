#include "CurveTreeModel.h"
#include "application/curve/CurveManager.h"
#include "domain/model/ThermalCurve.h"
#include <QDebug>

// ========== TreeItem 实现 ==========

TreeItem::TreeItem(const QString& curveId, TreeItem* parent)
    : m_parentItem(parent), m_curveId(curveId), m_isProjectNode(false)
{
}

TreeItem::TreeItem(const QString& projectName, bool isProject, TreeItem* parent)
    : m_parentItem(parent), m_projectName(projectName), m_isProjectNode(isProject)
{
}

TreeItem::~TreeItem()
{
    qDeleteAll(m_childItems);
}

void TreeItem::appendChild(TreeItem* child)
{
    m_childItems.append(child);
}

void TreeItem::removeChild(TreeItem* child)
{
    m_childItems.removeOne(child);
    delete child;
}

TreeItem* TreeItem::child(int row)
{
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

int TreeItem::childCount() const
{
    return m_childItems.count();
}

int TreeItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<TreeItem*>(this));
    return 0;
}

TreeItem* TreeItem::parentItem()
{
    return m_parentItem;
}

QString TreeItem::curveId() const
{
    return m_curveId;
}

QString TreeItem::projectName() const
{
    return m_projectName;
}

bool TreeItem::isProjectNode() const
{
    return m_isProjectNode;
}

// ========== CurveTreeModel 实现 ==========

CurveTreeModel::CurveTreeModel(CurveManager* curveManager, QObject* parent)
    : QAbstractItemModel(parent)
    , m_curveManager(curveManager)
    , m_rootItem(new TreeItem(""))
{
    Q_ASSERT(m_curveManager);

    // 连接信号
    connect(m_curveManager, &CurveManager::curveAdded,
            this, &CurveTreeModel::onCurveAdded);
    connect(m_curveManager, &CurveManager::curveRemoved,
            this, &CurveTreeModel::onCurveRemoved);
    connect(m_curveManager, &CurveManager::curvesCleared,
            this, &CurveTreeModel::onCurvesCleared);

    buildTree();
}

CurveTreeModel::~CurveTreeModel()
{
    delete m_rootItem;
}

QVariant CurveTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());

    // 如果是项目节点（虚拟节点）
    if (item->isProjectNode()) {
        if (role == Qt::DisplayRole) {
            return item->projectName();
        }
        // 项目节点没有checkbox
        return QVariant();
    }

    // 如果是曲线节点
    ThermalCurve* curve = m_curveManager->getCurve(item->curveId());
    if (!curve)
        return QVariant();

    if (role == Qt::DisplayRole) {
        return curve->name();
    }
    else if (role == Qt::CheckStateRole && index.column() == 0) {
        return m_checkedCurves.contains(item->curveId()) ? Qt::Checked : Qt::Unchecked;
    }

    return QVariant();
}

bool CurveTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    if (role == Qt::CheckStateRole) {
        TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
        bool checked = (value.toInt() == Qt::Checked);

        if (checked) {
            m_checkedCurves.insert(item->curveId());
        } else {
            m_checkedCurves.remove(item->curveId());
        }

        emit dataChanged(index, index);
        emit curveCheckStateChanged(item->curveId(), checked);
        return true;
    }

    return false;
}

Qt::ItemFlags CurveTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());

    // 项目节点只支持Enabled和Selectable，没有checkbox
    if (item->isProjectNode()) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    // 曲线节点支持checkbox
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

QVariant CurveTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0) {
        return QString("Curves");
    }
    return QVariant();
}

QModelIndex CurveTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeItem* parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem* childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex CurveTreeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem* childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem* parentItem = childItem->parentItem();

    if (parentItem == m_rootItem || parentItem == nullptr)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int CurveTreeModel::rowCount(const QModelIndex& parent) const
{
    TreeItem* parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int CurveTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QString CurveTreeModel::getCurveId(const QModelIndex& index) const
{
    if (!index.isValid())
        return QString();

    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    return item->curveId();
}

QStringList CurveTreeModel::getCheckedCurveIds() const
{
    return m_checkedCurves.values();
}

void CurveTreeModel::setCurveChecked(const QString& curveId, bool checked)
{
    TreeItem* item = findItem(curveId, m_rootItem);
    if (!item)
        return;

    if (checked) {
        m_checkedCurves.insert(curveId);
    } else {
        m_checkedCurves.remove(curveId);
    }

    // 找到对应的 index 并发射 dataChanged
    QModelIndex idx = createIndex(item->row(), 0, item);
    emit dataChanged(idx, idx);
    emit curveCheckStateChanged(curveId, checked);
}

void CurveTreeModel::onCurveAdded(const QString& curveId)
{
    // 新添加的曲线默认勾选
    m_checkedCurves.insert(curveId);
    refresh();
    // 发射信号通知曲线已勾选
    emit curveCheckStateChanged(curveId, true);
}

void CurveTreeModel::onCurveRemoved(const QString& curveId)
{
    Q_UNUSED(curveId);
    m_checkedCurves.remove(curveId);
    refresh();
}

void CurveTreeModel::onCurvesCleared()
{
    m_checkedCurves.clear();
    refresh();
}

void CurveTreeModel::refresh()
{
    beginResetModel();
    buildTree();
    endResetModel();
}

void CurveTreeModel::buildTree()
{
    // 清理旧树
    delete m_rootItem;
    m_rootItem = new TreeItem("");

    // 获取所有曲线
    const QMap<QString, ThermalCurve>& curves = m_curveManager->getAllCurves();

    // 创建所有曲线ID到TreeItem的映射
    QMap<QString, TreeItem*> itemMap;
    // 创建项目名称到项目节点的映射
    QMap<QString, TreeItem*> projectNodeMap;

    // 第一步：为每个项目创建项目节点（虚拟根节点）
    QSet<QString> projectNames;
    for (auto it = curves.begin(); it != curves.end(); ++it) {
        const ThermalCurve& curve = it.value();
        if (!curve.projectName().isEmpty()) {
            projectNames.insert(curve.projectName());
        }
    }

    for (const QString& projectName : projectNames) {
        TreeItem* projectNode = new TreeItem(projectName, true, m_rootItem);
        m_rootItem->appendChild(projectNode);
        projectNodeMap[projectName] = projectNode;
    }

    // 第二步：添加所有顶层曲线（没有父曲线的）到对应的项目节点下
    for (auto it = curves.begin(); it != curves.end(); ++it) {
        const ThermalCurve& curve = it.value();
        if (curve.parentId().isEmpty()) {
            TreeItem* projectNode = projectNodeMap.value(curve.projectName(), nullptr);
            if (projectNode) {
                TreeItem* curveItem = new TreeItem(curve.id(), projectNode);
                projectNode->appendChild(curveItem);
                itemMap[curve.id()] = curveItem;
            } else {
                qWarning() << "找不到项目节点" << curve.projectName() << "，跳过曲线" << curve.id();
            }
        }
    }

    // 第三步：使用迭代方式添加所有子节点，支持多层嵌套
    QSet<QString> processedCurves;
    for (auto it = curves.begin(); it != curves.end(); ++it) {
        if (it.value().parentId().isEmpty()) {
            processedCurves.insert(it.value().id());
        }
    }

    bool hasUnprocessed = true;
    int maxIterations = curves.size();
    int iteration = 0;

    while (hasUnprocessed && iteration < maxIterations) {
        hasUnprocessed = false;
        iteration++;

        for (auto it = curves.begin(); it != curves.end(); ++it) {
            const ThermalCurve& curve = it.value();

            // 跳过已处理的曲线
            if (processedCurves.contains(curve.id())) {
                continue;
            }

            // 跳过没有父曲线的（已在第二步处理）
            if (curve.parentId().isEmpty()) {
                continue;
            }

            // 检查父节点是否已经被创建
            TreeItem* parentItem = itemMap.value(curve.parentId(), nullptr);
            if (parentItem) {
                // 找到了父节点，创建子节点
                TreeItem* childItem = new TreeItem(curve.id(), parentItem);
                parentItem->appendChild(childItem);
                itemMap[curve.id()] = childItem;
                processedCurves.insert(curve.id());
            } else {
                // 父节点还未创建，本轮跳过，下一轮再处理
                hasUnprocessed = true;
            }
        }
    }

    // 第四步：处理孤儿节点（父节点不存在的曲线）
    for (auto it = curves.begin(); it != curves.end(); ++it) {
        const ThermalCurve& curve = it.value();
        if (!processedCurves.contains(curve.id())) {
            qWarning() << "找不到父曲线" << curve.parentId() << "，将曲线" << curve.id() << "添加到项目根节点";
            TreeItem* projectNode = projectNodeMap.value(curve.projectName(), nullptr);
            if (projectNode) {
                TreeItem* item = new TreeItem(curve.id(), projectNode);
                projectNode->appendChild(item);
                itemMap[curve.id()] = item;
            }
        }
    }
}

TreeItem* CurveTreeModel::findItem(const QString& curveId, TreeItem* parent) const
{
    if (!parent)
        parent = m_rootItem;

    if (parent->curveId() == curveId)
        return parent;

    for (int i = 0; i < parent->childCount(); ++i) {
        TreeItem* found = findItem(curveId, parent->child(i));
        if (found)
            return found;
    }

    return nullptr;
}
