#include "project_tree_manager.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include <QDebug>

ProjectTreeManager::ProjectTreeManager(CurveManager* curveManager, QObject* parent)
    : QObject(parent)
    , m_curveManager(curveManager)
    , m_model(new QStandardItemModel(this))
{
    Q_ASSERT(m_curveManager);
    qDebug() << "构造:   ProjectTreeManager";

    // 设置表头
    m_model->setHorizontalHeaderLabels({ "Curves" });

    // 连接 CurveManager 信号
    connect(m_curveManager, &CurveManager::curveAdded, this, &ProjectTreeManager::onCurveAdded);
    connect(m_curveManager, &CurveManager::curveRemoved, this, &ProjectTreeManager::onCurveRemoved);
    connect(m_curveManager, &CurveManager::curvesCleared, this, &ProjectTreeManager::onCurvesCleared);

    // 连接模型的 itemChanged 信号,监听 checkbox 状态变化
    connect(m_model, &QStandardItemModel::itemChanged, this, &ProjectTreeManager::onItemChanged);

    // 初始化树形结构
    buildTree();
}

QStringList ProjectTreeManager::getCheckedCurveIds() const
{
    QStringList result;

    // 从根节点开始递归收集所有勾选的曲线
    for (int i = 0; i < m_model->rowCount(); ++i) {
        collectCheckedItems(m_model->item(i), result);
    }

    return result;
}

void ProjectTreeManager::setCurveChecked(const QString& curveId, bool checked)
{
    QStandardItem* item = findCurveItem(curveId);
    if (!item) {
        qWarning() << "找不到曲线:" << curveId;
        return;
    }

    // 使用 QSignalBlocker 自动阻塞和恢复信号（RAII 模式）
    {
        QSignalBlocker blocker(m_model);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }

    // 手动发射信号
    emit curveCheckStateChanged(curveId, checked);
}

QString ProjectTreeManager::getCurveId(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QString();
    }

    QStandardItem* item = m_model->itemFromIndex(index);
    if (!item) {
        return QString();
    }

    // 从 UserRole 中获取存储的 curveId
    return item->data(Qt::UserRole).toString();
}

void ProjectTreeManager::onCurveAdded(const QString& curveId)
{
    ThermalCurve* curve = m_curveManager->getCurve(curveId);
    if (!curve) {
        qWarning() << "无法获取曲线:" << curveId;
        return;
    }

    // 强绑定曲线不在树中显示（如基线曲线）
    if (curve->isStronglyBound()) {
        qDebug() << "跳过强绑定曲线:" << curve->name() << "(id:" << curveId << ")";
        return;
    }

    // 查找或创建项目节点
    QStandardItem* projectItem = findOrCreateProjectItem(curve->projectName());

    // 创建曲线节点(新添加的曲线默认勾选)
    QStandardItem* curveItem = createCurveItem(*curve, true);

    // 如果有父曲线,添加到父曲线下;否则添加到项目节点下
    if (!curve->parentId().isEmpty()) {
        QStandardItem* parentItem = findCurveItem(curve->parentId());
        if (parentItem) {
            parentItem->appendRow(curveItem);
        } else {
            qWarning() << "找不到父曲线" << curve->parentId() << ",将曲线" << curveId << "添加到项目根节点";
            projectItem->appendRow(curveItem);
        }
    } else {
        projectItem->appendRow(curveItem);
    }

    // 发射勾选状态变化信号
    emit curveCheckStateChanged(curveId, true);
}

void ProjectTreeManager::onCurveRemoved(const QString& curveId)
{
    QStandardItem* item = findCurveItem(curveId);
    if (!item) {
        qWarning() << "找不到要删除的曲线:" << curveId;
        return;
    }

    // 获取父节点
    QStandardItem* parent = item->parent();
    if (!parent) {
        parent = m_model->invisibleRootItem();
    }

    // 从父节点中移除该行
    parent->removeRow(item->row());
}

void ProjectTreeManager::onCurvesCleared()
{
    // 清空整个模型
    m_model->clear();
    m_model->setHorizontalHeaderLabels({ "Curves" });
}

void ProjectTreeManager::refresh() { buildTree(); }

void ProjectTreeManager::onCurveItemDoubleClicked(const QModelIndex& index)
{
    // 从索引中提取曲线 ID
    QString curveId = getCurveId(index);

    // 如果是曲线节点（不是项目节点），发出信号
    if (!curveId.isEmpty()) {
        qDebug() << "ProjectTreeManager: 曲线项被双击 -" << curveId;
        emit curveItemDoubleClicked(curveId);
    }
}

void ProjectTreeManager::onItemChanged(QStandardItem* item)
{
    // 只处理曲线节点的勾选状态变化
    if (!item || !item->isCheckable()) {
        return;
    }

    QString curveId = item->data(Qt::UserRole).toString();
    if (curveId.isEmpty()) {
        return;
    }

    bool checked = (item->checkState() == Qt::Checked);
    emit curveCheckStateChanged(curveId, checked);
}

