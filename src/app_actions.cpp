#include "app_actions.h"
#include "main_window.h"

#include <QAction>
#include <QKeySequence>

AppActions::AppActions(QObject* parent) : QObject(parent){
    //file tab
    act_new_from_beatmap = new QAction("New Project from &Beatmap", this);
    act_new_from_beatmap->setShortcut(QKeySequence::New);

    act_open = new QAction("&Open Project...", this);
    act_open->setShortcut(QKeySequence::Open);

    act_save = new QAction("&Save", this);
    act_save->setShortcut(QKeySequence::Save);

    act_exit = new QAction("E&xit", this);
    act_exit->setShortcut(QKeySequence::Quit);

    //project tab
    act_run = new QAction(tr("&Run Script"), this);
    act_run->setShortcut(QKeySequence(Qt::Key_F5));
    act_run->setStatusTip(tr("Evaluate the current script and refresh the preview"));
    //setIcon(Qicon(":/icon/run.svg"));     //<-for the icon
    act_build = new QAction("&Build JSON", this);
    act_export = new QAction("E&xport &.osb", this);

    //help tab
    act_about = new QAction("&About cosby", this);

    //make shortcuts work app-wide
    for(QAction* a : {act_new, act_open, act_save, act_exit}){
        a->setShortcutContext(Qt::ApplicationShortcut);
    }
}

void AppActions::connect_slots(MainWindow* r){ //r is the receiver
    //file tab
    QObject::connect(act_new_from_beatmap, &QAction::triggered, r, &MainWindow::on_new_project_from_beatmap);
    QObject::connect(act_open, &QAction::triggered, r, &MainWindow::on_open_project);
    QObject::connect(act_save, &QAction::triggered, r, &MainWindow::on_save_project);

    QObject::connect(act_exit, &QAction::triggered, r, &MainWindow::close);

    //project tab
    QObject::connect(act_run, &QAction::triggered, r, &MainWindow::on_run_script);
    QObject::connect(act_build, &QAction::triggered, r, &MainWindow::on_build_json);
    QObject::connect(act_export, &QAction::triggered, r, &MainWindow::on_export_osb);

    //help tab
    QObject::connect(act_about, &QAction::triggered, r, &MainWindow::on_about);

}
