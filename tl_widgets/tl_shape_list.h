#ifndef __INC_SHAPE_LIST_H
#define __INC_SHAPE_LIST_H

#include <QListView>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextDocument>
#include <QPainter>
#include <QStringListModel>

#include "tl_shape.h"


class HTMLDelegate: public QStyledItemDelegate {
public:
    explicit HTMLDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QTextDocument      *doc_{nullptr};
};

class TlShapeListItem : public QStandardItem {
  public:
    TlShapeListItem() {};
    TlShapeListItem(const QString &text, const TlShape &shape);

    TlShapeListItem *clone() const override;
    void setShape(const TlShape &shape);
    TlShape shape() const;
};

// QStandardItemModel --> QAbstractItemModel --> QObject
class StandardItemModel : public QStandardItemModel {
    Q_OBJECT
public:
    //void __hash__();
    //void __repr__();
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

signals:
    void itemDropped();

private:
    // QAbstractListModel --> QAbstractItemModel --> QObject
    //QStringListModel *model_;
};

// QListView是列表形式的展示控件
// QListWidget继承自QListView, 是表格形式的展示控件
// 本质区别: QListView基于Model(需要自己建模), QListWidget基于Item
class TlShapeList : public QListView {
    Q_OBJECT
public:
    explicit TlShapeList(QWidget *parent = nullptr);

signals:
    void itemDropped();
    void itemChanged(TlShapeListItem *item);
    void itemDoubleClicked(TlShapeListItem *item);
    void itemSelectionChanged(const QList<TlShapeListItem *> &l1, const QList<TlShapeListItem *> &l2);

public slots:

public:
    //void __init__();
    int32_t len() const;
    QList<TlShapeListItem *> items() const;
    //void __iter__();

    QList<TlShapeListItem *>    selectedItems_;
    StandardItemModel          *model_{nullptr};

    void itemDroppedEvent();
    void itemChangedEvent(QStandardItem *item);
    void itemSelectionChangedEvent(const QItemSelection &selected, const QItemSelection &deselected);
    void itemDoubleClickedEvent(const QModelIndex &index);
    QList<TlShapeListItem *> selectedItems();
    void scrollToItem(QStandardItem *item);
    void addItem(TlShapeListItem *item);
    void removeItem(TlShapeListItem *item);
    void selectItem(TlShapeListItem *item);
    TlShapeListItem *findItemByShape(const TlShape &shape);
    void clear();

    bool empty() const;
};
#endif // __INC_SHAPE_LIST_H