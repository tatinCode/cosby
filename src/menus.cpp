#include "menus.h"
#include "app_actions.h"

#include <QMenuBar>
#include <QMenu>

namespace Menus{
    void build(QMenuBar* bar, AppActions* a){
        //file tab
        auto* file = bar->addMenu("&File");
        file->addAction(a->act_new);
        file->addAction(a->act_new_from_beatmap);
        file->addSeparator();
        file->addAction(a->act_open);
        a->menu_recent = file->addMenu("Open &Recent");
        file->addAction(a->act_save);
        file->addSeparator();
        file->addAction(a->act_exit);

        //project tab
        auto* proj = bar->addMenu("&Project");
        proj->addAction(a->act_update_beatmap);
        proj->addSeparator();
        proj->addAction(a->act_run);
        proj->addAction(a->act_build);
        proj->addAction(a->act_export);

        //playback tab
        auto* playback = bar->addMenu("P&layback");
        playback->addAction(a->act_play_pause);
        playback->addAction(a->act_stop);

        //view tab (dock toggles are added by MainWindow)
        a->menu_view = bar->addMenu("&View");

        //help tab
        auto* help = bar->addMenu("&Help");
        help->addAction(a->act_about);

        //action hints are tooltips (there is no status bar)
        for(QMenu* menu : {file, proj, playback}){
            menu->setToolTipsVisible(true);
        }
    }
}
