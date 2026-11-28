#include "timeline_view.h"
#include "ui/beat_snap.h"

#include "core/sprite_timing.h"
#include "core/time_format.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QShortcut>
#include <QToolButton>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace {
    constexpr double min_zoom = 0.002;  // px per ms: ~10 minutes in 1200 px
    constexpr double max_zoom = 5.0;

    // The footer's left part: the time, then the compact button.
    constexpr int compact_button_width = 22;
    constexpr int time_area_width = 220 - compact_button_width - 4;

    const QColor background_color(28, 28, 28);
    const QColor header_color(33, 33, 35);
    const QColor layer_row_color(38, 38, 42);
    const QColor footer_color(36, 36, 36);
    const QColor selection_color(14, 99, 156, 90);
    const QColor text_color(212, 212, 212);
    const QColor dim_text_color(128, 128, 136);
    const QColor playhead_color(200, 160, 60);
    const QColor separator_color(50, 50, 54);

    QString time_field_help(){
        return TimelineView::tr(
                "Click to show the time in milliseconds, ready to copy or edit.\n"
                "Type a time and press Enter to jump there:\n"
                "  12345        milliseconds\n"
                "  00:12:345    osu! editor timestamp\n"
                "  1:23.456     minutes:seconds\n"
                "  12.5s        seconds\n"
                "  +500  -1:00  relative to the playhead\n"
                "Up/Down: step on the snap grid (Shift: whole beat). Esc: close.\n"
                "Ctrl+G opens it from anywhere in the timeline.");
    }

    QColor layer_color(layer_t layer){
        switch(layer){
            case layer_t::background: return QColor(0x5a, 0x6b, 0x8c);
            case layer_t::fail:       return QColor(0x8c, 0x5a, 0x5a);
            case layer_t::pass:       return QColor(0x5a, 0x8c, 0x66);
            case layer_t::foreground: return QColor(0x4f, 0x7f, 0xa8);
            case layer_t::overlay:    return QColor(0x8c, 0x7a, 0x4f);
        }

        return QColor(0x4f, 0x7f, 0xa8);
    }

    QColor command_color(cmd_t type){
        switch(type){
            case cmd_t::fade:      return QColor(0xc8, 0xc8, 0xc8);
            case cmd_t::move:      return QColor(0x4f, 0x9d, 0xe0);
            case cmd_t::move_x:
            case cmd_t::move_y:    return QColor(0x6f, 0xb3, 0xe8);
            case cmd_t::scale:     return QColor(0x5c, 0xbf, 0x7a);
            case cmd_t::scale_vec: return QColor(0x7c, 0xcf, 0x95);
            case cmd_t::rotate:    return QColor(0xe0, 0x9a, 0x4f);
            case cmd_t::color:     return QColor(0xd4, 0x6f, 0xb0);
            case cmd_t::flip_h:
            case cmd_t::flip_v:
            case cmd_t::additive:  return QColor(0x9a, 0x7c, 0xe0);
        }

        return QColor(0xc8, 0xc8, 0xc8);
    }

    const QColor loop_color(0xd8, 0xb4, 0x4a);
    const QColor trigger_color(0xe0, 0x6c, 0x5a);
    const QColor sample_color(0x5a, 0xc8, 0xc8);

    QColor bar_color(const timeline_bar& bar, layer_t layer){
        switch(bar.kind){
            case timeline_bar_kind::command:  return command_color(bar.type);
            case timeline_bar_kind::loop:     return loop_color;
            case timeline_bar_kind::trigger:  return trigger_color;
            case timeline_bar_kind::sample:   return sample_color;
            case timeline_bar_kind::lifetime: break;
        }

        return layer_color(layer);
    }

    // Square shown before a lane's name, in its bars' colour.
    std::optional<QColor> lane_color(const timeline_row& row){
        switch(row.kind){
            case timeline_row_kind::command_lane: return command_color(row.command_type);
            case timeline_row_kind::loop_lane:    return loop_color;
            case timeline_row_kind::trigger_lane: return trigger_color;
            case timeline_row_kind::samples:      return sample_color;
            default:                              return std::nullopt;
        }
    }

    // Commands as a short list of their names: "Fade, Scale ×2".
    QString command_summary(const QVector<command>& cmds){
        QStringList parts;

        for(int type = int(cmd_t::fade); type <= int(cmd_t::additive); ++type){
            const auto count = std::count_if(cmds.begin(), cmds.end(), [type](const command& c){
                return int(c.type) == type;
            });

            if(count == 1){
                parts << timeline_model::command_name(cmd_t(type));
            }
            else if(count > 1){
                parts << QStringLiteral("%1 ×%2").arg(timeline_model::command_name(cmd_t(type))).arg(count);
            }
        }

        return parts.isEmpty() ? QStringLiteral("no commands") : parts.join(QStringLiteral(", "));
    }

    QString format_number(double value){
        return QString::number(value, 'g', 6);
    }

    QString command_values(const command& cmd){
        switch(cmd.type){
            case cmd_t::fade:
            case cmd_t::scale:
            case cmd_t::rotate:
            case cmd_t::move_x:
            case cmd_t::move_y:
                return QStringLiteral("%1 → %2").arg(format_number(cmd.v0), format_number(cmd.v1));

            case cmd_t::move:
            case cmd_t::scale_vec:
                return QStringLiteral("(%1, %2) → (%3, %4)").arg(
                        format_number(cmd.x0), format_number(cmd.y0),
                        format_number(cmd.x1), format_number(cmd.y1));

            case cmd_t::color:
                return QStringLiteral("rgb(%1, %2, %3) → rgb(%4, %5, %6)").arg(
                        format_number(cmd.r0), format_number(cmd.g0), format_number(cmd.b0),
                        format_number(cmd.r1), format_number(cmd.g1), format_number(cmd.b1));

            case cmd_t::flip_h:
            case cmd_t::flip_v:
            case cmd_t::additive:
                return cmd.t1 <= cmd.t0 ? QStringLiteral("from start time on") : QString();
        }

        return QString();
    }

    void draw_triangle(QPainter& p, const QRectF& box, bool down){
        QPainterPath path;

        if(down){
            path.moveTo(box.left(), box.top() + box.height() * 0.25);
            path.lineTo(box.right(), box.top() + box.height() * 0.25);
            path.lineTo(box.center().x(), box.bottom() - box.height() * 0.2);
        }
        else{
            path.moveTo(box.left() + box.width() * 0.25, box.top());
            path.lineTo(box.right() - box.width() * 0.2, box.center().y());
            path.lineTo(box.left() + box.width() * 0.25, box.bottom());
        }

        path.closeSubpath();
        p.fillPath(path, dim_text_color);
    }

    void draw_eye(QPainter& p, const QRectF& box, bool visible){
        const QColor color = visible ? text_color : dim_text_color;
        const QRectF eye = box.adjusted(1, box.height() * 0.25, -1, -box.height() * 0.25);

        p.save();
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(eye);

        if(visible){
            p.setBrush(color);
            p.drawEllipse(eye.center(), 2.2, 2.2);
        }
        else{
            p.drawLine(box.bottomLeft() + QPointF(2, -2), box.topRight() + QPointF(-2, 2));
        }

        p.restore();
    }
}

