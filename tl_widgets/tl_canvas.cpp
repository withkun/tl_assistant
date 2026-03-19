#include "tl_canvas.h"
#include "base/format_qt.h"
#include "np_utils.h"

#include <QWheelEvent>
#include <QApplication>


bool download_ai_model(const std::string &name, Canvas *) {
    return true;
}

namespace {
// 不能在QApplication前创建Qt对象
//const QCursor CURSOR_DEFAULT = Qt::ArrowCursor;
//const QCursor CURSOR_POINT   = Qt::PointingHandCursor;
//const QCursor CURSOR_DRAW    = Qt::CrossCursor;
//const QCursor CURSOR_MOVE    = Qt::ClosedHandCursor;
//const QCursor CURSOR_GRAB    = Qt::OpenHandCursor;

const double MOVE_SPEED     = 5.0f;
}

Canvas::Canvas(float epsilon,
               const QString &double_click,
               int32_t num_backups,
               const QMap<QString, bool> &crosshair,
               const std::string &model_name) : QWidget() {
    this->mode_             = CanvasMode::EDIT;

    // polygon, rectangle, line, or point
    this->createMode_       = "polygon";

    this->fill_drawing_     = false;

    this->sam_session_model_name_   = model_name;

    //def __init__(self, *args, **kwargs):
    this->epsilon_          = epsilon;
    this->double_click_     = double_click;
    if (!this->double_click_.isEmpty() && this->double_click_ != "close") {
        throw std::invalid_argument("Unexpected value for double_click event: " + double_click.toStdString());
    }
    this->num_backups_      = num_backups;
    this->crosshair_        = crosshair.size() == 8 ?
        crosshair :
        QMap<QString, bool> {
            { "polygon",    false },
            { "rectangle",  true  },
            { "circle",     false },
            { "line",       false },
            { "point",      false },
            { "linestrip",  false },
            { "ai_polygon", false },
            { "ai_mask",    false },
        };

    this->resetState();

    // self.line represents:
    //   - createMode == 'polygon': edge from last point to current
    //   - createMode == 'rectangle': diagonal line of the rectangle
    //   - createMode == 'line': the line
    //   - createMode == 'point': the point
    this->line_.clear();
    this->prevPoint_            = QPoint();
    this->prevMovePoint_        = QPoint();
    this->offsets_              = { QPoint(), QPoint() };
    this->scale_                = 1.0;
    this->sam_session_         = nullptr;
    this->visible_              = {};
    this->hideBackround_        = false;
    this->hideBackround1_       = false;
    this->snapping_             = true;
    this->hShapeIsSelected_     = false;
    this->painter_;
    this->dragging_start_pos_   = QPointF();
    this->is_dragging_          = false;
    this->is_dragging_enabled_  = false;
    // Menus
    // 0: right-click without selection and dragging of shapes
    // 1: right-click with selection and dragging of shapes
    this->menus_                = { new QMenu(), new QMenu() };
    // Set widget options.
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::WheelFocus);
}

bool Canvas::fillDrawing() const {
    return this->fill_drawing_;
}

void Canvas::setFillDrawing(bool value) {
    this->fill_drawing_ = value;
}

//@property
QString Canvas::createMode() const {
    return this->createMode_;
}

//@createMode.setter
void Canvas::createMode(const QString &value) {
    if (!QKey{
        "polygon",
        "rectangle",
        "circle",
        "line",
        "point",
        "linestrip",
        "ai_polygon",
        "ai_mask",
        }.contains(value)) {
        throw std::invalid_argument("Unsupported createMode: " + value.toStdString());
    }
    this->createMode_ = value;
}

void Canvas::set_ai_model_name(const std::string &model_name) {
    this->sam_session_model_name_ = model_name;
}

SamSession &Canvas::get_osam_session() {
    if (this->sam_session_ == nullptr ||
        this->sam_session_->model_name() != this->sam_session_model_name_) {
        this->sam_session_ = std::make_unique<SamSession>(this->sam_session_model_name_);
    }
    return *this->sam_session_;
}

void Canvas::update_shape_with_ai(const QList<QPointF> &points, const QList<int32_t> &labels, TlShape &shape) {
    std::vector<cv::Point2f> point_coords;
    point_coords.reserve(points.size());
    std::ranges::transform(points, std::back_inserter(point_coords), [](const auto &v) { return cv::Point2f(v.x(), v.y()); });

    std::vector<float> point_labels;
    point_labels.reserve(labels.size());
    std::ranges::transform(labels, std::back_inserter(point_labels), [](const auto &v) { return static_cast<float>(v); });

    const auto image = TlUtils::PixmapToMat(pixmap_);
    const GenerateResponse response = get_osam_session().run(
        image,
        pixmap_hash_,
        point_coords,
        point_labels
    );
    update_shape_with_ai_response(
        response,
        shape,
        createMode_.toStdString()
    );
}

void Canvas::storeShapes() {
    QList<TlShape> shapesBackup;
    for (const auto &shape : shapes_) {
        shapesBackup.append(shape);
    }
    while (shapesBackups_.length() > num_backups_) {
        shapesBackups_.pop_front();
    }
    shapesBackups_.append(shapesBackup);
}

//@property
bool Canvas::isShapeRestorable() {
    // We save the state AFTER each edit (not before) so for an
    // edit to be undoable, we expect the CURRENT and the PREVIOUS state
    // to be in the undo stack.
    if (shapesBackups_.length() < 2) {
        return false;
    }
    return true;
}

