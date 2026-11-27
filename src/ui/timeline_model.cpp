#include "ui/timeline_model.h"

#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace {
    struct span {
        int t0;
        int t1;
    };

    // Commands with t1 <= t0 are instants (or "from t0 on" for flags).
    int command_end(const command& cmd){
        return std::max(cmd.t0, cmd.t1);
    }

    bool lifetime(const sprite& s, span& out){
        if(s.cmds.isEmpty()){
            return false;
        }

        out = {s.cmds.front().t0, command_end(s.cmds.front())};

        for(const command& cmd : s.cmds){
            out.t0 = std::min(out.t0, cmd.t0);
            out.t1 = std::max(out.t1, command_end(cmd));
        }

        return true;
    }

    // Union of the members' lifetimes as few non-overlapping bars.
    QVector<timeline_bar> merged_lifetimes(const scene& sc, const QVector<qsizetype>& members){
        QVector<span> spans;
        spans.reserve(members.size());

        for(qsizetype index : members){
            span s{};
            if(lifetime(sc.sprites.at(index), s)){
                spans.push_back(s);
            }
        }

        std::sort(spans.begin(), spans.end(), [](const span& a, const span& b){
            return a.t0 < b.t0;
        });

        QVector<timeline_bar> bars;

        for(const span& s : spans){
            if(!bars.isEmpty() && s.t0 <= bars.back().t1){
                bars.back().t1 = std::max(bars.back().t1, s.t1);
                continue;
            }

            timeline_bar bar;
            bar.t0 = s.t0;
            bar.t1 = s.t1;
            bars.push_back(bar);
        }

        return bars;
    }

    QString display_name(const sprite& s){
        return s.name.isEmpty() ? QFileInfo(s.path).fileName() : s.name;
    }

    QString line_detail(int line){
        return line > 0 ? QStringLiteral("line %1").arg(line) : QString();
    }
}

QString timeline_model::layer_name(layer_t layer){
    switch(layer){
        case layer_t::background: return QStringLiteral("Background");
        case layer_t::fail:       return QStringLiteral("Fail");
        case layer_t::pass:       return QStringLiteral("Pass");
        case layer_t::foreground: return QStringLiteral("Foreground");
        case layer_t::overlay:    return QStringLiteral("Overlay");
    }

    return QStringLiteral("Foreground");
}

QString timeline_model::command_name(cmd_t type){
    switch(type){
        case cmd_t::fade:      return QStringLiteral("Fade");
        case cmd_t::move:      return QStringLiteral("Move");
        case cmd_t::scale:     return QStringLiteral("Scale");
        case cmd_t::rotate:    return QStringLiteral("Rotate");
        case cmd_t::color:     return QStringLiteral("Color");
        case cmd_t::move_x:    return QStringLiteral("Move X");
        case cmd_t::move_y:    return QStringLiteral("Move Y");
        case cmd_t::scale_vec: return QStringLiteral("Scale XY");
        case cmd_t::flip_h:    return QStringLiteral("Flip H");
        case cmd_t::flip_v:    return QStringLiteral("Flip V");
        case cmd_t::additive:  return QStringLiteral("Additive");
    }

    return QString();
}

void timeline_model::set_scene(const scene& sc){
    m_scene = sc;   // implicitly shared: cheap, and immune to the next run
    rebuild();
}

void timeline_model::clear(){
    m_scene = scene{};
    rebuild();
}

void timeline_model::toggle_expanded(int row){
    if(row < 0 || row >= m_rows.size() || !m_rows.at(row).expandable){
        return;
    }

    m_expanded.insert(m_rows.at(row).key, !m_rows.at(row).expanded);
    rebuild();
}

void timeline_model::set_expanded(const QString& key, bool expanded){
    m_expanded.insert(key, expanded);
    rebuild();
}

bool timeline_model::is_expanded(const QString& key, bool fallback) const{
    return m_expanded.value(key, fallback);
}

