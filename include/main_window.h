#pragma once
#include <QMainWindow>

class QAction;
class TimelineView;
class PreviewWidget;
class QPlainTextEdit;
class ScriptHost;
class AppActions;

class MainWindow : public QMainWindow {
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override;

        private slots:
        void on_play();
        void on_stop();
        void on_division_changed(int idx);

        public slots:
        //file tab
        void on_new_project();
        void on_open_project();
        void on_save_project();

        //project tab
        void on_run_script();
        void on_build_json();
        void on_export_osb();

        //about tab
        void on_about();

    private:
        void create_menus();

    private:
        //timeline
        TimelineView* m_timeline = nullptr;

        //editor
        QPlainTextEdit* m_code = nullptr;
        ScriptHost* m_script = nullptr;

        //preview
        PreviewWidget* m_preview = nullptr;
        QAction* m_act_play = nullptr;
        QAction* m_act_stop = nullptr;

        //menu bar
        AppActions* m_actions = nullptr;
};