void Canvas::restoreShape() {
    // This does _part_ of the job of restoring shapes.
    // The complete process is also done in app.py::undoShapeEdit
    // and app.py::loadShapes and our own Canvas::loadShapes function.
    if (!isShapeRestorable()) {
        return;
    }
    shapesBackups_.pop_back();  //latest

    // The application will eventually call Canvas.loadShapes which will
    // push this right back onto the stack.
    auto shapesBackup = shapesBackups_.back(); shapesBackups_.pop_back();
    shapes_ = { shapesBackup };
    selectedShapes_ = {};
    for (auto &shape : shapes_) {
        shape.selected_ = false;
    }
    this->update();
}

void Canvas::enterEvent(QEnterEvent *event) {
    overrideCursor(cursor_);
    update_status({});
}

void Canvas::leaveEvent(QEvent *event) {
    if (set_highlight(None, None, None)) {
        this->update();
    }
    restoreCursor();
    update_status({});
}

void Canvas::focusOutEvent(QFocusEvent *event) {
    restoreCursor();
    update_status({});
}

bool Canvas::isVisible(const TlShape &shape) {
    const auto it = visible_.find(shape.key());
    return it != visible_.end() ? it.value() : true;
}

bool Canvas::drawing() {
    return mode_ == CanvasMode::CREATE;
}

bool Canvas::editing() {
    return mode_ == CanvasMode::EDIT;
}

void Canvas::setEditing(bool value) {
    mode_ = value ? CanvasMode::EDIT : CanvasMode::CREATE;
    if (mode_ == CanvasMode::EDIT) {
        // CREATE -> EDIT
        repaint();  // clear crosshair
    } else {
        // EDIT -> CREATE
        bool need_update = set_highlight(
            None, None, None
        );
        need_update |= deSelectShape();
        if (need_update) {
            update();
        }
    }
}

bool Canvas::set_highlight(
    int32_t hShape, int32_t hEdge, int32_t hVertex
    ) {
    bool need_update = hShape != None;
    if (hShape_ != None) {
        shapes_[hShape_].highlightClear();
        need_update = true;
    }
    // NOTE: Store last highlighted for adding/removing points.
    this->lasthShape_  = hShape  == None ? hShape_  : hShape;
    this->lasthVertex_ = hVertex == None ? hVertex_ : hVertex;
    this->lasthEdge_   = hEdge   == None ? hEdge_   : hEdge;
    this->hShape_      = hShape;
    this->hVertex_     = hVertex;
    this->hEdge_       = hEdge;
    return need_update;
}

bool Canvas::selectedVertex() {
    return hVertex_ != None;
}

bool Canvas::selectedEdge() {
    return hEdge_ != None;
}

void Canvas::update_status(const std::list<QString> &extra_messages) {
    QStringList messages;
    if (drawing()) {
        messages.append(tr("Creating %1").arg(createMode_));
        messages.append(get_create_mode_message());
        if (current_) {
            messages.append(tr("ESC to cancel"));
        }
        if (canCloseShape()) {
            messages.append(tr("Enter or Space to finalize"));
        }
    } else {
        //assert self.editing();
        messages.append(QString(tr("Editing shapes")));
    }
    for (const auto &s : extra_messages) {
        messages.append(s);
    }
    emit statusUpdated(" • " + messages.join(""));
}

QString Canvas::get_create_mode_message() {
    //assert self.drawing()
    bool isNew = this->current_.points_.empty();
    if (createMode_ == "ai_polygon") {
        return tr(
            "Click points to include or Shift+Click to exclude for ai_polygon"
        );
    }
    if (createMode_ == "ai_mask") {
        return tr(
            "Click points to include or Shift+Click to exclude for ai_mask"
        );
    }
    if (createMode_ == "line") {
        if (isNew)
            return tr("Click start point for line");
        else
            return tr("Click end point for line");
    }
    if (createMode_ == "linestrip") {
        if (isNew)
            return tr("Click start point for linestrip");
        else
            return tr(
                "Click next point or finish by Ctrl/Cmd+Click for linestrip"
            );
    }
    if (createMode_ == "circle") {
        if (isNew)
            return tr("Click center point for circle");
        else
            return tr("Click point on circumference for circle");
    }
    if (createMode_ == "rectangle") {
        if (isNew)
            return tr("Click first corner for rectangle");
        else
            return tr("Click opposite corner for rectangle (Shift for square)");
    }
    return tr("Click to add point");
}