TimelineView::TimelineView(QWidget* parent):
    QWidget(parent),
    m_pixels_per_ms(0.2),
    m_playhead_ms(0),
    m_bpm(180.0),
    m_subdivision(4)    //1/4 by default
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_vbar = new QScrollBar(Qt::Vertical, this);
    m_vbar->setSingleStep(row_height);
    m_vbar->hide();

    connect(m_vbar, &QScrollBar::valueChanged, this, [this](int value){
        m_scroll_y = value;
        update();
    });

    //footer: the playhead time; the box only shows up when it's clicked
    m_time_edit = new QLineEdit(this);
    m_time_edit->setObjectName("timeline_time");
    m_time_edit->hide();

    QFont time_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    time_font.setPointSizeF(std::max(time_font.pointSizeF(), 10.0) + 3.0);
    m_time_edit->setFont(time_font);
    m_time_edit->setToolTip(time_field_help());
    m_time_edit->setContextMenuPolicy(Qt::CustomContextMenu);
    m_time_edit->installEventFilter(this);

    connect(m_time_edit, &QLineEdit::returnPressed, this, &TimelineView::commit_time_field);
    connect(m_time_edit, &QLineEdit::textEdited, this, [this]{
        set_time_field_error(false);
    });
    connect(m_time_edit, &QLineEdit::customContextMenuRequested, this, &TimelineView::show_time_field_menu);

    auto* go_to_time = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
    go_to_time->setContext(Qt::WidgetWithChildrenShortcut);
    connect(go_to_time, &QShortcut::activated, this, &TimelineView::open_time_field);

    //footer, next to the time: compact <-> detailed
    m_compact_action = new QAction(tr("Compact Timeline"), this);
    m_compact_action->setCheckable(true);
    connect(m_compact_action, &QAction::toggled, this, &TimelineView::setCompact);

    m_compact_button = new QToolButton(this);
    m_compact_button->setObjectName("timeline_compact");
    m_compact_button->setAutoRaise(true);
    m_compact_button->setFocusPolicy(Qt::NoFocus);
    connect(m_compact_button, &QToolButton::clicked, m_compact_action, &QAction::toggle);

    auto update_button = [this]{
        // The arrow points where the panel goes: down to shrink, up to grow.
        m_compact_button->setArrowType(m_compact ? Qt::UpArrow : Qt::DownArrow);
        m_compact_button->setToolTip(m_compact
                ? tr("Detailed timeline: show layers, sprites and commands")
                : tr("Compact timeline: only beats, samples and when sprites are on screen"));
    };
    update_button();
    connect(this, &TimelineView::compactChanged, this, update_button);
}

void TimelineView::setCompact(bool compact){
    if(compact == m_compact){
        return;
    }

    end_drag();
    m_compact = compact;
    m_compact_action->setChecked(compact);      // back here, but equal by now

    update_scrollbar();
    update();

    emit compactChanged(compact);
}

void TimelineView::setBpm(double bpm){
    m_bpm = bpm > 0.0 ? bpm : 180.0;
    update();
}

void TimelineView::setBeatOffsetMs(double offset_ms){
    m_beat_offset_ms = offset_ms;
    update();
}

void TimelineView::setSubdivision(int div){
    m_subdivision = std::max(1, div);
    update();
}

void TimelineView::setEmptyHint(const QString& hint){
    if(hint == m_empty_hint){
        return;
    }

    m_empty_hint = hint;
    update();
}

void TimelineView::setPlayheadMs(qint64 ms){
    m_playhead_ms = ms;
    refresh_time_field();
    update();
}

void TimelineView::setScene(const scene* sc){
    const bool was_empty = m_model.is_empty();

    if(sc){
        m_model.set_scene(*sc);
    }
    else{
        m_model.clear();
    }

    update_scrollbar();

    if(was_empty && !m_model.is_empty()){
        fitToScene();
    }

    update();
}