void ProjectTreeManager::buildTree()
{
    // 临时断开信号,避免构建过程中频繁触发
    disconnect(m_model, &QStandardItemModel::itemChanged, this, &ProjectTreeManager::onItemChanged);

    // 清空现有内容
    m_model->clear();
    m_model->setHorizontalHeaderLabels({ "Curves" });

    // 获取所有曲线
    const QMap<QString, ThermalCurve>& curves = m_curveManager->getAllCurves();

    // 第一步:创建所有项目节点
    QMap<QString, QStandardItem*> projectNodes;
    QSet<QString> projectNames;

    for (const ThermalCurve& curve : curves) {
        if (!curve.projectName().isEmpty()) {
            projectNames.insert(curve.projectName());
        }
    }

    for (const QString& projectName : projectNames) {
        QStandardItem* projectItem = new QStandardItem(projectName);
        projectItem->setCheckable(false); // 项目节点不可勾选
        projectItem->setEditable(false);  // 不可编辑
        m_model->appendRow(projectItem);
        projectNodes[projectName] = projectItem;
    }

    // 第二步:添加所有顶层曲线(没有父曲线的)
    QMap<QString, QStandardItem*> curveItems;

    for (const ThermalCurve& curve : curves) {
        if (curve.parentId().isEmpty()) {
            QStandardItem* curveItem = createCurveItem(curve, false);

            QStandardItem* projectItem = projectNodes.value(curve.projectName());
            if (projectItem) {
                projectItem->appendRow(curveItem);
                curveItems[curve.id()] = curveItem;
            } else {
                qWarning() << "找不到项目节点" << curve.projectName() << ",跳过曲线" << curve.id();
            }
        }
    }

    // 第三步:使用迭代方式添加所有子节点,支持多层嵌套
    QSet<QString> processedCurves;
    for (const QString& curveId : curveItems.keys()) {
        processedCurves.insert(curveId);
    }

    bool hasUnprocessed = true;
    int maxIterations = curves.size();
    int iteration = 0;

    while (hasUnprocessed && iteration < maxIterations) {
        hasUnprocessed = false;
        iteration++;

        for (const ThermalCurve& curve : curves) {
            // 跳过已处理的曲线
            if (processedCurves.contains(curve.id())) {
                continue;
            }

            // 跳过顶层曲线(已在第二步处理)
            if (curve.parentId().isEmpty()) {
                continue;
            }

            // 检查父节点是否已经被创建
            QStandardItem* parentItem = curveItems.value(curve.parentId());
            if (parentItem) {
                // 找到了父节点,创建子节点
                QStandardItem* childItem = createCurveItem(curve, false);
                parentItem->appendRow(childItem);
                curveItems[curve.id()] = childItem;
                processedCurves.insert(curve.id());
            } else {
                // 父节点还未创建,本轮跳过,下一轮再处理
                hasUnprocessed = true;
            }
        }
    }

    // 第四步:处理孤儿节点(父节点不存在的曲线)
    for (const ThermalCurve& curve : curves) {
        if (!processedCurves.contains(curve.id())) {
            qWarning() << "找不到父曲线" << curve.parentId() << ",将曲线" << curve.id() << "添加到项目根节点";

            QStandardItem* projectItem = projectNodes.value(curve.projectName());
            if (projectItem) {
                QStandardItem* item = createCurveItem(curve, false);
                projectItem->appendRow(item);
                curveItems[curve.id()] = item;
            }
        }
    }

    // 重新连接信号
    connect(m_model, &QStandardItemModel::itemChanged, this, &ProjectTreeManager::onItemChanged);
}

QStandardItem* ProjectTreeManager::findOrCreateProjectItem(const QString& projectName)
{
    // 在根节点下查找项目节点
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QStandardItem* item = m_model->item(i);
        if (item && item->text() == projectName && !item->isCheckable()) {
            return item;
        }
    }

    // 未找到,创建新的项目节点
    QStandardItem* projectItem = new QStandardItem(projectName);
    projectItem->setCheckable(false);
    projectItem->setEditable(false);
    m_model->appendRow(projectItem);
    return projectItem;
}

QStandardItem* ProjectTreeManager::findCurveItem(const QString& curveId, QStandardItem* parent) const
{
    if (!parent) {
        // 从根节点开始查找
        for (int i = 0; i < m_model->rowCount(); ++i) {
            QStandardItem* found = findCurveItem(curveId, m_model->item(i));
            if (found) {
                return found;
            }
        }
        return nullptr;
    }

    // 检查当前节点
    if (parent->data(Qt::UserRole).toString() == curveId) {
        return parent;
    }

    // 递归检查子节点
    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem* found = findCurveItem(curveId, parent->child(i));
        if (found) {
            return found;
        }
    }

    return nullptr;
}

void ProjectTreeManager::collectCheckedItems(QStandardItem* parent, QStringList& result) const
{
    if (!parent) {
        return;
    }

    // 如果当前节点是曲线节点且被勾选,添加到结果列表
    if (parent->isCheckable() && parent->checkState() == Qt::Checked) {
        QString curveId = parent->data(Qt::UserRole).toString();
        if (!curveId.isEmpty()) {
            result.append(curveId);
        }
    }

    // 递归处理子节点
    for (int i = 0; i < parent->rowCount(); ++i) {
        collectCheckedItems(parent->child(i), result);
    }
}

QStandardItem* ProjectTreeManager::createCurveItem(const ThermalCurve& curve, bool checked)
{
    QStandardItem* item = new QStandardItem(curve.name());
    item->setCheckable(true);
    item->setEditable(false);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);

    // 在 UserRole 中存储曲线ID,方便后续查找
    item->setData(curve.id(), Qt::UserRole);

    return item;
}
