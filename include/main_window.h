#pragma once
#include <QMainWindow>

class QAction;
class TimelineView;
class PreviewWidget;
class QPlainTextEdit;
class script_host;
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
            void on_new_project_from_beatmap();
        void on_run_script();
        void on_open_project();
        void on_save_project();

        //project tab
        void on_build_json();
        void on_export_osb();

        //about tab
        void on_about();

    private:
        void create_menus();
        void show_info(const QString& message, int timeout_ms);
        void show_error(const QString& message, int timeout_ms);
      
        bool open_project_file(const QString& project_file_path);

    private:
        //timeline
        TimelineView* m_timeline = nullptr;

        //editor
        QPlainTextEdit* m_code = nullptr;
        script_host* m_script = nullptr;

        //preview
        AppActions* m_actions = nullptr;
        PreviewWidget* m_preview = nullptr;
        QAction* m_act_play = nullptr;
        QAction* m_act_stop = nullptr;

        //project
        QString m_project_file_path;
        QString m_project_root;
        QString m_asset_root;
        QString m_script_relative_path = "scripts/main.js";

        QString m_osu_file;
        QString m_audio_file;
        QString m_background_file;

        double m_bpm = 180.0;
};
