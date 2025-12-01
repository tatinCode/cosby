#include "menus.h"
#include "app_actions.h"

#include <QMenuBar>
#include <QMenu>

namespace Menus{
    void build(QMenuBar* bar, AppActions* a){
        //file tab
        auto* file = bar->addMenu("&File");
        file->addAction(a->act_new);
        file->addAction(a->act_open);
        file->addAction(a->act_save);
        file->addSeparator();
        file->addAction(a->act_exit);

        //project tab
        auto* proj = bar->addMenu("&Project");
        proj->addAction(a->act_run);
        proj->addAction(a->act_build);
        proj->addAction(a->act_export);

        //help tab
        auto* help = bar->addMenu("&Help");
        help->addAction(a->act_about);

        //run
        auto* runMenu = menuBar()->addMenu(tr("&Run"));
        runMenu->addAction(m_actions->act_run);

        //toolbar
        auto* tb = addToolBar(tr("Run"));
        tb->addAction(m_actions->act_run);
    }
}
