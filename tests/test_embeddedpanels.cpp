#include <QComboBox>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QDir>

#include "../plugins/embedded-tools/embeddedcommandresolver.h"
#include "../plugins/embedded-tools/embeddedtoolspanel.h"
#include "../plugins/serial-monitor/serialmonitorcontroller.h"
#include "../plugins/serial-monitor/serialmonitorpanel.h"

namespace {

void writeWorkspaceFile(const QTemporaryDir &dir,
                        const QString &relativePath,
                        const QByteArray &content)
{
    const QFileInfo info(dir.filePath(relativePath));
    QDir().mkpath(info.absolutePath());

    QFile file(dir.filePath(relativePath));
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text), "Failed to create workspace file");
    file.write(content);
}

}

class TestEmbeddedPanels : public QObject
{
    Q_OBJECT

private slots:
    void testEmbeddedToolsPanelUpdatesHints();
    void testEmbeddedToolsPanelCommandOverrides();
    void testEmbeddedCommandResolverPlatformIo();
    void testEmbeddedCommandResolverNestedPlatformIo();
    void testEmbeddedCommandResolverEspIdf();
    void testEmbeddedCommandResolverZephyr();
    void testEmbeddedCommandResolverPyOcd();
    void testEmbeddedCommandResolverOpenOcdConfig();
    void testEmbeddedCommandResolverMakefileMonitor();
    void testEmbeddedCommandResolverCubeMx();
    void testEmbeddedCommandResolverCubeMxWithCubeProgrammerScript();
    void testEmbeddedCommandResolverFallback();
    void testSerialMonitorControllerRefreshEmitsStatus();
    void testSerialMonitorPanelStatusAndClear();
    void testSerialMonitorPanelPortsAndConnectionState();
    void testSerialMonitorPanelOptions();
};

void TestEmbeddedPanels::testEmbeddedToolsPanelUpdatesHints()
{
    EmbeddedToolsPanel panel;
    panel.setWorkspaceSummary(QStringLiteral("PlatformIO workspace detected."));
    panel.setRecommendedCommands(QStringLiteral("platformio run --target upload"),
                                 QStringLiteral("platformio device monitor"));

    auto *summary = panel.findChild<QLabel *>(QStringLiteral("embeddedToolsSummaryLabel"));
    auto *flashHint = panel.findChild<QLabel *>(QStringLiteral("embeddedToolsFlashHintLabel"));
    auto *monitorHint = panel.findChild<QLabel *>(QStringLiteral("embeddedToolsMonitorHintLabel"));
    auto *debugHint = panel.findChild<QLabel *>(QStringLiteral("embeddedToolsDebugHintLabel"));

    QVERIFY(summary != nullptr);
    QVERIFY(flashHint != nullptr);
    QVERIFY(monitorHint != nullptr);
    QVERIFY(debugHint != nullptr);
    QCOMPARE(summary->text(), QStringLiteral("PlatformIO workspace detected."));
    QVERIFY(flashHint->text().contains(QStringLiteral("platformio run --target upload")));
    QVERIFY(monitorHint->text().contains(QStringLiteral("platformio device monitor")));
    QVERIFY(debugHint->text().contains(QStringLiteral("no debug workflow inferred")));

    panel.setDebugRoute(QStringLiteral("use OpenOCD for stm32f4x"),
                        QStringLiteral("openocd -f debug/openocd-stm32.cfg"));
    QVERIFY(debugHint->text().contains(QStringLiteral("use OpenOCD for stm32f4x")));
    QVERIFY(debugHint->text().contains(QStringLiteral("openocd -f debug/openocd-stm32.cfg")));
}

