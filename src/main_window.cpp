#include "main_window.h"
#include "timeline_view.h"
#include "preview_widget.h"
#include "script_host.h"

#include <QDockWidget>
#include <QToolBar>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTimer>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent):
    QMainWindow(parent),
    m_timeline(new TimelineView(this)),
    m_preview(new PreviewWidget(this)),
    m_code(new QPlainTextEdit(this)),
    m_script(new ScriptHost(this))
{
    setWindowTitle("cosby");

    //center: preview
    setCentralWidget(m_preview);
//bottom: timeline
    auto* dock_timeline = new QDockWidget(tr("Timeline"), this);
    dock_timeline->setWidget(m_timeline);
    addDockWidget(Qt::BottomDockWidgetArea, dock_timeline);

    //right: code editor
    auto* dock_code = new QDockWidget(tr("Script (JS)"), this);
    dock_code->setWidget(m_code);
    addDockWidget(Qt::RightDockWidgetArea, dock_code);

    //toolbar
    auto* tb = addToolBar("Transport");
    m_act_play = tb->addAction("Play", this, &MainWindow::on_play);
    m_act_stop = tb->addAction("Stop", this, &MainWindow::on_stop);
    tb->addSeparator();

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

    QStatusBar().showMessage("Ready");

    //seed example script
    m_code->setPlainText(
            "//Example: build scene\n"
            "scene = [];\n"
            "function addSprite(path, t){scene.push({path, t}); }\n"
            "for (let c = 0; c < 16; c++){\n"
            "   addSprite('sprite.png', c * 250);\n"
            "}\n"
            );
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


void MainWindow::on_run_script(){
    auto result = m_script->run(m_code->toPlainText());
    QStatusBar().showMessage(result, 3000);
    
    // TODO: pull scene graph from ScriptHost and feed to Preview/Timeline
}