void TimelineView::fitToScene(){
    const QRect tracks = track_rect();

    if(m_model.time_begin() > m_model.time_end() || tracks.width() <= 0){
        return;
    }

    // The zoom that fits the whole storyboard across the tracks; the view
    // still follows the playhead.
    const double span = std::max(1000.0, double(m_model.time_end() - m_model.time_begin()));
    const double margin = span * 0.03;

    m_pixels_per_ms = std::clamp(tracks.width() / (span + 2 * margin), min_zoom, max_zoom);
    update();
}

void TimelineView::showAllLayers(){
    for(layer_t layer : timeline_model::layer_order){
        if(!is_layer_visible(layer)){
            toggle_layer(layer);
        }
    }
}

/**
 * geometry
 */
QRect TimelineView::track_rect() const{
    const int bar_width = m_vbar->isVisible() ? m_vbar->width() : 0;

    return QRect(header_w(), 0,
            std::max(0, width() - header_w() - bar_width),
            std::max(0, height() - footer_height));
}

double TimelineView::center_x() const{
    const QRect tracks = track_rect();
    return tracks.left() + tracks.width() / 2.0;
}

QRect TimelineView::footer_rect() const{
    return QRect(0, std::max(0, height() - footer_height), width(), footer_height);
}

QRect TimelineView::timeLabelRect() const{
    const QRect footer = footer_rect();
    const QFontMetrics metrics(m_time_edit->font());
    const int text_width = metrics.horizontalAdvance(format_osu_timestamp(m_playhead_ms));

    // Tucked into the bottom-left corner, left of the compact button; the
    // box opens in the same spot, so the text doesn't jump.
    constexpr int margin = 1;
    constexpr int box_height = 20;

    return QRect(margin, footer.bottom() - box_height,
            std::min(time_area_width - 2 * margin, text_width + 16), box_height);
}

int TimelineView::content_height() const{
    return int(m_model.rows().size()) * row_height;
}

int TimelineView::row_top(int row) const{
    return row * row_height - m_scroll_y;
}

int TimelineView::row_at(int y) const{
    if(m_compact || y < 0 || y >= footer_rect().top()){
        return -1;
    }

    const int row = (y + m_scroll_y) / row_height;
    return row < m_model.rows().size() ? row : -1;
}

int TimelineView::row_indent(const timeline_row& row) const{
    return 6 + row.depth * 14;
}

QRect TimelineView::eye_rect(int row) const{
    return QRect(header_width - 24, row_top(row) + (row_height - 14) / 2, 18, 14);
}

double TimelineView::x_for_ms(double ms) const{
    return center_x() + (ms - double(m_playhead_ms)) * m_pixels_per_ms;
}

double TimelineView::ms_for_x(double x) const{
    return double(m_playhead_ms) + (x - center_x()) / m_pixels_per_ms;
}

void TimelineView::update_scrollbar(){
    const int visible = track_rect().height();
    const int overflow = m_compact ? 0 : content_height() - visible;

    m_vbar->setVisible(overflow > 0);
    m_vbar->setGeometry(width() - m_vbar->sizeHint().width(), 0,
            m_vbar->sizeHint().width(), visible);
    m_vbar->setPageStep(std::max(row_height, visible));
    m_vbar->setRange(0, std::max(0, overflow));

    // setRange clamps the value; keep m_scroll_y in step even without a signal.
    m_scroll_y = m_vbar->value();
}

void TimelineView::resizeEvent(QResizeEvent*){
    update_scrollbar();

    // Same corner as the text, but wide enough to type in.
    const QRect label = timeLabelRect();
    m_time_edit->setGeometry(label.left(), label.top(), time_area_width - 2 * label.left(), label.height());

    const QRect footer = footer_rect();
    m_compact_button->setGeometry(time_area_width, footer.top() + 2, compact_button_width, footer.height() - 4);
}

qint64 TimelineView::snapMs(qint64 t) const{
    const double step = step_ms();

    if(step <= 0.0){
        return t;
    }

    const double k = std::round((t - m_beat_offset_ms) / step);
    return std::llround(m_beat_offset_ms + k * step);
}

/**
 * painting
 */
void TimelineView::paintEvent(QPaintEvent*){
    QPainter p(this);
    p.fillRect(rect(), background_color);

    const QRect tracks = track_rect();

    p.save();
    p.setClipRect(tracks);
    if(m_compact){
        paint_compact(p, tracks);
    }
    else{
        paint_grid(p, tracks, false);   // behind the rows
    }
    p.restore();

    if(!m_compact){
        paint_rows(p);
    }

    paint_footer(p);

    const int footer_top = footer_rect().top();

    if(!m_compact){
        p.setPen(separator_color);
        p.drawLine(header_width, 0, header_width, footer_top);
    }

    // The playhead, always in the middle.
    const double px = center_x();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(playhead_color, 2));
    p.drawLine(QPointF(px, 0), QPointF(px, footer_top));

    QPainterPath marker;
    marker.moveTo(px - 6, 0);
    marker.lineTo(px + 6, 0);
    marker.lineTo(px, 8);
    marker.closeSubpath();
    p.fillPath(marker, playhead_color);
    p.setRenderHint(QPainter::Antialiasing, false);

    if(m_model.is_empty() && !m_empty_hint.isEmpty()){
        p.setPen(dim_text_color);

        // In the names column; compact has none, so left in the strip.
        const QRect box = m_compact
            ? QRect(10, 0, std::max(0, int(px) - 20), footer_top)
            : QRect(10, 6, header_width - 20, std::max(0, footer_top - 12));
        const int align = m_compact ? Qt::AlignVCenter : Qt::AlignTop;

        p.drawText(box, Qt::AlignLeft | align | Qt::TextWordWrap, m_empty_hint);
    }
}