void TestEmbeddedPanels::testEmbeddedToolsPanelCommandOverrides()
{
    EmbeddedToolsPanel panel;
    QSignalSpy flashSpy(&panel, &EmbeddedToolsPanel::flashCommandOverrideChanged);
    QSignalSpy monitorSpy(&panel, &EmbeddedToolsPanel::monitorCommandOverrideChanged);
    QSignalSpy debugSpy(&panel, &EmbeddedToolsPanel::debugCommandOverrideChanged);

    panel.setCommandOverrides(QStringLiteral("west flash --runner pyocd"),
                              QStringLiteral("picocom COM5 -b 115200"),
                              QStringLiteral("pyocd gdbserver --target stm32f4"));

    QCOMPARE(panel.flashCommandOverride(), QStringLiteral("west flash --runner pyocd"));
    QCOMPARE(panel.monitorCommandOverride(), QStringLiteral("picocom COM5 -b 115200"));
    QCOMPARE(panel.debugCommandOverride(), QStringLiteral("pyocd gdbserver --target stm32f4"));
    QCOMPARE(flashSpy.count(), 0);
    QCOMPARE(monitorSpy.count(), 0);
    QCOMPARE(debugSpy.count(), 0);

    auto *flashEdit = panel.findChild<QLineEdit *>(QStringLiteral("embeddedToolsFlashCommandEdit"));
    auto *monitorEdit = panel.findChild<QLineEdit *>(QStringLiteral("embeddedToolsMonitorCommandEdit"));
    auto *debugEdit = panel.findChild<QLineEdit *>(QStringLiteral("embeddedToolsDebugCommandEdit"));

    QVERIFY(flashEdit != nullptr);
    QVERIFY(monitorEdit != nullptr);
    QVERIFY(debugEdit != nullptr);

    flashEdit->setText(QStringLiteral("openocd -f interface/stlink.cfg"));
    monitorEdit->setText(QStringLiteral("miniterm COM5 115200"));
    debugEdit->setText(QStringLiteral("openocd -f board/stm32.cfg"));

    QCOMPARE(panel.flashCommandOverride(), QStringLiteral("openocd -f interface/stlink.cfg"));
    QCOMPARE(panel.monitorCommandOverride(), QStringLiteral("miniterm COM5 115200"));
    QCOMPARE(panel.debugCommandOverride(), QStringLiteral("openocd -f board/stm32.cfg"));
    QCOMPARE(flashSpy.count(), 1);
    QCOMPARE(monitorSpy.count(), 1);
    QCOMPARE(debugSpy.count(), 1);
}

void TestEmbeddedPanels::testEmbeddedCommandResolverPlatformIo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir, QStringLiteral("platformio.ini"), "[env:uno]\nplatform = atmelavr\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("PlatformIO")));
    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("uno")));
    QCOMPARE(resolution.flashCommand,
             QStringLiteral("platformio run --target upload --environment uno"));
    QCOMPARE(resolution.monitorCommand, QStringLiteral("platformio device monitor"));
    QVERIFY(resolution.debugSummary.contains(QStringLiteral("PlatformIO debug")));
    QCOMPARE(resolution.debugCommand, QStringLiteral("platformio debug --environment uno"));
}

void TestEmbeddedPanels::testEmbeddedCommandResolverNestedPlatformIo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir,
                       QStringLiteral("firmware/platformio.ini"),
                       "[env:nucleo_f446re]\nplatform = ststm32\nupload_port = COM7\nmonitor_port = COM8\nmonitor_speed = 460800\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("PlatformIO")));
    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("nucleo_f446re")));
    QCOMPARE(resolution.flashCommand,
             QStringLiteral("platformio run --target upload --environment nucleo_f446re --upload-port COM7"));
    QCOMPARE(resolution.monitorCommand,
             QStringLiteral("platformio device monitor --port COM8 --baud 460800"));
    QVERIFY(resolution.debugSummary.contains(QStringLiteral("nucleo_f446re")));
    QCOMPARE(resolution.debugCommand,
             QStringLiteral("platformio debug --environment nucleo_f446re"));
}

