#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <random>
#include <ranges>

#include <QFileDialog>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>
#include <QDockWidget>
#include <QAction>
#include <QMenuBar>
#include <QScrollBar>
#include <QStatusBar>
#include <QMouseEvent>
#include <QWidgetAction>
#include <QMessageBox>
#include <QMimeData>
#include <QImageReader>
#include <QDirIterator>
#include <QTimer>
#include <QApplication>
#include <QCoreApplication>

#include "config/app_config.h"
#include "config/tl_yaml_config.h"
#include "tl_widgets/tl_utils.h"
#include "tl_widgets/tl_tool_bar.h"
#include "tl_widgets/tl_file_dialog.h"
#include "tl_widgets/tl_brightness.h"
#include "tl_widgets/status_stats.h"
#include "tl_modules/sam_apis.h"
#include "tl_modules/bbox_from_text.h"


std::vector<QColor> label_colormap() {
    std::vector<QColor> colormap(256);
    for (int i = 0; i < 256; ++i) {
        // 提取标签i的8个二进制位
        const uint8_t b0 = (i >> 0) & 1;
        const uint8_t b1 = (i >> 1) & 1;
        const uint8_t b2 = (i >> 2) & 1;
        const uint8_t b3 = (i >> 3) & 1;
        const uint8_t b4 = (i >> 4) & 1;
        const uint8_t b5 = (i >> 5) & 1;
        const uint8_t b6 = (i >> 6) & 1;
        const uint8_t b7 = (i >> 7) & 1;
        // 合成RGB通道色彩值.
        const uint8_t r = (b0 << 7) | (b3 << 6) | (b6 << 5);
        const uint8_t g = (b1 << 7) | (b4 << 6) | (b7 << 5);
        const uint8_t b = (b2 << 7) | (b5 << 6);
        colormap[i] = QColor(r, g, b);
    }
    return colormap;
}
std::vector<QColor> LABEL_COLORMAP = label_colormap();


const QMap<QString, QString> AI_TEXT_TO_ANNOTATION_CREATE_MODE_TO_SHAPE_TYPE {
    { "ai_mask",    "mask"},
    { "ai_polygon", "polygon"},
    { "polygon",    "polygon"},
    { "rectangle",  "rectangle"},
};


MainWindow::MainWindow(const QString &config_file,
                       const YAML::Node &config_overrides,
                       const QString &file_name,
                       const QString &output_dir)
    : QMainWindow(), ui_(new Ui::MainWindow), settings_("tl_assistant", "tl_assistant") {
    ui_->setupUi(this);
    this->setWindowTitle(qAppName());

    AppConfig &appConfig = AppConfig::instance();
    this->config_file_ = this->load_config(config_file, config_overrides);

    // set default shape colors
    TlShape::line_color = YAML_COLOR(config_["shape"]["line_color"]);
    TlShape::fill_color = YAML_COLOR(config_["shape"]["fill_color"]);
    TlShape::select_line_color  = YAML_COLOR(
        config_["shape"]["select_line_color"]
    );
    TlShape::select_fill_color  = YAML_COLOR(
        config_["shape"]["select_fill_color"]
    );
    TlShape::vertex_fill_color  = YAML_COLOR(
        config_["shape"]["vertex_fill_color"]
    );
    TlShape::hvertex_fill_color = YAML_COLOR(
        config_["shape"]["hvertex_fill_color"]
    );

    // Set point size from config file
    TlShape::point_size = config_["shape"]["point_size"].as<int32_t>();

    this->copied_shapes_ = {};

    this->label_dialog_ = new LabelDialog(
        this,
        YAML_VSTR(config_["labels"]),
        config_["sort_labels"].as<bool>(),
        config_["show_label_text_field"].as<bool>(),
        YAML_QSTR(config_["label_completion"]),
        YAML_QMAP(config_["fit_to_content"]),
        YAML_QMAP(config_["label_flags"])
    );

    this->prev_opened_dir_ = QString::fromStdString(appConfig.last_work_dir_);
    this->setup_dock_widgets();

    this->setAcceptDrops(true);
    this->setup_canvas();

    this->setup_actions();
    this->scalers_ = {
        { ZoomMode::FIT_WINDOW, [this]() { return scaleFitWindow(); } },
        { ZoomMode::FIT_WIDTH, [this]() { return scaleFitWidth(); } },
        { ZoomMode::MANUAL_ZOOM, []() { return 1.; } }
    };
    this->setup_menus();

    ai_assisted_annotation_widget_ = new AiAssistAnnotation(appConfig.ai_assist_name_, [this](const std::string &name){ this->canvas_->set_ai_model_name(name); }, this);
    ai_assisted_annotation_widget_->setEnabled(false);

    ai_text_to_annotation_widget_ = new AiPromptAnnotation(appConfig.ai_prompt_name_, [this](){ submit_ai_prompt(); }, this);
    ai_text_to_annotation_widget_->setEnabled(false);

    this->setup_toolbars();

    this->setup_status_bar();

    this->setup_app_state(output_dir, file_name);

    this->updateFileMenu();

    QObject::connect(zoom_widget_, &ZoomWidget::valueChanged, this, &MainWindow::paint_canvas);

    this->populateModeActions();
}

