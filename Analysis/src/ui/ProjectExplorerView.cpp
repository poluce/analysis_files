#include "ProjectExplorerView.h"
#include <QTreeView>
#include <QAbstractItemModel>
#include <QVBoxLayout>

ProjectExplorerView::ProjectExplorerView(QWidget *parent)
    : QWidget(parent)
{
    m_treeView = new QTreeView(this);
    // 隐藏不必要的列
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);
    m_treeView->setHeaderHidden(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeView);
    setLayout(layout);

    connect(m_treeView, &QTreeView::doubleClicked, this, &ProjectExplorerView::fileDoubleClicked);
}

void ProjectExplorerView::setModel(QAbstractItemModel *model)
{
    m_treeView->setModel(model);
}

QTreeView* ProjectExplorerView::treeView() const
{
    return m_treeView;
}
