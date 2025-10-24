#pragma once

#include <QObject>

class QAction;
class MainWindow;

class AppActions : public QObject{
    Q_OBJECT

public:
    explicit AppActions(QObject* parent = nullptr);

    //for file tab
    QAction* act_new = nullptr;
    QAction* act_open = nullptr;
    QAction* act_save = nullptr;
    QAction* act_exit = nullptr;

    //for the projects tab
    QAction* act_run = nullptr;
    QAction* act_build = nullptr;
    QAction* act_export = nullptr;

    //help!!!
    QAction* act_about = nullptr;

    void connect_slots(MainWindow* receiver);
};