void TimelineView::paint_grid(QPainter& p, const QRect& area, bool strip){
    const double beat = beat_ms();
    const double beat_px = beat * m_pixels_per_ms;

    const double t0 = ms_for_x(area.left());
    const double t1 = ms_for_x(area.right());

    // Ticks closer than this are left out, one division at a time: zoomed
    // out, 1/2 stays after 1/4 is gone.
    constexpr double min_tick_px = 6.0;

    // The finest division drawn: a divisor of the snap that is far enough apart.
    int finest = 1;
    for(int d = m_subdivision; d > 1; --d){
        if(m_subdivision % d == 0 && beat_px / d >= min_tick_px){
            finest = d;
            break;
        }
    }

    // In the strip a finer division is shorter; behind the rows it's fainter.
    auto tick_height = [](int division){
        return division == 2 ? 9 : division <= 4 ? 7 : 5;
    };

    auto pen_for = [strip](int division, int alpha){
        QColor color = beat_division_color(division);
        if(!strip){
            color.setAlpha(alpha);
        }
        return QPen(color);
    };

    if(finest > 1){
        const double step = beat / finest;
        const qint64 first = qint64(std::floor((t0 - m_beat_offset_ms) / step));
        const qint64 last = qint64(std::ceil((t1 - m_beat_offset_ms) / step));

        for(qint64 k = first; k <= last; ++k){
            const int division = beat_division(k, finest);
            if(division == 1){
                continue;   // drawn as a beat line
            }

            p.setPen(pen_for(division, 55));

            const double x = x_for_ms(m_beat_offset_ms + k * step);
            if(strip){
                p.drawLine(QPointF(x, area.bottom() - tick_height(division)), QPointF(x, area.bottom()));
            }
            else{
                p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
            }
        }
    }

    // Beats: every one if readable, otherwise every n-th, n a power of two.
    int beat_every = 1;
    while(beat_px * beat_every < 5.0 && beat_every < (1 << 20)){
        beat_every *= 2;
    }

    const qint64 first_beat = qint64(std::floor((t0 - m_beat_offset_ms) / beat));
    const qint64 last_beat = qint64(std::ceil((t1 - m_beat_offset_ms) / beat));

    // Round down to a multiple of beat_every (also for negative beats).
    const qint64 start = qint64(std::floor(double(first_beat) / beat_every)) * beat_every;

    for(qint64 k = start; k <= last_beat; k += beat_every){
        const double x = x_for_ms(m_beat_offset_ms + k * beat);
        const bool measure = k % 4 == 0;

        p.setPen(pen_for(1, measure ? 100 : 60));

        if(strip){
            p.drawLine(QPointF(x, area.bottom() - (measure ? 16 : 11)), QPointF(x, area.bottom()));
        }
        else{
            p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        }
    }
}

void TimelineView::paint_compact(QPainter& p, const QRect& area){
    // Top to bottom: when sprites are on screen, samples, beat ticks.
    paint_grid(p, area, true);

    const double view_t0 = ms_for_x(area.left());
    const double view_t1 = ms_for_x(area.right());

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(layer_color(layer_t::foreground));

    for(const timeline_bar& bar : m_model.activity()){
        if(bar.t0 > view_t1 || bar.t1 < view_t0){
            continue;
        }

        const double x0 = x_for_ms(bar.t0);
        const double x1 = std::max(x_for_ms(bar.t1), x0 + 2.0);
        p.drawRoundedRect(QRectF(x0, area.top() + 6, x1 - x0, 6), 2, 2);
    }

    const double cy = area.top() + 21;

    for(const sample& smp : m_model.current_scene().samples){
        if(smp.time < view_t0 - 10 || smp.time > view_t1 + 10){
            continue;
        }

        const double x = x_for_ms(smp.time);
        QPainterPath diamond;
        diamond.moveTo(x, cy - 4);
        diamond.lineTo(x + 4, cy);
        diamond.lineTo(x, cy + 4);
        diamond.lineTo(x - 4, cy);
        diamond.closeSubpath();
        p.fillPath(diamond, sample_color);
    }

    p.setRenderHint(QPainter::Antialiasing, false);
}

void TimelineView::paint_rows(QPainter& p){
    const auto& rows = m_model.rows();
    const QRect tracks = track_rect();

    p.fillRect(QRect(0, 0, header_width, tracks.height()), header_color);

    if(rows.isEmpty()){
        return;
    }

    const int first = std::max(0, m_scroll_y / row_height);
    const int last = std::min(int(rows.size()) - 1, (m_scroll_y + tracks.height()) / row_height);

    for(int i = first; i <= last; ++i){
        const timeline_row& row = rows.at(i);
        const QRect line(0, row_top(i), header_width + tracks.width(), row_height);
        const QRect track(header_width, line.top(), tracks.width(), row_height);

        if(row.kind == timeline_row_kind::layer){
            p.fillRect(line, layer_row_color);
        }

        if(!m_selected_key.isEmpty() && row.key == m_selected_key){
            p.fillRect(line, selection_color);
        }

        // Faint separator under layer rows.
        if(row.kind == timeline_row_kind::layer){
            p.setPen(QColor(50, 50, 54));
            p.drawLine(line.bottomLeft(), line.bottomRight());
        }

        p.save();
        p.setClipRect(track);
        paint_bars(p, row, track);
        p.restore();

        p.save();
        p.setClipRect(QRect(0, line.top(), header_width, row_height));
        paint_header(p, i, QRect(0, line.top(), header_width, row_height));
        p.restore();
    }
}

