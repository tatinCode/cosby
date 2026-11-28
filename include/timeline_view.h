#pragma once

#include "ui/timeline_model.h"

#include <QPoint>
#include <QWidget>

class QLineEdit;
class QScrollBar;

// Read-only track view of the last script run: layers, call-site groups,
// sprites and their commands over a beat grid.
//
//   wheel = scroll rows, Shift+wheel = pan time, Ctrl+wheel = zoom,
//   right-drag = pan, Home = fit, click/drag = seek, double-click = go to code.
//
// The footer shows the playhead time like osu! does (00:12:345). Clicking it
// (or Ctrl+G) opens a box with the time in milliseconds, selected, to copy
// or to type a time and press Enter to jump there. Up/Down steps on the
// grid, Esc closes it.
class TimelineView : public QWidget {
    Q_OBJECT

public:
    explicit TimelineView(QWidget* parent = nullptr);
    QSize minimumSizeHint() const override { return { 400, 180 }; }

    void setBpm(double bpm);
    void setBeatOffsetMs(double offset_ms);
    void setSubdivision(int div);
    void setPlayheadMs(qint64 ms);

    // Copies the scene; nullptr clears. The first non-empty scene is fitted.
    void setScene(const scene* sc);
    void fitToScene();

    // Makes every layer visible again (emits layerVisibilityChanged).
    void showAllLayers();

    const timeline_model& model() const { return m_model; }

    // The footer's time box; hidden until the time is clicked.
    QLineEdit* timeField() const { return m_time_edit; }
    QRect timeLabelRect() const;

signals:
    void requestSeek(qint64 ms);
    // Around a drag of the playhead (also a plain click), so playback can
    // go quiet while it moves.
    void scrubStarted();
    void scrubFinished();
    void sourceLineActivated(int line);
    void layerVisibilityChanged(layer_t layer, bool visible);

protected:
    bool event(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    //layout
    static constexpr int header_width = 220;
    static constexpr int ruler_height = 24;
    static constexpr int row_height = 22;
    static constexpr int footer_height = 24;

    QRect track_rect() const;           // bars area (right of header, below ruler, above footer)
    QRect footer_rect() const;
    int content_height() const;
    int row_at(int y) const;            // -1 when not on a row
    int row_top(int row) const;
    int row_indent(const timeline_row& row) const;
    QRect eye_rect(int row) const;      // layer rows only

    double x_for_ms(double ms) const;
    double ms_for_x(double x) const;

    // Bar under the point in `row`, or -1.
    int bar_at(int row, const QPoint& pos) const;
    QString tooltip_for(int row, int bar) const;

    void update_scrollbar();
    void clamp_view();
    void set_selected_row(int row);
    void seek_to_x(double x, bool snap);
    void toggle_layer(layer_t layer);
    bool is_layer_visible(layer_t layer) const;

    void paint_grid(QPainter& p, const QRect& area, bool ruler);
    void paint_rows(QPainter& p);
    void paint_header(QPainter& p, int row, const QRect& rect);
    void paint_bars(QPainter& p, const timeline_row& row, const QRect& rect);
    void paint_footer(QPainter& p);

    //time box
    void open_time_field();
    void close_time_field();            // drops unfinished input
    void refresh_time_field();          // shows the playhead unless the user is typing
    void commit_time_field();           // Enter
    void step_time_field(int direction, bool whole_beat);   // Up/Down
    void set_time_field_error(bool error);
    void show_time_field_menu(const QPoint& pos);
    qint64 time_field_base() const;     // the typed time if valid, else the playhead

    timeline_model m_model;
    QScrollBar* m_vbar = nullptr;
    QLineEdit* m_time_edit = nullptr;

    //view state
    double m_pixels_per_ms;     //zoom
    double m_view_start_ms;
    qint64 m_playhead_ms;
    int m_scroll_y = 0;

    //beat grid
    double m_bpm;
    double m_beat_offset_ms = 0.0;
    int m_subdivision;

    QString m_selected_key;
    unsigned m_hidden_layers = 0;   // bit per layer_t

    //mouse
    enum class drag_mode { none, seek, pan };
    drag_mode m_drag = drag_mode::none;
    QPoint m_last_drag_pos;

    double beat_ms() const { return 60000.0 / m_bpm; }
    double step_ms() const { return beat_ms() / m_subdivision; }
    qint64 snapMs(qint64 t) const;
};
