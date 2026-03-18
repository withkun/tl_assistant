#ifndef __INC_MAINWINDOW_H
#define __INC_MAINWINDOW_H

#include <QMainWindow>
#include <QWidgetAction>
#include <QGraphicsScene>
#include <QScrollArea>
#include <QSettings>
#include <QProcess>
#include <QLabel>

#include "tl_widgets/tl_canvas.h"
#include "tl_widgets/tl_tool_bar.h"
#include "tl_widgets/tl_shape_list.h"
#include "tl_widgets/tl_label_list.h"
#include "tl_widgets/tl_label_file.h"
#include "tl_widgets/tl_label_dialog.h"
#include "tl_widgets/zoom_widget.h"
#include "tl_widgets/tl_files_list.h"
#include "tl_widgets/tl_train_widget.h"

#include "tl_modules/sam_assist_annotation.h"
#include "tl_modules/sam_prompt_annotation.h"
#include "yaml-cpp/yaml.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
    enum class ZoomMode : int32_t {
        FIT_WIDTH,
        FIT_WINDOW,
        MANUAL_ZOOM
    };
    using Menus = struct {
        QAction *file_;
        QAction *edit_;
    };

public:
    MainWindow(const QString &config_file,
               const YAML::Node &config_overrides,
               const QString &file_name,
               const QString &output_file,
               const QString &output_dir);
    ~MainWindow() override;


private slots:
    // slot for DeepLearning
    void slotActionSetup();
    void slotActionTrain();
    void slotActionInfer();
    void slotReadyReadStandardOutput();
    void slotReadyReadStandardError();
    void slotFinishedProcess(int32_t exitCode, QProcess::ExitStatus exitStatus);
    void slotProcessExited(int32_t exitCode, QProcess::ExitStatus exitStatus);
    void slotError(QProcess::ProcessError);


