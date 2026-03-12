#include "tl_files_list.h"

#include <QScrollBar>
#include <QStandardItemModel>
#include <QWheelEvent>


TlFilesList::TlFilesList(QWidget *parent) : QListWidget(parent) {
    this->setSpacing(1);  // 设置单元项目间距
    this->setMovement(QListWidget::Static); // 设置不能移动
    this->setViewMode(QListView::ListMode); // 设置为列表显示模式
    this->setFlow(QListView::LeftToRight);  // 设置为从左往右排列
    this->setHorizontalScrollMode(QListWidget::ScrollPerItem);  // 设置按项目滚动
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 屏蔽水平滑动条
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // 屏蔽垂直滑动条
    this->setIconSize(QSize(128, 128));

    // 设置样式
    this->setStyleSheet(R"(QListWidget { outline: none; border:1px solid gray; color: black; }
                           QListWidget::Item { width: 102400px; height: 80px; }
                           QListWidget::Item:hover { background: #4CAF50; color: white; }
                           QListWidget::item:selected { background: #e7e7e7; color: #f44336; }
                           QListWidget::item:selected:!active { background: lightgreen; }
                           )");
}

TlFilesList::~TlFilesList() {
}

void TlFilesList::wheelEvent(QWheelEvent *event) {
    const QPoint wheel_degrees = event->angleDelta() / 8;
    const QPoint wheel_steps = wheel_degrees / 15;
    // 实现的是横排移动，所以这里把滚轮的上下移动实现为
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + wheel_steps.y());
    event->accept();
}

void TlFilesList::keyPressEvent(QKeyEvent *event) {
    QListWidget::keyPressEvent(event);
    QModelIndex curr_index = this->currentIndex();
    const auto *model = (QStandardItemModel *)this->model();
    switch (event->key()) {
        case Qt::Key_Left: {
            if (curr_index.row() > 0) {
                QModelIndex next_index = model->index(curr_index.row() - 1, curr_index.column());
                this->pressed(next_index);
            }
            break;
        }
        case Qt::Key_Right: {
            if (curr_index.row() + 1 < this->count()) {
                QModelIndex next_index = model->index(curr_index.row() + 1, curr_index.column());
                this->pressed(next_index);
            }
            break;
        }
        default: {
            break;
        }
    }
}