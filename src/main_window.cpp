#include "main_window.h"
#include "audio/playback_controller.h"
#include "timeline_view.h"
#include "ui/preview_widget.h"
#include "scripting/script_host.h"
#include "menus.h"
#include "app_actions.h"
#include "project/osu_parser.h"
#include "project/project_file.h"
#include "project/project_creator.h"
#include "project/beatmap_updater.h"
#include "project/path_utils.h"
#include "project/asset_paths.h"
#include "project/storyboard_export.h"
#include "project/asset_check.h"
#include "core/osb_writer.h"
#include "ui/script_editor.h"
#include "ui/script_editor_factory.h"

#include <QAction>
#include <QAudio>
#include <QCloseEvent>
#include <QDockWidget>
#include <QToolBar>
#include <QComboBox>
#include <QSaveFile>
#include <QTimer>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QFontDatabase>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QWidgetAction>
#include <QGridLayout>

#include <algorithm>

namespace {
    QStringList find_osu_files(const QString& folder){
        QDir dir(folder);
        
        return dir.entryList(QStringList() << "*.osu", QDir::Files);
    }

    QString choose_osu_file(QWidget* parent, const QString& folder_path, const QStringList& osu_files){
        if(osu_files.isEmpty()){
            return {};
        }

        if(osu_files.size() == 1){
            return QDir(folder_path).filePath(osu_files.first());
        }

        bool ok = false;

        const QString choice = QInputDialog::getItem(
                parent,
                "Select Beatmap",
                "Choose Difficulty (.osu)",
                osu_files,
                0,
                false,
                &ok
                );

        if(!ok || choice.isEmpty()){
            return {};
        }

        return QDir(folder_path).filePath(choice);
    }

    std::optional<BeatmapImportInfo> read_beatmap(const QString& beatmap_file_path, QString* error){
        const QFileInfo beatmap_file_info(beatmap_file_path);

        if(!beatmap_file_info.exists() ||
                !beatmap_file_info.isFile() ||
                !beatmap_file_info.isReadable()
                ){
            if(error){
                *error = QString("Beatmap file could not be opened: %1").arg(beatmap_file_path);
            }

            return std::nullopt;
        }

        const BeatmapImportInfo beatmap = parse_beatmap_metadata(beatmap_file_path);

        if(!beatmap.is_valid()){
            if(error){
                *error = "Beatmap metadata could not be parsed";
            }

            return std::nullopt;
        }

        return beatmap;
    }

    bool write_text_file(const QString& path, const QString& contents, QString* error){
        if(!QDir().mkpath(QFileInfo(path).absolutePath())){
            *error = QString("Could not create the folder for %1").arg(path);
            return false;
        }

        // QSaveFile writes to a temporary file and renames it over the
        // target, so a failed save never leaves a half-written script.
        QSaveFile file(path);

        if(!file.open(QIODevice::WriteOnly)){
            *error = QString("Could not open %1 for writing: %2").arg(path, file.errorString());
            return false;
        }

        const QByteArray data = contents.toUtf8();

        if(file.write(data) != data.size()){
            *error = QString("Could not write %1: %2").arg(path, file.errorString());
            file.cancelWriting();
            return false;
        }

        if(!file.commit()){
            *error = QString("Could not save %1: %2").arg(path, file.errorString());
            return false;
        }

        return true;
    }
}


static bool load_text_file(const QString& path, QString& contents, QString* error){
    if(error){
        error->clear();
    }

    QFile file(path);

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        if(error){
            *error = QString("Could not open script file: %1").arg(file.errorString());
        }

        return false;
    }

    QTextStream stream(&file);
    contents = stream.readAll();

    return true;
}

