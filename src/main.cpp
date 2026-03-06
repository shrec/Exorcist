#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("Exorcist");

    auto *editor = new QTextEdit();
    editor->setPlainText("Exorcist: fast, lightweight, cross-platform editor.");
    window.setCentralWidget(editor);

    window.resize(960, 600);
    window.show();

    return app.exec();
}
