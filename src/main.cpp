#include <QApplication>
#include "main_window.h"
#include "ui/script_editor_factory.h"
#include "ui/theme.h"

int main(int argc, char *argv[]){
    // Has to happen before QApplication exists (Monaco's URL scheme).
    prepare_script_editor_backend();

    QApplication app(argc, argv);

    QApplication::setApplicationName("cosby");
    QApplication::setOrganizationName("tatinCode");

    apply_app_theme(app);

    MainWindow w;
    w.show();

    return app.exec();
}
