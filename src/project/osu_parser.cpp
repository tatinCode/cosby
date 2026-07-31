#include "project/osu_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

namespace{
    QString unquote_osu_value(const QString& value){
        value = value.trimmed();

        if(value.size() >= 2 && value.front() == '"' && value.back() == '"'){
            value = value.mid(1, value.size() - 2);
        }

        return value;
    }

    QStringList split_osu_value(const QString& value){
        QStringList parts;
        QString current;
        bool in_quotes = false;

        for(const QChar& ch : value){
            if(ch == '"'){
                in_quotes = !in_quotes;
                current += ch;
                continue;
            }

            if(ch == ',' && !in_quotes){
                parts.push_back(current);
                current.clear();
                continue;
            }
            
            current += ch;
        }

        parts.push_back(current.trimmed());

        return parts;
    }
}

BeatmapImportInfo parse_beatmap_metadata(const QString& osu_file_path){
    BeatmapImportInfo info;

    info.osu_file_path = osu_file_path;
    info.beatmap_folder = QFileInfo(osu_file_path).absolutePath();

    QFile f(osu_file_path);

    if(!f.open(QIODevice::ReadOnly | QIODevice::Text)){
        return info;
    }

    QString section;
    bool found_bpm = false;
    QTextStream ts(&f);

    while(!ts.atEnd()){
        QString line = ts.readLine().trimmed();

        if(line.startsWith(QChar(0xfeff))){
            line.remove(0, 1);
        }

        if(line.isEmpty() || line.startsWith("//")){
            continue;
        }

        if(line.startsWith("[") && line.endsWith("]")){
            section = line;
            continue;
        }

        if(section == "[General]"){
            const int colon_idx = line.indexOf(':');
            if(colon < 0){
                continue;
            }

            const QString key = line.left(colon).trimmed();
            const QString value = line.mid(colon + 1).trimmed();

            if(key == "AudioFilename"){
                info.audio_file = value;
            }

            continue;
        }

        if(section == "[Events]" && info.background_file.isEmpty()){
            const QStringList parts = split_osu_value(line);

            if(parts.size() >= 3){
                const QString event_type = parts[0].trimmed();

                if(event_type == "0" || event_type.compare("Background", Qt::CaseInsensitive) == 0){
                    info.background_file = unquote_osu_value(parts[2]);
                }
            }

            continue;
        }

        if(section == "[TimingPoints]" && !found_bpm){
            const QStringList parts = split_osu_csv_line(line);

            if(parts.size() >= 2){
                bool ok = false;

                const double beat_length = parts[1].trimmed().toDouble(&ok);
                const bool uninherited = parts.size() < 7 || parts[6].trimmed() == "1";

                if(ok && uninherited && beat_length > 0.0){
                    info.bpm = 60000.0 / beat_length;

                    found_bpm = true;
                }

            }

            continue;
        }
        
    }

    return info;
}