private:
    Ui::MainWindow                                 *ui_{nullptr};
    TlToolBar                                      *tool_bar_{nullptr};
    QToolBar                                       *tool_bar_ex_{nullptr};
    QLabel                                         *status_left_{nullptr};
    QLabel                                         *status_right_{nullptr};

    YAML::Node                                      config_;
    QSettings                                       settings_;
    QByteArray                                      default_state_;
    QString                                         config_file_;
    QString                                         output_dir_;
    QString                                         output_file_;
    QString                                         prev_opened_dir_;

    bool                                            is_changed_{false};
    QList<TlShape>                                  copied_shapes_;

    Canvas                                         *canvas_{nullptr};
    QScrollArea                                    *scroll_area_{nullptr};
    QMap<Qt::Orientation, QScrollBar *>             scroll_bars_;
    QMap<Qt::Orientation, QMap<QString, int32_t>>   scroll_values_;

    ZoomMode                                        zoom_mode_{ZoomMode::FIT_WINDOW};
    int32_t                                         zoom_level_{100};
    bool                                            fit_window_{false};
    QMap<QString, std::pair<ZoomMode, float>>       zoom_values_;
    QMap<QString, std::pair<int32_t, int32_t>>      brightness_contrast_values_;
    QMap<ZoomMode, std::function<float()>>          scalers_;

    QDockWidget                                    *flags_dock_{nullptr};
    QListWidget                                    *flags_list_{nullptr};       // 标志列表

    QDockWidget                                    *label_dock_{nullptr};
    TlLabelList                                    *label_list_{nullptr};       // 标签列表
    QMenu                                          *label_menu_{nullptr};
    TlLabelDialog                                  *label_dialog_{nullptr};

    QDockWidget                                    *shape_dock_{nullptr};
    TlShapeList                                    *shape_list_{nullptr};       // 轮廓列表

    QDockWidget                                    *files_dock_{nullptr};
    TlFilesList                                    *files_list_{nullptr};       // 文件列表
    QLineEdit                                      *files_search_{nullptr};
    QVBoxLayout                                    *files_layout_{nullptr};
    QWidget                                        *files_widget_{nullptr};

    QWidgetAction                                  *zoom_{nullptr};
    ZoomWidget                                     *zoom_widget_{nullptr};

    QImage                                          image_;
    QString                                         filename_;
    QString                                         imagePath_;
    QStringList                                     recentFiles_;
    int32_t                                         maxRecent_{0};
    QByteArray                                      imageData_;
    QByteArray                                      other_data_;
    std::unique_ptr<TlLabelFile>                    labelFile_;

    std::string                                     sam_model_name_{"efficientsam:latest"};
    std::unique_ptr<SamSession>                     text_osam_session_{nullptr};

    QAction                                        *actSetup_{nullptr};
    QAction                                        *actTrain_{nullptr};
    QAction                                        *actInfer_{nullptr};

    SamAssistAnnotation                            *ai_assisted_annotation_widget_{nullptr};
    SamPromptAnnotation                            *ai_text_to_annotation_widget_{nullptr};
    QWidgetAction                                  *select_ai_model_{nullptr};
    QWidgetAction                                  *ai_prompt_action_{nullptr};

    std::list<QAction *>                            actions_;
    std::list<QAction *>                            on_shapes_present_actions_;
    std::list<std::pair<QString, QAction *>>        draw_actions_;
    std::list<QAction *>                            zoom_actions_;
    std::list<QAction *>                            on_load_active_actions_;
    std::list<QObject *>                            context_menu_actions_;
    std::list<QAction *>                            edit_menu_actions_;
    std::list<QMenu *>                              menus_;

    QMenu                                          *menu_file_{nullptr};
    QMenu                                          *menu_edit_{nullptr};
    QMenu                                          *menu_view_{nullptr};
    QMenu                                          *menu_help_{nullptr};
    QMenu                                          *menu_recent_{nullptr};

    TlTrainWidget                                  *train_widget_{nullptr};
    QProcess                                       *sub_process_;


    QString load_config(QString config_file, const YAML::Node &config_overrides);
    QMenu *menu(const QString &title, const std::list<QObject *> &actions={});
    bool noShapes();
    void populateModeActions();
    QString get_window_title(bool dirty);
    void setDirty();
    void setClean();
    void toggleActions(bool value=true);
    //void queueEvent(function);
    void show_status_message(const QString &message, int32_t delay=5000);
    void set_ai_model_name(const std::string &name);
    void submit_ai_prompt();
    void resetState();
    QListWidgetItem *currentItem();
    void addRecentFile(const QString &filename);
    void undoShapeEdit();
    void tutorial();
    void about();
    void toggleDrawingSensitive(bool drawing=true);
    void switch_canvas_mode(bool edit, const QString &createMode="");
    void updateFileMenu();
    void popLabelListMenu(const QPoint &point);
    bool validateLabel(const QString &label);
    void edit_label(bool value=false);
    void fileSearchChanged();
    void fileSelectionChanged();
    void shapeSelectionChanged(const QList<int32_t> &selected_shapes);
    void addLabel(TlShape &shape);
    void update_shape_color(TlShape &shape);
    std::vector<int32_t> get_rgb_by_label(const QString &label);
    void remLabels(const QList<TlShape> &shapes);
    void load_shapes(QList<TlShape> &shapes, bool replace=true);
    void load_shape_dicts(const QList<ShapeDict> &shapes);
    void load_flags(const YAML::Node &flags) const;
    bool saveLabels(const QString &filename);

    void duplicateSelectedShape();
    void pasteSelectedShape();
    void copySelectedShape();
    void label_selection_changed();
    void labelItemChanged(TlShapeListItem *item);
    void labelOrderChanged();
    void newShape();
    void scrollRequest(int32_t delta, Qt::Orientation orientation);
    void setScroll(Qt::Orientation orientation, float value);
    void set_zoom(int32_t value, QPointF pos=QPointF());
    void set_zoom_to_original();
    void add_zoom(float increment=1.1, QPointF pos=QPointF());
    void zoom_requested(int32_t delta, QPointF pos);
    void setFitWindow(bool value=true);
    void setFitWidth(bool value=true);
    void enableKeepPrevScale(bool enabled);
    void onNewBrightnessContrast(const QImage &image);
    void brightnessContrast(bool value=false, bool is_initial_load=false);
    void toggleShapes(int32_t value);
    bool load_file(QString filename);
    void resizeEvent(QResizeEvent *event) override;
    void paint_canvas();
    void adjust_scale(bool initial=false);
    float scaleFitWindow() const;
    float scaleFitWidth() const;
    void enableSaveImageWithData(bool enabled);
    void reset_layout();
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void loadRecent(const QString &filename);
    void open_prev_image(bool value=false);
    void open_next_image(bool value=false);
    void open_file_with_dialog(bool value=false);
    void changeOutputDirDialog(bool value=false);
    void saveFile(bool value=false);
    void saveFileAs(bool value=false);
    QString saveFileDialog();
    void saveFile_(const QString &filename);
    void closeFile(bool value=false);
    QString getLabelFile();
    void deleteFile();
    void open_config_file();
    bool hasLabels();
    bool hasLabelFile();
    bool can_continue();
    void errorMessage(const QString &title, const QString &message);
    QString currentPath();
    void toggleKeepPrevMode();
    void removeSelectedPoint();
    void deleteSelectedShape();
    void copyShape();
    void moveShape();
    void open_dir_with_dialog(bool value=false);
    QStringList imageList();
    void importDroppedImageFiles(const QStringList &imageFiles);
    void import_images_from_dir(const QString &root_dir, const QString &pattern="");
    void update_status_stats(const QPointF &mouse_pos);
    QStringList scan_image_files(const QString &folderPath) const;

