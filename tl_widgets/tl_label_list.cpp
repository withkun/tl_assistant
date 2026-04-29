#include "tl_label_list.h"
#include "tl_shape_list.h"

#include <format>
#include <QLabel>
#include <QMouseEvent>


void EscapableQListWidget::keyPressEvent(QKeyEvent *keyEvent) {
    QListWidget::keyPressEvent(keyEvent);
    if (keyEvent->key() == Qt::Key_Escape) {
        this->clearSelection();
    }
}

LabelList::LabelList() {
    this->setItemDelegate(new HTMLDelegate(this));
}

void LabelList::mousePressEvent(QMouseEvent *mouseEvent) {
    QListWidget::mousePressEvent(mouseEvent);
    if (!this->indexAt(mouseEvent->pos()).isValid()) {
        clearSelection();
    }
}

QListWidgetItem *LabelList::find_label_item(const QString &label) {
    for (auto row = 0; row < this->count(); ++row) {
        auto *item = this->item(row);
        if (item->data(Qt::UserRole) == label) {
            return item;
        }
    }
    return nullptr;
}

void LabelList::add_label_item(const QString &label, const std::vector<int32_t> &color) {
    if (this->find_label_item(label)) {
        std::cerr << "[TlLabelList::add_label_item] " << label.toStdString() << " already exists" << std::endl;
        throw std::logic_error(std::format("Item for label '{}' already exists", label.toStdString()));
    }

    auto *item = new QListWidgetItem();
    item->setData(Qt::UserRole, label);  // for find_label_item
    item->setText(
        QString("%1 <font color='#%2%3%4'>●</font>").arg(label.toHtmlEscaped())
            .arg(color[0], 2, 16, QChar('0')).arg(color[1], 2, 16, QChar('0')).arg(color[2], 2, 16, QChar('0'))
    );
    this->addItem(item);
}