void timeline_model::add_sprite_rows(qsizetype index, int depth, const QString& key){
    const sprite& s = m_scene.sprites.at(index);

    timeline_row row;
    row.kind = timeline_row_kind::sprite;
    row.layer = s.layer;
    row.depth = depth;
    row.label = display_name(s);
    row.detail = line_detail(s.source_line);
    row.source_line = s.source_line;
    row.key = key;
    row.expandable = !s.cmds.isEmpty();
    row.expanded = row.expandable && is_expanded(key, false);
    row.first_sprite = index;
    row.sprite_count = 1;

    span life{};
    if(lifetime(s, life)){
        timeline_bar bar;
        bar.t0 = life.t0;
        bar.t1 = life.t1;
        bar.sprite = index;
        row.bars.push_back(bar);
    }

    const bool expanded = row.expanded;
    m_rows.push_back(std::move(row));

    if(!expanded){
        return;
    }

    // One lane per command type, in enum order, only for types it uses.
    for(int type = int(cmd_t::fade); type <= int(cmd_t::additive); ++type){
        timeline_row lane;
        lane.kind = timeline_row_kind::command_lane;
        lane.layer = s.layer;
        lane.depth = depth + 1;
        lane.label = command_name(cmd_t(type));
        lane.command_type = cmd_t(type);
        lane.source_line = s.source_line;
        lane.key = key + QStringLiteral("/") + QString::number(type);
        lane.first_sprite = index;
        lane.sprite_count = 1;

        for(qsizetype c = 0; c < s.cmds.size(); ++c){
            const command& cmd = s.cmds.at(c);

            if(int(cmd.type) != type){
                continue;
            }

            timeline_bar bar;
            bar.t0 = cmd.t0;
            bar.t1 = cmd.t1;
            bar.sprite = index;
            bar.command = c;
            bar.type = cmd.type;
            lane.bars.push_back(bar);
        }

        if(!lane.bars.isEmpty()){
            m_rows.push_back(std::move(lane));
        }
    }
}

void timeline_model::rebuild(){
    m_rows.clear();

    m_time_begin = 1;
    m_time_end = 0;

    for(const sprite& s : m_scene.sprites){
        span life{};
        if(!lifetime(s, life)){
            continue;
        }

        if(m_time_begin > m_time_end){
            m_time_begin = life.t0;
            m_time_end = life.t1;
        }
        else{
            m_time_begin = std::min(m_time_begin, life.t0);
            m_time_end = std::max(m_time_end, life.t1);
        }
    }

    for(layer_t layer : layer_order){
        QVector<qsizetype> members;

        for(qsizetype i = 0; i < m_scene.sprites.size(); ++i){
            if(m_scene.sprites.at(i).layer == layer){
                members.push_back(i);
            }
        }

        if(members.isEmpty()){
            continue;
        }

        const QString layer_key = QStringLiteral("layer|%1").arg(layer_name(layer));

        timeline_row layer_row;
        layer_row.kind = timeline_row_kind::layer;
        layer_row.layer = layer;
        layer_row.label = layer_name(layer);
        layer_row.detail = members.size() == 1
            ? QStringLiteral("1 sprite")
            : QStringLiteral("%1 sprites").arg(members.size());
        layer_row.key = layer_key;
        layer_row.expandable = true;
        layer_row.expanded = is_expanded(layer_key, true);
        layer_row.first_sprite = members.front();
        layer_row.sprite_count = members.size();
        layer_row.bars = merged_lifetimes(m_scene, members);

        const bool layer_expanded = layer_row.expanded;
        m_rows.push_back(std::move(layer_row));

        if(!layer_expanded){
            continue;
        }

        // Group by call site, in order of first appearance (= draw order).
        struct entry {
            QString key;
            QVector<qsizetype> members;
        };

        QVector<entry> entries;
        QHash<QString, qsizetype> entry_of;
        QHash<QString, int> ungrouped_count;

        for(qsizetype index : members){
            const sprite& s = m_scene.sprites.at(index);
            QString key = QStringLiteral("%1|%2|%3|%4")
                .arg(layer_name(layer), QString::number(s.source_line), s.path, s.name);

            // Unknown line: nothing to group by, every sprite stands alone.
            if(s.source_line <= 0){
                const int ordinal = ungrouped_count[key]++;
                key += QStringLiteral("#") + QString::number(ordinal);
            }

            auto it = entry_of.find(key);
            if(it == entry_of.end()){
                it = entry_of.insert(key, entries.size());
                entries.push_back(entry{key, {}});
            }

            entries[*it].members.push_back(index);
        }

        for(const entry& e : entries){
            if(e.members.size() == 1){
                add_sprite_rows(e.members.front(), 1, QStringLiteral("sprite|") + e.key);
                continue;
            }

            const sprite& first = m_scene.sprites.at(e.members.front());
            const QString group_key = QStringLiteral("group|") + e.key;

            timeline_row group;
            group.kind = timeline_row_kind::group;
            group.layer = layer;
            group.depth = 1;
            group.label = QStringLiteral("%1 ×%2").arg(display_name(first), QString::number(e.members.size()));
            group.detail = line_detail(first.source_line);
            group.source_line = first.source_line;
            group.key = group_key;
            group.expandable = true;
            group.expanded = is_expanded(group_key, false);
            group.first_sprite = e.members.front();
            group.sprite_count = e.members.size();
            group.bars = merged_lifetimes(m_scene, e.members);

            const bool group_expanded = group.expanded;
            m_rows.push_back(std::move(group));

            if(!group_expanded){
                continue;
            }

            // Member keys use the position in the group, which is stable
            // as long as the script creates them in the same order.
            for(qsizetype n = 0; n < e.members.size(); ++n){
                add_sprite_rows(e.members.at(n), 2,
                        QStringLiteral("sprite|%1#%2").arg(e.key, QString::number(n)));
            }
        }
    }
}