MainWindow::MainWindow(QWidget* parent):
    QMainWindow(parent),
    m_timeline(new TimelineView(this)),
    m_preview(new PreviewWidget(this)),
    m_script(new script_host(this)),
    m_playback(new playback_controller(this))
{
    //center: preview
    setCentralWidget(m_preview);

    m_actions = new AppActions(this);

    //objectNames let saveState()/restoreState() remember the layout
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);

    //bottom: timeline
    m_dock_timeline = new QDockWidget(tr("Timeline"), this);
    m_dock_timeline->setObjectName("dock_timeline");
    m_dock_timeline->setWidget(m_timeline);
    addDockWidget(Qt::BottomDockWidgetArea, m_dock_timeline);

    //bottom right: script output and errors
    create_output_dock();

    //right: code editor (Monaco when available, plain text otherwise)
    m_dock_code = new QDockWidget(tr("Script (JS)"), this);
    m_dock_code->setObjectName("dock_script");
    addDockWidget(Qt::RightDockWidgetArea, m_dock_code);

    QString editor_notice;
    attach_editor(create_script_editor(m_dock_code, &editor_notice));

    //click to focus, so Space (play/pause) works there
    m_preview->setFocusPolicy(Qt::ClickFocus);
    m_timeline->setFocusPolicy(Qt::ClickFocus);
    m_preview->addAction(m_actions->act_play_pause);
    m_timeline->addAction(m_actions->act_play_pause);

    create_toolbar();

    //playback drives the preview and the timeline's playhead
    connect(m_timeline, &TimelineView::requestSeek, m_playback, &playback_controller::seek);
    connect(m_timeline, &TimelineView::scrubStarted, m_playback, &playback_controller::begin_scrub);
    connect(m_timeline, &TimelineView::scrubFinished, m_playback, &playback_controller::end_scrub);
    connect(m_playback, &playback_controller::timeChanged, m_preview, &PreviewWidget::set_time_ms);
    connect(m_playback, &playback_controller::timeChanged, m_timeline, &TimelineView::setPlayheadMs);
    connect(m_playback, &playback_controller::playingChanged, this, &MainWindow::update_playback_action);
    connect(m_playback, &playback_controller::songProblem, this, [this](const QString& message){
        append_output(message, OutputKind::warning);
    });
    connect(m_playback, &playback_controller::sampleProblem, this, [this](const QString& path, const QString& reason){
        append_output(tr("%1: can't play this sound (%2)").arg(QDir::toNativeSeparators(path), reason),
                      OutputKind::warning);
    });

    //timeline -> code and preview
    connect(m_timeline, &TimelineView::sourceLineActivated, this, [this](int line){
        m_dock_code->show();
        m_dock_code->raise();
        m_editor->go_to_line(line);
    });
    connect(m_timeline, &TimelineView::layerVisibilityChanged, m_preview, &PreviewWidget::set_layer_visible);

    connect(m_script, &script_host::output, this, [this](const QString& text){
        append_output(text, OutputKind::print);
    });

    create_menus();
    m_actions->connect_slots(this);

    m_actions->menu_view->addAction(m_dock_code->toggleViewAction());
    m_actions->menu_view->addAction(m_dock_timeline->toggleViewAction());
    m_actions->menu_view->addAction(m_dock_output->toggleViewAction());
    m_actions->menu_view->addSeparator();
    m_actions->menu_view->addAction(m_timeline->compactAction());
    rebuild_recent_menu();

    restore_window_layout();

    update_window_title();
    update_action_state();
    update_bpm_display();
    update_timeline_hint();

    if(!editor_notice.isEmpty()){
        show_info(editor_notice);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::create_toolbar(){
    auto* tb = addToolBar(tr("Main"));
    tb->setObjectName("toolbar_main");
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize(QSize(18, 18));

    tb->addAction(m_actions->act_open);
    tb->addAction(m_actions->act_save);
    tb->addSeparator();
    tb->addAction(m_actions->act_run);
    tb->addSeparator();
    tb->addAction(m_actions->act_play_pause);
    tb->addAction(m_actions->act_stop);
    create_audio_controls(tb);
    tb->addSeparator();

    //beatmap BPM (hidden until a project is open)
    m_bpm_label = new QLabel(tb);
    m_bpm_label->setObjectName("toolbar_bpm");
    m_bpm_label->setToolTip(tr("Beatmap BPM"));
    m_bpm_action = tb->addWidget(m_bpm_label);

    //beat division selector
    auto* snap_label = new QLabel(tr("Snap "), tb);
    tb->addWidget(snap_label);

    auto* division_box = new QComboBox(tb);
    for(const int division : {1, 2, 3, 4, 6, 8, 12, 16, 32}){
        division_box->addItem(QString("1/%1").arg(division), division);
    }
    division_box->setCurrentIndex(division_box->findData(4));  // TimelineView's default
    division_box->setToolTip(tr("Beat snap divisor for the timeline"));
    division_box->setFocusPolicy(Qt::ClickFocus);

    connect(division_box, &QComboBox::currentIndexChanged, this, [this, division_box](int index){
        m_timeline->setSubdivision(division_box->itemData(index).toInt());
    });
    tb->addWidget(division_box);

    //push export actions to the right edge
    auto* spacer = new QWidget(tb);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    tb->addAction(m_actions->act_export);
}

void MainWindow::create_output_dock(){
    m_output = new QListWidget(this);
    m_output->setWordWrap(true);
    m_output->setUniformItemSizes(false);
    m_output->setSelectionMode(QAbstractItemView::SingleSelection);
    QFont output_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    output_font.setPointSizeF(std::max(output_font.pointSizeF(), 11.0));
    m_output->setFont(output_font);
    m_output->setToolTip(tr("Script output. Double-click an error to jump to its line."));

    connect(m_output, &QListWidget::itemActivated, this, &MainWindow::on_output_activated);

    m_dock_output = new QDockWidget(tr("Output"), this);
    m_dock_output->setObjectName("dock_output");
    m_dock_output->setWidget(m_output);
    addDockWidget(Qt::BottomDockWidgetArea, m_dock_output);
    splitDockWidget(m_dock_timeline, m_dock_output, Qt::Horizontal);
}

void MainWindow::append_output(const QString& text, OutputKind kind, int line){
    // Cap the panel so a print() in a big loop can't grow it forever.
    constexpr int max_items = 2000;

    while(m_output->count() >= max_items){
        delete m_output->takeItem(0);
    }

    auto* item = new QListWidgetItem(text, m_output);

    switch(kind){
        case OutputKind::info:
            item->setForeground(QColor(0x80, 0x80, 0x88));
            break;
        case OutputKind::print:
            break;
        case OutputKind::warning:
            item->setForeground(QColor(0xe5, 0xc0, 0x7b));
            item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
            item->setData(Qt::UserRole, line);
            break;
        case OutputKind::error:
            item->setForeground(QColor(0xf4, 0x87, 0x71));
            item->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
            item->setData(Qt::UserRole, line);
            break;
    }

    m_output->scrollToItem(item);
}

void MainWindow::on_output_activated(QListWidgetItem* item){
    const int line = item ? item->data(Qt::UserRole).toInt() : 0;

    if(line > 0){
        m_editor->go_to_line(line);
    }
}

void MainWindow::on_play(){
    m_playback->toggle_playback();
}

void MainWindow::on_stop(){
    m_playback->stop();
}

void MainWindow::update_playback_action(bool playing){
    QAction* action = m_actions->act_play_pause;

    action->setText(playing ? tr("&Pause") : tr("&Play"));
    action->setIcon(style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
}

void MainWindow::update_bpm_display(){
    const bool has_project = !m_project_file_path.isEmpty();

    m_bpm_label->setText(has_project ? tr("%1 BPM ").arg(m_bpm, 0, 'g', 6) : QString());
    m_bpm_action->setVisible(has_project);
}


/**
 * fucntions for menu bar below
 */
//file
void MainWindow::create_menus(){
    Menus::build(menuBar(), m_actions);
}

void MainWindow::on_new_project_from_beatmap(){
    after_editor_sync([this]{
        if(maybe_save()){
            new_project_from_beatmap();
        }
    });
}

void MainWindow::new_project_from_beatmap(){
    const QString folder = QFileDialog::getExistingDirectory(
            this,
            "Select Beatmap Folder",
            QDir::homePath()
            );

    if(folder.isEmpty()){
        return;
    }

    const QStringList osu_files = find_osu_files(folder);
    if(osu_files.isEmpty()){
        report_error("Create Project Failed", QString("No .osu files found in:\n%1").arg(folder));
        return;
    }

    const QString osu_file_path = choose_osu_file(this, folder, osu_files);
    if(osu_file_path.isEmpty()){
        return;
    }

    const QString project_directory = QFileDialog::getExistingDirectory(
            this,
            "Select Empty Project Folder",
            QDir::homePath()
            );

    if(project_directory.isEmpty()){
        return;
    }

    QString error;

    const auto project_file_path = create_project_from_directory(
            folder,
            osu_file_path,
            project_directory,
            &error
            );

    if(!project_file_path.has_value()){
        report_error("Create Project Failed", error);
        return;
    }

    open_project_file(*project_file_path);
}

void MainWindow::on_update_beatmap(){
    if(m_project_file_path.isEmpty()){
        show_error("Open a project before updating its beatmap");
        return;
    }

    QString error;

    const auto project = load_project_file(m_project_file_path, &error);

    if(!project.has_value()){
        report_error("Update Beatmap Failed", error);
        return;
    }

    // The stored source is only a hint: lazer's external-edit mount is temporary.
    const QString start_directory = QFileInfo(project->source_directory).isDir()
        ? project->source_directory
        : QDir::homePath();

    const QString folder = QFileDialog::getExistingDirectory(
            this,
            "Select Updated Beatmap Folder",
            start_directory
            );

    if(folder.isEmpty()){
        return;
    }

    const QStringList osu_files = find_osu_files(folder);
    if(osu_files.isEmpty()){
        report_error("Update Beatmap Failed", QString("No .osu files found in:\n%1").arg(folder));
        return;
    }

    // Keep the current difficulty if it still exists, otherwise ask again
    // (renaming a difficulty in osu! renames its .osu file).
    const QString current_difficulty = QFileInfo(m_osu_file).fileName();

    const QString osu_file_path = osu_files.contains(current_difficulty)
        ? QDir(folder).filePath(current_difficulty)
        : choose_osu_file(this, folder, osu_files);

    if(osu_file_path.isEmpty()){
        return;
    }

    // Let go of the song file before its folder is replaced.
    m_playback->stop();
    const QString old_song = m_playback->song();
    m_playback->set_song({});

    const auto beatmap_file = update_beatmap_snapshot(
            m_project_file_path,
            folder,
            osu_file_path,
            &error
            );

    if(!beatmap_file.has_value()){
        m_playback->set_song(old_song);     // the old beatmap is still there
        report_error("Update Beatmap Failed", error);
        return;
    }

    const auto beatmap = read_beatmap(QDir(m_project_root).filePath(*beatmap_file), &error);

    if(!beatmap.has_value()){
        // The new copy is in place but unreadable: its song may still play.
        m_playback->set_song(old_song);
        report_error("Update Beatmap Failed", error);
        return;
    }

    // Only the beatmap is reloaded; the script editor (and unsaved edits) is untouched.
    apply_beatmap(*beatmap);

    show_info(QString("Beatmap updated: BPM %1").arg(m_bpm));
}

void MainWindow::on_open_project(){
    after_editor_sync([this]{
        if(maybe_save()){
            open_project();
        }
    });
}

void MainWindow::open_project(){
    const QString project_file_path = QFileDialog::getOpenFileName(
            this,
            "Open Project",
            QDir::homePath(),
            "Cosby Projects (*.cosby)"
            );

    if(project_file_path.isEmpty()){
        return;
    }

    open_project_file(project_file_path);
}

void MainWindow::on_save_project(){
    if(m_project_root.isEmpty()){
        show_error("Open or create a project before saving");
        return;
    }

    after_editor_sync([this]{
        save_script();
    });
}

bool MainWindow::save_script(){
    if(m_project_root.isEmpty()){
        show_error("Open or create a project before saving");
        return false;
    }

    const QString root = QDir::cleanPath(QFileInfo(m_project_root).absoluteFilePath());
    const QString path = QDir::cleanPath(QDir(root).absoluteFilePath(m_script_relative_path));

    // scriptFile comes from project.cosby; never write outside the project.
    if(path == root || !same_or_child_path(root, path)){
        QMessageBox::critical(this, "Save Failed",
                QString("The project's script file is outside the project folder:\n%1").arg(path));
        return false;
    }

    const QString text = m_editor->text();
    QString error;

    if(!write_text_file(path, text, &error)){
        QMessageBox::critical(this, "Save Failed", error);
        return false;
    }

    m_editor->mark_saved(text);
    show_info(QString("Saved %1").arg(m_script_relative_path));

    return true;
}

bool MainWindow::maybe_save(){
    if(!m_editor->is_modified()){
        return true;
    }

    if(m_project_root.isEmpty()){
        const auto answer = QMessageBox::warning(this, "Unsaved Script",
                "This script is not part of a project and will be lost.\nDiscard it?",
                QMessageBox::Discard | QMessageBox::Cancel,
                QMessageBox::Cancel);

        return answer == QMessageBox::Discard;
    }

    const auto answer = QMessageBox::warning(this, "Unsaved Changes",
            QString("Save changes to %1?").arg(m_script_relative_path),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

    if(answer == QMessageBox::Save){
        return save_script();
    }

    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* event){
    if(m_close_confirmed){
        event->accept();
        return;
    }

    // The editor may still have to deliver its last keystroke; ask after that.
    event->ignore();

    after_editor_sync([this]{
        if(!maybe_save()){
            return;
        }

        m_close_confirmed = true;
        save_window_layout();

        // Not close() directly: this may still run inside closeEvent(),
        // where a nested close() is ignored by Qt.
        QTimer::singleShot(0, this, &QWidget::close);
    });
}

//project tab
void MainWindow::on_run_script(){
    after_editor_sync([this]{
        run_script();
    });
}

bool MainWindow::run_script(){
    // Each run starts a fresh log; print() output arrives during evaluate().
    m_output->clear();

    const auto error = m_script->evaluate(m_editor->text());

    if(error.has_value()){
        QList<ScriptError> errors;
        const bool in_script = !error->in_preload && error->line > 0;

        if(in_script){
            errors.append(ScriptError{error->line, 0, error->message});
        }

        m_editor->show_errors(errors);

        // One multi-arg call: chained .arg() would substitute into a message containing "%3".
        const QString message = QString("%1: %2 (line %3)").arg(
                    error->in_preload ? QStringLiteral("Preload error") : QStringLiteral("Error"),
                    error->message,
                    QString::number(error->line));

        append_output(message, OutputKind::error, in_script ? error->line : 0);
        m_dock_output->show();
        m_dock_output->raise();

        m_last_run = ScriptRun::failed;
        update_timeline_hint();
        return false;
    }

    m_editor->show_errors({});
    m_last_run = ScriptRun::succeeded;
    update_timeline_hint();

    m_preview->set_scene(&m_script->current_scene());
    m_timeline->setScene(&m_script->current_scene());

    const scene& sc = m_script->current_scene();

    // Without a project there's nowhere to find sound files.
    m_playback->set_samples(sc.samples, m_project_root.isEmpty()
            ? QStringList() : asset_folders(m_project_root, m_asset_root));
    const QString result = tr("Script ran successfully: %n sprite(s)", nullptr, int(sc.sprites.size()));

    show_info(result);

    // Every run, so a fixed file name or a newly added image shows up at once.
    if(m_project_root.isEmpty()){
        if(!sc.sprites.isEmpty() || !sc.samples.isEmpty()){
            append_output(tr("No project open: images can't be loaded, so the preview shows "
                             "magenta placeholders. Open or create a project to see them."),
                          OutputKind::warning);
        }
    }
    else{
        for(const asset_problem& problem : check_assets(sc, asset_folders(m_project_root, m_asset_root))){
            append_output(problem.message, OutputKind::warning, problem.source_line);
        }
    }

    return true;
}

void MainWindow::on_export_osb(){
    if(m_project_root.isEmpty()){
        show_error("Open or create a project before exporting");
        return;
    }

    after_editor_sync([this]{
        export_storyboard();
    });
}

void MainWindow::export_storyboard(){
    // Export what the script makes now, not what it made at the last run.
    if(!run_script()){
        show_error("Nothing exported: fix the script error first");
        return;
    }

    const scene& sc = m_script->current_scene();

    QString error;
    const auto project = load_project_file(m_project_file_path, &error);

    if(!project.has_value()){
        report_error("Export Failed", error);
        return;
    }

    // The folder the beatmap came from is where osu! reads it. lazer's
    // external-edit folder is temporary, so it may be gone.
    QString target = project->source_directory;

    if(!QFileInfo(target).isDir()){
        target = QFileDialog::getExistingDirectory(
                this,
                "Choose the Beatmap Folder to Export To",
                QDir::homePath()
                );

        if(target.isEmpty()){
            return;
        }
    }

    const QString assets = QDir(m_project_root).filePath(assets_folder_name);

    const auto plan = plan_export(sc, target, osb_file_name(m_osu_file),
            m_project_root, assets, m_asset_root, &error);

    if(!plan.has_value()){
        report_error("Export Failed", error);
        return;
    }

    // Never overwrite silently.
    if(plan->replaces_files() || !plan->other_osb_files.isEmpty()){
        QStringList replaced;

        if(plan->osb_replaces){
            replaced << QFileInfo(plan->osb_path).fileName();
        }

        for(const asset_copy& copy : plan->copies){
            if(copy.replaces){
                replaced << copy.path;
            }
        }

        QString details;

        if(!replaced.isEmpty()){
            details += "This replaces:\n    " + replaced.join("\n    ");
        }

        if(!plan->other_osb_files.isEmpty()){
            if(!details.isEmpty()){
                details += "\n\n";
            }

            details += QString("Also in this folder: %1\nosu!lazer may load that instead of %2.")
                .arg(plan->other_osb_files.join(", "), QFileInfo(plan->osb_path).fileName());
        }

        QMessageBox box(QMessageBox::Question, "Export Storyboard",
                QString("Export the storyboard to\n%1?").arg(QDir::toNativeSeparators(plan->target_folder)),
                QMessageBox::Cancel, this);
        box.setInformativeText(details);
        QPushButton* export_button = box.addButton("Export", QMessageBox::AcceptRole);
        box.setDefaultButton(export_button);
        box.exec();

        if(box.clickedButton() != export_button){
            return;
        }
    }

    QList<osb_warning> warnings;
    const QString osb_text = write_osb(sc, &warnings);

    if(!apply_export(*plan, osb_text, &error)){
        report_error("Export Failed", error);
        return;
    }

    for(const QString& path : plan->missing){
        warnings.append({QString("%1: not found in assets/ or the beatmap folder; "
                                 "osu! will skip it").arg(path), 0});
    }

    if(sc.sprites.isEmpty() && sc.samples.isEmpty()){
        warnings.append({"The storyboard is empty: the script made no sprites or samples", 0});
    }

    for(const osb_warning& warning : warnings){
        append_output(warning.message, OutputKind::warning, warning.line);
    }

    show_info(tr("Exported %n sprite(s) to %1", nullptr, int(sc.sprites.size()))
            .arg(QDir::toNativeSeparators(plan->osb_path)));

    if(!plan->copies.isEmpty()){
        show_info(tr("Copied %n file(s) into the beatmap folder", nullptr, int(plan->copies.size())));
    }

    if(!warnings.isEmpty()){
        m_dock_output->show();
        m_dock_output->raise();
    }
}

//about tab
void MainWindow::on_about(){
    QMessageBox::about(this, "About cosby",
            QString("cosby storyboard editor\nWrite osu! storyboards in JavaScript, export them as .osb\n\nScript editor: %1")
                .arg(m_editor->backend_name()));
}

void MainWindow::show_info(const QString& message){
    append_output(message, OutputKind::info);
}

void MainWindow::show_error(const QString& message){
    append_output(message, OutputKind::error);
    m_dock_output->show();
    m_dock_output->raise();
}

void MainWindow::report_error(const QString& title, const QString& message){
    append_output(QString("%1: %2").arg(title, message), OutputKind::error);
    QMessageBox::critical(this, title, message);
}

/**
 * recent projects and window layout, stored with QSettings
 */
namespace {
    const char* const recent_key = "recentProjects";
    constexpr int max_recent = 8;

    QStringList load_recent(){
        return QSettings().value(recent_key).toStringList();
    }

    void store_recent(const QStringList& list){
        QSettings().setValue(recent_key, list);
    }
}

void MainWindow::add_recent_project(const QString& project_file_path){
    const QString path = QFileInfo(project_file_path).absoluteFilePath();

    QStringList list = load_recent();
    list.removeAll(path);
    list.prepend(path);

    while(list.size() > max_recent){
        list.removeLast();
    }

    store_recent(list);
    rebuild_recent_menu();
}

void MainWindow::rebuild_recent_menu(){
    QMenu* menu = m_actions->menu_recent;
    menu->clear();

    const QStringList list = load_recent();

    for(int i = 0; i < list.size(); ++i){
        const QString& path = list.at(i);
        const QFileInfo info(path);

        // "&1 name", with the folder in the tooltip.
        QString label = QString("%1 %2").arg(i + 1).arg(info.dir().dirName());
        if(i < 9){
            label.prepend('&');
        }

        QAction* action = menu->addAction(label);
        action->setToolTip(QDir::toNativeSeparators(path));

        connect(action, &QAction::triggered, this, [this, path]{
            open_recent_project(path);
        });
    }

    if(!list.isEmpty()){
        menu->addSeparator();
    }

    menu->addAction(m_actions->act_clear_recent);
    m_actions->act_clear_recent->setEnabled(!list.isEmpty());
    menu->setToolTipsVisible(true);
}

void MainWindow::open_recent_project(const QString& project_file_path){
    after_editor_sync([this, project_file_path]{
        if(!maybe_save()){
            return;
        }

        if(!QFileInfo::exists(project_file_path)){
            const auto answer = QMessageBox::warning(this, "Project Not Found",
                    QString("%1\nno longer exists. Remove it from the recent list?")
                        .arg(QDir::toNativeSeparators(project_file_path)),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

            if(answer == QMessageBox::Yes){
                QStringList list = load_recent();
                list.removeAll(project_file_path);
                store_recent(list);
                rebuild_recent_menu();
            }
            return;
        }

        open_project_file(project_file_path);
    });
}

void MainWindow::on_clear_recent_projects(){
    store_recent({});
    rebuild_recent_menu();
}

namespace {
    const QString timeline_compact_key = QStringLiteral("timeline/compact");
    const QString timeline_detailed_height_key = QStringLiteral("timeline/detailed_height");
}

void MainWindow::restore_window_layout(){
    QSettings settings;

    if(!restoreGeometry(settings.value("window/geometry").toByteArray())){
        // First start: 1400x900, but never larger than the screen.
        QSize size(1400, 900);

        if(const QScreen* screen = this->screen()){
            size = size.boundedTo(screen->availableSize() * 0.9);
        }

        resize(size);

        // Give the script editor a useful share of the width.
        resizeDocks({m_dock_code}, {size.width() * 2 / 5}, Qt::Horizontal);
        resizeDocks({m_dock_timeline}, {260}, Qt::Vertical);
        resizeDocks({m_dock_timeline, m_dock_output},
                    {size.width() * 3 / 4, size.width() / 4}, Qt::Horizontal);
    }

    // Before the state, so a compact panel's saved height fits its minimum.
    m_timeline->setCompact(settings.value(timeline_compact_key, false).toBool());
    restoreState(settings.value("window/state").toByteArray());

    // From now on the panel shrinks and grows with the mode; the detailed
    // height comes back when leaving compact.
    connect(m_timeline, &TimelineView::compactChanged, this, [this](bool compact){
        QSettings settings;
        settings.setValue(timeline_compact_key, compact);

        if(m_dock_timeline->isFloating()){
            return;
        }

        if(compact){
            settings.setValue(timeline_detailed_height_key, m_dock_timeline->height());
            resizeDocks({m_dock_timeline}, {TimelineView::compact_height}, Qt::Vertical);
        }
        else{
            const int height = settings.value(timeline_detailed_height_key, 260).toInt();
            resizeDocks({m_dock_timeline}, {std::max(height, m_timeline->minimumSizeHint().height())}, Qt::Vertical);
        }
    });
}

void MainWindow::save_window_layout(){
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());
}

bool MainWindow::open_project_file(const QString& project_file_path){
    QString error;

    const auto project = load_project_file(project_file_path, &error);

    if(!project.has_value()){
        report_error("Open Project Failed", error);

        return false;
    }

    const QString project_root = QFileInfo(project_file_path).absolutePath();

    QString beatmap_file_path = project->beatmap_file;

    if(QDir::isRelativePath(beatmap_file_path)){
        beatmap_file_path = QDir(project_root).filePath(beatmap_file_path);
    }

    const auto beatmap = read_beatmap(beatmap_file_path, &error);

    if(!beatmap.has_value()){
        report_error("Open Project Failed", error);

        return false;
    }

    const QString script_file_path = QDir(project_root).filePath(project->script_file);

    QString script_contents;

    if(!load_text_file(script_file_path, script_contents, &error)){
        report_error("Open Project Failed", error);
        return false;
    }

    //Only replace the current project after everything loads successfully
    m_project_file_path = project_file_path;
    m_project_root = project_root;

    // Projects from before assets/ existed get one, so there's an obvious
    // place for storyboard images (failing is harmless: nothing is in it).
    QDir(project_root).mkpath(assets_folder_name);
    m_script_relative_path = project->script_file;

    // Drop everything that belonged to the previous project.
    m_playback->stop();
    m_playback->set_samples({}, {});
    m_preview->set_scene(nullptr);
    m_timeline->setScene(nullptr);
    m_timeline->showAllLayers();
    m_output->clear();
    m_last_run = ScriptRun::none;

    apply_beatmap(*beatmap);
    m_editor->set_text(script_contents);
    m_editor->show_errors({});

    add_recent_project(project_file_path);

    update_window_title();
    update_action_state();
    update_bpm_display();
    update_timeline_hint();

    show_info(tr("Opened %1").arg(QDir::toNativeSeparators(project_file_path)));

    return true;
}

void MainWindow::apply_beatmap(const BeatmapImportInfo& beatmap){
    m_asset_root = beatmap.beatmap_folder;
    m_osu_file = beatmap.osu_file_path;
    m_audio_file = beatmap.audio_file;
    m_background_file = beatmap.background_file;
    m_bpm = beatmap.bpm;

    m_timeline->setBpm(m_bpm);
    m_timeline->setBeatOffsetMs(beatmap.beat_offset_ms);
    m_preview->set_asset_folders(asset_folders(m_project_root, m_asset_root));
    m_playback->set_song(beatmap_file_path(m_audio_file));
    m_preview->set_placeholder_background(beatmap_file_path(m_background_file));

    // The difficulty (in the title) and BPM can change on a beatmap update.
    update_window_title();
    update_bpm_display();
}

QString MainWindow::beatmap_file_path(const QString& name) const{
    if(name.isEmpty() || m_asset_root.isEmpty()){
        return {};
    }

    // Not find_asset(): a missing song still gets a path, so the warning
    // can name it.
    const QString file = normalize_asset_path(name);

    return is_safe_asset_path(file) ? QDir(m_asset_root).filePath(file) : QString();
}

void MainWindow::update_timeline_hint(){
    QString hint;

    switch(m_last_run){
        case ScriptRun::failed:
            hint = tr("Script error: see Output");
            break;
        case ScriptRun::succeeded:
            hint = tr("The script made no sprites yet");
            break;
        case ScriptRun::none:
            hint = m_project_file_path.isEmpty()
                ? tr("Open or create a project to start")
                : tr("Press F5 (or Run) to run the script");
            break;
    }

    m_timeline->setEmptyHint(hint);
}

namespace {
    // Slider positions (0-100) are loudness as heard; the outputs want gain.
    float slider_gain(int position){
        return float(QAudio::convertVolume(position / 100.0, QAudio::LogarithmicVolumeScale,
                                           QAudio::LinearVolumeScale));
    }

    constexpr const char* music_volume_key = "audio/music_volume";
    constexpr const char* sample_volume_key = "audio/sample_volume";
    constexpr const char* muted_key = "audio/muted";
}

void MainWindow::create_audio_controls(QToolBar* tb){
    QSettings settings;

    //volume: click mutes, the arrow opens the sliders
    m_mute_button = new QToolButton(tb);
    m_mute_button->setObjectName("toolbar_mute");
    m_mute_button->setCheckable(true);
    m_mute_button->setPopupMode(QToolButton::MenuButtonPopup);
    m_mute_button->setFocusPolicy(Qt::NoFocus);

    auto* menu = new QMenu(m_mute_button);
    auto* panel = new QWidget(menu);
    auto* grid = new QGridLayout(panel);
    grid->setContentsMargins(10, 8, 10, 8);

    auto add_slider = [&](int row, const QString& label, const char* key, auto apply){
        auto* slider = new QSlider(Qt::Horizontal, panel);
        slider->setRange(0, 100);
        slider->setMinimumWidth(160);
        slider->setValue(std::clamp(settings.value(key, 100).toInt(), 0, 100));

        auto* value = new QLabel(panel);
        value->setMinimumWidth(36);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto update = [this, slider, value, key, apply](int position){
            value->setText(tr("%1%").arg(position));
            (m_playback->*apply)(slider_gain(position));
            QSettings().setValue(key, position);
        };

        connect(slider, &QSlider::valueChanged, this, update);
        update(slider->value());

        grid->addWidget(new QLabel(label, panel), row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(value, row, 2);
    };

    add_slider(0, tr("Music"), music_volume_key, &playback_controller::set_music_volume);
    add_slider(1, tr("Samples"), sample_volume_key, &playback_controller::set_sample_volume);

    auto* panel_action = new QWidgetAction(menu);
    panel_action->setDefaultWidget(panel);
    menu->addAction(panel_action);
    m_mute_button->setMenu(menu);

    m_mute_button->setChecked(settings.value(muted_key, false).toBool());
    m_playback->set_muted(m_mute_button->isChecked());
    connect(m_mute_button, &QToolButton::toggled, this, [this](bool muted){
        m_playback->set_muted(muted);
        QSettings().setValue(muted_key, muted);
        update_mute_button();
    });
    update_mute_button();
    tb->addWidget(m_mute_button);

    //speed, for checking timing; not saved, so every session starts at 1x
    auto* speed_box = new QComboBox(tb);
    speed_box->setObjectName("toolbar_speed");
    speed_box->setToolTip(tr("Playback speed (the song keeps its pitch)"));
    speed_box->setFocusPolicy(Qt::ClickFocus);

    for(const double rate : {1.0, 0.75, 0.5, 0.25}){
        speed_box->addItem(QString::fromUtf8("%1\u00d7").arg(rate), rate);
    }

    connect(speed_box, &QComboBox::currentIndexChanged, this, [this, speed_box](int index){
        m_playback->set_rate(speed_box->itemData(index).toDouble());
    });
    tb->addWidget(speed_box);
}

void MainWindow::update_mute_button(){
    const bool muted = m_mute_button->isChecked();

    m_mute_button->setIcon(style()->standardIcon(muted ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
    m_mute_button->setToolTip(muted ? tr("Sound off: click to turn it on, or use the arrow for volume")
                                    : tr("Sound on: click to mute, or use the arrow for volume"));
}

void MainWindow::attach_editor(ScriptEditor* editor){
    m_editor = editor;
    m_dock_code->setWidget(editor);

    connect(editor, &ScriptEditor::modified_changed, this, &MainWindow::update_window_title);
    connect(editor, &ScriptEditor::save_requested, this, &MainWindow::on_save_project);
    connect(editor, &ScriptEditor::run_requested, this, &MainWindow::on_run_script);
    connect(editor, &ScriptEditor::failed, this, &MainWindow::on_editor_failed);

    update_window_title();
}

void MainWindow::on_editor_failed(const QString& reason){
    ScriptEditor* failed_editor = m_editor;

    if(qobject_cast<ScriptEditor*>(sender()) != failed_editor){
        return;
    }

    // Keep the text and the saved/unsaved state; only the widget changes.
    ScriptEditor* fallback = create_plain_script_editor(m_dock_code);
    fallback->restore_state(failed_editor->text(), failed_editor->saved_text());

    failed_editor->disconnect(this);
    attach_editor(fallback);

    // Deferred: this slot runs inside a signal of the failed editor.
    failed_editor->hide();
    failed_editor->deleteLater();

    show_error(QString("%1. Switched to the plain text editor.").arg(reason));
}

void MainWindow::after_editor_sync(std::function<void()> action){
    m_editor->sync(std::move(action));
}

void MainWindow::update_window_title(){
    QString title = "cosby";

    if(!m_project_file_path.isEmpty()){
        title += " - " + QFileInfo(m_project_file_path).completeBaseName();

        // .osu files are named "Artist - Title (Creator) [Difficulty].osu"
        const QString osu_name = QFileInfo(m_osu_file).completeBaseName();
        const qsizetype open = osu_name.lastIndexOf('[');
        const qsizetype close = osu_name.lastIndexOf(']');

        if(open >= 0 && close > open){
            title += " " + osu_name.mid(open, close - open + 1);
        }
    }

    // [*] is where Qt shows the unsaved-changes marker.
    setWindowTitle(title + "[*]");
    setWindowModified(m_editor && m_editor->is_modified());
}

void MainWindow::update_action_state(){
    const bool has_project = !m_project_file_path.isEmpty();

    m_actions->act_save->setEnabled(has_project);
    m_actions->act_update_beatmap->setEnabled(has_project);
    m_actions->act_export->setEnabled(has_project);
}


