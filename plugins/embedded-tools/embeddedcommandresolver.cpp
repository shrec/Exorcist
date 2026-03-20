#include "embeddedcommandresolver.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

namespace {

QString relativeWorkspacePath(const QString &root, const QString &path)
{
    if (root.isEmpty() || path.isEmpty())
        return path;

    return QDir(root).relativeFilePath(path);
}

QString findWorkspaceFile(const QString &root, const QStringList &patterns)
{
    if (root.isEmpty())
        return {};

    QDir rootDir(root);
    for (const QString &pattern : patterns) {
        const QString directPath = rootDir.filePath(pattern);
        if (!pattern.contains(QLatin1Char('*')) && QFile::exists(directPath))
            return directPath;
    }

    QDirIterator it(root,
                    patterns,
                    QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    if (it.hasNext())
        return it.next();

    return {};
}

QString readFileText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    return QString::fromUtf8(file.readAll());
}

bool workspaceContainsAnyGlob(const QString &root, const QStringList &patterns)
{
    return !findWorkspaceFile(root, patterns).isEmpty();
}

bool workspaceContainsFile(const QString &root, const QString &name)
{
    return !findWorkspaceFile(root, {name}).isEmpty();
}

bool fileContainsAny(const QString &path, const QStringList &needles)
{
    const QString content = readFileText(path);
    if (content.isEmpty())
        return false;

    for (const QString &needle : needles) {
        if (content.contains(needle, Qt::CaseInsensitive))
            return true;
    }

    return false;
}

bool workspaceFileContainsAny(const QString &root,
                             const QString &name,
                             const QStringList &needles)
{
    const QString path = findWorkspaceFile(root, {name});
    return !path.isEmpty() && fileContainsAny(path, needles);
}

bool isPlatformIoWorkspace(const QString &root)
{
    return workspaceContainsFile(root, QStringLiteral("platformio.ini"));
}

QString findPlatformIoConfig(const QString &root)
{
    return findWorkspaceFile(root, {QStringLiteral("platformio.ini")});
}

QString iniValue(const QString &content, const QString &key)
{
    const QRegularExpression pattern(
        QStringLiteral("^\\s*%1\\s*[:=]\\s*([^\\r\\n;#]+)")
            .arg(QRegularExpression::escape(key)),
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(content);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString firstPlatformIoEnvironment(const QString &content)
{
    const QRegularExpression pattern(QStringLiteral("^\\s*\\[env:([^\\]]+)\\]"),
                                     QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = pattern.match(content);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString pyOcdTargetOverride(const QString &content)
{
    return iniValue(content, QStringLiteral("target_override"));
}

QString openOcdIncludeValue(const QString &content, const QString &segment)
{
    const QRegularExpression pattern(
        QStringLiteral("source\\s*\\[find\\s+%1/([^\\]\\s]+)\\]")
            .arg(QRegularExpression::escape(segment)),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(content);
    if (!match.hasMatch())
        return {};

    QString value = match.captured(1).trimmed();
    if (value.endsWith(QStringLiteral(".cfg"), Qt::CaseInsensitive))
        value.chop(4);
    return value;
}

bool isEspIdfWorkspace(const QString &root)
{
    return workspaceFileContainsAny(root,
                                    QStringLiteral("CMakeLists.txt"),
                                    {QStringLiteral("ESP-IDF"),
                                     QStringLiteral("idf_component_register")})
        || workspaceContainsFile(root, QStringLiteral("sdkconfig"));
}

bool isZephyrWorkspace(const QString &root)
{
    return workspaceContainsFile(root, QStringLiteral("west.yml"))
        || workspaceFileContainsAny(root,
                                    QStringLiteral("CMakeLists.txt"),
                                    {QStringLiteral("find_package(Zephyr"),
                                     QStringLiteral("zephyr_interface")});
}

bool isGenericMakeWorkspace(const QString &root)
{
    return workspaceContainsFile(root, QStringLiteral("Makefile"))
        && fileContainsAny(QDir(root).filePath(QStringLiteral("Makefile")),
                           {QStringLiteral("arm-none-eabi"),
                            QStringLiteral("avrdude"),
                            QStringLiteral("openocd"),
                            QStringLiteral("flash:")});
}

bool hasCubeMxProject(const QString &root)
{
    return workspaceContainsAnyGlob(root, {QStringLiteral("*.ioc")});
}

QString findOpenOcdConfig(const QString &root)
{
    return findWorkspaceFile(root,
                             {QStringLiteral("openocd.cfg"),
                              QStringLiteral("openocd*.cfg"),
                              QStringLiteral("*openocd*.cfg")});
}

bool hasPyOcdConfig(const QString &root)
{
    return workspaceContainsAnyGlob(root,
                                    {QStringLiteral("pyocd.yaml"),
                                     QStringLiteral("pyocd.yml"),
                                     QStringLiteral("*.pyocd.yaml"),
                                     QStringLiteral("*.pyocd.yml")});
}

QString findCubeProgrammerScript(const QString &root)
{
    const QStringList scriptPatterns = {
        QStringLiteral("flash*.bat"),
        QStringLiteral("flash*.cmd"),
        QStringLiteral("flash*.ps1"),
        QStringLiteral("flash*.sh"),
        QStringLiteral("upload*.bat"),
        QStringLiteral("upload*.cmd"),
        QStringLiteral("upload*.ps1"),
        QStringLiteral("upload*.sh")
    };

    QDirIterator it(root,
                    scriptPatterns,
                    QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString scriptPath = it.next();
        if (fileContainsAny(scriptPath,
                            {QStringLiteral("STM32_Programmer_CLI"),
                             QStringLiteral("STM32CubeProgrammer"),
                             QStringLiteral("CubeProgrammer")})) {
            return scriptPath;
        }
    }

    return {};
}

bool makefileHasMonitorTarget(const QString &root)
{
    return workspaceContainsFile(root, QStringLiteral("Makefile"))
        && fileContainsAny(QDir(root).filePath(QStringLiteral("Makefile")),
                           {QStringLiteral("monitor:"),
                            QStringLiteral("serial:"),
                            QStringLiteral("uart:")});
}

}

EmbeddedCommandResolution EmbeddedCommandResolver::resolve(const QString &workspaceRoot) const
{
    const QString platformIoConfig = findPlatformIoConfig(workspaceRoot);
    if (!platformIoConfig.isEmpty()) {
        const QString platformIoContent = readFileText(platformIoConfig);
        const QString environment = firstPlatformIoEnvironment(platformIoContent);
        const QString uploadPort = iniValue(platformIoContent, QStringLiteral("upload_port"));
        const QString monitorPort = iniValue(platformIoContent, QStringLiteral("monitor_port"));
        const QString monitorSpeed = iniValue(platformIoContent, QStringLiteral("monitor_speed"));

        QString flashCommand = QStringLiteral("platformio run --target upload");
        if (!environment.isEmpty())
            flashCommand += QStringLiteral(" --environment %1").arg(environment);
        if (!uploadPort.isEmpty())
            flashCommand += QStringLiteral(" --upload-port %1").arg(uploadPort);

        QString monitorCommand = QStringLiteral("platformio device monitor");
        if (!monitorPort.isEmpty())
            monitorCommand += QStringLiteral(" --port %1").arg(monitorPort);
        if (!monitorSpeed.isEmpty())
            monitorCommand += QStringLiteral(" --baud %1").arg(monitorSpeed);

        return {
            environment.isEmpty()
                ? QStringLiteral("PlatformIO workspace detected. Upload and monitor commands are ready.")
                : QStringLiteral("PlatformIO workspace detected. Default environment: %1.").arg(environment),
            flashCommand,
            monitorCommand,
            environment.isEmpty()
                ? QStringLiteral("use the PlatformIO debug workflow or its configured GDB server target")
                : QStringLiteral("use PlatformIO debug for environment %1").arg(environment),
            environment.isEmpty()
                ? QStringLiteral("platformio debug")
                : QStringLiteral("platformio debug --environment %1").arg(environment)
        };
    }

    if (isEspIdfWorkspace(workspaceRoot)) {
        return {
            QStringLiteral("ESP-IDF workspace detected. Flash and monitor commands are routed through idf.py."),
            QStringLiteral("idf.py flash"),
            QStringLiteral("idf.py monitor"),
            QStringLiteral("launch OpenOCD and connect the Xtensa/RISC-V GDB flow through idf.py"),
            QStringLiteral("idf.py openocd")
        };
    }

    if (isZephyrWorkspace(workspaceRoot)) {
        return {
            QStringLiteral("Zephyr workspace detected. Flashing is available through west; serial monitoring depends on the active runner."),
            QStringLiteral("west flash"),
            QString(),
            QStringLiteral("use west debug or west attach with the active board runner"),
            QStringLiteral("west debug")
        };
    }

    const QString pyOcdConfig = hasPyOcdConfig(workspaceRoot)
        ? findWorkspaceFile(workspaceRoot,
                            {QStringLiteral("pyocd.yaml"),
                             QStringLiteral("pyocd.yml"),
                             QStringLiteral("*.pyocd.yaml"),
                             QStringLiteral("*.pyocd.yml")})
        : QString();
    if (!pyOcdConfig.isEmpty()) {
        const QString targetOverride = pyOcdTargetOverride(readFileText(pyOcdConfig));
        return {
            targetOverride.isEmpty()
                ? QStringLiteral("pyOCD configuration detected. Flashing is routed through the workspace target config.")
                : QStringLiteral("pyOCD configuration detected. Default target: %1.").arg(targetOverride),
            targetOverride.isEmpty()
                ? QStringLiteral("pyocd flash")
                : QStringLiteral("pyocd flash --target %1").arg(targetOverride),
            QString(),
            targetOverride.isEmpty()
                ? QStringLiteral("start pyocd gdbserver and attach with your Cortex-M GDB client")
                : QStringLiteral("start pyocd gdbserver --target %1 and attach with your Cortex-M GDB client").arg(targetOverride),
            targetOverride.isEmpty()
                ? QStringLiteral("pyocd gdbserver")
                : QStringLiteral("pyocd gdbserver --target %1").arg(targetOverride)
        };
    }

    const QString openOcdConfig = findOpenOcdConfig(workspaceRoot);
    if (!openOcdConfig.isEmpty()) {
        const QString openOcdContent = readFileText(openOcdConfig);
        const QString interfaceName = openOcdIncludeValue(openOcdContent, QStringLiteral("interface"));
        const QString targetName = openOcdIncludeValue(openOcdContent, QStringLiteral("target"));

        QString summary = QStringLiteral("OpenOCD configuration detected. Flashing uses the discovered workspace config.");
        if (!interfaceName.isEmpty() || !targetName.isEmpty()) {
            const QStringList parts = {
                interfaceName.isEmpty() ? QString() : QStringLiteral("interface %1").arg(interfaceName),
                targetName.isEmpty() ? QString() : QStringLiteral("target %1").arg(targetName)
            };

            QStringList filteredParts;
            for (const QString &part : parts) {
                if (!part.isEmpty())
                    filteredParts.append(part);
            }

            summary = QStringLiteral("OpenOCD configuration detected for %1.").arg(filteredParts.join(QStringLiteral(", ")));
        }

        return {
            summary,
            QStringLiteral("openocd -f %1").arg(relativeWorkspacePath(workspaceRoot, openOcdConfig)),
            QString(),
            targetName.isEmpty()
                ? QStringLiteral("start OpenOCD and attach your GDB client to localhost:3333")
                : QStringLiteral("start OpenOCD and attach your GDB client to localhost:3333 for %1").arg(targetName),
            QStringLiteral("openocd -f %1").arg(relativeWorkspacePath(workspaceRoot, openOcdConfig))
        };
    }

    if (isGenericMakeWorkspace(workspaceRoot)) {
        return {
            QStringLiteral("Cross-toolchain Makefile detected. Flashing uses the workspace Makefile targets."),
            QStringLiteral("make flash"),
            makefileHasMonitorTarget(workspaceRoot) ? QStringLiteral("make monitor") : QString(),
            QStringLiteral("use the workspace GDB/OpenOCD runner or add a custom debug command override"),
            QString()
        };
    }

    if (hasCubeMxProject(workspaceRoot)) {
        const QString cubeProgrammerScript = findCubeProgrammerScript(workspaceRoot);
        return {
            cubeProgrammerScript.isEmpty()
                ? QStringLiteral("STM32CubeMX project detected. Generated flash and monitor scripts depend on the selected board toolchain.")
                : QStringLiteral("STM32CubeMX project detected with STM32CubeProgrammer flash script support."),
            cubeProgrammerScript.isEmpty()
                ? QString()
                : relativeWorkspacePath(workspaceRoot, cubeProgrammerScript),
            QString(),
            QStringLiteral("use the board vendor GDB server or generated launch scripts for debugging"),
            QString()
        };
    }

    return {
        QStringLiteral("No embedded workspace markers detected."),
        QString(),
        QString(),
        QString(),
        QString()
    };
}