void TestEmbeddedPanels::testEmbeddedCommandResolverEspIdf()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir,
                       QStringLiteral("CMakeLists.txt"),
                       "include($ENV{IDF_PATH}/tools/cmake/project.cmake)\nproject(app)\n# ESP-IDF\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("ESP-IDF")));
    QCOMPARE(resolution.flashCommand, QStringLiteral("idf.py flash"));
    QCOMPARE(resolution.monitorCommand, QStringLiteral("idf.py monitor"));
}

void TestEmbeddedPanels::testEmbeddedCommandResolverZephyr()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir,
                       QStringLiteral("CMakeLists.txt"),
                       "cmake_minimum_required(VERSION 3.20.0)\nfind_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})\nproject(app)\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("Zephyr")));
    QCOMPARE(resolution.flashCommand, QStringLiteral("west flash"));
    QVERIFY(resolution.monitorCommand.isEmpty());
}

void TestEmbeddedPanels::testEmbeddedCommandResolverPyOcd()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir,
                       QStringLiteral("board/pyocd.yaml"),
                       "target_override: stm32f429zitx\nfrequency: 8000000\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("pyOCD")));
    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("stm32f429zitx")));
    QCOMPARE(resolution.flashCommand, QStringLiteral("pyocd flash --target stm32f429zitx"));
    QVERIFY(resolution.monitorCommand.isEmpty());
    QVERIFY(resolution.debugSummary.contains(QStringLiteral("gdbserver --target stm32f429zitx")));
    QCOMPARE(resolution.debugCommand, QStringLiteral("pyocd gdbserver --target stm32f429zitx"));
}

void TestEmbeddedPanels::testEmbeddedCommandResolverOpenOcdConfig()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir,
                       QStringLiteral("debug/openocd-stm32.cfg"),
                       "source [find interface/stlink.cfg]\nsource [find target/stm32f4x.cfg]\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("OpenOCD")));
    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("stlink")));
    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("stm32f4x")));
    QCOMPARE(resolution.flashCommand,
             QStringLiteral("openocd -f debug/openocd-stm32.cfg"));
    QVERIFY(resolution.monitorCommand.isEmpty());
    QVERIFY(resolution.debugSummary.contains(QStringLiteral("localhost:3333")));
    QVERIFY(resolution.debugSummary.contains(QStringLiteral("stm32f4x")));
    QCOMPARE(resolution.debugCommand,
             QStringLiteral("openocd -f debug/openocd-stm32.cfg"));
}

void TestEmbeddedPanels::testEmbeddedCommandResolverMakefileMonitor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir,
                       QStringLiteral("Makefile"),
                       "CC=arm-none-eabi-gcc\nflash:\n\tavrdude -p m328p\nmonitor:\n\tpython -m serial.tools.miniterm\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("Makefile")));
    QCOMPARE(resolution.flashCommand, QStringLiteral("make flash"));
    QCOMPARE(resolution.monitorCommand, QStringLiteral("make monitor"));
}

void TestEmbeddedPanels::testEmbeddedCommandResolverCubeMx()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir, QStringLiteral("board/project.ioc"), "# STM32CubeMX project\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("STM32CubeMX")));
    QVERIFY(resolution.flashCommand.isEmpty());
    QVERIFY(resolution.monitorCommand.isEmpty());
}

void TestEmbeddedPanels::testEmbeddedCommandResolverCubeMxWithCubeProgrammerScript()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeWorkspaceFile(dir, QStringLiteral("board/project.ioc"), "# STM32CubeMX project\n");
    writeWorkspaceFile(dir,
                       QStringLiteral("tools/flash-board.sh"),
                       "#!/bin/sh\nSTM32_Programmer_CLI --connect port=SWD --download build/app.bin 0x08000000 --start\n");

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QVERIFY(resolution.workspaceSummary.contains(QStringLiteral("STM32CubeProgrammer")));
    QCOMPARE(resolution.flashCommand, QStringLiteral("tools/flash-board.sh"));
    QVERIFY(resolution.monitorCommand.isEmpty());
}