void Canvas::mouseMoveEvent(QMouseEvent *event) {
    // Update line with last point and current coordinates.
    // Python中的 localPos 已废弃‌, 推荐使用 position 替代, 其功能完全相同
    QPointF pos;
    try {
        pos = transformPos(event->position());
    } catch (...) {
        return;
    }
    emit mouseMoved(pos);

    prevMovePoint_ = pos;

    bool is_shift_pressed = event->modifiers() & Qt::ShiftModifier;

    if (is_dragging_) {
        overrideCursor(Qt::OpenHandCursor);
        QPointF delta = pos - dragging_start_pos_;
        emit scrollRequest(static_cast<int>(delta.x()), Qt::Horizontal);
        emit scrollRequest(static_cast<int>(delta.y()), Qt::Vertical);
        return;
    }

    // Polygon drawing.
    if (drawing()) {
        if (QKey{"ai_polygon", "ai_mask"}.contains(createMode_)) {
            line_.shape_type("points");
        } else {
            line_.shape_type(createMode_);
        }

        overrideCursor(Qt::CrossCursor);
        if (!current_) {
            repaint();  // draw crosshair
            update_status({});
            return;
        }

        if (outOfPixmap(pos)) {
            // Don't allow the user to draw outside the pixmap.
            // Project the point to the pixmap's edges.
            pos = intersectionPoint(current_[-1], pos);
        } else if (
                   snapping_ &&
                   current_.len() > 1 &&
                   createMode_ == "polygon" &&
                   closeEnough(pos, current_[0])) {
            // Attract line to starting point and
            // colorise to alert the user.
            pos = current_[0];
            overrideCursor(Qt::PointingHandCursor);
            current_.highlightVertex(0, TlShape::NEAR_VERTEX);
        }
        if (QKey{"polygon", "linestrip"}.contains(createMode_)) {
            line_.points_ = { current_[-1], pos };
            line_.point_labels_ = { 1, 1 };
        } else if (QKey{"ai_polygon", "ai_mask"}.contains(createMode_)) {
            line_.points_ = { current_.points_.back(), pos };
            line_.point_labels_ = {
                current_.point_labels_.back(),
                is_shift_pressed ? 0 : 1,
            };
        } else if (createMode_ == "rectangle") {
            if (is_shift_pressed) {
                pos = snap_cursor_pos_for_square(  // override
                    pos, current_[0]
                );
                prevMovePoint_ = pos;
            }
            line_.points_ = { current_[0], pos };
            line_.point_labels_ = { 1, 1 };
            line_.close();
        } else if (createMode_ == "circle") {
            line_.points_ = { current_[0], pos };
            line_.point_labels_ = { 1, 1 };
            line_.shape_type("circle");
        } else if (createMode_ == "line") {
            line_.points_ = { current_[0], pos };
            line_.point_labels_ = { 1, 1 };
            line_.close();
        } else if (createMode_ == "point") {
            line_.points_ = { current_[0] };
            line_.point_labels_ = { 1 };
            line_.close();
        }
        assert(line_.points_.size() == line_.point_labels_.size());
        repaint();
        current_.highlightClear();
        update_status({});
        return;
    }

    // Polygon copy moving.
    if (Qt::RightButton & event->buttons()) {
        if (!selectedShapesCopy_.empty() && !prevPoint_.isNull()) {
            overrideCursor(Qt::ClosedHandCursor);
            QList<int32_t> indexes(selectedShapesCopy_.size());
            std::iota(indexes.begin(), indexes.end(), 0); // 使用 std::iota 填充序列, 从0开始
            boundedMoveShapes(selectedShapesCopy_, indexes, pos);
            repaint();
        } else if (!selectedShapes_.empty()) {
            selectedShapesCopy_ = {};
            std::ranges::transform(selectedShapes_, std::back_inserter(selectedShapesCopy_), [this](int32_t idx){ return shapes_[idx]; });
            repaint();
        }
        update_status({});
        return;
    }

    // Polygon/Vertex moving.
    if (Qt::LeftButton & event->buttons()) {
        if (selectedVertex()) {
            boundedMoveVertex(pos, is_shift_pressed);
            repaint();
            movingShape_ = true;
        } else if (!selectedShapes_.empty() && !prevPoint_.isNull()) {
            overrideCursor(Qt::ClosedHandCursor);
            boundedMoveShapes(shapes_, selectedShapes_, pos);
            repaint();
            movingShape_ = true;
        }
        return;
    }

    bool is_for_breaked = false;
    // Just hovering over the canvas, 2 possibilities:
    // - Highlight shapes
    // - Highlight vertex
    // Update shape/vertex fill and tooltip value accordingly.
    std::list<QString> status_messages;
    //for shape in ([self.hShape] if self.hShape else []) + [s for s in reversed(self.shapes) if self.isVisible(s) and s != self.hShape]:
    std::vector<int32_t> indexes;
    if (hShape_ != None) { indexes.push_back(hShape_); }
    for (int32_t idx = shapes_.size() - 1; idx >= 0; --idx) { if (isVisible(shapes_[idx]) && idx != hShape_) { indexes.push_back(idx); } }
    for (const auto &idx : indexes) {
        auto &shape = shapes_[idx];
        // Look for a nearby vertex to highlight.
        auto index = shape.nearestVertex(pos, epsilon_);
        if (index != None) {
            set_highlight(idx, None, index);
            shape.highlightVertex(index, shape.MOVE_VERTEX);
            overrideCursor(Qt::PointingHandCursor);
            status_messages.push_back(tr("Click & drag to move point"));
            if (shape.canRemovePoint()) {
                status_messages.push_back(
                    tr("ALT + SHIFT + Click to delete point")
                );
            }
            this->update();
            is_for_breaked = true;
            break;
        }
        // Look for a nearby edge to highlight.
        auto index_edge = shape.nearestEdge(pos, epsilon_);
        if (index_edge != None && shape.canAddPoint()) {
            set_highlight(idx, index_edge, None);
            overrideCursor(Qt::PointingHandCursor);
            status_messages.push_back(tr("ALT + Click to create point on shape"));
            this->update();
            is_for_breaked = true;
            break;
        }
        // Check if we happen to be inside a shape.
        if (shape.containsPoint(pos)) {
            set_highlight(idx, index_edge, None);
            status_messages.push_back(
                tr("Click & drag to move shape"));
            status_messages.push_back(
                tr("Right-click & drag to copy shape")
            );
            overrideCursor(Qt::OpenHandCursor);
            this->update();
            is_for_breaked = true;
            break;
        }
    }
    // 如果没有通过 break 跳出循环, 执行这里的代码.
    if (!is_for_breaked) {  // Nothing found, clear highlights, reset state.
        restoreCursor();
        if (set_highlight(None, None, None)) {
            this->update();
        }
    }
    emit vertexSelected(hVertex_ != None);
    update_status(status_messages);
}

void Canvas::addPointToEdge() {
    auto shape = lasthShape_;
    auto index = lasthEdge_;
    auto point = prevMovePoint_;
    if (shape == None || index == None || point.isNull()) {
        return;
    }
    const auto saved = shapes_[shape];
    shapes_[shape].insertPoint(index, point);
    shapes_[shape].highlightVertex(index, TlShape::MOVE_VERTEX);
    hShape_ = shape;
    hVertex_ = index;
    hEdge_ = None;
    movingShape_ = true;
}

