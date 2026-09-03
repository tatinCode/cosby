#include "main_window.h"
#include "timeline_view.h"
#include "ui/preview_widget.h"
#include "scripting/script_host.h"
#include "menus.h"
#include "app_actions.h"
#include "project/osu_parser.h"
#include "project/project_file.h"

#include <QDockWidget>
#include <QToolBar>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTimer>
#include <QMessageBox>
#include <QStatusBar>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

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
    m_code(new QPlainTextEdit(this)),
    m_script(new script_host(this))
{
    setWindowTitle("cosby");

    //center: preview
    setCentralWidget(m_preview);

    m_actions = new AppActions(this);

    //bottom: timeline
    auto* dock_timeline = new QDockWidget(tr("Timeline"), this);
    dock_timeline->setWidget(m_timeline);
    addDockWidget(Qt::BottomDockWidgetArea, dock_timeline);

    //right: code editor
    auto* dock_code = new QDockWidget(tr("Script (JS)"), this);
    dock_code->setWidget(m_code);
    addDockWidget(Qt::RightDockWidgetArea, dock_code);

    //toolbar
    auto* tb = addToolBar("Project");
    m_act_play = tb->addAction("Play", this, &MainWindow::on_play);
    m_act_stop = tb->addAction("Stop", this, &MainWindow::on_stop);
    tb->addSeparator();
    tb->addAction(m_actions->act_run);
    tb->addAction(m_actions->act_build);
    tb->addAction(m_actions->act_export);

    //beat division selector
    auto* division_box = new QComboBox(tb);
    division_box->addItems({"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"});
    
    connect(division_box, &QComboBox::currentIndexChanged,
            this, &MainWindow::on_division_changed);
    tb->addWidget(division_box);

    tb->addSeparator();
    //tb->addAction("Run Script", this, &MainWindow::on_run_script);

    //wire timeline <-> preview (shared playhead in ms)
    connect(m_timeline, &TimelineView::requestSeek, m_preview, &PreviewWidget::seekMs);
    connect(m_preview, &PreviewWidget::timeChanged, m_timeline, &TimelineView::setPlayheadMs);

    create_menus();
    m_actions->connect_slots(this);

    show_info("Ready", 3000);

    //connect(m_actions->act_run, &QAction::triggered, this, &MainWindow::on_run_script);
}

MainWindow::~MainWindow() = default;


void MainWindow::on_play(){
    m_preview->play();
}

void MainWindow::on_stop(){
    m_preview->stop();
}


void MainWindow::on_division_changed(int idx){
    static const int divs[] = {1, 2, 4, 8, 16, 32};
    m_timeline->setSubdivision(divs[idx]);
}

/**
 * fucntions for menu bar below
 */
//file
void MainWindow::create_menus(){
    Menus::build(menuBar(), m_actions);
}

void MainWindow::on_new_project_from_beatmap(){
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
        show_error("No .osu files found in the selected folder", 5000);
        return;
    }

    const QString osu_file_path = choose_osu_file(this, folder, osu_files);
    if(osu_file_path.isEmpty()){
        return;
    }

    const BeatmapImportInfo beatmap = parse_beatmap_metadata(osu_file_path);
    if(!beatmap.is_valid()){
        show_error("Beatmap metadata could not be parsed", 5000);
        return;
    }

    m_asset_root = beatmap.beatmap_folder;
    m_osu_file = osu_file_path;
    m_audio_file = beatmap.audio_file;
    m_background_file = beatmap.background_file;
    m_bpm = beatmap.bpm;

    m_timeline->setBpm(m_bpm);

    show_info(
            QString("Loaded beatmap: BPM %1, Audio: %2, Background: %3")
            .arg(m_bpm)
            .arg(m_audio_file)
            .arg(m_background_file),
            5000
            );
}

void MainWindow::on_open_project(){
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

}

//project tab
void MainWindow::on_run_script(){
    auto result = m_script->run(m_code->toPlainText());
    if(!result.isEmpty()){
        show_error(result, 5000);
        return;
    }
    result = "Script ran successfully";

    //m_preview->set_base_path(m_project_root.isEmpty() ? "." : m_project_root);
    const QString base_path = !m_asset_root.isEmpty() ? m_asset_root
        : (m_project_root.isEmpty() ? "." : m_project_root);

    m_preview->set_base_path(base_path);


    m_preview->set_scene(&m_script->current_scene());
    show_info(result, 3000);
}

void MainWindow::on_build_json(){

}

void MainWindow::on_export_osb(){

}

//about tab
void MainWindow::on_about(){
    QMessageBox::about(this, "About cosby",
            "cosby storyboard editor\nC++, JSON-based pipeline");
}

void MainWindow::show_info(const QString& message, int timeout_ms){
    statusBar()->showMessage(message, timeout_ms);
}

void MainWindow::show_error(const QString& message, int timeout_ms){
    statusBar()->showMessage(message, timeout_ms);
}

bool MainWindow::open_project_file(const QString& project_file_path){
    QString error;

    const auto project = load_project_file(project_file_path, &error);

    if(!project.has_value()){
        show_error(error, 5000);

        return false;
    }

    const QString project_root = QFileInfo(project_file_path).absolutePath();

    QString beatmap_file_path = project->beatmap_file;

    if(QDir::isRelativePath(beatmap_file_path)){
        beatmap_file_path = QDir(project_root).filePath(beatmap_file_path);
    }

    const QFileInfo beatmap_file_info(beatmap_file_path);

    if(!beatmap_file_info.exists() ||
            !beatmap_file_info.isFile() || 
            !beatmap_file_info.isReadable()
            ){
        show_error(QString("Beatmap file could not be opened: %1").arg(beatmap_file_path), 5000);
        
        return false;
    }

    const BeatmapImportInfo beatmap = parse_beatmap_metadata(beatmap_file_path);
    
    if(!beatmap.is_valid()){
        show_error("Beatmap metadata could not be parsed", 5000);

        return false;
    }

    const QString script_file_path = QDir(project_root).filePath(project->script_file);

    QString script_contents;

    if(!load_text_file(script_file_path, script_contents, &error)){
        show_error(error, 5000);
        return false;
    }

    //Only replace the current project after everything loads successfully
    m_project_file_path = project_file_path;
    m_project_root = project_root;
    m_script_relative_path = project->script_file;

    m_asset_root = beatmap.beatmap_folder;
    m_osu_file = beatmap.osu_file_path;
    m_audio_file = beatmap.audio_file;
    m_background_file = beatmap.background_file;
    m_bpm = beatmap.bpm;

    m_code->setPlainText(script_contents);
    m_timeline->setBpm(m_bpm);
    m_preview->set_base_path(m_asset_root);

    setWindowTitle(QString("cosby - %1").arg(QFileInfo(m_project_file_path).baseName()));

    show_info("Project loaded successfully", 3000);

    return true;
}