void MainWindow::setup_actions() {
    const auto action = [this](const QString &text, auto slot, const QList<QString> &shortcut={}, const QString &file="", const QString &tip="", bool checkable=false, bool enabled=true, bool checked=false) {
        auto *a = TlUtils::newAction(text, shortcut, file, tip, checkable, enabled, checked);
        QObject::connect(a, &QAction::triggered, this, slot);
        return a;
    };
    const auto shortcuts = [this](const std::string &key) { return YAML_KEYS(config_["shortcuts"][key]); };

    about_ = action(
        tr("&About"), &MainWindow::about, {}, ":/icons/question.svg", tr("Show about page"), false, true, false
    );
    save_ = action(
        tr("&Save\n"), &MainWindow::saveFile, shortcuts("save"), ":/icons/floppy-disk.svg", tr("Save labels to file"), false, false, false
    );
    save_as_ = action(
        tr("&Save As"), &MainWindow::saveFileAs, shortcuts("save_as"), ":/icons/floppy-disk.svg", tr("Save labels to a different file"), false, false, false
    );
    save_auto_ = action(
        tr("Save &Automatically"), [this](auto x) { save_auto_->setChecked(x); }, {}, ":/icons/save1.svg", tr("Save automatically"), true, true, false
    );
    save_auto_->setChecked(config_["auto_save"].as<bool>());
    save_with_image_data_ = action(
        tr("Save With Image Data"), &MainWindow::enableSaveImageWithData, {}, ":/icons/icon-256.png", tr("Save image data in label file"), true, true, false
    );
    change_output_dir_ = action(
        tr("&Change Output Dir"), &MainWindow::changeOutputDirDialog, shortcuts("save_to"), ":/icons/folders.svg", tr("Change where annotations are loaded/saved"), false, true, false
    );
    open_ = action(
        tr("&Open\n"), &MainWindow::open_file_with_dialog, shortcuts("open"), ":/icons/folder-open.svg", tr("Open image or label file"), false, true, false
    );
    open_dir_ = action(
        tr("Open Dir"), [this]() { open_dir_with_dialog(); }, shortcuts("open_dir"), ":/icons/folder-open.svg", tr("Open Dir"), false, true, false
    );
    close_ = action(
        tr("&Close"), &MainWindow::closeFile, shortcuts("close"), ":/icons/x-circle.svg", tr("Close current file"), false, true, false
    );
    delete_file_ = action(
        tr("&Delete File"), &MainWindow::deleteFile, shortcuts("delete_file"), ":/icons/file-x.svg", tr("Delete current label file"), false, false, false
    );
    toggle_keep_prev_mode_ = action(
        tr("Keep Previous Annotation"), &MainWindow::toggleKeepPrevMode, shortcuts("toggle_keep_prev_mode"), ":/icons/icon-256.png", tr("Toggle \"keep previous annotation\" mode"), true, true, false
    );
    toggle_keep_prev_mode_->setChecked(config_["keep_prev"].as<bool>());
    toggle_keep_prev_brightness_contrast_ = action(
        tr("Keep Previous Brightness/Contrast"),
        [this]() { config_["keep_prev_brightness_contrast"] = !config_["keep_prev_brightness_contrast"].as<bool>(); },
        {}, ":/icons/question.svg", tr("Show about page"), true, true,
        config_["keep_prev_brightness_contrast"].as<bool>()
    );
    delete_ = action(
        tr("Delete Shapes"), &MainWindow::deleteSelectedShape, shortcuts("delete_shape"), ":/icons/trash.svg", tr("Delete the selected shapes"), false, false, false
    );
    edit_ = action(
        tr("&Edit Label"), &MainWindow::edit_label, shortcuts("edit_label"), ":/icons/note-pencil.svg", tr("Modify the label of the selected shape"), false, false, false
    );
    duplicate_ = action(
        tr("Duplicate Shapes"), &MainWindow::duplicateSelectedShape, shortcuts("duplicate_shape"), ":/icons/copy.svg", tr("Create a duplicate of the selected shapes"), false, false, false
    );
    copy_ = action(
        tr("Copy Shapes"), &MainWindow::copySelectedShape, shortcuts("copy_shape"), ":/icons/copy_clipboard.svg", tr("Copy selected shapes to clipboard"), false, false, false
    );
    paste_ = action(
        tr("Paste Shapes"), &MainWindow::pasteSelectedShape, shortcuts("paste_shape"), ":/icons/paste.svg", tr("Paste copied shapes"), false, false, false
    );
    undo_last_point_ = action(
        tr("Undo last point"), &Canvas::undoLastPoint, shortcuts("undo_last_point"), ":/icons/arrow-u-up-left.svg", tr("Undo last drawn point"), false, false, false
    );
    undo_ = action(
        tr("Undo\n"), &MainWindow::undoShapeEdit, shortcuts("undo"), ":/icons/arrow-u-up-left.svg", tr("Undo last add and edit of shape"), false, false, false
    );
    remove_point_ = action(
        tr("Remove Selected Point"), &MainWindow::removeSelectedPoint, shortcuts("remove_selected_point"), ":/icons/trash.svg", tr("Remove selected point from polygon"), false, false, false
    );
    create_mode_ = action(
        tr("Create Polygons"), [this]() { this->switch_canvas_mode(false, "polygon"); }, shortcuts("create_polygon"), ":/icons/polygon.svg", tr("Start drawing polygons"), false, false, false
    );
    edit_mode_ = action(
        tr("Edit Shapes"), [this]() { this->switch_canvas_mode(true); }, shortcuts("edit_shape"), ":/icons/note-pencil.svg", tr("Move and edit the selected shapes"), false, false, false
    );
    create_rectangle_mode_ = action(
        tr("Create Rectangle"), [this]() { this->switch_canvas_mode(false, "rectangle"); }, shortcuts("create_rectangle"), ":/icons/rectangle.svg", tr("Start drawing rectangles"), false, false, false
    );
    create_circle_mode_ = action(
        tr("Create Circle"), [this]() { this->switch_canvas_mode(false, "circle"); }, shortcuts("create_circle"), ":/icons/circle.svg", tr("Start drawing circles"), false, false, false
    );
    create_line_mode_ = action(
        tr("Create Line"), [this]() { this->switch_canvas_mode(false, "line"); }, shortcuts("create_line"), ":/icons/line-segment.svg", tr("Start drawing lines"), false, false, false
    );
    create_point_mode_ = action(
        tr("Create Point"), [this]() { this->switch_canvas_mode(false, "point"); }, shortcuts("create_point"), ":/icons/circles-four.svg", tr("Start drawing points"), false, false, false
    );
    create_line_strip_mode_ = action(
        tr("Create LineStrip"), [this]() { this->switch_canvas_mode(false, "linestrip"); }, shortcuts("create_linestrip"), ":/icons/line-segments.svg", tr("Start drawing linestrip. Ctrl+LeftClick ends creation."), false, false, false
    );
    create_ai_polygon_mode_ = action(
        tr("Create AI-Polygon"), [this]() { this->switch_canvas_mode(false, "ai_polygon"); }, shortcuts("create_ai_polygon"), ":/icons/ai-polygon.svg", tr("Start drawing ai_polygon. Ctrl+LeftClick ends creation."), false, false, false
    );
    create_ai_mask_mode_ = action(
        tr("Create AI-Mask"), [this]() { this->switch_canvas_mode(false, "ai_mask"); }, shortcuts("create_ai_mask"), ":/icons/ai-mask.svg", tr("Start drawing ai_mask. Ctrl+LeftClick ends creation."), false, false, false
    );
    open_next_img_ = action(
        tr("&Next Image"), &MainWindow::open_next_image, shortcuts("open_next"), ":/icons/arrow-fat-right.svg", tr("Open next (hold Ctl+Shift to copy labels)"), false, false, false
    );
    open_prev_img_ = action(
        tr("&Prev Image"), &MainWindow::open_prev_image, shortcuts("open_prev"), ":/icons/arrow-fat-left.svg", tr("Open prev (hold Ctl+Shift to copy labels)"), false, false, false
    );
    keep_prev_scale_ = action(
        tr("&Keep Previous Scale"), &MainWindow::enableKeepPrevScale, {}, ":/icons/icon-256.png", tr("Keep previous zoom scale"), true, true, false
    );
    fit_window_ = action(
        tr("&Fit Window"), &MainWindow::setFitWindow, shortcuts("fit_window"), ":/icons/frame-corners.svg", tr("Zoom follows window size"), true, false, false
    );
    fit_width_ = action(
        tr("Fit &Width"), &MainWindow::setFitWidth, shortcuts("fit_width"), ":/icons/frame-arrows-horizontal.svg", tr("Zoom follows window width"), true, false, false
    );
    brightness_contrast_ = action(
        tr("&Brightness Contrast"), [this]() { brightnessContrast(); }, {}, ":/icons/brightness-contrast.svg", tr("Adjust brightness and contrast"), false, false, false
    );
    zoom_in_ = action(
        tr("Zoom &In"), [this]() { add_zoom(1.1); }, shortcuts("zoom_in"), ":/icons/magnifying-glass-minus.svg", tr("Increase zoom level"), false, false, false
    );
    zoom_out_ = action(
        tr("&Zoom Out"), [this]() { add_zoom(0.9); }, shortcuts("zoom_out"), ":/icons/magnifying-glass-plus.svg", tr("Decrease zoom level"), false, false, false
    );
    zoom_org_ = action(
        tr("&Original size"), &MainWindow::set_zoom_to_original, shortcuts("zoom_to_original"), ":/icons/image-square.svg", tr("Zoom to original size"), false, false, false
    );
    reset_layout_ = action(
        tr("Reset Layout"), &MainWindow::reset_layout, {}, ":/icons/layout-duotone.svg", "", false, false, false
    );
    fill_drawing_ = action(
        tr("Fill Drawing Polygon"), &Canvas::setFillDrawing, {}, ":/icons/paint-bucket.svg", tr("Fill polygon while drawing"), true, true, false
    );
    if (config_["canvas"]["fill_drawing"].as<bool>()) {
        fill_drawing_->trigger();
    }
    hide_all_ = action(
        tr("&Hide\nShapes"), [this]() { toggleShapes(false); }, shortcuts("hide_all_shapes"), ":/icons/eye.svg", tr("Hide all shapes"), false, false, false
    );
    show_all_ = action(
        tr("&Show\nShapes"), [this]() { toggleShapes(true); }, shortcuts("show_all_shapes"), ":/icons/eye.svg", tr("Show all shapes"), false, false, false
    );
    toggle_all_ = action(
        tr("&Toggle\nShapes"), [this]() { toggleShapes(None); }, shortcuts("toggle_all_shapes"), ":/icons/eye.svg", tr("Toggle all shapes"), false, false, false
    );

    zoom_action_ = new QWidgetAction(this);
    const auto zoom_box_layout = new QVBoxLayout();
    const auto zoom_label = new QLabel(tr("Zoom"));
    zoom_label->setAlignment(Qt::AlignCenter);
    zoom_box_layout->addWidget(zoom_label);
    zoom_box_layout->addWidget(zoom_widget_);
    zoom_action_->setDefaultWidget(new QWidget());
    zoom_action_->defaultWidget()->setLayout(zoom_box_layout);
    zoom_widget_->setWhatsThis(
        QString(
            tr(
                "Zoom in or out of the image. Also accessible with "
                "%1 %2 and %3 from the canvas."
            )
        ).arg(
            TlUtils::fmtShortcut(shortcuts("zoom_in")), TlUtils::fmtShortcut(shortcuts("zoom_out")),
            tr("Ctrl+Wheel")
        )
    );
    zoom_widget_->setEnabled(false);

    this->zoom_mode_ = ZoomMode::FIT_WINDOW;
    fit_window_->setChecked(true);

    QObject::connect(canvas_, &Canvas::vertexSelected, [this](bool value){ remove_point_->setEnabled(value); });

    draw_actions_ = {
        {"polygon",     create_mode_},
        {"rectangle",   create_rectangle_mode_},
        {"circle",      create_circle_mode_},
        {"point",       create_point_mode_},
        {"line",        create_line_mode_},
        {"linestrip",   create_line_strip_mode_},
        {"ai_polygon",  create_ai_polygon_mode_},
        {"ai_mask",     create_ai_mask_mode_},
    };

    zoom_actions_ = {
        //zoom_widget_,
        zoom_in_,
        zoom_out_,
        zoom_org_,
        fit_window_,
        fit_width_
    };
    on_load_active_actions_ = {
        close_,
        create_mode_,
        create_rectangle_mode_,
        create_circle_mode_,
        create_line_mode_,
        create_point_mode_,
        create_line_strip_mode_,
        create_ai_polygon_mode_,
        create_ai_mask_mode_,
        brightness_contrast_,
    };
    on_shapes_present_actions_ = {save_as_, hide_all_, show_all_, toggle_all_};
    std::list<QAction *> context_menu_actions = {
        edit_mode_,
        edit_,
        duplicate_,
        copy_,
        paste_,
        delete_,
        undo_,
        undo_last_point_,
        remove_point_,
    };
    std::ranges::for_each(draw_actions_, [this](auto &p) { context_menu_actions_.push_back(p.second); });
    std::ranges::for_each(context_menu_actions, [this](auto &a) { context_menu_actions_.push_back(a); });
    edit_menu_actions_ = {
        edit_,
        duplicate_,
        copy_,
        paste_,
        delete_,
        nullptr,
        undo_,
        undo_last_point_,
        nullptr,
        remove_point_,
        nullptr,
        toggle_keep_prev_mode_,
    };
    //return _Actions(
    //    about=about,
    //    save=save,
    //    save_as=save_as,
    //    save_auto=save_auto,
    //    save_with_image_data=save_with_image_data,
    //    change_output_dir=change_output_dir,
    //    open=open_,
    //    close=close,
    //    delete_file=delete_file,
    //    toggle_keep_prev_mode=toggle_keep_prev_mode,
    //    toggle_keep_prev_brightness_contrast=toggle_keep_prev_brightness_contrast,
    //    delete=delete,
    //    edit=edit,
    //    duplicate=duplicate,
    //    copy=copy,
    //    paste=paste,
    //    undo_last_point=undo_last_point,
    //    undo=undo,
    //    remove_point=remove_point,
    //    create_mode=create_mode,
    //    edit_mode=edit_mode,
    //    create_rectangle_mode=create_rectangle_mode,
    //    create_circle_mode=create_circle_mode,
    //    create_line_mode=create_line_mode,
    //    create_point_mode=create_point_mode,
    //    create_line_strip_mode=create_line_strip_mode,
    //    create_ai_polygon_mode=create_ai_polygon_mode,
    //    create_ai_mask_mode=create_ai_mask_mode,
    //    open_next_img=open_next_img,
    //    open_prev_img=open_prev_img,
    //    keep_prev_scale=keep_prev_scale,
    //    fit_window=fit_window,
    //    fit_width=fit_width,
    //    brightness_contrast=brightness_contrast,
    //    zoom_in=zoom_in,
    //    zoom_out=zoom_out,
    //    zoom_org=zoom_org,
    //    reset_layout=reset_layout,
    //    fill_drawing=fill_drawing,
    //    hide_all=hide_all,
    //    show_all=show_all,
    //    toggle_all=toggle_all,
    //    open_dir=open_dir,
    //    zoom_widget_action=zoom_widget_action,
    //    draw=draw,
    //    zoom=zoom,
    //    on_load_active=on_load_active,
    //    on_shapes_present=on_shapes_present,
    //    context_menu=context_menu,
    //    edit_menu=edit_menu,
    //)
}

void MainWindow::setup_menus() {
    const auto action = [this](const QString &text, auto slot, const QList<QString> &shortcut={}, const QString &file="", const QString &tip="", bool checkable=false, bool enabled=true, bool checked=false) {
        auto *a = TlUtils::newAction(text, shortcut, file, tip, checkable, enabled, checked);
        QObject::connect(a, &QAction::triggered, this, slot);
        return a;
    };
    const auto shortcuts = [this](const std::string &key) { return YAML_KEYS(config_["shortcuts"][key]); };

    quit_ = action(
        tr("&Quit"), &MainWindow::close, shortcuts("quit"), ":/icons/quit.png", tr("Quit application"), false, true, false
    );
    open_config_ = action(
        tr("Preferences…"), &MainWindow::open_config_file, {"Ctrl+Shift+,"}, ":/icons/icon-256.png", tr("Open image or label file"), false, true, false
    );
    open_config_->setMenuRole(QAction::PreferencesRole);
    help_ = action(
        tr("&Tutorial"), &MainWindow::tutorial, {}, ":/icons/question.svg", tr("Show tutorial page"), false, true, false
    );

    file_menu_ = menu(tr("&File"));
    edit_menu_ = menu(tr("&Edit"));
    view_menu_ = menu(tr("&View"));
    help_menu_ = menu(tr("&Help"));
    recent_menu_ = menu(tr("Open &Recent"));

    label_menu_ = new QMenu();
    TlUtils::addActions(label_menu_, {edit_, delete_});
    shape_list_->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(shape_list_, &ShapeListWidget::customContextMenuRequested, this, &MainWindow::popLabelListMenu);

    TlUtils::addActions(
        file_menu_,
        {
            open_,
            open_next_img_,
            open_prev_img_,
            open_dir_,
            recent_menu_,
            save_,
            save_as_,
            save_auto_,
            change_output_dir_,
            save_with_image_data_,
            close_,
            delete_file_,
            nullptr,
            open_config_,
            nullptr,
            quit_
        }
    );
    TlUtils::addActions(help_menu_, {help_, about_});
    TlUtils::addActions(
        view_menu_,
        {
            flags_dock_->toggleViewAction(),
            label_dock_->toggleViewAction(),
            shape_dock_->toggleViewAction(),
            files_dock_->toggleViewAction(),
            nullptr,
            reset_layout_,
            nullptr,
            fill_drawing_,
            nullptr,
            hide_all_,
            show_all_,
            toggle_all_,
            nullptr,
            zoom_in_,
            zoom_out_,
            zoom_org_,
            keep_prev_scale_,
            nullptr,
            fit_window_,
            fit_width_,
            nullptr,
            brightness_contrast_,
            toggle_keep_prev_brightness_contrast_,
        }
    );

    QObject::connect(file_menu_, &QMenu::aboutToShow, this, &MainWindow::updateFileMenu);

    TlUtils::addActions(
        canvas_->menus_[0], this->context_menu_actions_
    );
    TlUtils::addActions(
        canvas_->menus_[1],
        {
            action("&Copy here", [this]() { this->copyShape(); }),
            action("&Move here", [this]() { this->moveShape(); }),
        }
    );

    //return _Menus(
    //    file=file_menu,
    //    edit=edit_menu,
    //    view=view_menu,
    //    help=help_menu,
    //    recent_files=recent_files,
    //    label_list=label_menu,
    //)
}