void Canvas::removeSelectedPoint() {
    auto shape = lasthShape_;
    auto index = lasthVertex_;
    if (shape == None || index == None) {
        return;
    }
    const auto saved = shapes_[shape];
    shapes_[shape].removePoint(index);
    shapes_[shape].highlightClear();
    hShape_ = shape;
    lasthVertex_ = None;
    movingShape_ = true;  // Save changes
}

void Canvas::mousePressEvent(QMouseEvent *event) {
    QPointF pos = transformPos(event->position());
    bool is_shift_pressed = event->modifiers() & Qt::ShiftModifier;

    if (event->button() == Qt::LeftButton) {
        if (drawing()) {
            if (current_) {
                // Add point to existing shape.
                if (createMode_ == "polygon") {
                    current_.addPoint(line_[1]);
                    line_[0] = current_[-1];
                    if (current_.isClosed()) {
                        finalise();
                    }
                } else if (QKey{"rectangle", "circle",  "line"}.contains(createMode_)) {
                    assert(current_.points_.size() == 1);
                    current_.points_ = line_.points_;
                    finalise();
                } else if (createMode_ == "linestrip") {
                    current_.addPoint(line_[1]);
                    line_[0] = current_[-1];
                    if (event->modifiers() == Qt::ControlModifier) {
                        finalise();
                    }
                } else if (QKey{"ai_polygon", "ai_mask"}.contains(createMode_)) {
                    current_.addPoint(
                        line_.points_[1],
                        line_.point_labels_[1]
                    );
                    line_.points_[0] = current_.points_.back();
                    line_.point_labels_[0] = current_.point_labels_.back();
                    if (event->modifiers() & Qt::ControlModifier) {
                        finalise();
                    }
                }
            } else if (!outOfPixmap(pos)) {
                if (QKey{"ai_polygon", "ai_mask"}.contains(createMode_)) {
                    if (!download_ai_model(this->sam_session_model_name_, this)) {
                        return;
                    }
                }

                // Create new shape.
                current_ = TlShape("",
                    TlShape::line_color,
                    QKey{"ai_polygon", "ai_mask"}.contains(createMode_)
                        ? "points" : createMode_
                );
                current_.addPoint(pos, is_shift_pressed ? 0 : 1);
                if (createMode_ == "point") {
                    finalise();
                } else if (
                    QKey{"ai_polygon", "ai_mask"}.contains(createMode_)
                    && (event->modifiers() & Qt::ControlModifier)
                ) {
                    finalise();
                } else {
                    if (createMode_ == "circle") {
                        current_.shape_type("circle");
                    }
                    line_.points_ = {pos, pos};
                    if (
                        QKey{"ai_polygon", "ai_mask"}.contains(createMode_)
                        && is_shift_pressed
                    ) {
                        line_.point_labels_ = {0, 0};
                    } else {
                        line_.point_labels_ = {1, 1};
                    }
                    setHiding();
                    emit drawingPolygon(true);
                    update();
                }
            }
        } else if (editing()) {
            if (selectedEdge() && event->modifiers() == Qt::AltModifier) {
                addPointToEdge();       // 增加节点
            } else if (selectedVertex() && event->modifiers() == (
                Qt::AltModifier | Qt::ShiftModifier
            )) {
                removeSelectedPoint();  // 删除节点
            }
            auto group_mode = event->modifiers() == Qt::ControlModifier;
            selectShapePoint(pos, group_mode);
            prevPoint_ = pos;
            repaint();
        }
    } else if (event->button() == Qt::RightButton && editing()) {
        auto group_mode = event->modifiers() == Qt::ControlModifier;
        if (selectedShapes_.empty() || (
             hShape_ != None && !selectedShapes_.contains(hShape_)
        )) {
            selectShapePoint(pos, group_mode);
            repaint();
        }
        prevPoint_ = pos;
    } else if (event->button() == Qt::MiddleButton && is_dragging_enabled_) {
        overrideCursor(Qt::OpenHandCursor);
        dragging_start_pos_ = pos;
        is_dragging_ = true;
    }
    update_status({});
}

void Canvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        auto menu = menus_[selectedShapesCopy_.size() > 0];
        restoreCursor();
        if (!menu->exec(mapToGlobal(event->pos())) && !selectedShapesCopy_.empty()) {
            // Cancel the move by deleting the shadow copy.
            selectedShapesCopy_ = {};
            repaint();
        }
    } else if (event->button() == Qt::LeftButton) {
        if (editing()) {
            if (
                hShape_ != None &&
                hShapeIsSelected_ &&
                !movingShape_
            ) {
                QList<int32_t> selected_shapes;
                for (auto x : selectedShapes_) {
                    if (x != hShape_) { selected_shapes.push_back(x); }
                }
                emit selectionChanged(selected_shapes);
            }
        }
    } else if (event->button() == Qt::MiddleButton) {
        is_dragging_ = false;
        restoreCursor();
    }

    if (movingShape_ && hShape_ != None) {
        auto index = hShape_;
        if (shapesBackups_.back()[index].points_ != shapes_[index].points_) {
            storeShapes();
            emit shapeMoved();
        }

        movingShape_ = false;
    }
    update_status({});
}

