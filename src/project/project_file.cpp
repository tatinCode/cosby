#include "project/project_file.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

namespace{
    void set_error(QString* error, const QString& message){
        if(error){
            *error = message;
        }
    }
}

bool save_project_file(
        const QString& path,
        const ProjectData& project,
        QString* error
        ){

    if(error){
        error->clear();
    }

    if(project.format_version != 1){
        set_error(error, "Unsupported project format version");
        return false;
    }

    if(project.beatmap_file.isEmpty()){
        set_error(error, "Project beatmap cannot be empty");
        return false;
    }

    if(project.script_file.isEmpty()){
        set_error(error, "Project script file cannot be empty");
        return false;
    }

    QJsonObject object;
    object["formatVersion"] = project.format_version;
    object["beatmapFile"] = project.beatmap_file;
    object["scriptFile"] = project.script_file;

    const QJsonDocument document(object);
    const QByteArray json = document.toJson(QJsonDocument::Indented);

    QSaveFile file(path);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        set_error(error, QString("Could not open project file for writing: %1").arg(file.errorString()));
        return false;
    }

    const qint64 bytes_written = file.write(json);

    if(bytes_written != json.size()){
        set_error(error, QString("Could not write project file: %1").arg(file.errorString()));
        file.cancelWriting();
        return false;
    }

    if(!file.commit()){
        set_error(error, QString("Could not commit project file: %1").arg(file.errorString()));
        return false;
    }

    return true;
}

std::optional<ProjectData> load_project_file(
        const QString& path,
        QString* error
        ){
    if(error){
        error->clear();
    }

    QFile file(path);

    if(!file.open(QIODevice::ReadOnly)){
        set_error(error, QString("Could not open project file: %1").arg(file.errorString()));

        return std::nullopt;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);

    if(parse_error.error != QJsonParseError::NoError){
        set_error(error, QString("Invalid project JSON: %1").arg(parse_error.errorString()));

        return std::nullopt;
    }

    if(!document.isObject()){
        set_error(error, "Project JSON must contain an object");

        return std::nullopt;
    }

    const QJsonObject object = document.object();

    const QJsonValue format_version = object["formatVersion"];
    const QJsonValue beatmap_value = object["beatmapFile"];
    const QJsonValue script_value = object["scriptFile"];

    if(!format_version.isDouble()){
        set_error(error, "Project formatVersion must be a number");

        return std::nullopt;
    }

    if(format_version.toDouble() != 1.0){
        set_error(error, QString("Unsupported project format version: %1").arg(format_version.toDouble()));

        return std::nullopt;
    }

    if(!beatmap_value.isString()){
        set_error(error, "Project beatmapFile must be a string");

        return std::nullopt;
    }

    if(!script_value.isString()){
        set_error(error, "Project scriptFile must be a string");

        return std::nullopt;
    }

    ProjectData project;
    project.format_version = 1;
    project.beatmap_file = beatmap_value.toString();
    project.script_file = script_value.toString();

    if(project.beatmap_file.isEmpty()){
        set_error(error, "Project beatmapFile cannot be empty");

        return std::nullopt;
    }

    if(project.script_file.isEmpty()){
        set_error(error, "Project scriptFile cannot be empty");

        return std::nullopt;
    }

    return project;
}
