#include "app_actions.h"
#include "main_window.h"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QStyle>

AppActions::AppActions(QObject* parent) : QObject(parent){
    QStyle* style = QApplication::style();

    //file tab
    act_new = new QAction("&New Project", this);
    act_new->setShortcut(QKeySequence::New);
    //not implemented yet: projects are created from a beatmap for now
    act_new->setEnabled(false);
    act_new->setToolTip(tr("Not available yet: use New Project from Beatmap Folder"));

    act_new_from_beatmap = new QAction("New Project from Beatmap &Folder...", this);
    act_new_from_beatmap->setIcon(style->standardIcon(QStyle::SP_FileDialogNewFolder));

    act_open = new QAction("&Open Project...", this);
    act_open->setShortcut(QKeySequence::Open);
    act_open->setIcon(style->standardIcon(QStyle::SP_DialogOpenButton));

    act_save = new QAction("&Save", this);
    act_save->setShortcut(QKeySequence::Save);
    act_save->setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    act_save->setToolTip(tr("Save the script (Ctrl+S)"));

    act_clear_recent = new QAction("&Clear Recent Projects", this);

    act_exit = new QAction("E&xit", this);
    act_exit->setShortcut(QKeySequence::Quit);

    //project tab
    act_update_beatmap = new QAction("&Update Beatmap from Source...", this);
    act_update_beatmap->setToolTip(tr("Replace the project's beatmap copy with a newer version"));
    act_update_beatmap->setIcon(style->standardIcon(QStyle::SP_BrowserReload));

    act_run = new QAction(tr("&Run Script"), this);
    act_run->setShortcut(QKeySequence(Qt::Key_F5));
    act_run->setToolTip(tr("Run the script and refresh the preview (F5)"));
    act_run->setIcon(style->standardIcon(QStyle::SP_MediaSeekForward));
    //setIcon(Qicon(":/icon/run.svg"));     //<-for the icon
    act_export = new QAction(tr("E&xport .osb"), this);
    act_export->setToolTip(tr("Write the storyboard into the beatmap folder in osu! (asks before replacing files)"));

    //playback; Space only while the preview or timeline has focus
    //(MainWindow adds the action to those widgets), so it never eats
    //spaces typed in the editor
    act_play_pause = new QAction(tr("&Play"), this);
    act_play_pause->setShortcut(QKeySequence(Qt::Key_Space));
    act_play_pause->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    act_play_pause->setToolTip(tr("Play/Pause (Space in preview or timeline)"));
    act_play_pause->setIcon(style->standardIcon(QStyle::SP_MediaPlay));

    act_stop = new QAction(tr("S&top"), this);
    act_stop->setToolTip(tr("Stop and go back to 0:00"));
    act_stop->setIcon(style->standardIcon(QStyle::SP_MediaStop));

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
    QObject::connect(act_clear_recent, &QAction::triggered, r, &MainWindow::on_clear_recent_projects);

    QObject::connect(act_exit, &QAction::triggered, r, &MainWindow::close);

    //project tab
    QObject::connect(act_update_beatmap, &QAction::triggered, r, &MainWindow::on_update_beatmap);
    QObject::connect(act_run, &QAction::triggered, r, &MainWindow::on_run_script);
    QObject::connect(act_export, &QAction::triggered, r, &MainWindow::on_export_osb);

    //playback
    QObject::connect(act_play_pause, &QAction::triggered, r, &MainWindow::on_play);
    QObject::connect(act_stop, &QAction::triggered, r, &MainWindow::on_stop);

    //help tab
    QObject::connect(act_about, &QAction::triggered, r, &MainWindow::on_about);

}