void TimelineView::paint_header(QPainter& p, int index, const QRect& rect){
    const timeline_row& row = m_model.rows().at(index);
    const int indent = row_indent(row);
    const bool hidden = row.kind != timeline_row_kind::samples && !is_layer_visible(row.layer);

    if(row.expandable){
        draw_triangle(p, QRectF(indent, rect.center().y() - 4.5, 9, 9), row.expanded);
    }

    int text_left = indent + 14;
    int text_right = rect.right() - 8;

    if(row.kind == timeline_row_kind::layer){
        draw_eye(p, eye_rect(index), !hidden);
        text_right = eye_rect(index).left() - 6;
    }

    if(const auto color = lane_color(row)){
        p.fillRect(QRect(text_left, rect.center().y() - 4, 8, 8), *color);
        text_left += 14;
    }

    QFont font = p.font();
    font.setBold(row.kind == timeline_row_kind::layer);
    p.setFont(font);

    const QFontMetrics metrics(font);
    const int detail_width = row.detail.isEmpty() ? 0 : metrics.horizontalAdvance(row.detail) + 8;
    const int label_width = std::max(0, text_right - text_left - detail_width);

    p.setPen(hidden ? dim_text_color : text_color);
    p.drawText(QRect(text_left, rect.top(), label_width, rect.height()), Qt::AlignVCenter | Qt::AlignLeft,
            metrics.elidedText(row.label, Qt::ElideMiddle, label_width));

    if(!row.detail.isEmpty()){
        font.setBold(false);
        p.setFont(font);
        p.setPen(dim_text_color);
        p.drawText(QRect(text_left, rect.top(), text_right - text_left, rect.height()),
                Qt::AlignVCenter | Qt::AlignRight, row.detail);
    }
}

void TimelineView::paint_footer(QPainter& p){
    const QRect footer = footer_rect();

    p.fillRect(footer, footer_color);
    p.setPen(separator_color);
    p.drawLine(footer.topLeft(), footer.topRight());

    // Plain text, like osu!'s editor; the box replaces it while open.
    if(!m_time_edit->isVisible()){
        p.setFont(m_time_edit->font());
        p.setPen(text_color);
        // Lined up with the text inside the box (measured), so nothing jumps.
        p.drawText(timeLabelRect().adjusted(3, 1, 0, 1), Qt::AlignVCenter | Qt::AlignLeft,
                format_osu_timestamp(m_playhead_ms));
    }
}

void TimelineView::paint_bars(QPainter& p, const timeline_row& row, const QRect& rect){
    const double view_t0 = ms_for_x(rect.left());
    const double view_t1 = ms_for_x(rect.right());
    const bool hidden = row.kind != timeline_row_kind::samples && !is_layer_visible(row.layer);
    const scene& sc = m_model.current_scene();

    // Layer rows get a thin summary line, everything else a full bar.
    const int inset = row.kind == timeline_row_kind::layer ? 8 : 4;
    const double top = rect.top() + inset;
    const double bar_height = rect.height() - 2 * inset;

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    for(const timeline_bar& bar : row.bars){
        if(bar.t0 > view_t1 || std::max(bar.t0, bar.t1) < view_t0){
            continue;
        }

        QColor color = bar_color(bar, row.layer);
        if(hidden){
            color.setAlpha(70);
        }

        const double x0 = x_for_ms(bar.t0);
        const bool instant = bar.t1 <= bar.t0;

        if(instant){
            // Instants: a small diamond so they stay visible at any zoom.
            QPainterPath diamond;
            const double cy = top + bar_height / 2.0;
            const double r = std::min(5.0, bar_height / 2.0);
            diamond.moveTo(x0, cy - r);
            diamond.lineTo(x0 + r, cy);
            diamond.lineTo(x0, cy + r);
            diamond.lineTo(x0 - r, cy);
            diamond.closeSubpath();
            p.fillPath(diamond, color);
            continue;
        }

        const double x1 = std::max(x_for_ms(bar.t1), x0 + 2.0);
        const QRectF box(x0, top, x1 - x0, bar_height);

        if(bar.kind == timeline_bar_kind::trigger){
            // Outlined: the commands only play if the trigger fires.
            QColor fill = color;
            fill.setAlpha(hidden ? 25 : 60);
            p.setBrush(fill);
            p.setPen(QPen(color, 1.0, Qt::DashLine));
            p.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
            p.setPen(Qt::NoPen);
            continue;
        }

        p.setBrush(color);
        p.drawRoundedRect(box, 3, 3);

        if(bar.kind == timeline_bar_kind::loop){
            // A tick between passes, when they are far enough apart to see.
            const loop_group& loop = sc.sprites.at(bar.sprite).loops.at(bar.command);
            const int pass = timing_of(loop).duration();
            const double pass_width = pass * (x1 - x0) / std::max(1, bar.t1 - bar.t0);

            if(pass > 0 && pass_width >= 4.0){
                p.setPen(QPen(QColor(0, 0, 0, 110), 1.0));

                // Start at the first tick in view; loops can have many passes.
                const int first_tick = std::max(1, int((view_t0 - bar.t0) / pass));

                for(int k = first_tick; k < loop_passes(loop); ++k){
                    const double x = x_for_ms(bar.t0 + double(k) * pass);

                    if(x > rect.right()){
                        break;
                    }
                    if(x >= rect.left()){
                        p.drawLine(QPointF(x, top + 2), QPointF(x, top + bar_height - 2));
                    }
                }

                p.setPen(Qt::NoPen);
            }
        }
    }

    p.setRenderHint(QPainter::Antialiasing, false);
}

/**
 * hit testing and tooltips
 */
