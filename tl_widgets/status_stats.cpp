#include "status_stats.h"


StatusStats::StatusStats(QWidget *parent) : QLabel(parent) {
    QFont font;
    font.setFamily("monospace");
    font.setStyleHint(QFont::Monospace);
    this->setFont(font);
}