bool Canvas::endMove(bool copy) {
    assert(!selectedShapes_.empty() && !selectedShapesCopy_.empty());
    assert(selectedShapesCopy_.size() == selectedShapes_.size());
    if (copy) {
        for (auto i = 0; i < selectedShapesCopy_.size(); ++i) {
            auto &shape = selectedShapesCopy_[i];
            shapes_.append(shape);
            shapes_[selectedShapes_[i]].selected_ = false;
            selectedShapes_[i] = shapes_.count() - 1;
        }
    } else {
        for (auto i = 0; i < selectedShapesCopy_.size(); ++i) {
            auto &shape = selectedShapesCopy_[i];
            shapes_[selectedShapes_[i]].points_ = shape.points_;
        }
    }
    selectedShapesCopy_ = {};
    repaint();
    storeShapes();
    return true;
}

void Canvas::hideBackroundShapes(bool value) {
    hideBackround_ = value;
    if (!selectedShapes_.empty()) {
        // Only hide other shapes if there is a current selection.
        // Otherwise the user will not be able to select a shape.
        setHiding(true);
        update();
    }
}

void Canvas::setHiding(bool enable) {
    hideBackround1_ = enable ? hideBackround_ : false;
}

bool Canvas::canCloseShape() {
    if (!drawing())
        return false;
    if (!current_)
        return false;
    if (QKey{"ai_polygon", "ai_mask"}.contains(createMode_))
        return true;
    if (createMode_ == "linestrip")
        return current_.len() >= 2;
    return current_.len() >= 3;
}

void Canvas::mouseDoubleClickEvent(QMouseEvent *event) {
    if (double_click_ != "close") {
        return;
    }

    if (canCloseShape()) {
        finalise();
    }
}

void Canvas::selectShapes(const QList<TlShape> &shapes) {
    setHiding();

    QList<int32_t> indexs;
    for (auto &shape : shapes) {
        indexs.push_back(shapes_.indexOf(shape));
    }
    emit selectionChanged(indexs);
    update();
}

void Canvas::selectShapePoint(const QPointF &point, bool multiple_selection_mode) {
    // Select the first shape created which contains this point.
    if (hVertex_ != None) {
        //assert self.hShape is not None
        shapes_[hShape_].highlightVertex(hVertex_, TlShape::MOVE_VERTEX);
    } else {
        //shape: Shape
        for (int32_t idx = shapes_.size() - 1; idx >= 0; --idx) {
            auto &shape = shapes_[idx];
            if (isVisible(shape) && shape.containsPoint(point)) {
                setHiding();
                if (!selectedShapes_.contains(idx)) {
                    if (multiple_selection_mode) {
                        auto select_shapes = selectedShapes_;
                        select_shapes.append(idx);
                        emit selectionChanged(select_shapes);
                    } else {
                        emit selectionChanged({idx});
                    }
                    hShapeIsSelected_ = false;
                } else {
                    hShapeIsSelected_ = true;
                }
                calculateOffsets(point);
                return;
            }
        }
    }
    if (deSelectShape())
        update();
}

void Canvas::calculateOffsets(const QPointF &point) {
    auto left = pixmap_.width() - 1;
    auto right = 0;
    auto top = pixmap_.height() - 1;
    auto bottom = 0;
    for (const auto &idx : selectedShapes_) {
        auto rect = shapes_[idx].boundingRect();
        if (rect.left() < left) {
            left = rect.left();
        }
        if (rect.right() > right) {
            right = rect.right();
        }
        if (rect.top() < top) {
            top = rect.top();
        }
        if (rect.bottom() > bottom) {
            bottom = rect.bottom();
        }
    }
    auto x1 = left - point.x();
    auto y1 = top - point.y();
    auto x2 = right - point.x();
    auto y2 = bottom - point.y();
    offsets_ = { QPointF(x1, y1), QPointF(x2, y2) };
}

void Canvas::boundedMoveVertex(QPointF pos, bool is_shift_pressed) {
    if (hVertex_ == None) {
        SPDLOG_WARN("hVertex is None, so cannot move vertex: pos={}", pos);
        return;
    }

    if (outOfPixmap(pos)) {
        pos = intersectionPoint(shapes_[hShape_][hVertex_], pos);
    }
    if (is_shift_pressed && shapes_[hShape_].shape_type() == "rectangle")
        pos = snap_cursor_pos_for_square(
            pos, shapes_[hShape_][1 - hVertex_]
        );

    shapes_[hShape_].moveVertex(hVertex_, pos);
}

bool Canvas::boundedMoveShapes(QList<TlShape> &shapes, const QList<int32_t> &indexes, QPointF pos) {
    if (outOfPixmap(pos)) {
        return false;  // No need to move
    }
    auto o1 = pos + offsets_[0];
    if (outOfPixmap(o1)) {
        pos -= QPointF(std::min(0., o1.x()), std::min(0., o1.y()));
    }
    auto o2 = pos + offsets_[1];
    if (outOfPixmap(o2)) {
        pos += QPointF(
            std::min(0., pixmap_.width() - o2.x()),
            std::min(0., pixmap_.height() - o2.y())
        );
    }
    // XXX: The next line tracks the new position of the cursor
    // relative to the shape, but also results in making it
    // a bit "shaky" when nearing the border and allows it to
    // go outside of the shape's area for some reason.
    // self.calculateOffsets(self.selectedShapes, pos)
    auto dp = pos - prevPoint_;
    if (!dp.isNull()) {
        for (auto idx : indexes) {
            shapes[idx].moveBy(dp);
        }
        prevPoint_ = pos;
        return true;
    }
    return false;
}

bool Canvas::deSelectShape() {
    bool need_update = false;
    if (!selectedShapes_.empty()) {
        setHiding(false);
        emit selectionChanged({});
        hShapeIsSelected_ = false;
        need_update = true;
    }
    return need_update;
}