int TimelineView::bar_at(int index, const QPoint& pos) const{
    if(index < 0 || pos.x() < header_w()){
        return -1;
    }

    const auto& bars = m_model.rows().at(index).bars;

    // Last drawn is on top.
    for(int i = int(bars.size()) - 1; i >= 0; --i){
        const timeline_bar& bar = bars.at(i);
        const double x0 = x_for_ms(bar.t0);
        const double x1 = bar.t1 <= bar.t0 ? x0 : std::max(x_for_ms(bar.t1), x0 + 2.0);

        if(pos.x() >= x0 - 4 && pos.x() <= x1 + 4){
            return i;
        }
    }

    return -1;
}

QString TimelineView::tooltip_for(int index, int bar_index) const{
    const timeline_row& row = m_model.rows().at(index);
    const scene& sc = m_model.current_scene();

    QString where = row.source_line > 0 ? tr("line %1").arg(row.source_line) : QString();

    if(bar_index < 0){
        // Header tooltip.
        QStringList lines{row.label};

        if(row.first_sprite >= 0 && row.kind != timeline_row_kind::layer
                && row.kind != timeline_row_kind::samples){
            lines << sc.sprites.at(row.first_sprite).path;
        }
        if(!where.isEmpty()){
            lines << tr("Created on %1 (double-click to open)").arg(where);
        }
        if(row.kind == timeline_row_kind::layer){
            lines << tr("%1. The eye hides it in the preview only.").arg(row.detail);
        }

        return lines.join('\n');
    }

    const timeline_bar& bar = row.bars.at(bar_index);
    const QString range = bar.t1 > bar.t0
        ? QStringLiteral("%1 – %2   (%3 – %4 ms)").arg(format_osu_timestamp(bar.t0),
                format_osu_timestamp(bar.t1), QString::number(bar.t0), QString::number(bar.t1))
        : QStringLiteral("%1   (%2 ms)").arg(format_osu_timestamp(bar.t0), QString::number(bar.t0));

    if(bar.is_command()){
        const command& cmd = sc.sprites.at(bar.sprite).cmds.at(bar.command);
        QStringList lines{
            QStringLiteral("%1 · %2").arg(timeline_model::command_name(cmd.type), easing_name(cmd.easing)),
            range
        };

        const QString values = command_values(cmd);
        if(!values.isEmpty()){
            lines << values;
        }

        return lines.join('\n');
    }

    if(bar.kind == timeline_bar_kind::loop){
        const loop_group& loop = sc.sprites.at(bar.sprite).loops.at(bar.command);
        const int pass = timing_of(loop).duration();

        QStringList lines{
            tr("Loop ×%1 from %2 ms").arg(loop_passes(loop)).arg(loop.start),
            range,
            tr("%1 ms per pass").arg(pass),
            command_summary(loop.cmds)
        };

        if(!where.isEmpty()){
            lines << where;
        }

        return lines.join('\n');
    }

    if(bar.kind == timeline_bar_kind::trigger){
        const trigger_group& trigger = sc.sprites.at(bar.sprite).triggers.at(bar.command);

        QStringList lines{
            trigger.group != 0 ? tr("Trigger %1 (group %2)").arg(trigger.trigger).arg(trigger.group)
                               : tr("Trigger %1").arg(trigger.trigger),
            range,
            command_summary(trigger.cmds),
            tr("Plays in osu! only; the preview can't fire triggers.")
        };

        if(!where.isEmpty()){
            lines << where;
        }

        return lines.join('\n');
    }

    if(bar.kind == timeline_bar_kind::sample){
        const sample& smp = sc.samples.at(bar.command);

        QStringList lines{
            smp.path,
            range,
            tr("%1 layer, volume %2").arg(timeline_model::layer_name(smp.layer)).arg(smp.volume),
            tr("Not played in the preview yet.")
        };

        if(smp.source_line > 0){
            lines << tr("line %1").arg(smp.source_line);
        }

        return lines.join('\n');
    }

    QStringList lines{row.label, range};

    if(row.kind == timeline_row_kind::sprite && bar.sprite >= 0){
        const sprite& spr = sc.sprites.at(bar.sprite);
        qsizetype count = spr.cmds.size();

        for(const loop_group& loop : spr.loops){
            count += loop.cmds.size();
        }
        lines << (count == 1 ? tr("1 command") : tr("%1 commands").arg(count));
    }
    else if(row.sprite_count > 1){
        lines << tr("%1 sprites").arg(row.sprite_count);
    }

    if(!where.isEmpty()){
        lines << where;
    }

    return lines.join('\n');
}

bool TimelineView::event(QEvent* e){
    // The mouse can be taken away mid-drag (a popup, the window losing it):
    // the drag ends there, or playback would stay quiet.
    if(e->type() == QEvent::UngrabMouse){
        end_drag();
    }

    if(e->type() == QEvent::ToolTip){
        auto* help = static_cast<QHelpEvent*>(e);

        if(!m_time_edit->isVisible() && timeLabelRect().contains(help->pos())){
            QToolTip::showText(help->globalPos(), time_field_help(), this, timeLabelRect());
            return true;
        }
        const int row = row_at(help->pos().y());

        if(row < 0){
            QToolTip::hideText();
            e->ignore();
            return true;
        }

        const int bar = bar_at(row, help->pos());

        if(help->pos().x() >= header_width && bar < 0){
            QToolTip::hideText();
            e->ignore();
            return true;
        }

        QToolTip::showText(help->globalPos(), tooltip_for(row, bar), this);
        return true;
    }

    return QWidget::event(e);
}

/**
 * interaction
 */
void TimelineView::set_selected_row(int row){
    m_selected_key = row >= 0 ? m_model.rows().at(row).key : QString();
    update();
}

void TimelineView::seek_to_x(double x, bool snap){
    const qint64 t = qint64(std::llround(ms_for_x(x)));
    emit requestSeek(snap ? snapMs(t) : t);
}

