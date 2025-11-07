#include "BarcodeWidget.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    BarcodeWidget w;
    w.show();
    return app.exec();
}
