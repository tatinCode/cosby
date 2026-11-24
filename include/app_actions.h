#pragma once

#include <QObject>

class QAction;
class QMenu;
class MainWindow;

class AppActions : public QObject{
    Q_OBJECT

public:
    explicit AppActions(QObject* parent = nullptr);

    //for file tab
    QAction* act_new = nullptr;
    QAction* act_new_from_beatmap = nullptr;
    QAction* act_open = nullptr;
    QAction* act_save = nullptr;
    QAction* act_clear_recent = nullptr;
    QAction* act_exit = nullptr;

    //for the projects tab
    QAction* act_update_beatmap = nullptr;
    QAction* act_run = nullptr;
    QAction* act_build = nullptr;
    QAction* act_export = nullptr;

    //playback
    QAction* act_play_pause = nullptr;
    QAction* act_stop = nullptr;

    //project flow
    QAction* on_new_project = nullptr;

    //help!!!
    QAction* act_about = nullptr;

    //filled in by Menus::build(); MainWindow fills them at run time
    QMenu* menu_recent = nullptr;
    QMenu* menu_view = nullptr;

    void connect_slots(MainWindow* receiver);
};
