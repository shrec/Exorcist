#include "commandpalette.h"

#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

CommandPalette::CommandPalette(Mode mode, QWidget *parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint),
      m_mode(mode),
      m_input(new QLineEdit(this)),
      m_list(new QListWidget(this))
{
    setMinimumWidth(560);

    const QString hint = (mode == FileMode)
        ? tr("Go to file  (type to filter)")
        : tr("Run command (type to filter)");

    auto *hint_label = new QLabel(hint, this);
    hint_label->setStyleSheet("color: #888; padding: 2px 6px;");

    m_input->setPlaceholderText(mode == FileMode ? tr("file name...") : tr("command name..."));
    m_input->setStyleSheet("QLineEdit { background: #1e1e1e; color: #ddd;"
                           " border: none; padding: 6px; font-size: 14px; }");

    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setStyleSheet("QListWidget { background: #252526; color: #ccc; font-size: 13px; }"
                          "QListWidget::item:selected { background: #094771; color: #fff; }"
                          "QListWidget::item:hover { background: #2a2d2e; }");
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(hint_label);
    layout->addWidget(m_input);
    layout->addWidget(m_list);
    setLayout(layout);

    connect(m_input, &QLineEdit::textChanged, this, &CommandPalette::filterItems);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
        acceptCurrent();
    });

    m_input->installEventFilter(this);
}

void CommandPalette::setFiles(const QStringList &filePaths)
{
    m_allItems = filePaths;
    filterItems({});
}

void CommandPalette::setCommands(const QStringList &commands)
{
    m_allItems = commands;
    filterItems({});
}

QString CommandPalette::selectedFile() const
{
    if (m_mode == FileMode && m_selectedIndex >= 0 && m_selectedIndex < m_allItems.size()) {
        return m_allItems.at(m_selectedIndex);
    }
    return {};
}

int CommandPalette::selectedCommandIndex() const
{
    return m_selectedIndex;
}

void CommandPalette::filterItems(const QString &text)
{
    m_list->clear();

    const QString lower = text.toLower();
    for (int i = 0; i < m_allItems.size(); ++i) {
        const QString &item = m_allItems.at(i);
        // For files show only the filename in the label but keep full path in UserRole.
        const QString display = (m_mode == FileMode)
            ? QFileInfo(item).fileName() + "  \t" + item
            : item;

        if (lower.isEmpty() || item.toLower().contains(lower)) {
            auto *wi = new QListWidgetItem(display, m_list);
            wi->setData(Qt::UserRole, i);
        }
    }

    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
}

void CommandPalette::acceptCurrent()
{
    QListWidgetItem *cur = m_list->currentItem();
    if (!cur) {
        return;
    }
    m_selectedIndex = cur->data(Qt::UserRole).toInt();
    accept();
}

bool CommandPalette::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        switch (ke->key()) {
        case Qt::Key_Down:
            m_list->setCurrentRow(qMin(m_list->currentRow() + 1, m_list->count() - 1));
            return true;
        case Qt::Key_Up:
            m_list->setCurrentRow(qMax(m_list->currentRow() - 1, 0));
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            acceptCurrent();
            return true;
        case Qt::Key_Escape:
            reject();
            return true;
        default:
            break;
        }
    }
    return QDialog::eventFilter(obj, event);
}
