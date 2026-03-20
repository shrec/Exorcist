#pragma once

#include <QObject>

class CommandServiceImpl;

class WorkbenchCommandBootstrap : public QObject
{
    Q_OBJECT

public:
    struct Callbacks
    {
        std::function<void()> newTab;
        std::function<void()> openFile;
        std::function<void()> openFolder;
        std::function<void()> save;
        std::function<void()> goToFile;
        std::function<void()> findReplace;
        std::function<void()> toggleProject;
        std::function<void()> toggleSearch;
        std::function<void()> toggleTerminal;
        std::function<void()> toggleAi;
        std::function<void()> goToSymbol;
        std::function<void()> quit;
        std::function<void()> commandPalette;
    };

    explicit WorkbenchCommandBootstrap(QObject *parent = nullptr);

    void initialize(CommandServiceImpl *commands, const Callbacks &callbacks);
};
