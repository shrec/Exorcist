#include "qsspreviewpane.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>

namespace exo::forms {

QssPreviewPane::QssPreviewPane(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(m_scroll);

    // Sample host: this is the widget we apply user QSS to. Keep it as a
    // dedicated subtree so the editor's own chrome does not get restyled.
    m_sampleHost = new QWidget;
    m_sampleHost->setObjectName(QStringLiteral("QssSampleHost"));
    m_scroll->setWidget(m_sampleHost);

    buildSampleGallery();
    applyBaselineTheme();

    setMinimumSize(180, 120);
}

QssPreviewPane::~QssPreviewPane() = default;

void QssPreviewPane::buildSampleGallery()
{
    auto *outer = new QVBoxLayout(m_sampleHost);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(12);

    // ── Buttons (states side by side: normal / hover / pressed / disabled).
    auto *buttonsBox = new QGroupBox(tr("Buttons (states)"), m_sampleHost);
    auto *buttonsLay = new QHBoxLayout(buttonsBox);
    buttonsLay->setSpacing(8);
    {
        auto *normal = new QPushButton(tr("Normal"));
        auto *hover  = new QPushButton(tr("Hover"));
        // We can't truly force :hover without input simulation, so we
        // visually hint via an objectName so users can target it in QSS.
        hover->setObjectName(QStringLiteral("hoverSample"));
        auto *pressed = new QPushButton(tr("Pressed"));
        pressed->setObjectName(QStringLiteral("pressedSample"));
        pressed->setDown(true);
        auto *disabled = new QPushButton(tr("Disabled"));
        disabled->setEnabled(false);
        auto *flat = new QPushButton(tr("Click Me"));
        flat->setDefault(true);
        buttonsLay->addWidget(flat);
        buttonsLay->addWidget(normal);
        buttonsLay->addWidget(hover);
        buttonsLay->addWidget(pressed);
        buttonsLay->addWidget(disabled);
        buttonsLay->addStretch(1);
    }
    outer->addWidget(buttonsBox);

    // ── Inputs (line edit + combo).
    auto *inputsBox = new QGroupBox(tr("Inputs"), m_sampleHost);
    auto *inputsLay = new QFormLayout(inputsBox);
    {
        auto *line = new QLineEdit;
        line->setPlaceholderText(tr("Type here..."));
        inputsLay->addRow(new QLabel(tr("QLineEdit:")), line);

        auto *combo = new QComboBox;
        combo->addItems({tr("Option One"), tr("Option Two"), tr("Option Three")});
        inputsLay->addRow(new QLabel(tr("QComboBox:")), combo);
    }
    outer->addWidget(inputsBox);

    // ── Toggles (checkbox + radios).
    auto *togglesBox = new QGroupBox(tr("Toggles"), m_sampleHost);
    auto *togglesLay = new QHBoxLayout(togglesBox);
    {
        auto *cb = new QCheckBox(tr("Enabled"));
        cb->setChecked(true);
        togglesLay->addWidget(cb);

        auto *r1 = new QRadioButton(tr("Choice A"));
        auto *r2 = new QRadioButton(tr("Choice B"));
        r1->setChecked(true);
        auto *grp = new QButtonGroup(togglesBox);
        grp->addButton(r1);
        grp->addButton(r2);
        togglesLay->addWidget(r1);
        togglesLay->addWidget(r2);
        togglesLay->addStretch(1);
    }
    outer->addWidget(togglesBox);

    // ── Progress + slider.
    auto *progBox = new QGroupBox(tr("Progress / Slider"), m_sampleHost);
    auto *progLay = new QFormLayout(progBox);
    {
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue(50);
        progLay->addRow(new QLabel(tr("QSlider:")), slider);

        auto *bar = new QProgressBar;
        bar->setRange(0, 100);
        bar->setValue(60);
        progLay->addRow(new QLabel(tr("QProgressBar:")), bar);
    }
    outer->addWidget(progBox);

    // ── QTabWidget with 2 tabs.
    auto *tabs = new QTabWidget(m_sampleHost);
    {
        auto *page1 = new QWidget;
        auto *p1Lay = new QVBoxLayout(page1);
        p1Lay->addWidget(new QLabel(tr("Sample text on tab 1")));
        p1Lay->addWidget(new QLabel(tr("Style QTabBar::tab and QTabWidget::pane to customize.")));
        p1Lay->addStretch(1);
        tabs->addTab(page1, tr("First"));

        auto *page2 = new QWidget;
        auto *p2Lay = new QVBoxLayout(page2);
        p2Lay->addWidget(new QLabel(tr("Sample text on tab 2")));
        p2Lay->addStretch(1);
        tabs->addTab(page2, tr("Second"));
    }
    outer->addWidget(tabs);

    // ── QListWidget with 5 items.
    auto *listBox = new QGroupBox(tr("List"), m_sampleHost);
    auto *listLay = new QVBoxLayout(listBox);
    {
        auto *list = new QListWidget;
        list->addItems({tr("Apple"), tr("Banana"), tr("Cherry"),
                        tr("Date"), tr("Elderberry")});
        list->setCurrentRow(1);
        list->setMaximumHeight(110);
        listLay->addWidget(list);
    }
    outer->addWidget(listBox);

    // ── A grouped composite section (group containing some of above).
    auto *compositeBox = new QGroupBox(tr("Group (composite)"), m_sampleHost);
    auto *compLay = new QGridLayout(compositeBox);
    {
        compLay->addWidget(new QLabel(tr("Sample text")), 0, 0);
        auto *btn = new QPushButton(tr("Action"));
        compLay->addWidget(btn, 0, 1);
        auto *cb2 = new QCheckBox(tr("Option"));
        cb2->setChecked(true);
        compLay->addWidget(cb2, 1, 0);
        auto *line = new QLineEdit;
        line->setPlaceholderText(tr("Search..."));
        compLay->addWidget(line, 1, 1);
    }
    outer->addWidget(compositeBox);

    outer->addStretch(1);
}

void QssPreviewPane::applyBaselineTheme()
{
    // VS dark-modern baseline. The sample host is the QSS scope — user QSS
    // is appended on top of this baseline so users see overrides clearly.
    static const QString kBaseline = QStringLiteral(
        "#QssSampleHost { background-color: #1e1e1e; color: #d4d4d4; }"
        "#QssSampleHost QLabel { color: #d4d4d4; }"
        "#QssSampleHost QGroupBox { color: #cccccc; border: 1px solid #3e3e42; "
        "                           border-radius: 2px; margin-top: 12px; padding-top: 8px; }"
        "#QssSampleHost QGroupBox::title { subcontrol-origin: margin; "
        "                                  subcontrol-position: top left; "
        "                                  padding: 0 6px; color: #9cdcfe; }"
        "#QssSampleHost QPushButton { background-color: #3a3d41; color: #ffffff; "
        "                             border: 1px solid #3e3e42; border-radius: 2px; "
        "                             padding: 4px 12px; }"
        "#QssSampleHost QPushButton:hover { background-color: #45494e; }"
        "#QssSampleHost QPushButton:pressed { background-color: #094771; }"
        "#QssSampleHost QPushButton:default { border: 1px solid #007acc; }"
        "#QssSampleHost QPushButton:disabled { color: #6e6e6e; background-color: #2d2d30; }"
        "#QssSampleHost QLineEdit, #QssSampleHost QComboBox { "
        "    background-color: #1e1e1e; color: #d4d4d4; "
        "    border: 1px solid #3e3e42; border-radius: 2px; padding: 3px 6px; }"
        "#QssSampleHost QLineEdit:focus, #QssSampleHost QComboBox:focus { border-color: #007acc; }"
        "#QssSampleHost QCheckBox, #QssSampleHost QRadioButton { color: #d4d4d4; }"
        "#QssSampleHost QListWidget { background-color: #252526; color: #d4d4d4; "
        "                             border: 1px solid #3e3e42; border-radius: 2px; }"
        "#QssSampleHost QListWidget::item:selected { background-color: #094771; color: #ffffff; }"
        "#QssSampleHost QProgressBar { background-color: #2d2d30; color: #ffffff; "
        "                              border: 1px solid #3e3e42; border-radius: 2px; "
        "                              text-align: center; }"
        "#QssSampleHost QProgressBar::chunk { background-color: #007acc; }"
        "#QssSampleHost QSlider::groove:horizontal { background: #3e3e42; height: 4px; border-radius: 2px; }"
        "#QssSampleHost QSlider::handle:horizontal { background: #007acc; width: 12px; "
        "                                            margin: -4px 0; border-radius: 2px; }"
        "#QssSampleHost QTabWidget::pane { border: 1px solid #3e3e42; background: #1e1e1e; }"
        "#QssSampleHost QTabBar::tab { background: #2d2d30; color: #d4d4d4; "
        "                              padding: 4px 12px; border: 1px solid #3e3e42; }"
        "#QssSampleHost QTabBar::tab:selected { background: #1e1e1e; color: #ffffff; "
        "                                       border-bottom: 1px solid #1e1e1e; }"
    );
    if (m_sampleHost) {
        // Combine baseline with user QSS — user rules come last and win.
        const QString combined = kBaseline + QLatin1Char('\n') + m_lastQss;
        m_sampleHost->setStyleSheet(combined);
    }
}

bool QssPreviewPane::applyStyleSheet(const QString &qss)
{
    m_lastQss = qss;
    applyBaselineTheme();
    emit stylesheetApplied();
    return true;
}

void QssPreviewPane::reload()
{
    applyBaselineTheme();
    emit stylesheetApplied();
}

void QssPreviewPane::clear()
{
    m_lastQss.clear();
    applyBaselineTheme();
    emit stylesheetApplied();
}

} // namespace exo::forms
