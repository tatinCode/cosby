#include "project/path_utils.h"

#include <QDir>

bool same_or_child_path(const QString& parent_path, const QString& child_path){
    QString parent = QDir::fromNativeSeparators(QDir::cleanPath(parent_path));
    QString child = QDir::fromNativeSeparators(QDir::cleanPath(child_path));

#ifdef Q_OS_WIN
    const Qt::CaseSensitivity path_case = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity path_case = Qt::CaseSensitive;
#endif

    if(child.compare(parent, path_case) == 0){
        return true;
    }

    if(!parent.endsWith('/')){
        parent += '/';
    }

    return child.startsWith(parent, path_case);
}