void MainWindow::setup_toolbars() {
    select_ai_model_ = new QWidgetAction(this);
    select_ai_model_->setDefaultWidget(ai_assisted_annotation_widget_);

    ai_prompt_action_ = new QWidgetAction(this);
    ai_prompt_action_->setDefaultWidget(ai_text_to_annotation_widget_);

    this->addToolBar(
        Qt::TopToolBarArea,
        new TlToolBar(
            "Tools",
            {
                open_,
                open_dir_,
                open_prev_img_,
                open_next_img_,
                save_,
                delete_file_,
                nullptr,
                edit_mode_,
                duplicate_,
                delete_,
                undo_,
                brightness_contrast_,
                nullptr,
                fit_window_,
                zoom_action_,
                nullptr,
                select_ai_model_,
                nullptr,
                ai_prompt_action_
            },
            Qt::Horizontal,
            Qt::ToolButtonTextUnderIcon,
            this->font()
        )
    );

    std::list<QAction *> draw_actions;
    std::ranges::for_each(draw_actions_, [&](const auto &p) { draw_actions.push_back(p.second); });
    this->addToolBar(
        Qt::LeftToolBarArea,
        new TlToolBar(
            "CreateShapeTools",
            draw_actions,
            Qt::Vertical,
            Qt::ToolButtonTextUnderIcon,
            this->font()
        )
    );
}

void MainWindow::setup_app_state(const QString &output_dir, const QString &file_name) {
    this->output_dir_ = output_dir;

    this->image_;
    this->label_file_;
    this->image_path_;
    this->max_recent_ = 7;
    this->other_data_ = nullptr;
    this->zoom_values_ = {};
    this->brightness_contrast_values_ = {};
    this->scroll_values_ = {
        {Qt::Horizontal, {}},
        {Qt::Vertical, {}}
    }; // key=filename, value=scroll_value

    if (!config_["file_search"].IsNull()) {
        files_search_->setText(YAML_QSTR(config_["file_search"]));
    }

    default_state_ = saveState();
    //
    // XXX: Could be completely declarative.
    // Restore application settings.
    //settings_ = QSettings("tl_assistant", "tl_assistant");
    //
    // Bump this when dock/toolbar layout changes to reset window state
    // for users upgrading from an older version.
    int32_t SETTINGS_VERSION = 1;
    if (settings_.value("settingsVersion", 0).toInt() != SETTINGS_VERSION) {
        reset_layout();
        settings_.setValue("settingsVersion", SETTINGS_VERSION);
    }
    this->recent_files_ = this->settings_.value("recentFiles", {}).toStringList();
    this->resize(this->settings_.value("window/size", QSize(900, 500)).toSize());
    this->move(this->settings_.value("window/position", QPoint(0, 0)).toPoint());
    this->restoreState(this->settings_.value("window/state", QByteArray()).toByteArray());
    // Recover window position when the saved screen is no longer connected.
    if (std::ranges::none_of(QApplication::screens(), [this](auto &s) {
            return s->availableGeometry().intersects(this->frameGeometry());
        }) &&
        QApplication::primaryScreen() != nullptr) {
        this->move(QApplication::primaryScreen()->availableGeometry().topLeft());
    }

    if (!file_name.isEmpty()) {
        if (QFileInfo(file_name).isDir()) {
            this->import_images_from_dir(
                file_name, this->files_search_->text()
            );
            this->open_next_image();
        } else {
            this->load_file(file_name);
        }
    } else {
        filename_ = "";
    }
}

void MainWindow::setup_status_bar() {
    this->message_ = new QLabel(tr("%1 started.").arg(qAppName()));
    this->stats_ = new StatusStats();
    this->statusBar()->addWidget(message_, 1);
    this->statusBar()->addWidget(stats_, 0);
    this->statusBar()->show();
    //return _StatusBarWidgets(message=message, stats=stats)
}

void MainWindow::setup_canvas() {
    zoom_widget_ = new ZoomWidget();

    canvas_ = new Canvas(
        config_["epsilon"].as<float>(),
        YAML_QSTR(config_["canvas"]["double_click"]),
        config_["canvas"]["num_backups"].as<int32_t>(),
        YAML_QMAP(config_["canvas"]["crosshair"])
    );
    QObject::connect(canvas_, &Canvas::zoomRequest, this, &MainWindow::zoom_requested);
    QObject::connect(canvas_, &Canvas::mouseMoved, this, &MainWindow::update_status_stats);
    QObject::connect(canvas_, &Canvas::statusUpdated, [this](const auto &text) {
        this->message_->setText(text); }
    );

    scroll_area_ = new QScrollArea();
    scroll_area_->setWidget(canvas_);
    scroll_area_->setWidgetResizable(true);
    scroll_bars_ = {
        { Qt::Vertical, this->scroll_area_->verticalScrollBar() },
        { Qt::Horizontal, this->scroll_area_->horizontalScrollBar() }
    };
    QObject::connect(canvas_, &Canvas::scrollRequest, this, &MainWindow::scrollRequest);

    QObject::connect(canvas_, &Canvas::newShape, this, &MainWindow::newShape);
    QObject::connect(canvas_, &Canvas::shapeMoved, this, &MainWindow::setDirty);
    QObject::connect(canvas_, &Canvas::selectionChanged, this, &MainWindow::shapeSelectionChanged);
    QObject::connect(canvas_, &Canvas::drawingPolygon, this, &MainWindow::toggleDrawingSensitive);

    this->setCentralWidget(scroll_area_);
    //return _CanvasWidgets(
    //    canvas=canvas,
    //    zoom_widget=zoom_widget,
    //    scroll_bars=scroll_bars,
    //)
}

void MainWindow::setup_dock_widgets() {
    this->flags_list_ = new QListWidget();
    this->flags_dock_ = new QDockWidget(tr("Flags"), this);
    this->flags_dock_->setObjectName("Flags");
    if (!this->config_["flags"].IsNull()) {
        this->load_flags(this->config_["flags"], this->flags_list_);
    }
    this->flags_dock_->setWidget(this->flags_list_);
    QObject::connect(flags_list_, &QListWidget::itemChanged, this, &MainWindow::setDirty);

    this->shape_list_ =  new ShapeListWidget();   // LabelListWidget
    QObject::connect(shape_list_, &ShapeListWidget::itemSelectionChanged, this, &MainWindow::label_selection_changed);
    QObject::connect(shape_list_, &ShapeListWidget::itemDoubleClicked, this, &MainWindow::edit_label);
    QObject::connect(shape_list_, &ShapeListWidget::itemChanged, this, &MainWindow::labelItemChanged);
    QObject::connect(shape_list_, &ShapeListWidget::itemDropped, this, &MainWindow::labelOrderChanged);
    this->shape_dock_ = new QDockWidget(tr("Polygon Labels"), this);
    this->shape_dock_->setObjectName("Labels");
    this->shape_dock_->setWidget(this->shape_list_);

    this->label_list_ =  new TlLabelList();    // UniqueLabelQListWidget
    this->label_list_->setToolTip(
        tr("Select label to start annotating for it. Press 'Esc' to deselect.")
    );
    if (!config_["labels"].IsNull()) {
        for (auto &lbl : YAML_KEYS(config_["labels"]))
            this->label_list_->add_label_item(
                lbl, get_rgb_by_label(lbl)
            );
    }
    this->label_dock_ = new QDockWidget(tr("Label List"), this);
    this->label_dock_->setObjectName("Label List");
    this->label_dock_->setWidget(this->label_list_);

    files_search_ = new QLineEdit();
    files_search_->setPlaceholderText(tr("Search Filename"));
    QObject::connect(files_search_, &QLineEdit::textChanged, this, &MainWindow::fileSearchChanged);
    files_list_ = new QListWidget();
    QObject::connect(files_list_, &QListWidget::itemSelectionChanged, this, &MainWindow::fileSelectionChanged);
    auto *files_layout = new QVBoxLayout();
    files_layout->setContentsMargins(0, 0, 0, 0);
    files_layout->setSpacing(0);
    files_layout->addWidget(files_search_);
    files_layout->addWidget(files_list_);
    this->files_dock_ = new QDockWidget(tr("File List"), this);
    this->files_dock_->setObjectName("Files");
    auto *files_widget = new QWidget();
    files_widget->setLayout(files_layout);
    this->files_dock_->setWidget(files_widget);

    for (auto &[config_key, dock_widget] : std::map<std::string, QDockWidget *>{
        {"flag_dock", flags_dock_},
        {"label_dock", label_dock_},
        {"shape_dock", shape_dock_},
        {"file_dock", files_dock_}
    }) {
        auto features = QDockWidget::DockWidgetFeatures();
        if (config_[config_key]["closable"].as<bool>())
            features = features | QDockWidget::DockWidgetClosable;
        if (config_[config_key]["floatable"].as<bool>())
            features = features | QDockWidget::DockWidgetFloatable;
        if (config_[config_key]["movable"].as<bool>())
            features = features | QDockWidget::DockWidgetMovable;
        dock_widget->setFeatures(features);
        if (config_[config_key]["show"].as<bool>() == false)
            dock_widget->setVisible(false);
        this->addDockWidget(Qt::RightDockWidgetArea, dock_widget);
    }
    //return _DockWidgets(
    //    flag_dock=flag,
    //    flag_list=flag_list,
    //    shape_dock=shape,
    //    label_list=label_list,
    //    label_dock=label,
    //    unique_label_list=unique_label_list,
    //    file_dock=file,
    //    file_search=file_search,
    //    file_list=file_list,
    //)
}

QString MainWindow::load_config(
    QString config_file, const YAML::Node &config_overrides
) { // -> tuple[Path | None, dict]:
    try {
        config_ = TlConfig::load_config(
            config_file.toStdString(), config_overrides
        );
    } catch (const YAML::BadFile &e) {
        QMessageBox msg_box(this);
        msg_box.setIcon(QMessageBox::Warning);
        msg_box.setWindowTitle(this->tr("Configuration Errors"));
        msg_box.setText(
            this->tr(
                "Errors were found while loading the configuration. "
                "Please review the errors below and reload your configuration or "
                "ignore the erroneous lines."
            )
        );
        msg_box.setInformativeText(e.what());
        msg_box.setStandardButtons(QMessageBox::Ignore);
        msg_box.setModal(false);
        msg_box.show();

        config_file.clear();
        //config_overrides = {}
        config_ = TlConfig::load_config(
            config_file.toStdString(), {}
        );
    }
    return config_file; //, config
}

QMenu *MainWindow::menu(const QString &title, const std::list<QObject *> &actions) {
    auto *menu = this->menuBar()->addMenu(title);
    if (!actions.empty()) {
        TlUtils::addActions(menu, actions);
    }
    return menu;
}
// Support Functions

bool MainWindow::noShapes() {
    return shape_list_->empty();
}

void MainWindow::populateModeActions() {
    canvas_->menus_[0]->clear();
    TlUtils::addActions(
        this->canvas_->menus_[0], context_menu_actions_
    );
    this->edit_menu_->clear();
    std::list<QObject *> actions;
    std::ranges::transform(draw_actions_, std::back_inserter(actions), [](auto &it){ return it.second; });
    actions.push_back(edit_mode_);
    std::ranges::transform(edit_menu_actions_, std::back_inserter(actions), [](auto &it){ return it; });

    TlUtils::addActions(this->edit_menu_, actions);
}

