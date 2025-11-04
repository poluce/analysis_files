#ifndef PROJECTEXPLORERVIEW_H
#define PROJECTEXPLORERVIEW_H

#include <QModelIndex>
#include <QWidget>

class QTreeView;
class QAbstractItemModel;

class ProjectExplorerView : public QWidget {
    Q_OBJECT
public:
    explicit ProjectExplorerView(QWidget* parent = nullptr);
    void setModel(QAbstractItemModel* model);
    QTreeView* treeView() const;

signals:
    void fileDoubleClicked(const QModelIndex& index);

private:
    QTreeView* m_treeView;
};

#endif // PROJECTEXPLORERVIEW_H