void TimelineView::slide_to(const QPoint& pos){
    // From where the press began, not the playhead now: the view moves with
    // each seek, and the time under the mouse has to stay put.
    const double moved_ms = (pos.x() - m_press_pos.x()) / m_pixels_per_ms;
    emit requestSeek(qint64(std::llround(double(m_press_ms) - moved_ms)));
}

void TimelineView::end_drag(){
    const bool was_sliding = m_drag == drag_mode::slide;
    m_drag = drag_mode::none;

    if(was_sliding){
        unsetCursor();
        emit scrubFinished();
    }
}

bool TimelineView::is_layer_visible(layer_t layer) const{
    return !(m_hidden_layers & (1u << unsigned(layer)));
}

void TimelineView::toggle_layer(layer_t layer){
    m_hidden_layers ^= 1u << unsigned(layer);
    emit layerVisibilityChanged(layer, is_layer_visible(layer));
    update();
}

void TimelineView::wheelEvent(QWheelEvent* e){
    const QPoint delta = e->angleDelta();

    //ctrl + wheel = zoom around the playhead
    if(e->modifiers() & Qt::ControlModifier){
        const double steps = (delta.y() != 0 ? delta.y() : delta.x()) / 120.0;

        m_pixels_per_ms = std::clamp(m_pixels_per_ms * std::pow(1.15, steps), min_zoom, max_zoom);
        update();
        e->accept();
        return;
    }

    //shift + wheel (or a horizontal wheel/touchpad swipe) = move through time,
    //which is seeking since the view follows the playhead. Some platforms
    //already turn shift+wheel into a horizontal delta.
    //In compact mode there are no rows to scroll, so the plain wheel does it.
    const bool horizontal = m_compact || (e->modifiers() & Qt::ShiftModifier) || (delta.x() != 0 && delta.y() == 0);

    if(horizontal){
        const int amount = delta.x() != 0 ? delta.x() : delta.y();
        emit requestSeek(qint64(std::llround(double(m_playhead_ms) - amount / m_pixels_per_ms * 0.5)));
        e->accept();
        return;
    }

    //plain wheel = scroll rows
    if(m_vbar->isVisible()){
        m_vbar->setValue(m_vbar->value() - delta.y() * row_height / 40);
    }

    e->accept();
}

void TimelineView::mousePressEvent(QMouseEvent* e){
    const QPoint pos = e->position().toPoint();
    m_last_drag_pos = pos;

    //footer: clicking the time opens the box; the rest does nothing
    if(footer_rect().contains(pos)){
        if(e->button() == Qt::LeftButton && timeLabelRect().contains(pos)){
            open_time_field();
        }
        return;
    }

    if(m_drag != drag_mode::none){
        return;     // a second button during a drag
    }

    m_press_pos = pos;
    m_press_ms = m_playhead_ms;
    m_press_snap = !(e->modifiers() & Qt::AltModifier);

    //right/middle: slide at once (and scroll rows)
    if(e->button() == Qt::RightButton || e->button() == Qt::MiddleButton){
        m_drag = drag_mode::slide;
        setCursor(Qt::ClosedHandCursor);
        emit scrubStarted();
        return;
    }

    if(e->button() != Qt::LeftButton){
        return;
    }

    //header: expand arrow, layer eye, or select
    if(pos.x() < header_w()){
        const int index = row_at(pos.y());
        if(index < 0){
            return;
        }

        const timeline_row& row = m_model.rows().at(index);

        if(row.kind == timeline_row_kind::layer && eye_rect(index).adjusted(-3, -3, 3, 3).contains(pos)){
            toggle_layer(row.layer);
            return;
        }

        if(row.expandable && pos.x() < row_indent(row) + 14){
            const QString key = row.key;
            m_model.toggle_expanded(index);
            m_selected_key = key;
            update_scrollbar();
            update();
            return;
        }

        set_selected_row(index);
        return;
    }

    //tracks: a click seeks there on release; moving first makes it a slide
    set_selected_row(row_at(pos.y()));
    m_drag = drag_mode::press;
}

void TimelineView::mouseMoveEvent(QMouseEvent* e){
    const QPoint pos = e->position().toPoint();

    if(m_drag == drag_mode::none){
        if(timeLabelRect().contains(pos) && !m_time_edit->isVisible()){
            setCursor(Qt::PointingHandCursor);
        }
        else{
            unsetCursor();
        }
    }

    if(m_drag == drag_mode::press && (pos - m_press_pos).manhattanLength() >= QApplication::startDragDistance()){
        m_drag = drag_mode::slide;
        setCursor(Qt::ClosedHandCursor);
        emit scrubStarted();
    }

    if(m_drag == drag_mode::slide){
        slide_to(pos);

        // A right-drag also scrolls the rows.
        if((e->buttons() & (Qt::RightButton | Qt::MiddleButton)) && m_vbar->isVisible()){
            m_vbar->setValue(m_vbar->value() - (pos.y() - m_last_drag_pos.y()));
        }
    }

    m_last_drag_pos = pos;
}

void TimelineView::mouseReleaseEvent(QMouseEvent* e){
    if(m_drag == drag_mode::press && e->button() == Qt::LeftButton){
        m_drag = drag_mode::none;
        seek_to_x(m_press_pos.x(), m_press_snap);
        return;
    }

    end_drag();
}

