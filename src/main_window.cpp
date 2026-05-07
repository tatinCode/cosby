#include "main_window.h"
#include "timeline_view.h"
#include "ui/preview_widget.h"
#include "scripting/script_host.h"
#include "menus.h"
#include "app_actions.h"

#include <QDockWidget>
#include <QToolBar>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTimer>
#include <QMessageBox>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QFileInfo>

namespace{
    struct BeatmapImportInfo{
        QString beatmap_folder;
        QString osu_file_path;
        QString audio_file;
        QString background_file;

        double bpm;

        bool is_valid(){
            return !beatmap_folder.isEmpty() && !osu_file_path.isEmpty();
        }
    };

    QStringList find_osu_files(const QString& path){
        QDir dir(folder_path);
        return dir.entryList({"*.osu"}, QDir::Files, QDir::Name);
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

    BeatmapImportInfo sparse_beatmap_metadata(const QString& osu_file_path){
        BeatmapImportInfo info;

        info.osu_file_path = osu_file_path;
        info.beatmap_folder = QFileInfo(osu_file_path).absolutePath();

        /**
         * Implement .osu file parser here
         */

        return info;
    }
}

static QString load_text_file(const QString& path){
    QFile f(path);
    
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text)){
        return {};
    }

    QTextStream ts(&f);

    return ts.readAll();
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
    tb->addAction("Run Script", this, &MainWindow::on_run_script);

    //wire timeline <-> preview (shared playhead in ms)
    connect(m_timeline, &TimelineView::requestSeek, m_preview, &PreviewWidget::seekMs);
    connect(m_preview, &PreviewWidget::timeChanged, m_timeline, &TimelineView::setPlayheadMs);

    create_menus();
    m_actions->connect_slots(this);

    show_info("Ready", 3000);

    connect(m_actions->act_run, &QAction::triggered, this, &MainWindow::on_run_script);
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

    /**
     * This is where:
     * 1. Create a new project
     * 2. create project.cosby
     * 3. create scripts/main.js
     * 4. load project into preview
     *
     * goes
     */
}

void MainWindow::on_open_project(){

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

    m_preview->set_base_path(m_project_root.isEmpty() ? "." : m_project_root);

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

