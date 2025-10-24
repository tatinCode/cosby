#include <QApplication>
#include "main_window.h"

int main(int argc, char *argv[]){
    QApplication app(argc, argv);

    QApplication::setApplicationName("cosby");
    QApplication::setOrganizationName("tatinCode");

    MainWindow w;
    w.resize(1400,900);
    w.show();

    return app.exec();
}