void TimelineView::mouseDoubleClickEvent(QMouseEvent* e){
    if(e->button() != Qt::LeftButton){
        return;
    }

    const QPoint pos = e->position().toPoint();
    const int index = footer_rect().contains(pos) ? -1 : row_at(pos.y());

    if(index < 0){
        return;
    }

    const timeline_row& row = m_model.rows().at(index);

    // The eye and the arrow already acted on the first click.
    if(pos.x() < header_width){
        const bool on_eye = row.kind == timeline_row_kind::layer
            && eye_rect(index).adjusted(-3, -3, 3, 3).contains(pos);
        const bool on_arrow = row.expandable && pos.x() < row_indent(row) + 14;

        if(on_eye || on_arrow){
            return;
        }
    }

    if(row.source_line > 0){
        emit sourceLineActivated(row.source_line);
        return;
    }

    // Nothing to open (layer rows): expand/collapse instead.
    if(row.expandable){
        m_model.toggle_expanded(index);
        update_scrollbar();
        update();
    }
}

void TimelineView::keyPressEvent(QKeyEvent* e){
    if(e->key() == Qt::Key_Home){
        fitToScene();
        return;
    }

    QWidget::keyPressEvent(e);
}

/**
 * time box
 */
void TimelineView::open_time_field(){
    set_time_field_error(false);
    m_time_edit->setText(QString::number(m_playhead_ms));
    m_time_edit->show();
    m_time_edit->setFocus(Qt::ShortcutFocusReason);
    m_time_edit->selectAll();
    unsetCursor();
    update();
}

void TimelineView::close_time_field(){
    if(!m_time_edit->isVisible()){
        return;
    }

    if(m_time_edit->hasFocus()){
        setFocus(Qt::OtherFocusReason);
    }

    set_time_field_error(false);
    m_time_edit->hide();
    update();
}

void TimelineView::refresh_time_field(){
    // Never overwrite what the user is typing.
    if(!m_time_edit->isVisible() || m_time_edit->isModified()){
        return;
    }

    const QString text = QString::number(m_playhead_ms);
    if(text == m_time_edit->text()){
        return;
    }

    // Keep a full selection (ready to copy) while the time ticks.
    const bool all_selected = m_time_edit->hasSelectedText()
        && m_time_edit->selectedText() == m_time_edit->text();

    m_time_edit->setText(text);     // also clears isModified()

    if(all_selected){
        m_time_edit->selectAll();
    }
}

qint64 TimelineView::time_field_base() const{
    if(m_time_edit->isModified()){
        if(const auto typed = parse_time_input(m_time_edit->text(), m_playhead_ms)){
            return *typed;
        }
    }

    return m_playhead_ms;
}

void TimelineView::commit_time_field(){
    const auto target = parse_time_input(m_time_edit->text(), m_playhead_ms);

    if(!target){
        set_time_field_error(true);
        QToolTip::showText(m_time_edit->mapToGlobal(QPoint(0, m_time_edit->height())),
                time_field_help(), m_time_edit);
        return;
    }

    // Exactly what was typed: no snapping.
    m_time_edit->setModified(false);
    close_time_field();
    emit requestSeek(*target);
}

void TimelineView::step_time_field(int direction, bool whole_beat){
    const double step = whole_beat ? beat_ms() : step_ms();
    if(step <= 0.0){
        return;
    }

    // Next grid tick in that direction. Ticks fall between whole ms, so
    // anything within 0.6 ms of one counts as on it.
    const double k = (time_field_base() - m_beat_offset_ms) / step;
    const double eps = 0.6 / step;
    const double next = direction > 0 ? std::floor(k + eps) + 1 : std::ceil(k - eps) - 1;
    const qint64 target = std::llround(m_beat_offset_ms + next * step);

    set_time_field_error(false);
    m_time_edit->setText(QString::number(target));
    m_time_edit->selectAll();
    emit requestSeek(target);
}

void TimelineView::set_time_field_error(bool error){
    if(m_time_edit->property("invalid").toBool() == error){
        return;
    }

    m_time_edit->setProperty("invalid", error);
    m_time_edit->setStyleSheet(error
            ? QStringLiteral("QLineEdit { border: 1px solid #d05050; border-radius: 2px; }")
            : QString());
}

void TimelineView::show_time_field_menu(const QPoint& pos){
    // The standard menu (copy, paste, select all) plus copies in the
    // formats scripts and osu! use. Nothing is copied unless chosen.
    std::unique_ptr<QMenu> menu(m_time_edit->createStandardContextMenu());
    const qint64 t = time_field_base();

    menu->addSeparator();
    menu->addAction(tr("Copy as Milliseconds (%1)").arg(t), this, [t]{
        QGuiApplication::clipboard()->setText(QString::number(t));
    });
    menu->addAction(tr("Copy as osu! Timestamp (%1)").arg(format_osu_timestamp(t)), this, [t]{
        QGuiApplication::clipboard()->setText(format_osu_timestamp(t));
    });

    menu->exec(m_time_edit->mapToGlobal(pos));
}

bool TimelineView::eventFilter(QObject* watched, QEvent* e){
    if(watched != m_time_edit){
        return QWidget::eventFilter(watched, e);
    }

    switch(e->type()){
        case QEvent::FocusOut:
            // Clicking elsewhere closes it (but not its own right-click menu).
            if(static_cast<QFocusEvent*>(e)->reason() != Qt::PopupFocusReason){
                QTimer::singleShot(0, this, [this]{
                    if(!m_time_edit->hasFocus()){
                        close_time_field();
                    }
                });
            }
            break;

        case QEvent::KeyPress: {
            auto* key = static_cast<QKeyEvent*>(e);

            if(key->key() == Qt::Key_Escape){
                close_time_field();
                return true;
            }

            if(key->key() == Qt::Key_Up || key->key() == Qt::Key_Down){
                step_time_field(key->key() == Qt::Key_Up ? 1 : -1,
                        key->modifiers() & Qt::ShiftModifier);
                return true;
            }
            break;
        }

        default:
            break;
    }

    return QWidget::eventFilter(watched, e);
}