QString MainWindow::get_window_title(bool dirty) {
    QString window_title = qAppName();
    if (!image_path_.isEmpty()) {
        window_title = QString("%1 - %2").arg(window_title, image_path_);
        if (files_list_->count() && files_list_->currentItem())
            window_title = QString(
                "%1 "   // window_title
                "[%2"   // {self.fileListWidget.currentRow() + 1}
                "/%3]"  // {self.fileListWidget.count()}
            ).arg(window_title).arg(files_list_->currentRow() + 1).arg(files_list_->count());
    }
    if (!this->image_.isNull()) {
        window_title = QString("%1 | %2x%3").arg(window_title).arg(image_.width()).arg(image_.height());
    }
    if (dirty) {
        window_title = window_title + "*";
    }
    return window_title;
}

void MainWindow::setDirty() {
    // Even if we autosave the file, we keep the ability to undo
    this->undo_->setEnabled(this->canvas_->isShapeRestorable());

    if (config_["auto_save"].as<bool>() || save_auto_->isChecked()) {
        std::filesystem::path file_path(image_path_.toStdString());
        auto label_file = QString::fromStdString(file_path.replace_extension("json").string());
        if (!output_dir_.isEmpty()) {
            label_file = output_dir_ + "/" + QFileInfo(label_file).baseName();
        }
        this->saveLabels(label_file);
        return;
    }
    this->is_changed_ = true;
    this->save_->setEnabled(true);
    this->setWindowTitle(get_window_title(true));
}

void MainWindow::setClean() {
    this->is_changed_ = false;
    this->save_->setEnabled(false);
    for (auto &[fst, action] : this->draw_actions_) {
        action->setEnabled(true);
    }
    this->setWindowTitle(get_window_title(false));

    if (this->hasLabelFile()) {
        this->delete_file_->setEnabled(true);
    } else {
        this->delete_file_->setEnabled(false);
    }
}

void MainWindow::toggleActions(bool value) {
    // Enable/Disable widgets which depend on an opened image.
    zoom_widget_->setEnabled(value);
    for (auto &z : zoom_actions_) {
        z->setEnabled(value);
    }
    for (auto &action : on_load_active_actions_) {
        action->setEnabled(value);
    }
}

//void MainWindow::queueEvent(function) {
//    QtCore.QTimer.singleShot(0, function)
//}

void MainWindow::show_status_message(const QString &message, int32_t delay) {
    this->statusBar()->showMessage(message, delay);
}

void MainWindow::submit_ai_prompt() {
    if (!AI_TEXT_TO_ANNOTATION_CREATE_MODE_TO_SHAPE_TYPE.contains(canvas_->createMode_)) {
        SPDLOG_WARN("Unsupported createMode={}", canvas_->createMode_);
        return;
    }
    const auto shape_type = AI_TEXT_TO_ANNOTATION_CREATE_MODE_TO_SHAPE_TYPE[canvas_->createMode_];

    const auto texts = ai_text_to_annotation_widget_->get_texts_prompt();
    if (texts.empty()) {
        return;
    }

    const auto model_name = ai_text_to_annotation_widget_->get_model_name();
    //model_type = osam.apis.get_model_type_by_name(model_name);
    //if not (_is_already_downloaded := model_type.get_size() is not None):
    //    if not download_ai_model(model_name=model_name, parent=self):
    //        return;
    if (text_osam_session_ == nullptr ||
        text_osam_session_->model_name() != model_name) {
        text_osam_session_ = std::make_unique<SamSession>(model_name);
    }

    const auto image = TlUtils::ImageToMat(image_);
    const auto image_id = std::hash<QString>{}(filename_);
    const GenerateResponse response = text_osam_session_->run(image, image_id, {}, {}, texts);
    //boxes, scores, labels, masks = bbox_from_text.get_bboxes_from_texts(
    //    session=this->_text_osam_session,
    //    image=utils.img_qt_to_arr(this->image)[:, :, :3],
    //    image_id=str(hash(this->imagePath)),
    //    texts=texts,
    //);
    std::vector<float> scores;
    std::vector<int32_t> labels;
    std::vector<cv::Rect> boxes;
    std::vector<cv::Mat> masks;
    get_bboxes_from_texts(response, scores, labels, boxes, masks);

    //SCORE_FOR_EXISTING_SHAPE: float = 1.01;
    //for shape in this->canvas.shapes:
    //    if shape.shape_type != shape_type or shape.label not in texts:
    //        continue;
    //    points: NDArray[np.float64] = np.array(
    //        [[p.x(), p.y()] for p in shape.points]
    //    );
    //    xmin, ymin = points.min(axis=0);
    //    xmax, ymax = points.max(axis=0);
    //    box = np.array([xmin, ymin, xmax, ymax], dtype=np.float32);
    //    boxes = np.r_[boxes, [box]];
    //    scores = np.r_[scores, [SCORE_FOR_EXISTING_SHAPE]];
    //    labels = np.r_[labels, [texts.index(shape.label)]];
    //
    //boxes, scores, labels, indices = bbox_from_text.nms_bboxes(
    //    boxes=boxes,
    //    scores=scores,
    //    labels=labels,
    //    iou_threshold=this->_ai_text_to_annotation_widget.get_iou_threshold(),
    //    score_threshold=this->_ai_text_to_annotation_widget.get_score_threshold(),
    //    max_num_detections=100,
    //);
    //
    //is_new = scores != SCORE_FOR_EXISTING_SHAPE;
    //boxes = boxes[is_new];
    //scores = scores[is_new];
    //labels = labels[is_new];
    //indices = indices[is_new];
    //
    //if masks is not None:
    //    masks = masks[indices]
    //del indices;
    //
    //shapes: list[Shape] = bbox_from_text.get_shapes_from_bboxes(
    //    boxes=boxes,
    //    scores=scores,
    //    labels=labels,
    //    texts=texts,
    //    masks=masks,
    //    shape_type=shape_type,
    //);
    QList<TlShape> shapes;
    for (const auto &annotation : response.annotations) {
        shapes.push_back({});
        auto &shape = shapes.back();
        shape.label_ = QString::fromStdString(texts[0]);
        shape.closed_ = true;
        if (annotation.mask.empty()) {
            shape.shape_type_ = "rectangle";
            shape.points_ = {QPointF(annotation.bbox.x1, annotation.bbox.y1), QPointF(annotation.bbox.x2, annotation.bbox.y2)};
        } else {
            shape.shape_type_ = "polygon";
            const auto x1 = annotation.bbox.x1;
            const auto y1 = annotation.bbox.y1;
            auto points = compute_polygon_from_mask(annotation.mask);
            std::ranges::transform(points, std::back_inserter(shape.points_), [x1, y1](auto &p) { return QPointF(x1+p.x, y1+p.y); });
        }
    }

    this->canvas_->storeShapes();
    this->load_shapes(shapes, false);
    this->setDirty();
}

void MainWindow::resetState() {
    this->shape_list_->clear();
    this->filename_.clear();
    this->image_path_.clear();
    this->imageData_.clear();
    this->label_file_.reset();
    this->other_data_.clear();
    this->canvas_->resetState();
}

QListWidgetItem *MainWindow::currentItem() {
    const auto items = this->label_list_->selectedItems();
    if (!items.empty()) {
        return items[0];
    }
    return nullptr;
}

void MainWindow::addRecentFile(const QString &filename) {
    if (recent_files_.contains(filename)) {
        recent_files_.removeOne(filename);
    } else if (recent_files_.count() >= max_recent_) {
        recent_files_.pop_back();
    }
    recent_files_.insert(0, filename);
}
// Callbacks

void MainWindow::undoShapeEdit() {
    this->canvas_->restoreShape();
    this->shape_list_->clear();
    this->load_shapes(this->canvas_->shapes_);
    this->undo_->setEnabled(this->canvas_->isShapeRestorable());
}

void MainWindow::tutorial() {
    //url = "https://github.com/labelmeai/labelme/tree/main/examples/tutorial"  # NOQA
    //webbrowser.open(url)
}

void MainWindow::about() {
}

void MainWindow::toggleDrawingSensitive(bool drawing) {
    //Toggle drawing sensitive.
    //
    //In the middle of drawing, toggling between modes should be disabled.
    //
    this->edit_mode_->setEnabled(!drawing);
    this->undo_last_point_->setEnabled(drawing);
    this->undo_->setEnabled(!drawing);
    this->delete_->setEnabled(!drawing);
}

void MainWindow::switch_canvas_mode(bool edit, const QString &createMode) {
    this->canvas_->setEditing(edit);
    if (!createMode.isEmpty()) {
        this->canvas_->createMode_ = createMode;
    }
    if (edit) {
        for (auto draw_action : this->draw_actions_ | std::views::values) {
            draw_action->setEnabled(true);
        }
    } else {
        for (auto &[draw_mode, draw_action] : this->draw_actions_) {
            draw_action->setEnabled(createMode != draw_mode);
        }
    }
    this->edit_mode_->setEnabled(!edit);
    this->ai_text_to_annotation_widget_->setEnabled(
        !edit && AI_TEXT_TO_ANNOTATION_CREATE_MODE_TO_SHAPE_TYPE.contains(createMode)
    );
    this->ai_assisted_annotation_widget_->setEnabled(
        !edit && std::set<QString>{"ai_polygon", "ai_mask"}.contains(createMode)
    );
}

void MainWindow::updateFileMenu() {
    const auto current = this->filename_;

    std::vector<QString> files;
    std::ranges::for_each(recent_files_, [&](auto &f) { if (f != current && QFile::exists(f)) files.push_back(f); });

    this->recent_menu_->clear();
    //files = [f for f in self.recentFiles if f != current and exists(f)]
    for (auto i = 0; i < files.size(); ++i) {
        const auto f = files[i];
        auto icon = TlUtils::newIcon("labels");
        const auto action = new QAction(
            icon, QString("&%1 %2").arg(i + 1).arg(QFileInfo(f).fileName()), this
        );
        QObject::connect(action, &QAction::triggered, this, [this, f]() { this->loadRecent(f); });
        this->recent_menu_->addAction(action);
    }
}

void MainWindow::popLabelListMenu(const QPoint &point) {
    this->label_menu_->exec(this->shape_list_->mapToGlobal(point));
}

bool MainWindow::validateLabel(const QString &label) {
    // no validation
    if (YAML_STR(config_["validate_label"]).empty()) {
        return true;
    }

    for (auto i = 0; i < label_list_->count(); ++i) {
        auto label_i = label_list_->item(i)->data(Qt::UserRole);
        if (YAML_STR(config_["validate_label"]) == "exact") {
            if (label_i == label) {
                return true;
            }
        }
    }
    return false;
}

