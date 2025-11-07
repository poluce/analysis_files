#include "project_explorer_view.h"
#include <QAbstractItemModel>
#include <QDebug>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

ProjectExplorerView::ProjectExplorerView(QWidget* parent)
    : QWidget(parent)
{
    qDebug() << "构造:  ProjectExplorerView";
    m_treeView = new QTreeView(this);

    // 隐藏不必要的列
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);
    m_treeView->setHeaderHidden(true);
    m_treeView->setContextMenuPolicy(Qt::ActionsContextMenu);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeView);
    setLayout(layout);

    QAction *deleteAction = new QAction("删除节点", this);
    m_treeView->addAction(deleteAction);

    connect(deleteAction, &QAction::triggered, this, &ProjectExplorerView::deleteActionClicked);
    connect(m_treeView, &QTreeView::doubleClicked, this, &ProjectExplorerView::curveItemDoubleClicked);
}

void ProjectExplorerView::setModel(QAbstractItemModel* model) { m_treeView->setModel(model); }

QTreeView* ProjectExplorerView::treeView() const { return m_treeView; }
