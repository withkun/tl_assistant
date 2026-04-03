#ifndef __INC_SHAPE_LIST_H
#define __INC_SHAPE_LIST_H

#include <QListView>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextDocument>
#include <QPainter>

#include "tl_shape.h"

class HTMLDelegate: public QStyledItemDelegate {
public:
    explicit HTMLDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QTextDocument      *doc_{nullptr};
};

class ShapeListItem : public QStandardItem {
public:
    explicit ShapeListItem(const QString &text) { InitItem(text); };
    ShapeListItem(const QString &text, const TlShape &shape);

    ShapeListItem *clone() const override;
    void setShape(const TlShape &shape);
    TlShape shape() const;

private:
    void InitItem(const QString &text);
};

// QStandardItemModel --> QAbstractItemModel --> QObject
class StandardItemModel : public QStandardItemModel {
    Q_OBJECT
public:
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

signals:
    void itemDropped();
};

// QListView是列表形式的展示控件
// QListWidget继承自QListView, 是表格形式的展示控件
// 本质区别: QListView基于Model(需要自己建模), QListWidget基于Item
class ShapeListWidget : public QListView {
    Q_OBJECT
public:
    explicit ShapeListWidget(QWidget *parent = nullptr);

signals:
    void itemDropped();
    void itemChanged(ShapeListItem *item);
    void itemDoubleClicked(ShapeListItem *item);
    void itemSelectionChanged(const QList<ShapeListItem *> &selected, const QList<ShapeListItem *> &deselect);

public slots:

private:
    QList<ShapeListItem *>      selectedItems_;
    StandardItemModel          *model_{nullptr};

public:
    //void __init__();
    int32_t len() const;
    QList<ShapeListItem *> items() const;
    //void __iter__();

    void itemDroppedEvent();
    void itemChangedEvent(QStandardItem *item);
    void itemSelectionChangedEvent(const QItemSelection &selected, const QItemSelection &deselect);
    void itemDoubleClickedEvent(const QModelIndex &index);
    QList<ShapeListItem *> selectedItems();
    void scrollToItem(ShapeListItem *item);
    void addItem(ShapeListItem *item);
    void removeItem(ShapeListItem *item);
    void selectItem(ShapeListItem *item);
    ShapeListItem *findItemByShape(const TlShape &shape);
    void clear();

    bool empty() const;
};
#endif //__INC_SHAPE_LIST_H