void MainWindow::edit_label(bool value) {
    auto items = shape_list_->selectedItems();
    if (items.empty()) {
        SPDLOG_WARN("No label is selected, so cannot edit label.");
        return;
    }

    auto shape = items[0]->shape();

    bool edit_text = true;
    bool edit_flags = true;
    bool edit_group_id = true;
    bool edit_description = true;
    if (items.size() > 1) {
        edit_text = std::all_of(items.begin() + 1, items.end(), [&](const auto &item) { return item->shape().label_ == shape.label_ ; });
        edit_flags = std::all_of(items.begin() + 1, items.end(), [&](const auto &item) { return item->shape().flags_ == shape.flags_ ; });
        edit_group_id = std::all_of(items.begin() + 1, items.end(), [&](const auto &item) {
            return item->shape().group_id_ == shape.group_id_ ;
        });
        edit_description = std::all_of(items.begin() + 1, items.end(), [&](const auto &item) {
            return item->shape().description_ == shape.description_ ;
        });
    }
    if (!edit_text) {
        this->label_dialog_->edit_->setDisabled(true);
        this->label_dialog_->labelList_->setDisabled(true);
    }
    if (!edit_group_id)
        this->label_dialog_->edit_group_id_->setDisabled(true);
    if (!edit_description)
        this->label_dialog_->editDescription_->setDisabled(true);

    const auto pop = this->label_dialog_->popUp(
        edit_text ? shape.label_ : "",
        edit_flags ? shape.flags_ : QMap<QString, bool>{},
        edit_group_id ? shape.group_id_ : None,
        edit_description ? shape.description_ : "",
        !edit_flags
    );
    const auto text = std::get<0>(pop);
    const auto flags = std::get<1>(pop);
    const auto group_id = std::get<2>(pop);
    const auto description = std::get<3>(pop);

    if (!edit_text) {
        this->label_dialog_->edit_->setDisabled(false);
        this->label_dialog_->labelList_->setDisabled(false);
    }
    if (!edit_group_id)
        this->label_dialog_->edit_group_id_->setDisabled(false);
    if (not edit_description)
        this->label_dialog_->editDescription_->setDisabled(false);

    if (text.isEmpty()) { // canceled
        //assert flags is None
        //assert group_id is None
        //assert description is None
        return;
    }

    if (!this->validateLabel(text)) {
        this->errorMessage(
            this->tr("Invalid label"),
            this->tr("Invalid label '%1' with validation type '%2'").arg(
                text, YAML_QSTR(config_["validate_label"])
            )
        );
        return;
    }

    this->canvas_->storeShapes();
    for (const auto &item : items) {
        auto shape = item->shape();

        if (edit_text)
            shape.label_ = text;
        if (edit_flags)
            shape.flags_ = flags;
        if (edit_group_id)
            shape.group_id_ = group_id;
        if (edit_description)
            shape.description_ = description;

        this->update_shape_color(shape);
        if (shape.group_id_ == None) {
            int32_t r, g, b;
            shape.fill_color.getRgb(&r, &g, &b);
            item->setText(
                QString("%1 <font color=\"#%2%3%4\">●</font>").arg(text.toHtmlEscaped())
                    .arg(r, 2, 16, QLatin1Char('0')).arg(g, 2, 16, QLatin1Char('0')).arg(b, 2, 16, QLatin1Char('0'))
            );
        } else {
            item->setText(QString("%1 (%2)").arg(shape.label_, shape.group_id_));
        }
        this->setDirty();
        if (this->label_list_->find_label_item(shape.label_) == nullptr)
            this->label_list_->add_label_item(
                shape.label_, get_rgb_by_label(shape.label_)
            );
        item->setShape(shape);  // 由于保存的是对象, 需要更新回去.
    }
}

void MainWindow::fileSearchChanged() {
    import_images_from_dir(
        prev_opened_dir_, files_search_->text()
    );
}

void MainWindow::fileSelectionChanged() {
    const auto items = files_list_->selectedItems();
    if (items.empty()) {
        return;
    }
    const auto &item = *items[0];

    if (!can_continue()) {
        return;
    }

    const auto imagelist = imageList();
    auto currIndex = imagelist.indexOf(item.text());
    if (currIndex < imagelist.count()) {
        auto filename = imagelist[currIndex];
        if (!filename.isEmpty()) {
            load_file(filename);
        }
    }
}

// React to canvas signals.
void MainWindow::shapeSelectionChanged(const QList<int32_t> &selected_shapes) {
    QObject::disconnect(shape_list_, &ShapeListWidget::itemSelectionChanged, this, &MainWindow::label_selection_changed);
    for (auto &idx : canvas_->selectedShapes_) {
        canvas_->shapes_[idx].selected_ = false;
    }
    shape_list_->clearSelection();
    canvas_->selectedShapes_ = selected_shapes;
    for (auto &idx : canvas_->selectedShapes_) {
        canvas_->shapes_[idx].selected_ = true;
        const auto item = shape_list_->findItemByShape(canvas_->shapes_[idx]);
        shape_list_->selectItem(item);
        shape_list_->scrollToItem(item);
    }
    QObject::connect(shape_list_, &ShapeListWidget::itemSelectionChanged, this, &MainWindow::label_selection_changed);
    auto n_selected = selected_shapes.size();
    delete_->setEnabled(n_selected);
    duplicate_->setEnabled(n_selected);
    copy_->setEnabled(n_selected);
    edit_->setEnabled(n_selected);
}

void MainWindow::addLabel(TlShape &shape) {
    QString text;
    if (shape.group_id_ == None) {
        text = shape.label_;
    } else {
        text = QString("%1 (%2)").arg(shape.label_).arg(shape.group_id_);
    }
    auto *const shape_list_item = new ShapeListItem(text);
    shape_list_->addItem(shape_list_item);
    if (label_list_->find_label_item(shape.label_) == nullptr) {
        this->label_list_->add_label_item(
            shape.label_, get_rgb_by_label(shape.label_)
        );
    }
    label_dialog_->addLabelHistory(shape.label_);
    for (const auto &action : on_shapes_present_actions_) {
        action->setEnabled(true);
    }

    int32_t r, g, b;
    update_shape_color(shape);
    shape.fill_color_.getRgb(&r, &g, &b);
    shape_list_item->setText(
        QString("%1 <font color=\"#%2%3%4\">●</font>").arg(text.toHtmlEscaped())
            .arg(r, 2, 16, QLatin1Char('0')).arg(g, 2, 16, QLatin1Char('0')).arg(b, 2, 16, QLatin1Char('0'))
    );
    shape_list_item->setShape(shape);   // 更新完颜色后再设置.
}

void MainWindow::update_shape_color(TlShape &shape) {
    const auto v = get_rgb_by_label(shape.label_);
    shape.line_color_ = QColor(v[0], v[1], v[2]);
    shape.vertex_fill_color_ = QColor(v[0], v[1], v[2]);
    shape.hvertex_fill_color_ = QColor(255, 255, 255);
    shape.fill_color_ = QColor(v[0], v[1], v[2], 128);
    shape.select_line_color_ = QColor(255, 255, 255);
    shape.select_fill_color_ = QColor(v[0], v[1], v[2], 155);
}

std::vector<int32_t> MainWindow::get_rgb_by_label(const QString &label) {   // tuple[int, int, int]
    const auto label_colors = YAML_QSTR(config_["label_colors"]);
    if (YAML_STR(config_["shape_color"]) == "auto") {
        const auto *item = label_list_->find_label_item(label);
        const int32_t item_index = ((item != nullptr) ?
            this->label_list_->indexFromItem(item).row() :
            this->label_list_->count()
        );
        const int32_t label_id = (
            1   // skip black color by default
            + item_index
            + this->config_["shift_auto_shape_color"].as<int32_t>()
        );
        int32_t r, g, b;
        LABEL_COLORMAP[label_id % LABEL_COLORMAP.size()].getRgb(&r, &g, &b);
        return { r, g, b };
    } else if (
        YAML_STR(config_["shape_color"]) == "manual"
        && !label_colors.isNull()
        && label_colors.contains(label)
    ) {
        if (this->config_["label_colors"][label.toStdString()].as<std::vector<int32_t>>().size() != 3) {
            throw std::runtime_error(
                "Color for label must be 0-255 RGB tuple, but got: "
            );
        }
        return this->config_["label_colors"][label.toStdString()].as<std::vector<int32_t>>();
    } else if (!this->config_["default_shape_color"].as<std::vector<int32_t>>().empty()) {
        return this->config_["default_shape_color"].as<std::vector<int32_t>>();
    }

    return {0, 255, 0};
}

void MainWindow::remLabels(const QList<TlShape> &shapes) {
    for (const auto &shape : shapes) {
        auto *item = shape_list_->findItemByShape(shape);
        shape_list_->removeItem(item);
    }
}

void MainWindow::load_shapes(QList<TlShape> &shapes, bool replace) {
    QObject::disconnect(shape_list_, &ShapeListWidget::itemSelectionChanged, this, &MainWindow::label_selection_changed);
    for (auto &shape : shapes) {
        addLabel(shape);
    }
    shape_list_->clearSelection();
    QObject::connect(shape_list_, &ShapeListWidget::itemSelectionChanged, this, &MainWindow::label_selection_changed);
    canvas_->loadShapes(shapes, replace);
}

void MainWindow::load_shape_dicts(const QList<ShapeDict> &shape_dicts) {
    QList<TlShape> shapes;
    for (auto &shape_dict : shape_dicts) {
        TlShape shape;
        shape.label_=shape_dict.label;
        shape.shape_type_=shape_dict.shape_type;
        shape.group_id_=shape_dict.group_id;
        shape.description_=shape_dict.description;
        shape.mask_=shape_dict.mask;

        for (auto &p : shape_dict.points) {
            shape.addPoint(QPointF(p.x(), p.y()));
        }
        shape.close();

        QMap<QString, bool> default_flags = {};
        //if self._config["label_flags"]:
        //    for pattern, keys in self._config["label_flags"].items():
        //        if not isinstance(shape.label, str):
        //            logger.warning("shape.label is not str: {}", shape.label)
        //            continue
        //        if re.match(pattern, shape.label):
        //            for key in keys:
        //                default_flags[key] = False
        shape.flags_ = default_flags;
        //shape.flags.update(shape_dict["flags"])
        shape.other_data_ = shape_dict.other_data;

        shapes.append(shape);
    }
    load_shapes(shapes);
}

void MainWindow::load_flags(const YAML::Node &flags, QListWidget *const widget) const {
    widget->clear();
    for (const auto &it : flags) {
        const auto key = it.first.as<std::string>();
        const auto flag = it.second.as<bool>();
        auto *item = new QListWidgetItem(QString::fromStdString(key));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(flag ? Qt::Checked : Qt::Unchecked);
        widget->addItem(item);
    }
}

bool MainWindow::saveLabels(const QString &filename) {
    auto lf = std::unique_ptr<TlLabelFile>(new TlLabelFile());

    const auto format_shape = [](const TlShape &s) {
        auto data = s.other_data_;
        QMap<QString, bool> flags;
        ShapeDict shape_dict;
        //data.update(
        //    dict(
                //shape_dict.other_data=s.other_data_;
                shape_dict.label=s.label_; //.encode("utf-8") if PY2 else s.label,
                shape_dict.points=s.points_; // [(p.x(), p.y()) for p in s.points],
                shape_dict.group_id=s.group_id_;
                shape_dict.description=s.description_;
                shape_dict.shape_type=s.shape_type_;
                shape_dict.flags=s.flags_;
                shape_dict.mask=s.mask_;
                //if s.mask is None
                //else utils.img_arr_to_b64(s.mask.astype(np.uint8)),
        //    );
        //);
        return shape_dict;
    };

    //shapes = [format_shape(item.shape()) for item in this->labelList];
    QMap<QString, bool> flags = {};
    for (auto i = 0; i < this->flags_list_->count(); ++i) {
        const auto *item = this->flags_list_->item(i);
        const auto key = item->text();
        const auto flag = item->checkState() == Qt::Checked;
        flags[key] = flag;
    }
    try {
        QFileInfo fileInfo(image_path_);
        QString imagePath = fileInfo.fileName();
        QByteArray imageData = config_["with_image_data"].as<bool>() ? imageData_ : QByteArray{};
        if (!fileInfo.path().isEmpty() && !QFile::exists(fileInfo.path())) {
            (void)QDir().mkdir(fileInfo.path());
        }
        lf->save(
            filename,
            canvas_->shapes_,
            imagePath,
            imageData,
            image_.height(),
            image_.width(),
            other_data_,
            flags
        );
        label_file_ = std::move(lf);
        auto items = files_list_->findItems(image_path_, Qt::MatchExactly);
        if (items.count() > 0) {
            if (items.count() != 1) {
                throw std::runtime_error("There are duplicate files.");
            }
            items[0]->setCheckState(Qt::Checked);
        }
        //disable allows next and previous ima ge to proceed
        //this->filename = filename
        return true;
    } catch (const LabelFileError &e) {
        errorMessage(
            tr("Error saving label data"), tr("<b>%1</b>").arg(e.what())
        );
    }
    return false;
}

