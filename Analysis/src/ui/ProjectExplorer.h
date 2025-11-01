#ifndef PROJECTEXPLORER_H
#define PROJECTEXPLORER_H

#include <QWidget>
#include <QModelIndex>

class QTreeView;
class QAbstractItemModel;

class ProjectExplorer : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectExplorer(QWidget *parent = nullptr);
    void setModel(QAbstractItemModel *model);
    QTreeView* treeView() const;

signals:
    void fileDoubleClicked(const QModelIndex &index);

private:
    QTreeView *m_treeView;
};

#endif // PROJECTEXPLORER_H
