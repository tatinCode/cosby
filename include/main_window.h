#pragma once
#include <QMainWindow>

#include <functional>

class QAction;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QToolBar;
class QToolButton;
class TimelineView;
class PreviewWidget;
class ScriptEditor;
class script_host;
class AppActions;
class playback_controller;
struct BeatmapImportInfo;

class MainWindow : public QMainWindow {
    Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override;

    public slots:
        //playback
        void on_play();     // toggles play/pause
        void on_stop();

        //file tab
        void on_new_project_from_beatmap();
        void on_run_script();
        void on_open_project();
        void on_save_project();
        void on_clear_recent_projects();

        //project tab
        void on_update_beatmap();
        void on_export_osb();

        //about tab
        void on_about();

    protected:
        void closeEvent(QCloseEvent* event) override;

    private:
        void create_menus();
        void create_toolbar();
        // Volume popup and speed box; settings restored from QSettings.
        void create_audio_controls(QToolBar* tb);
        void update_mute_button();
        void create_output_dock();
        // Short notes go to the Output panel; errors also bring it forward.
        void show_info(const QString& message);
        void show_error(const QString& message);

        // Failures the user has to notice (create/open/update): a dialog.
        void report_error(const QString& title, const QString& message);

        //output panel
        enum class OutputKind { info, print, warning, error };
        void append_output(const QString& text, OutputKind kind, int line = 0);
        void on_output_activated(QListWidgetItem* item);

        //recent projects + window layout (QSettings)
        void add_recent_project(const QString& project_file_path);
        void rebuild_recent_menu();
        void open_recent_project(const QString& project_file_path);
        void restore_window_layout();
        void save_window_layout();

        void update_bpm_display();
        void update_playback_action(bool playing);
      
        bool open_project_file(const QString& project_file_path);
        void apply_beatmap(const BeatmapImportInfo& beatmap);
        // A file of the beatmap copy, by the name the .osu gives it; empty
        // when there's no name or it points outside the folder. The file
        // may not exist.
        QString beatmap_file_path(const QString& name) const;

        // The timeline's hint while it has no rows, from the project and
        // the last script run.
        void update_timeline_hint();

        //script editor
        void attach_editor(ScriptEditor* editor);
        void on_editor_failed(const QString& reason);
        void after_editor_sync(std::function<void()> action);

        void new_project_from_beatmap();
        void open_project();
        // Runs the editor's script; false (errors shown) when it fails.
        bool run_script();
        void export_storyboard();

        // Writes the editor text to the project's script file.
        bool save_script();

        // Asks what to do with unsaved script changes; false means cancel.
        bool maybe_save();

        void update_window_title();
        void update_action_state();

    private:
        //timeline
        TimelineView* m_timeline = nullptr;
        QDockWidget* m_dock_timeline = nullptr;

        //editor
        QDockWidget* m_dock_code = nullptr;
        ScriptEditor* m_editor = nullptr;
        script_host* m_script = nullptr;

        //output
        QDockWidget* m_dock_output = nullptr;
        QListWidget* m_output = nullptr;

        //preview
        AppActions* m_actions = nullptr;
        PreviewWidget* m_preview = nullptr;

        //play/pause/seek, song and samples
        playback_controller* m_playback = nullptr;
        QToolButton* m_mute_button = nullptr;

        //toolbar BPM readout (no status bar: the timeline's clock is the
        //bottom-left corner of the window)
        QLabel* m_bpm_label = nullptr;
        QAction* m_bpm_action = nullptr;

        //project
        QString m_project_file_path;
        QString m_project_root;
        QString m_asset_root;
        QString m_script_relative_path = "scripts/main.js";

        QString m_osu_file;
        QString m_audio_file;
        QString m_background_file;

        double m_bpm = 180.0;

        // Set once the user agreed to close, so the re-sent close is accepted.
        bool m_close_confirmed = false;

        // The last script run since the project opened.
        enum class ScriptRun { none, succeeded, failed };
        ScriptRun m_last_run = ScriptRun::none;
};