void MainWindow::duplicateSelectedShape() {
    copySelectedShape();
    pasteSelectedShape();
}

void MainWindow::pasteSelectedShape() {
    // 粘贴时需要为图形生成新的uuid.
    QList<TlShape> copied_shapes;
    std::ranges::for_each(copied_shapes_, [&copied_shapes](auto &s){ copied_shapes.push_back(s.clone()); });
    this->load_shapes(copied_shapes, false);
    this->canvas_->selectShapes(copied_shapes);
    setDirty();
}

void MainWindow::copySelectedShape() {
    copied_shapes_.clear();
    std::ranges::for_each(this->canvas_->selectedShapes_, [&](auto &s){
        copied_shapes_.push_back(canvas_->shapes_[s]);}
    );
    paste_->setEnabled(copied_shapes_.size() > 0);
}

void MainWindow::label_selection_changed() {
    QList<TlShape> selected_shapes = {};
    for (const auto &item : shape_list_->selectedItems()) {
        selected_shapes.append(item->shape());
    }
    if (!selected_shapes.empty()) {
        canvas_->selectShapes(selected_shapes);
    } else {
        canvas_->deSelectShape();
    }
}

void MainWindow::labelItemChanged(const ShapeListItem *item) {
    const auto shape = item->shape();
    canvas_->setShapeVisible(shape, item->checkState() == Qt::Checked);
}

void MainWindow::labelOrderChanged() {
    setDirty();
    // 不能且不需要重新加载, shape_list中保存的原始图形, 不包含锚点调整信息。
    //QList<TlShape> shapes;
    //QList<ShapeListItem *> items = shape_list_->items();
    //std::ranges::transform(items, std::back_inserter(shapes), [](auto &item){ return item->shape(); });
    //this->canvas_->loadShapes(shapes);
}

// Callback functions:

void MainWindow::newShape() {
    //Pop-up and give focus to the label editor.
    //
    //position MUST be in global coordinates.
    //
    auto items = label_list_->selectedItems();
    QString text;
    if (!items.isEmpty()) {
        text = items[0]->data(Qt::UserRole).toString();
    }
    QMap<QString, bool> flags = {};
    int32_t group_id = None;
    QString description;
    if (config_["display_label_popup"].as<bool>() || text.isEmpty()) {
        QString previous_text = label_dialog_->edit_->text();
        std::tie(text, flags, group_id, description) = label_dialog_->popUp(text);
        if (text.isEmpty()) {
            label_dialog_->edit_->setText(previous_text);
        }
    }

    if (!text.isEmpty() && !validateLabel(text)) {
        errorMessage(
            tr("Invalid label"),
            tr("Invalid label '%1' with validation type '%2'").arg(
                text, config_["validate_label"].as<bool>()
            )
        );
        text = "";
    }
    if (!text.isEmpty()) {
        label_list_->clearSelection();
        auto &shape = canvas_->setLastLabel(text, flags);
        shape.group_id_ = group_id;
        shape.description_ = description;
        addLabel(shape);
        edit_mode_->setEnabled(true);
        undo_last_point_->setEnabled(false);
        undo_->setEnabled(true);
        setDirty();
    } else {
        canvas_->undoLastLine();
        canvas_->shapesBackups_.pop_back();
    }
}

void MainWindow::scrollRequest(int32_t delta, Qt::Orientation orientation) {
    auto units = -delta * 0.1;  // natural scroll
    auto *bar = scroll_bars_[orientation];
    auto value = bar->value() + bar->singleStep() * units;
    setScroll(orientation, value);
}

void MainWindow::setScroll(Qt::Orientation orientation, float value) {
    scroll_bars_[orientation]->setValue(value);
    scroll_values_[orientation][filename_] = value;
}

void MainWindow::set_zoom(int32_t value, QPointF pos) {
    if (filename_.isEmpty()) {
        SPDLOG_WARN("filename is None, cannot set zoom");
        return;
    }

    if (pos.isNull())
        pos = QPointF(this->canvas_->visibleRegion().boundingRect().center());
    int32_t canvas_width_old = this->canvas_->width();

    this->fit_width_->setChecked(zoom_mode_ == ZoomMode::FIT_WIDTH);
    this->fit_window_->setChecked(zoom_mode_ == ZoomMode::FIT_WINDOW);
    this->canvas_->enableDragging(
        value > scalers_[ZoomMode::FIT_WINDOW]() * 100
    );
    this->zoom_widget_->setValue(value);  // triggers self._paint_canvas
    this->zoom_values_[filename_] = {this->zoom_mode_, value};

    int32_t canvas_width_new = this->canvas_->width();
    if (canvas_width_old == canvas_width_new) {
        return;
    }
    float canvas_scale_factor = 1.0*canvas_width_new / canvas_width_old;
    float x_shift = pos.x() * canvas_scale_factor - pos.x();
    float y_shift = pos.y() * canvas_scale_factor - pos.y();
    setScroll(
        Qt::Horizontal,
        this->scroll_bars_[Qt::Horizontal]->value() + x_shift
    );
    setScroll(
        Qt::Vertical,
        this->scroll_bars_[Qt::Vertical]->value() + y_shift
    );
}

void MainWindow::set_zoom_to_original() {
    zoom_mode_ = ZoomMode::MANUAL_ZOOM;
    set_zoom(100);
}

void MainWindow::add_zoom(float increment, QPointF pos) {
    int32_t zoom_value;
    if (increment > 1) {
        zoom_value = std::ceil(zoom_widget_->value() * increment);
    } else {
        zoom_value = std::floor(zoom_widget_->value() * increment);
    }
    zoom_mode_ = ZoomMode::MANUAL_ZOOM;
    set_zoom(zoom_value, pos);
}

void MainWindow::zoom_requested(int32_t delta, QPointF pos) {
    add_zoom(delta > 0 ? 1.1 : 0.9, pos);
}

void MainWindow::setFitWindow(bool value) {
    if (value) {
        fit_width_->setChecked(false);
    }
    zoom_mode_ = value ? ZoomMode::FIT_WINDOW : ZoomMode::MANUAL_ZOOM;
    adjust_scale();
}

void MainWindow::setFitWidth(bool value) {
    if (value) {
        fit_window_->setChecked(false);
    }
    zoom_mode_ = value ? ZoomMode::FIT_WIDTH : ZoomMode::MANUAL_ZOOM;
    adjust_scale();
}

void MainWindow::enableKeepPrevScale(bool enabled) {
    config_["keep_prev_scale"] = enabled;
    keep_prev_scale_->setChecked(enabled);
}

void MainWindow::onNewBrightnessContrast(const QImage &image) {
    canvas_->loadPixmap(QPixmap::fromImage(image), this->filename_, false);
}

void MainWindow::brightnessContrast(bool value, bool is_initial_load) {
    if (filename_.isEmpty()) {
        SPDLOG_WARN("filename is None, cannot set brightness/contrast");
        return;
    }

    int32_t brightness = None;
    int32_t contrast = None;
    if (const auto it = brightness_contrast_values_.find(this->filename_); it != brightness_contrast_values_.end()) {
        brightness = it->first; contrast = it->second;
    }

    if (is_initial_load) {
        QString prev_filename = recent_files_.empty() ? "" : recent_files_[0];
        if (config_["keep_prev_brightness_contrast"].as<bool>() && !prev_filename.isEmpty())
            if (const auto it = brightness_contrast_values_.find(prev_filename); it != brightness_contrast_values_.end()) {
                brightness = it->first, contrast = it->second;
            }
        if (brightness == None && contrast == None) {
            return;
        }
    }

    SPDLOG_DEBUG(
        "Opening brightness/contrast dialog with brightness={}, contrast={}",
        brightness,
        contrast,
    );
    auto dialog = BrightnessContrast(
        QImage::fromData(imageData_).convertedTo(QImage::Format_RGB888),
        [this] { onNewBrightnessContrast(this->image_); },
        this
    );

    if (brightness != None)
        dialog.slider_brightness_->setValue(brightness);
    if (contrast != None)
        dialog.slider_contrast_->setValue(contrast);

    if (is_initial_load) {
        dialog.onNewValue(None);
    } else {
        dialog.exec();
        brightness = dialog.slider_brightness_->value();
        contrast = dialog.slider_contrast_->value();
    }

    this->brightness_contrast_values_[this->filename_] = {brightness, contrast};
    SPDLOG_DEBUG(
        "Updated states for {}: brightness={}, contrast={}",
        filename_,
        brightness,
        contrast
    );
}

void MainWindow::toggleShapes(int32_t value) {
    bool flag = (value == None) ? false : value;
    for (auto *item : shape_list_->items()) {
        if (value == None) {
            flag = item->checkState() == Qt::Unchecked;
        }
        item->setCheckState(flag ? Qt::Checked : Qt::Unchecked); // emit itemChanged
    }
}

