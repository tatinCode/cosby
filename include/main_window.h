#pragma once
#include <QMainWindow>

class TimelineView;
class PreviewWidget;
class QPlainTextEdit;
class ScriptHost;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_play();
    void on_stop();
    void on_division_changed(int idx);
    void on_run_script();

private:
    TimelineView* m_timeline;
    QPlainTextEdit* m_code;
    ScriptHost* m_script;

    PreviewWidget* m_preview;
    QAction* m_act_play;
    QAction* m_act_stop;
};
