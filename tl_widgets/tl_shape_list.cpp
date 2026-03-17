#include "tl_shape_list.h"

#include "spdlog/spdlog.h"
#include "base/format_qt.h"

#include <QApplication>
#include <QAbstractTextDocumentLayout>


HTMLDelegate::HTMLDelegate(QObject *parent) : QStyledItemDelegate(parent) {
    doc_ = new QTextDocument(this);
}

void HTMLDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();

    auto *options = new QStyleOptionViewItem(option);

    initStyleOption(options, index);
    doc_->setHtml(options->text);
    options->text = "";

    auto *style = (
        options->widget == nullptr ? QApplication::style() : options->widget->style()
    );
    style->drawControl(QStyle::CE_ItemViewItem, options, painter);

    auto ctx = QAbstractTextDocumentLayout::PaintContext();

    if (option.state & QStyle::State_Selected) {
        ctx.palette.setColor(
            QPalette::Text,
            option.palette.color(QPalette::Active, QPalette::HighlightedText)
        );
    } else {
        ctx.palette.setColor(
            QPalette::Text,
            option.palette.color(QPalette::Active, QPalette::Text)
        );
    }

    auto textRect = style->subElementRect(QStyle::SE_ItemViewItemText, options);

    if (index.column() != 0) {
        textRect.adjust(5, 0, 0, 0);
    }

    const int32_t margin_shift = 4;
    auto margin = (option.rect.height() - options->fontMetrics.height()) / 2;
    margin = margin - margin_shift;
    textRect.setTop(textRect.top() + margin);

    painter->translate(textRect.topLeft());
    painter->setClipRect(textRect.translated(-textRect.topLeft()));
    doc_->documentLayout()->draw(painter, ctx);

    painter->restore();
}

QSize HTMLDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    const int32_t margin_shift = 4;
    return QSize(
        int(doc_->idealWidth()),
        int(doc_->size().height() - margin_shift)
    );
}


TlShapeListItem::TlShapeListItem(const QString &text, const TlShape &shape) : QStandardItem() {
    setText(text);
    setShape(shape);

    setCheckable(true);
    setCheckState(Qt::Checked);
    setEditable(false);
    setTextAlignment(Qt::AlignBottom);
}

TlShapeListItem *TlShapeListItem::clone() const {
    return new TlShapeListItem(text(), shape());
}

void TlShapeListItem::setShape(const TlShape &shape) {
    this->setData(QVariant::fromValue(shape), Qt::UserRole);
}

TlShape TlShapeListItem::shape() const {
    return this->data(Qt::UserRole).value<TlShape>();
}

//def __hash__(self):
//    return id(self)
//
//def __repr__(self):
//    return '{}("{}")'.format(self.__class__.__name__, self.text())


bool StandardItemModel::removeRows(int row, int count, const QModelIndex &parent) {
    const auto ret = QStandardItemModel::removeRows(row, count, parent);
    emit itemDropped();
    return ret;
}

bool StandardItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    // NOTE: By default, PyQt will overwrite items when dropped on them, so we need
    // to adjust the row/parent to insert after the item instead.
    //
    // If row is -1, we're dropping on an item (which would overwrite)
    // Instead, we want to insert after it
    QModelIndex parent_ = parent;
    if (row == -1 && parent.isValid()) {
        row = parent.row() + 1;
        parent_ = parent.parent();
    }

    // If still -1, append to end
    if (row == -1) {
        row = this->rowCount(parent_);
    }

    return QStandardItemModel::dropMimeData(data, action, row, column, parent_);
}

TlShapeList::TlShapeList(QWidget *parent) : QListView(parent) {
    this->selectedItems_.clear();

    this->setWindowFlags(Qt::Window);

    this->model_ = new StandardItemModel();
    this->model_->setItemPrototype(new TlShapeListItem());
    this->setModel(this->model_);

    this->setItemDelegate(new HTMLDelegate());
    this->setSelectionMode(QAbstractItemView::ExtendedSelection);  // 选中模式
    this->setDragDropMode(QAbstractItemView::InternalMove);
    this->setDefaultDropAction(Qt::MoveAction);

    QObject::connect(this->model_, &StandardItemModel::itemDropped, this, &TlShapeList::itemDroppedEvent);
    QObject::connect(this->model_, &StandardItemModel::itemChanged, this, &TlShapeList::itemChangedEvent);
    QObject::connect(this, &TlShapeList::doubleClicked, this, &TlShapeList::itemDoubleClickedEvent);
    QObject::connect(this->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TlShapeList::itemSelectionChangedEvent);
}