bool MainWindow::load_file(QString filename) {
    const auto imagelist = imageList();
    //Load the specified file, or the last opened file if None.
    //changing fileListWidget loads file
    if (imagelist.contains(filename) &&
        files_list_->currentRow() != imagelist.indexOf(filename)
    ) {
        this->files_list_->setCurrentRow(imagelist.indexOf(filename));
        this->files_list_->repaint();
        return true;
    }

    QList<TlShape> prev_shapes;
    if (config_["keep_prev"].as<bool>() ||
        QApplication::keyboardModifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        prev_shapes = canvas_->shapes_;
    }
    this->resetState();
    this->canvas_->setEnabled(false);
    if (filename.isEmpty()) {
        filename = settings_.value("filename", "").toString();
    }
    QFileInfo fileInfo(filename);
    std::filesystem::path file_path(filename.toStdString());
    if (!QFile::exists(filename)) {
        errorMessage(
            tr("Error opening file"),
            tr("No such file: <b>%1</b>").arg(filename)
        );
        return false;
    }
    // assumes same name, but json extension
    show_status_message(tr("Loading %1...").arg(fileInfo.baseName()));
    const auto t0_load_file = std::chrono::system_clock::now();
    auto label_file = QString::fromStdString(file_path.replace_extension("json").string());
    if (!output_dir_.isEmpty()) {
        QFileInfo labelInfo(label_file);
        auto label_file_without_path = labelInfo.baseName();
        label_file = output_dir_ + "/" + label_file_without_path;
    }
    if (QFile(label_file).exists() && TlLabelFile::is_label_file(label_file)) {
        try {
            label_file_ = std::unique_ptr<TlLabelFile>(new TlLabelFile(label_file));
        } catch (LabelFileError &e) {
            errorMessage(
                tr("Error opening file"),
                tr(
                    "<p><b>%1</b></p>"
                    "<p>Make sure <i>%2</i> is a valid label file."
                )
                .arg(e.what(), label_file)
            );
            show_status_message(tr("Error reading %1").arg(label_file));
            return false;
        }
        imageData_ = label_file_->imageData_;
        image_path_ = QFileInfo(label_file).absolutePath() + "/" + label_file_->imagePath_;
        other_data_ = label_file_->otherData_;
    } else {
        try {
            imageData_ = TlLabelFile::load_image_file(filename);
        } catch (LabelFileError &e) {

        }
        if (!imageData_.isEmpty()) {
            image_path_ = filename;
        }
        label_file_.reset();
    }
    const auto t0 = std::chrono::system_clock::now();
    auto image = QImage::fromData(imageData_);
    const auto t1 = std::chrono::system_clock::now();
    SPDLOG_INFO("Created QImage in {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    if (image.isNull()) {
        QStringList formats;
        for (auto &fmt : QImageReader::supportedImageFormats()) {
            formats.append(QString("*.%1").arg(fmt.toLower()));
        }
        errorMessage(
            tr("Error opening file"),
            tr(
                "<p>Make sure <i>{%1}</i> is a valid image file.<br/>"
                "Supported image formats: {%2}</p>"
            ).arg(filename, formats.join(","))
        );
        show_status_message(tr("Error reading %1").arg(filename));
        return false;
    }
    this->image_ = image;
    this->filename_ = filename;
    const auto t2 = std::chrono::system_clock::now();
    canvas_->loadPixmap(QPixmap::fromImage(image), filename);
    const auto t3 = std::chrono::system_clock::now();
    SPDLOG_INFO("Loaded pixmap in {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count());
    //flags = {k: False for k in config_["flags"] or []}
    if (label_file_) {
        load_shape_dicts(label_file_->shapes1_);
        //if (labelFile_->flags_ is not None) {
        //    flags.update(this->labelFile.flags);
        //}
    }
    //loadFlags(flags);
    if (config_["keep_prev"].as<bool>() && noShapes()) {
        load_shapes(prev_shapes, false);
        setDirty();
    } else {
        setClean();
    }
    canvas_->setEnabled(true);

    // set zoom values
    bool is_initial_load = !zoom_values_.empty();
    if (zoom_values_.contains(filename_)) {
        zoom_mode_ = zoom_values_[filename_].first;
        set_zoom(zoom_values_[filename_].second);
    } else if (is_initial_load || !config_["keep_prev_scale"].as<bool>()) {
        adjust_scale(true);
    }
    // set scroll values
    for (auto &orientation : scroll_values_.keys()) {
        if (scroll_values_[orientation].contains(filename_)) {
            setScroll(
                orientation, scroll_values_[orientation][filename_]);
        }
    }
    this->brightnessContrast(false, true);
    this->paint_canvas();
    this->addRecentFile(filename);
    this->toggleActions(true);
    this->canvas_->setFocus();
    show_status_message(tr("Loaded ") + filename_);
    const auto t4 = std::chrono::system_clock::now();
    SPDLOG_INFO("Loaded file: {} in {}ms",
        filename_,
        std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
    );
    return true;
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    if (
        canvas_ &&
        !image_.isNull() &&
        zoom_mode_ != ZoomMode::MANUAL_ZOOM) {
        adjust_scale();
    }
    QMainWindow::resizeEvent(event);
}

void MainWindow::paint_canvas() {
    if (image_.isNull()) {
        SPDLOG_WARN("image is null, cannot paint canvas");
        return;
    }

    canvas_->scale_ = 0.01 * zoom_widget_->value();
    canvas_->adjustSize();
    canvas_->update();
}

void MainWindow::adjust_scale(bool initial) {
    set_zoom(scalers_[zoom_mode_]() * 100);
}

float MainWindow::scaleFitWindow() const {
    float EPSILON_TO_HIDE_SCROLLBAR = 2.0;
    auto w1 = this->centralWidget()->width() - EPSILON_TO_HIDE_SCROLLBAR;
    auto h1 = this->centralWidget()->height() - EPSILON_TO_HIDE_SCROLLBAR;
    auto a1 = w1 / h1;

    auto w2 = this->canvas_->pixmap_.width() * 1.;
    auto h2 = this->canvas_->pixmap_.height() * 1.;
    auto a2 = w2 / h2;

    return (a2 >= a1) ? (w1 / w2) : (h1 / h2);
}

float MainWindow::scaleFitWidth() const {
    float EPSILON_TO_HIDE_SCROLLBAR = 15.0;
    auto w = this->centralWidget()->width() - EPSILON_TO_HIDE_SCROLLBAR;
    return w / this->canvas_->pixmap_.width();
}

void MainWindow::enableSaveImageWithData(bool enabled) {
    config_["with_image_data"] = enabled;
    save_with_image_data_->setChecked(enabled);
}

void MainWindow::reset_layout() {
    settings_.remove("window/state");
    restoreState(default_state_);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!can_continue()) {
        event->ignore();
    }
    settings_.setValue("filename", this->filename_);
    settings_.setValue("window/size", this->size());
    settings_.setValue("window/position", this->pos());
    settings_.setValue("window/state", this->saveState());
    settings_.setValue("recentFiles", this->recent_files_);
    //ask the use for where to save the labels
    //this->settings.setValue('window/geometry', this->saveGeometry())
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    QStringList extensions;
    for (auto &fmt : QImageReader::supportedImageFormats()) {
        extensions.append(QString(".%1").arg(fmt.toLower()));
    }
    if (event->mimeData()->hasUrls()) {
        bool endsWidth = false;
        for (auto &i : event->mimeData()->urls()) {
            const auto url = i.toLocalFile().toLower();
            for (auto &ext : extensions) {
                endsWidth |= url.endsWith(ext);
            }
            if (endsWidth) {
                event->accept();
                break;
            }
        }
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    if (!can_continue()) {
        event->ignore();
        return;
    }
    QStringList items;
    std::ranges::for_each(event->mimeData()->urls(), [&](auto &i){ items.append(i.toLocalFile()); });
    importDroppedImageFiles(items);
}

void MainWindow::loadRecent(const QString &filename) {
    if (can_continue()) {
        load_file(filename);
    }
}

void MainWindow::open_prev_image(bool value) {
    int32_t row_prev = files_list_->currentRow() - 1;
    if (row_prev < 0) {
        SPDLOG_INFO("there is no prev image");
        return;
    }
    SPDLOG_INFO("setting current row to {}", row_prev);
    files_list_->setCurrentRow(row_prev);
    files_list_->repaint();
}

void MainWindow::open_next_image(bool value) {
    int32_t row_next = files_list_->currentRow() + 1;
    if (row_next >= files_list_->count()) {
        SPDLOG_INFO("there is no next image");
        return;
    }
    SPDLOG_INFO("setting current row to {}", row_next);
    files_list_->setCurrentRow(row_next);
    files_list_->repaint();
}

void MainWindow::open_file_with_dialog(bool value) {
    if (!can_continue()) {
        return;
    }
    QString path = QString::fromStdString(AppConfig::instance().last_work_dir_);
    QString filters(tr("Images(*.png *.jpg *.jpeg *.bmp)"));
    //formats = [
    //    f"*.{fmt.data().decode()}"
    //    for fmt in QtGui.QImageReader.supportedImageFormats()
    //]
    //filters = self.tr("Image & Label files (%s)") % " ".join(
    //    formats + [f"*{LabelFile.suffix}"]
    //)
    FileDialogPreview fileDialog(this);
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setNameFilter(filters);
    fileDialog.setWindowTitle(
        tr("%1 - Choose Image or Label file").arg(qAppName())
    );
    fileDialog.setWindowFilePath(path);
    fileDialog.setViewMode(FileDialogPreview::Detail);
    if (fileDialog.exec()) {
        const auto fileName = fileDialog.selectedFiles()[0];
        if (!fileName.isEmpty()) {
            this->load_file(fileName);
        }
    }
}

void MainWindow::changeOutputDirDialog(bool _value) {
    auto default_output_dir = this->output_dir_;
    if (default_output_dir.isEmpty() && !this->filename_.isEmpty()) {
        default_output_dir = QFileInfo(this->filename_).path();
    }
    if (default_output_dir.isEmpty()) {
        default_output_dir = this->currentPath();
    }

    auto output_dir = QFileDialog::getExistingDirectory(
        this,
        tr("%1 - Save/Load Annotations in Directory").arg(qAppName()),
        default_output_dir,
        QFileDialog::ShowDirsOnly |
        QFileDialog::DontResolveSymlinks
    );
    //output_dir = str(output_dir)

    if (output_dir.isEmpty()) {
        return;
    }
    output_dir_ = output_dir;

    statusBar()->showMessage(
        tr("%1 . Annotations will be saved/loaded in %2")
        .arg("Change Annotations Dir", output_dir_)
    );
    statusBar()->show();

    auto current_filename = filename_;
    import_images_from_dir(prev_opened_dir_);

    const auto imagelist = imageList();
    if (imagelist.contains(current_filename)) {
        // retain currently selected file
        files_list_->setCurrentRow(imagelist.indexOf(current_filename));
        files_list_->repaint();
    }
}

void MainWindow::saveFile(bool value) {
    //assert not self.image.isNull(), "cannot save empty image"
    if (label_file_ != nullptr) {
        // DL20180323 - overwrite when in directory
        saveFile_(label_file_->filename_);
    } else {
        saveFile_(saveFileDialog());
    }
}

void MainWindow::saveFileAs(bool value) {
    //assert not self.image.isNull(), "cannot save empty image"
    saveFile_(saveFileDialog());
}

QString MainWindow::saveFileDialog() {
    //assert self.filename is not None
    const QString caption = tr("%1 - Choose File").arg(qAppName());
    const QString filters = tr("Label files (*%1)").arg(".json");
    const auto directory = output_dir_.isEmpty() ? currentPath() : output_dir_;
    auto dlg = QFileDialog(this, caption, directory, filters);
    dlg.setDefaultSuffix("json");
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setOption(QFileDialog::DontConfirmOverwrite, false);
    dlg.setOption(QFileDialog::DontUseNativeDialog, false);

    QString default_label_file_name;
    QFileInfo fileInfo(filename_);
    if (!output_dir_.isEmpty()) {
        default_label_file_name = QString("%1/%2%3").arg(output_dir_, fileInfo.baseName(), TlLabelFile::suffix);
    } else {
        default_label_file_name = QString("%1/%2%3").arg(currentPath(), fileInfo.baseName(), TlLabelFile::suffix);
    }
    QString filename = dlg.getSaveFileName(
        this,
        tr("Choose File"),
        default_label_file_name,
        tr("Label files (*%1)").arg(TlLabelFile::suffix)
    );
    //if (isinstance(filename, tuple)) {
    //    filename, _ = filename;
    //}
    return filename;
}

void MainWindow::saveFile_(const QString &filename) {
    if (!filename.isEmpty() && saveLabels(filename)) {
        addRecentFile(filename);
        setClean();
    }
}

void MainWindow::closeFile(bool value) {
    if (!can_continue()) {
        return;
    }
    this->resetState();
    this->setClean();
    this->toggleActions(false);
    this->canvas_->setEnabled(false);
    this->files_list_->setFocus();
    this->save_as_->setEnabled(false);
}

QString MainWindow::getLabelFile() {
    //assert self.filename is not None
    QString label_file;
    if (filename_.endsWith(".json", Qt::CaseInsensitive)) {
        label_file = filename_;
    } else {
        std::filesystem::path file_path(filename_.toStdString());
        label_file = QString::fromStdString(file_path.replace_extension("json").string());
    }
    return label_file;
}

void MainWindow::deleteFile() {
    QMessageBox mb;
    const auto msg = tr(
        "You are about to permanently delete this label file, proceed anyway?"
    );
    const auto answer = mb.warning(this, tr("Attention"), msg, mb.Yes | mb.No);
    if (answer != mb.Yes) {
        return;
    }

    const auto label_file = getLabelFile();
    if (QFileInfo::exists(label_file)) {
        QFile::remove(label_file);
        SPDLOG_INFO("Label file is removed: {}", label_file);

        auto *const item = files_list_->currentItem();
        if (item) {
            item->setCheckState(Qt::Unchecked);
        }

        // 修改: 删除标签文件后保持当前打开图像.
        //resetState();
        this->shape_list_->clear();
        this->label_file_.reset();
        this->other_data_.clear();
        this->canvas_->resetState();
        this->canvas_->loadPixmap(QPixmap::fromImage(this->image_), this->filename_);
    }
}

void MainWindow::open_config_file() {
//    if self._config_file is None:
//        QtWidgets.QMessageBox.information(
//            self,
//            self.tr("No Config File"),
//            self.tr(
//                "Configuration was provided as a YAML expression via "
//                "command line.\n\n"
//                "To use the preferences editor, start Labelme with a config file:\n"
//                "  tl_assistant --config ~/.labelmerc"
//            ),
//        )
//        return
//    config_file: Path = self._config_file
//
//    system: str = platform.system()
//    if system == "Darwin":
//        subprocess.Popen(["open", "-t", config_file])
//    elif system == "Windows":
//        os.startfile(config_file)  # type: ignore[attr-defined]
//    else:
//        subprocess.Popen(["xdg-open", config_file])
}

// Message Dialogs. #
bool MainWindow::hasLabels() {
    if (noShapes()) {
        this->errorMessage(
            "No objects labeled",
            "You must label at least one object to save the file."
        );
        return false;
    }
    return true;
}

bool MainWindow::hasLabelFile() {
    if (filename_.isEmpty()) {
        return false;
    }

    auto label_file = getLabelFile();
    return QFile::exists(label_file);
}

bool MainWindow::can_continue() {
    if (!is_changed_) {
        return true;
    }
    QMessageBox mb;
    const QString msg = QString(tr("Save annotations to \"{%1}\" before closing?")).arg(filename_);
    auto answer = mb.question(
        this,
        tr("Save annotations?"),
        msg,
        mb.Save | mb.Discard | mb.Cancel,
        mb.Save
    );
    if (answer == mb.Discard) {
        return true;
    } else if (answer == mb.Save) {
        saveFile();
        return true;
    } else {  // answer == mb.Cancel
        return false;
    }
}

void MainWindow::errorMessage(const QString &title, const QString &message) {
    QMessageBox::critical(
        this, title, QString("<p><b>%1</b></p>%2").arg(title, message)
    );
}

QString MainWindow::currentPath() {
    return filename_.isEmpty() ? "." : QFileInfo(filename_).path();
}

void MainWindow::toggleKeepPrevMode() {
    config_["keep_prev"] = !config_["keep_prev"].as<bool>();
}

void MainWindow::removeSelectedPoint() {
    canvas_->removeSelectedPoint();
    canvas_->update();
    if (canvas_->hShape_ != None && canvas_->shapes_[canvas_->hShape_].points_.empty()) {
        canvas_->deleteShape(canvas_->shapes_[canvas_->hShape_]);
        remLabels({ canvas_->shapes_[canvas_->hShape_] });
        if (noShapes()) {
            for (const auto &action : on_shapes_present_actions_) {
                action->setEnabled(false);
            }
        }
    }
    setDirty();
}

void MainWindow::deleteSelectedShape() {
    const auto yes = QMessageBox::Yes, no = QMessageBox::No;
    const auto msg = tr(
        "You are about to permanently delete %1 shapes, proceed anyway?"
    ).arg(canvas_->selectedShapes_.length());
    if (yes == QMessageBox::warning(
        this, tr("Attention"), msg, yes | no, yes
    )) {
        remLabels(canvas_->deleteSelected());
        setDirty();
        if (noShapes()) {
            for (auto *action : on_shapes_present_actions_) {
                action->setEnabled(false);
            }
        }
    }
}

void MainWindow::copyShape() {
    canvas_->endMove(true);
    for (auto shape : canvas_->selectedShapes_) {
        addLabel(canvas_->shapes_[shape]);
    }
    label_list_->clearSelection();
    setDirty();
}

void MainWindow::moveShape() {
    canvas_->endMove(false);
    setDirty();
}

void MainWindow::open_dir_with_dialog(bool value) {
    if (!can_continue()) {
        return;
    }

    QString defaultOpenDirPath;
    if (!prev_opened_dir_.isEmpty() && QFile::exists(prev_opened_dir_)) {
        defaultOpenDirPath = prev_opened_dir_;
    } else {
        defaultOpenDirPath = filename_.isEmpty() ? "." : QFileInfo(filename_).path();
    }

    auto targetDirPath = QString(
        QFileDialog::getExistingDirectory(
            this,
            tr("%1 - Open Directory").arg(qAppName()),
            defaultOpenDirPath,
            QFileDialog::ShowDirsOnly |
            QFileDialog::DontResolveSymlinks
        )
    );
    import_images_from_dir(targetDirPath);
    open_next_image();
}

QStringList MainWindow::imageList() {
    QStringList lst;
    for (auto i = 0; i < files_list_->count(); ++i) {
        auto *const item = files_list_->item(i);
        lst.append(item->text());
    }
    return lst;
}

void MainWindow::importDroppedImageFiles(const QStringList &imageFiles) {
    QStringList extensions;
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    std::ranges::transform(formats, std::back_inserter(extensions), [](auto &fmt) { return fmt.toLower(); });

    filename_.clear();
    for (auto &file : imageFiles) {
        if (imageList().contains(file) || std::none_of(extensions.begin(), extensions.end(), [&](auto &e) { return file.endsWith(e, Qt::CaseInsensitive); }))
            continue;
        auto label_file = QFileInfo(file).completeBaseName() + ".json";
        if (!output_dir_.isEmpty()) {
            auto label_file_without_path = QFileInfo(label_file).fileName();
            label_file = output_dir_ + "/" + label_file_without_path;
        }
        auto *item = new QListWidgetItem(file);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (QFile::exists(label_file) && TlLabelFile::is_label_file(label_file)) {
            item->setCheckState(Qt::Checked);
        } else {
            item->setCheckState(Qt::Unchecked);
        }
        files_list_->addItem(item);
    }

    if (imageList().count() > 1) {
        open_next_img_->setEnabled(true);
        open_prev_img_->setEnabled(true);
    }

    open_next_image();
}

void MainWindow::import_images_from_dir(const QString &root_dir, const QString &pattern) {
    open_next_img_->setEnabled(true);
    open_prev_img_->setEnabled(true);

    if (!can_continue() || root_dir.isEmpty()) {
        return;
    }

    prev_opened_dir_ = root_dir;
    AppConfig::instance().last_work_dir_ = prev_opened_dir_.toStdString();
    filename_.clear();
    files_list_->clear();

    auto filenames = scan_image_files(root_dir);
    QRegularExpression re(pattern);
    if (!pattern.isEmpty() && re.isValid()) {
        QStringList names;
        foreach (const auto &f, filenames) {
            const auto match = re.match(f);
            if (match.hasMatch()) {
                names.append(f);
            }
        }
        filenames = names;
    }
    for (auto &filename : filenames) {
        std::filesystem::path file_path(filename.toStdString());
        auto label_file = QString::fromStdString(file_path.replace_extension("json").string());
        if (!output_dir_.isEmpty()) {
            QFileInfo fileInfo(label_file);
            auto label_file_without_path = fileInfo.baseName();
            label_file = output_dir_ + "/" + label_file_without_path;
        }
        auto *const item = new QListWidgetItem(filename);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (QFile::exists(label_file) && TlLabelFile::is_label_file(label_file)) {
            item->setCheckState(Qt::Checked);
        } else {
            item->setCheckState(Qt::Unchecked);
        }
        files_list_->addItem(item);
    }
}

void MainWindow::update_status_stats(const QPointF &mouse_pos) {
    QStringList stats;
    stats.append(QString("mode=%1").arg(ModeName(canvas_->mode_)));
    stats.append(QString("x=%1, y=%2").arg(mouse_pos.x(), 0, 'f', 1).arg(mouse_pos.y(), 0, 'f', 1));
    stats_->setText(stats.join(" | "));
}

QStringList MainWindow::scan_image_files(const QString &folderPath) const {
    QStringList images;
    QDir dir(folderPath);
    if (!dir.exists()) {
        return images;
    }

    QStringList extensions;
    for (auto &fmt : QImageReader::supportedImageFormats()) {
        QString name = fmt.toLower();
        extensions.append(QString("*.%1").arg(name));
    }

    QDirIterator iterator(folderPath, extensions, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        QFileInfo fileInfo = iterator.fileInfo();
        if (fileInfo.isFile()) {
            images.append(fileInfo.absoluteFilePath());
        }
    }

    return images;
}

MainWindow::~MainWindow() {
    delete ui_;
    SamApis::instance().unregister_all("");
    AppConfig::instance().save();
}

void MainWindow::slotActionSetup() {
    train_widget_->show();
}

#include <QProcess>
void MainWindow::slotActionTrain() {
    sub_process_ = new QProcess();        //使用进程运行子进程窗口
    QObject::connect(sub_process_, &QProcess::readyReadStandardOutput, this, &MainWindow::slotReadyReadStandardOutput);
    QObject::connect(sub_process_, &QProcess::readyReadStandardError, this, &MainWindow::slotReadyReadStandardError);
    QObject::connect(sub_process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, qOverload<int , QProcess::ExitStatus>(&MainWindow::slotFinishedProcess));
    //connect(m_pProcessVM, &QProcess::readyRead, this, &CncOpWindows::slotReadProcessVM);

    QObject::connect(sub_process_, &QProcess::stateChanged, this, [=](QProcess::ProcessState state) {
        if (state == QProcess::ProcessState::NotRunning) {
        }
    });
    QObject::connect(sub_process_, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotProcessExited()));
    QObject::connect(qApp, SIGNAL(aboutToQuit()), sub_process_, SLOT(terminate()));
    QObject::connect(sub_process_, SIGNAL(error(QProcess::ProcessError)), this, SLOT(slotError(QProcess::ProcessError)));

    QObject::connect(sub_process_,&QProcess::started,[=]() {//启动完成
        std::cerr << "进程已启动" << std::endl;
    });
    QObject::connect(sub_process_,&QProcess::stateChanged,[=]() {//进程状态改变
        if (sub_process_->state()==QProcess::Running) {
            std::cerr << "正在运行" << std::endl;
        } else if(sub_process_->state()==QProcess::NotRunning) {
            std::cerr << "不在运行" << std::endl;
        } else {
            std::cerr << "正在启动" << std::endl;
        }
    });
    QObject::connect(sub_process_,&QProcess::errorOccurred,[=]() {
        std::cerr << sub_process_->errorString().toStdString(); //输出错误信息
    });

    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (auto &item : env.toStringList()) {
            std::cerr << "env<===" << item.toStdString() << std::endl;
        }
    }

    // 设置启动所需环境(效率比setEnvironment较高)
    QProcessEnvironment env;
    env.insert("PATH", "D:/PYTHON_HOME/Python312");
    env.insert("PYTHONPATH", "D:/PYTHON_HOME/yolo-packages");
    env.insert("YOLO_CONFIG_DIR", "datasets/data_lidian_cls");
    env.insert("HOMEPATH", "C:/Users/njtl007");
    //qputenv("PATH", "D:/PYTHON_HOME/Python312");
    //qputenv("PYTHONPATH", "D:/PYTHON_HOME/yolo-packages");
    sub_process_->setProcessEnvironment(env);

    sub_process_->setWorkingDirectory("D:/PYTHON_HOME/ultralytics-Yolo11");

    // 启动时设置的环境变量未生效, 需要设置目标程序绝对路径.
    // C:/WORK/TlAssistant/PYTHON_HOME/Ultralytics-Yolo11/train_cls.py
    QStringList arguments{"train_cls.py", "--config=datasets/data_lidian_cls/args.yaml"};
    //QStringList arguments{"--version"};
    sub_process_->start("D:/PYTHON_HOME/Python312/python.exe", arguments);

    sub_process_->waitForStarted(2000);
}

void MainWindow::slotActionInfer() {
}

void MainWindow::slotReadyReadStandardOutput() {
    std::cerr << "<==ReadStandardOut " << sub_process_->readAllStandardOutput().data();
}

void MainWindow::slotReadyReadStandardError() {
    std::cerr << "<==ReadStandardErr " << sub_process_->readAllStandardError().data();
}

void MainWindow::slotFinishedProcess(int32_t exitCode, QProcess::ExitStatus exitStatus) {
    std::cerr << "<==finishedProcess " << exitCode;
}

void MainWindow::slotProcessExited(int32_t exitCode, QProcess::ExitStatus exitStatus) {
    std::cerr << "<==processExited " << exitCode;
}

void MainWindow::slotError(QProcess::ProcessError) {

}