void TestEmbeddedPanels::testEmbeddedCommandResolverFallback()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    EmbeddedCommandResolver resolver;
    const EmbeddedCommandResolution resolution = resolver.resolve(dir.path());

    QCOMPARE(resolution.workspaceSummary,
             QStringLiteral("No embedded workspace markers detected."));
    QVERIFY(resolution.flashCommand.isEmpty());
    QVERIFY(resolution.monitorCommand.isEmpty());
}

void TestEmbeddedPanels::testSerialMonitorControllerRefreshEmitsStatus()
{
    SerialMonitorController controller;
    QSignalSpy statusSpy(&controller, &SerialMonitorController::statusChanged);
    QSignalSpy portsSpy(&controller, &SerialMonitorController::portsChanged);

    controller.refreshPorts();

    QCOMPARE(statusSpy.count(), 1);
    QCOMPARE(portsSpy.count(), 1);
}

void TestEmbeddedPanels::testSerialMonitorPanelStatusAndClear()
{
    SerialMonitorPanel panel;
    panel.setStatusText(QStringLiteral("Listening on COM3 @ 115200"));
    panel.clearMessages();
    panel.appendMessage(QStringLiteral("boot message"));
    panel.appendMessage(QStringLiteral("sensor: 42"));

    auto *status = panel.findChild<QLabel *>(QStringLiteral("serialMonitorStatusLabel"));
    auto *output = panel.findChild<QPlainTextEdit *>(QStringLiteral("serialMonitorOutput"));

    QVERIFY(status != nullptr);
    QVERIFY(output != nullptr);
    QCOMPARE(status->text(), QStringLiteral("Listening on COM3 @ 115200"));
    QVERIFY(output->toPlainText().contains(QStringLiteral("boot message")));
    QVERIFY(output->toPlainText().contains(QStringLiteral("sensor: 42")));

    panel.clearMessages();
    QVERIFY(output->toPlainText().isEmpty());
}

void TestEmbeddedPanels::testSerialMonitorPanelPortsAndConnectionState()
{
    SerialMonitorPanel panel;
    panel.setAvailablePorts({QStringLiteral("COM3"), QStringLiteral("COM4")});

    auto *ports = panel.findChild<QComboBox *>(QStringLiteral("serialMonitorPortCombo"));
    auto *baud = panel.findChild<QComboBox *>(QStringLiteral("serialMonitorBaudCombo"));
    auto *input = panel.findChild<QLineEdit *>(QStringLiteral("serialMonitorInput"));

    QVERIFY(ports != nullptr);
    QVERIFY(baud != nullptr);
    QVERIFY(input != nullptr);
    QCOMPARE(ports->count(), 2);
    QCOMPARE(panel.selectedBaudRate(), 115200);

    panel.setConnected(true);
    QVERIFY(!ports->isEnabled());
    QVERIFY(!baud->isEnabled());
    QVERIFY(input->isEnabled());

    panel.setConnected(false);
    QVERIFY(ports->isEnabled());
    QVERIFY(baud->isEnabled());
    QVERIFY(!input->isEnabled());
}

void TestEmbeddedPanels::testSerialMonitorPanelOptions()
{
    SerialMonitorPanel panel;
    auto *newline = panel.findChild<QComboBox *>(QStringLiteral("serialMonitorNewlineCombo"));
    auto *timestamps = panel.findChild<QCheckBox *>(QStringLiteral("serialMonitorTimestampCheck"));

    QVERIFY(newline != nullptr);
    QVERIFY(timestamps != nullptr);

    panel.setSelectedNewlineMode(SerialMonitorPanel::NewlineMode::CrLf);
    QCOMPARE(panel.selectedNewlineMode(), SerialMonitorPanel::NewlineMode::CrLf);

    panel.setTimestampsEnabled(true);
    QVERIFY(panel.timestampsEnabled());
}

QTEST_MAIN(TestEmbeddedPanels)
#include "test_embeddedpanels.moc"