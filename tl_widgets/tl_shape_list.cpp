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

ShapeListItem::ShapeListItem(const QString &text, const TlShape &shape) : QStandardItem() {
    InitItem(text);
    setShape(shape);
}

void ShapeListItem::InitItem(const QString &text) {
    setText(text);
    setCheckable(true);
    setCheckState(Qt::Checked);
    setEditable(false);
    setTextAlignment(Qt::AlignBottom);
}

ShapeListItem *ShapeListItem::clone() const {
    return new ShapeListItem(text(), shape());
}

void ShapeListItem::setShape(const TlShape &shape) {
    this->setData(QVariant::fromValue(shape), Qt::UserRole);
}

TlShape ShapeListItem::shape() const {
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

ShapeListWidget::ShapeListWidget(QWidget *parent) : QListView(parent) {
    this->selectedItems_.clear();

    this->setWindowFlags(Qt::Window);

    this->model_ = new StandardItemModel();
    this->model_->setItemPrototype(new ShapeListItem(""));
    this->setModel(this->model_);

    this->setItemDelegate(new HTMLDelegate());
    this->setSelectionMode(QAbstractItemView::ExtendedSelection);  // 选中模式
    this->setDragDropMode(QAbstractItemView::InternalMove);
    this->setDefaultDropAction(Qt::MoveAction);

    QObject::connect(this, &ShapeListWidget::doubleClicked, this, &ShapeListWidget::itemDoubleClickedEvent);
    QObject::connect(this->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ShapeListWidget::itemSelectionChangedEvent);
    QObject::connect(this->model_, &StandardItemModel::itemDropped, this, &ShapeListWidget::itemDroppedEvent);
    QObject::connect(this->model_, &StandardItemModel::itemChanged, this, &ShapeListWidget::itemChangedEvent);
}

int32_t ShapeListWidget::len() const {
    return this->model_->rowCount();
}

QList<ShapeListItem *> ShapeListWidget::items() const {
    QList<ShapeListItem *> items;
    for (auto row = 0; row < model_->rowCount(); ++row) {
        items.push_back(static_cast<ShapeListItem *>(model_->item(row)));
    }
    return items;
}
//void TlShapeList::__getitem__(i) {
//}

//void TlShapeList::__iter__() {
//}

void ShapeListWidget::itemDroppedEvent() {
    emit itemDropped();
}

void ShapeListWidget::itemChangedEvent(QStandardItem *item) {
    emit itemChanged(static_cast<ShapeListItem *>(item));
}

void ShapeListWidget::itemSelectionChangedEvent(const QItemSelection &selected, const QItemSelection &deselect) {
    QList<ShapeListItem *> selected1, deselect1;
    const QList<QModelIndex> sel_indexes = selected.indexes(), des_indexes = deselect.indexes();
    std::ranges::transform(sel_indexes, std::back_inserter(selected1), [this](const auto &idx) { return static_cast<ShapeListItem *>(this->model_->itemFromIndex(idx)); });
    std::ranges::transform(des_indexes, std::back_inserter(deselect1), [this](const auto &idx) { return static_cast<ShapeListItem *>(this->model_->itemFromIndex(idx)); });
    emit itemSelectionChanged(selected1, deselect1);
}

void ShapeListWidget::itemDoubleClickedEvent(const QModelIndex &index) {
    emit itemDoubleClicked(static_cast<ShapeListItem *>(this->model_->itemFromIndex(index)));
}

QList<ShapeListItem *> ShapeListWidget::selectedItems() {
    //return [self.model().itemFromIndex(i) for i in self.selectedIndexes()]
    QList<ShapeListItem *> selected;
    const QList<QModelIndex> indexes = selectedIndexes();
    std::ranges::transform(indexes, std::back_inserter(selected), [this](const auto &idx) { return static_cast<ShapeListItem *>(this->model_->itemFromIndex(idx)); });
    return selected;
}

void ShapeListWidget::scrollToItem(ShapeListItem *item) {
    this->scrollTo(this->model_->indexFromItem(item));
}

void ShapeListWidget::addItem(ShapeListItem *item) {
    if (item == nullptr) {
        throw std::invalid_argument("item must be LabelListWidgetItem");
    }
    this->model_->setItem(this->model_->rowCount(), 0, item);
    item->setSizeHint(this->itemDelegate()->sizeHint(QStyleOptionViewItem(), QModelIndex()));
}

void ShapeListWidget::removeItem(ShapeListItem *item) {
    const auto index = this->model_->indexFromItem(item);
    this->model_->removeRows(index.row(), 1);
}

void ShapeListWidget::selectItem(ShapeListItem *item) {
    const auto index = this->model_->indexFromItem(item);
    selectionModel()->select(index, QItemSelectionModel::Select);
}

ShapeListItem *ShapeListWidget::findItemByShape(const TlShape &shape) {
    for (auto row = 0; row < this->model_->rowCount(); ++row) {
        auto *const item = static_cast<ShapeListItem *>(this->model_->item(row, 0));
        if (item->shape() == shape) {
            return item;
        }
    }
    throw std::runtime_error("cannot find shape: {}"); //.format(shape));
}

void ShapeListWidget::clear() {
    this->model_->clear();
    this->selectedItems_.clear();
}

bool ShapeListWidget::empty() const {
    return this->model_->rowCount() == 0;
}