QList<TlShape> Canvas::deleteSelected() {
    QList<TlShape> deleted_shapes = {};
    if (!selectedShapes_.empty()) {
        for (const auto &index : selectedShapes_) {
            deleted_shapes.push_back(shapes_[index]);
        }
        for (auto &shape : deleted_shapes) {
            SPDLOG_INFO("deleteSelected, removeOne: {}", shape.label_);
            shapes_.removeOne(shape);
        }
        storeShapes();
        selectedShapes_ = {};
        update();
    }
    return deleted_shapes;
}

void Canvas::deleteShape(const TlShape &shape) {
    const auto idx = shapes_.indexOf(shape);
    if (selectedShapes_.count(idx)) {
        selectedShapes_.removeOne(idx);
    }
    if (shapes_.contains(shape)) {
        shapes_.removeOne(shape);
    }
    storeShapes();
    update();
}

void Canvas::paintEvent(QPaintEvent *event) {
    if (pixmap_.isNull()) {
        QWidget::paintEvent(event);
        return;
    }

    auto &p = painter_;
    p.begin(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.scale(scale_, scale_);
    p.translate(offsetToCenter());

    p.drawPixmap(0, 0, pixmap_);

    p.scale(1 / scale_, 1 / scale_);

    // draw crosshair
    if (
        crosshair_[createMode_] &&
        drawing() &&
        !prevMovePoint_.isNull() &&
        !outOfPixmap(prevMovePoint_)) {
        p.setPen(QColor(0, 0, 0));
        p.drawLine(
            0,
            (prevMovePoint_.y() * scale_),
            this->width() - 1,
            (prevMovePoint_.y() * scale_)
        );
        p.drawLine(
            (prevMovePoint_.x() * scale_),
            0,
            (prevMovePoint_.x() * scale_),
            this->height() - 1
        );
    }

    TlShape::scale = scale_;
    TlShape::scale_ = scale_;
    for (int32_t idx = 0; idx < shapes_.size(); ++idx) {
        auto &shape = shapes_[idx];
        if ((shape.selected_ || !hideBackround_) && isVisible(shape)) {
            shape.fill_ = (shape.selected_ || idx == hShape_);
            shape.paint(p);
        }
    }

    if (current_) {
        current_.paint(p);
        assert(line_.points_.size() == line_.point_labels_.size());
        line_.paint(p);
    }

    if (!selectedShapesCopy_.empty()) {
        for (auto &s : selectedShapesCopy_) {
            s.paint(p);
        }
    }

    if (!current_ || !QKey{
        "polygon",
        "ai_polygon",
        "ai_mask"}.contains(createMode_)) {
        p.end();
        return;
    }

    auto drawing_shape = current_.copy();
    if (createMode_ == "polygon") {
        if (fillDrawing() && current_.points_.size() >= 2) {
            //assert drawing_shape.fill_color is not None
            if (drawing_shape.fill_color_.alpha() == 0) {
                SPDLOG_WARN(
                    "fill_drawing=true, but fill_color is transparent,"
                    " so forcing to be opaque."
                );
                drawing_shape.fill_color_.setAlpha(64);
            }
            drawing_shape.addPoint(line_[1]);
        }
    } else if (QKey{"ai_polygon", "ai_mask"}.contains(createMode_)) {
        drawing_shape.addPoint(
            line_.points_[1],
            line_.point_labels_[1]
        );
        update_shape_with_ai(
            drawing_shape.points_,
            drawing_shape.point_labels_,
            drawing_shape);
    }
    drawing_shape.fill_ = fillDrawing();
    drawing_shape.selected_ = fillDrawing();
    drawing_shape.paint(p);
    p.end();
}

QPointF Canvas::transformPos(QPointF point) {
    // Convert from widget-logical coordinates to painter-logical ones.
    return point / scale_ - offsetToCenter();
}

void Canvas::enableDragging(bool enabled) {
    is_dragging_enabled_ = enabled;
}

QPointF Canvas::offsetToCenter() {
    auto s = scale_;
    auto area = QWidget::size();
    float w = pixmap_.width() * s; float h = pixmap_.height() * s;
    float aw = area.width(); float ah = area.height();
    float x = (aw > w) ? ((aw - w) / (2 * s)) : 0.;
    float y = (ah > h) ? ((ah - h) / (2 * s)) : 0.;
    return QPointF(x, y);
}

bool Canvas::outOfPixmap(const QPointF &p) {
    auto w = pixmap_.width();
    auto h = pixmap_.height();
    return !(0 <= p.x() && p.x() <= w && 0 <= p.y() && p.y() <= h);
}

void Canvas::finalise() {
    assert(current_);
    if (QKey{"ai_polygon", "ai_mask"}.contains(createMode_)) {
        update_shape_with_ai(
            current_.points_,
            current_.point_labels_,
            current_
        );
    }
    current_.close();

    shapes_.append(current_);
    storeShapes();
    current_.clear();
    setHiding(false);
    emit newShape();
    update();
}

bool Canvas::closeEnough(const QPointF &p1, const QPointF &p2) {
    // d = distance(p1 - p2)
    // m = (p1-p2).manhattanLength()
    // print "d %.2f, m %d, %.2f" % (d, m, d - m)
    // divide by scale to allow more precision when zoomed in
    return TlUtils::distance(p1 - p2) < (epsilon_ / scale_);
}

QPointF Canvas::intersectionPoint(const QPointF &p1, const QPointF &p2) {
    // Cycle through each image edge in clockwise fashion,
    // and find the one intersecting the current line segment.
    // http://paulbourke.net/geometry/lineline2d/
    auto size = pixmap_.size();
    const std::vector<QPointF> points = {
        {0., 0.},
        {size.width() - 0., 0.},
        {size.width() - 0., size.height() - 0.},
        {0., size.height() - 0.},
    };
    // x1, y1 should be in the pixmap, x2, y2 should be out of the pixmap
    auto x1 = std::min(std::max(p1.x(), 0.), size.width() * 1.);
    auto y1 = std::min(std::max(p1.y(), 0.), size.height() * 1.);
    //d, i, (x, y) = min(self.intersectingEdges((x1, y1), (x2, y2), points))
    const auto result = intersectingEdges(QPointF(x1, y1), p2, points);
    if (result.empty()) {   // 无交点 -- 调用前判断过, 这里肯定是有交点的.
        return QPointF(-1, -1);
    }
    const auto minVal = *std::ranges::min_element(result, [](const auto &a, const auto &b) {
        return std::get<0>(a) < std::get<0>(b);
    });
    const auto d = std::get<0>(minVal);
    const auto i = std::get<1>(minVal);
    const auto x = std::get<2>(minVal).x(), y = std::get<2>(minVal).y();

    const auto x3 = points[i].x(), y3 = points[i].y();
    const auto x4 = points[(i+1)%4].x(), y4 = points[(i+1)%4].y();
    if ((x, y) == (x1, y1)) {
        // Handle cases where previous point is on one of the edges.
        if (x3 == x4) {
            return QPointF(x3, std::min(std::max(0., p2.y()), std::max(y3, y4)));
        } else {  // y3 == y4
            return QPointF(std::min(std::max(0., p2.x()), std::max(x3, x4)), y3);
        }
    }
    return QPointF(x, y);
}

std::vector<std::tuple<qreal, int32_t, QPointF>> Canvas::intersectingEdges(const QPointF &point1, const QPointF &point2, const std::vector<QPointF> &points) {
    std::vector<std::tuple<qreal, int32_t, QPointF>> results;
    //Find intersecting edges.
    //
    // For each edge formed by `points', yield the intersection
    // with the line segment `(x1,y1) - (x2,y2)`, if it exists.
    // Also return the distance of `(x2,y2)' to the middle of the
    // edge along with its index, so that the one closest can be chosen.
    //
    const auto x1 = point1.x(), y1 = point1.y();
    const auto x2 = point2.x(), y2 = point2.y();
    for (int32_t i = 0; i < 4; ++i) {
        const auto x3 = points[i].x(), y3 = points[i].y();
        const auto x4 = points[(i+1)%4].x(), y4 = points[(i+1)%4].y();
        const auto denom = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
        const auto nua = (x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3);
        const auto nub = (x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3);
        if (abs(denom) < 1e-9) { // 平行或重合
            // This covers two cases:
            //   nua == nub == 0: Coincident
            //   otherwise: Parallel
            continue;
        }
        const auto ua = nua / denom, ub = nub / denom;
        if ((0 <= ua && ua <= 1) && (0 <= ub && ub <= 1)) {     // 验证交点有效性
            const auto x = x1 + ua * (x2 - x1);
            const auto y = y1 + ua * (y2 - y1);
            const auto m = QPointF((x3 + x4) / 2, (y3 + y4) / 2);
            const auto d = TlUtils::distance(m - QPointF(x2, y2));
            //yield d, i, (x, y);
            results.emplace_back(std::make_tuple(d, i, QPointF(x,y)));
        }
    }
    return results;
}

// These two, along with a call to adjustSize are required for the
// scroll area.
QSize Canvas::sizeHint() const {
    return minimumSizeHint();
}

QSize Canvas::minimumSizeHint() const {
    if (pixmap_.isNull()) {
        return QWidget::minimumSizeHint();
    }

    QSize min_size = scale_ * pixmap_.size();
    if (is_dragging_enabled_) {
        // When drag buffer should be enabled, add a bit of buffer around the image
        // This lets dragging the image around have a bit of give on the edges
        min_size = 1.167 * min_size;
    }
    return min_size;
}

void Canvas::wheelEvent(QWheelEvent *event) {
    Qt::KeyboardModifiers mods = event->modifiers();
    QPoint delta = event->angleDelta();
    if (event->modifiers() == Qt::ControlModifier) {
        // Ctrl + 滚轮向上滚动, 放大
        // Ctrl + 滚轮向下滚动, 缩小
        emit zoomRequest(delta.y(), event->position());
    } else {
        // 滚轮向上滚动, 上移
        // 滚轮向下滚动, 下移
        emit scrollRequest(delta.x(), Qt::Horizontal);
        emit scrollRequest(delta.y(), Qt::Vertical);
    }

    event->accept();
}

void Canvas::moveByKeyboard(QPointF offset) {
    if (!selectedShapes_.empty()) {
        boundedMoveShapes(shapes_, selectedShapes_, prevPoint_ + offset);
        repaint();
        movingShape_ = true;
    }
}

void Canvas::keyPressEvent(QKeyEvent *event) {
    const auto modifiers = event->modifiers();
    const auto key = event->key();
    if (drawing()) {
        if (key == Qt::Key_Escape && current_) {
            current_.clear();
            emit drawingPolygon(false);
            update();
        } else if ((key == Qt::Key_Return || key == Qt::Key_Space) && canCloseShape()) {
            finalise();
        } else if (modifiers == Qt::AltModifier) {
            snapping_ = false;
        }
    } else if (editing()) {
        if (key == Qt::Key_Up) {
            moveByKeyboard(QPointF(0.0, -MOVE_SPEED));
        } else if (key == Qt::Key_Down) {
            moveByKeyboard(QPointF(0.0, MOVE_SPEED));
        } else if (key == Qt::Key_Left) {
            moveByKeyboard(QPointF(-MOVE_SPEED, 0.0));
        } else if (key == Qt::Key_Right) {
            moveByKeyboard(QPointF(MOVE_SPEED, 0.0));
        }
    }
    update_status({});
}

void Canvas::keyReleaseEvent(QKeyEvent *event) {
    const auto modifiers = event->modifiers();
    if (drawing()) {
        if (modifiers == Qt::NoModifier) {
            snapping_ = true;
        }
    } else if (editing()) {
        if (movingShape_ && !selectedShapes_.empty()) {
            auto index = selectedShapes_[0];
            if (shapesBackups_.back()[index].points_ != shapes_[index].points_) {
                storeShapes();
                emit shapeMoved();
            }

            movingShape_ = false;
        }
    }
}

TlShape &Canvas::setLastLabel(const QString &text, const QMap<QString, bool> &flags) {
    assert(text);
    shapes_.back().label_ = text;
    shapes_.back().flags_ = flags;
    shapesBackups_.pop_back();
    storeShapes();
    return shapes_.back();
}

void Canvas::undoLastLine() {
    assert(self.shapes);
    current_ = shapes_.back();
    shapes_.pop_back();
    current_.setOpen();
    current_.restoreShapeRaw();
    if (QKey{"polygon", "linestrip"}.contains(createMode_)) {
        line_.points_ = { current_[-1], current_[0] };
    } else if (QKey{"rectangle", "line", "circle"}.contains(createMode_)) {
        current_.points_ = { current_.points_[0], current_.points_[1] };
    } else if (createMode_ == "point") {
        current_.clear();
    }
    emit drawingPolygon(true);
}

void Canvas::undoLastPoint() {
    if (!current_ || current_.isClosed()) {
        return;
    }
    current_.popPoint();
    if (current_.len() > 0) {
        line_[0] = current_[-1];
    } else {
        current_.clear();
        emit drawingPolygon(false);
    }
    update();
}

void Canvas::loadPixmap(const QPixmap &pixmap, const QString &filename, bool clear_shapes) {
    pixmap_ = pixmap;
    pixmap_hash_ = std::hash<QString>{}(filename);

    if (clear_shapes) {
         shapes_.clear();
    }
    update();
}

void Canvas::loadShapes(const QList<TlShape> &shapes, bool replace) {
    if (replace) {
        shapes_ = shapes;
    } else {
        shapes_.append(shapes);
    }
    storeShapes();
    current_.clear();
    hShape_ = None;
    hVertex_ = None;
    hEdge_ = None;
    update();
}

void Canvas::setShapeVisible(const TlShape &shape, bool value) {
    visible_[shape.key()] = value;
    update();
}

void Canvas::overrideCursor(const QCursor &cursor) {
    if (cursor_ == cursor) {
        return;
    }
    restoreCursor();
    cursor_ = cursor;
    QApplication::setOverrideCursor(cursor);
}

void Canvas::restoreCursor() {
    this->cursor_ = Qt::ArrowCursor;
    QApplication::restoreOverrideCursor();
}

void Canvas::resetState() {
    this->restoreCursor();
    this->pixmap_ = QPixmap();
    this->pixmap_hash_ = None;
    this->shapes_.clear();
    this->shapesBackups_.clear();
    this->movingShape_ = false;
    this->selectedShapes_.clear();
    this->selectedShapesCopy_.clear();
    this->current_.clear();
    this->hShape_ = None;
    this->lasthShape_ = None;
    this->hVertex_ = None;
    this->lasthVertex_ = None;
    this->hEdge_ = None;
    this->lasthEdge_ = None;
    this->update();
}

void Canvas::update_shape_with_ai_response(const GenerateResponse &response, TlShape &shape, const std::string &createMode) {
    if (!std::set<std::string>{"ai_polygon", "ai_mask"}.contains(createMode)) {
        throw std::logic_error("createMode must be 'ai_polygon' or 'ai_mask', not " + createMode);
    }

    if (response.annotations.empty()) {
        SPDLOG_WARN("No annotations returned");
        return;
    }

    if (createMode_ == "ai_mask") {
        int32_t y1;
        int32_t x1;
        int32_t y2;
        int32_t x2;
        if (response.annotations[0].bbox.isNone()) {
            const cv::Rect bbox = TlUtils::masks_to_bboxes(response.annotations[0].mask);
            x1 = bbox.x; y1 = bbox.y;
            x2 = bbox.x + bbox.width; y2 = bbox.y + bbox.height;
        } else {
            y1 = response.annotations[0].bbox.y1;
            x1 = response.annotations[0].bbox.x1;
            y2 = response.annotations[0].bbox.y2;
            x2 = response.annotations[0].bbox.x2;
        }
        shape.setShapeRefined(
            "mask",
            {QPointF(x1, y1), QPointF(x2, y2)},
            {1, 1},
            response.annotations[0].mask(cv::Rect(x1, y1, x2-x1, y2-y1)).clone()
        );
    } else if (createMode_ == "ai_polygon") {
        auto points = compute_polygon_from_mask(
            response.annotations[0].mask
        );
        if (points.size() < 2) {
            return;
        }

        QList<QPointF> point_coords;
        point_coords.reserve(points.size());
        std::ranges::transform(points, std::back_inserter(point_coords), [](const auto &v) { return QPointF(v.x, v.y); });

        QList<int32_t> point_labels(points.size(), 1);

        shape.setShapeRefined(
            "polygon",
            point_coords,
            point_labels,
            cv::Mat()
        );
    }
}

QPointF Canvas::snap_cursor_pos_for_square(QPointF pos, QPointF opposite_vertex) {
    QPointF pos_from_opposite = pos - opposite_vertex;
    float square_size = std::min(abs(pos_from_opposite.x()), abs(pos_from_opposite.y()));
    return opposite_vertex + QPointF(
        np::sign(pos_from_opposite.x()) * square_size,
        np::sign(pos_from_opposite.y()) * square_size
    );
}