int32_t TlShapeList::len() const {
    return this->model_->rowCount();
}

QList<TlShapeListItem *> TlShapeList::items() const {
    QList<TlShapeListItem *> items;
    for (auto row = 0; row < model_->rowCount(); ++row) {
        auto *item = static_cast<TlShapeListItem *>(model_->item(row));
        items.push_back(item);
    }
    return items;
}
//void TlShapeList::__getitem__(i) {
//}

//void TlShapeList::__iter__() {
//}

//@property
//def itemDropped(self):
//    return self._model.itemDropped
//
//@property
//def itemChanged(self):
//    return self._model.itemChanged

void TlShapeList::itemDroppedEvent() {
    emit itemDropped();
}

void TlShapeList::itemChangedEvent(QStandardItem *item) {
    emit itemChanged(static_cast<TlShapeListItem *>(item));
}

void TlShapeList::itemSelectionChangedEvent(const QItemSelection &selected, const QItemSelection &deselected) {
    QList<TlShapeListItem *> selected_;
    QList<TlShapeListItem *> deselected_;
    for (auto &i : selected.indexes()) {
        auto *item = this->model_->itemFromIndex(i);
        selected_.push_back(static_cast<TlShapeListItem *>(item));
    }
    for (auto &i : deselected.indexes()) {
        auto *item = this->model_->itemFromIndex(i);
        deselected_.push_back(static_cast<TlShapeListItem *>(item));
    }
    //std::transform(selected.indexes().begin(), selected.indexes().end(), std::back_inserter(selected_), [this](const auto &idx) { return static_cast<TlShapeListItem *>(this->model_->itemFromIndex(idx)); });
    //std::transform(deselected.indexes().begin(), deselected.indexes().end(), std::back_inserter(deselected_), [this](const auto &idx) { return static_cast<TlShapeListItem *>(this->model_->itemFromIndex(idx)); });
    emit itemSelectionChanged(selected_, deselected_);
}

void TlShapeList::itemDoubleClickedEvent(const QModelIndex &index) {
    emit itemDoubleClicked(static_cast<TlShapeListItem *>(this->model_->itemFromIndex(index)));
}

QList<TlShapeListItem *> TlShapeList::selectedItems() {
    //return [self.model().itemFromIndex(i) for i in self.selectedIndexes()]
    QList<TlShapeListItem *> selected;
    const auto indexes = selectedIndexes();
    std::ranges::transform(indexes, std::back_inserter(selected), [this](const auto &idx) { return static_cast<TlShapeListItem *>(this->model_->itemFromIndex(idx)); });
    return selected;
}

void TlShapeList::scrollToItem(QStandardItem *item) {
    this->scrollTo(this->model_->indexFromItem(item));
}

void TlShapeList::addItem(TlShapeListItem *item) {
    if (item == nullptr) {
        throw std::invalid_argument("item must be LabelListWidgetItem");
    }
    this->model_->setItem(this->model_->rowCount(), 0, item);
    item->setSizeHint(this->itemDelegate()->sizeHint(QStyleOptionViewItem(), QModelIndex()));
}

void TlShapeList::removeItem(TlShapeListItem *item) {
    auto index = this->model_->indexFromItem(item);
    SPDLOG_INFO("remove {} item: {}", item->text(), index.row());
    this->model_->removeRows(index.row(), 1);
}

void TlShapeList::selectItem(TlShapeListItem *item) {
    const auto index = this->model_->indexFromItem(item);
    selectionModel()->select(index, QItemSelectionModel::Select);
}

TlShapeListItem *TlShapeList::findItemByShape(const TlShape &shape) {
    for (auto row = 0; row < this->model_->rowCount(); ++row) {
        auto *const item = static_cast<TlShapeListItem *>(this->model_->item(row, 0));
        if (item->shape() == shape) {
            return item;
        }
    }
    throw std::runtime_error("cannot find shape: {}"); //.format(shape));
}

void TlShapeList::clear() {
    this->model_->clear();
    this->selectedItems_.clear();
}

bool TlShapeList::empty() const {
    return this->model_->rowCount() == 0;
}