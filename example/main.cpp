#include "ExampleWidget.h"

#include <QApplication>
#include <QDialog>
#include <QBoxLayout>


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(true);

    QDialog mainDialog;
    auto mainLayout = new QVBoxLayout(&mainDialog);
    auto exampleWidget = new ExampleWidget;
    mainLayout->addWidget(exampleWidget);
    mainDialog.adjustSize();
    mainDialog.show();

    return app.exec();
}