private slots:

private:
    QAction                *quit_{nullptr};
    QAction                *open_{nullptr};
    QAction                *open_config_{nullptr};
    QAction                *opendir_{nullptr};
    QAction                *openNextImg_{nullptr};
    QAction                *openPrevImg_{nullptr};
    QAction                *save_{nullptr};
    QAction                *saveAs_{nullptr};
    QAction                *deleteFile_{nullptr};
    QAction                *changeOutputDir_{nullptr};
    QAction                *saveAuto_{nullptr};
    QAction                *saveWithImageData_{nullptr};
    QAction                *close_{nullptr};
    QAction                *toggle_keep_prev_mode_{nullptr};
    QAction                *toggle_keep_prev_brightness_contrast_{nullptr};
    QAction                *reset_layout_{nullptr};

    QAction                *createMode_{nullptr};
    QAction                *createRectangleMode_{nullptr};
    QAction                *createCircleMode_{nullptr};
    QAction                *createLineMode_{nullptr};
    QAction                *createPointMode_{nullptr};
    QAction                *createLineStripMode_{nullptr};
    QAction                *createAiPolygonMode_{nullptr};
    QAction                *createAiMaskMode_{nullptr};

    QAction                *editMode_{nullptr};
    QAction                *delete_{nullptr};
    QAction                *duplicate_{nullptr};
    QAction                *copy_{nullptr};
    QAction                *paste_{nullptr};
    QAction                *undoLastPoint_{nullptr};
    QAction                *removePoint_{nullptr};
    QAction                *undo_{nullptr};

    QAction                *hideAll_{nullptr};
    QAction                *showAll_{nullptr};
    QAction                *toggleAll_{nullptr};
    QAction                *help_{nullptr};
    QAction                *about_{nullptr};

    QAction                *zoomIn_{nullptr};
    QAction                *zoomOut_{nullptr};
    QAction                *zoomOrg_{nullptr};
    QAction                *keepPrevScale_{nullptr};
    QAction                *fitWindow_{nullptr};
    QAction                *fitWidth_{nullptr};
    QAction                *brightnessContrast_{nullptr};

    QAction                *edit_{nullptr};
    QAction                *fill_drawing_{nullptr};
};
#endif // __INC_MAINWINDOW_H