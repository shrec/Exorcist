#include "renamesuggestionwidget.h"
#include "editorview.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTextBlock>

RenameSuggestionWidget::RenameSuggestionWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::ToolTip);
    setAttribute(Qt::WA_ShowWithoutActivating, false);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setSpacing(4);

    m_label = new QLabel(this);
    m_label->setStyleSheet(QStringLiteral("color: #aaa; font-size: 11px;"));

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setMinimumWidth(120);
    m_nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #2d2d2d; color: #e0e0e0; border: 1px solid #555; "
        "padding: 2px 4px; font-size: 12px; }"));

    m_acceptBtn = new QPushButton(tr("Tab ✓"), this);
    m_acceptBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2ea043; color: white; border: none; "
        "padding: 2px 8px; font-size: 11px; }"));

    m_rejectBtn = new QPushButton(tr("Esc ✕"), this);
    m_rejectBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #555; color: white; border: none; "
        "padding: 2px 8px; font-size: 11px; }"));

    layout->addWidget(m_label);
    layout->addWidget(m_nameEdit);
    layout->addWidget(m_acceptBtn);
    layout->addWidget(m_rejectBtn);

    setStyleSheet(QStringLiteral(
        "RenameSuggestionWidget { background: #252526; border: 1px solid #444; }"));

    connect(m_acceptBtn, &QPushButton::clicked, this, [this] {
        emit accepted(m_oldName, m_nameEdit->text());
        QWidget::hide();
    });
    connect(m_rejectBtn, &QPushButton::clicked, this, [this] {
        emit rejected();
        QWidget::hide();
    });
}

void RenameSuggestionWidget::showSuggestion(const QString &oldName,
                                             const QString &newName,
                                             EditorView *editor,
                                             int line, int column)
{
    Q_UNUSED(column)
    m_oldName = oldName;
    m_label->setText(tr("Rename '%1' →").arg(oldName));
    m_nameEdit->setText(newName);
    m_nameEdit->selectAll();

    // Position below the cursor in the editor
    if (editor) {
        const QTextBlock block = editor->document()->findBlockByNumber(line);
        if (block.isValid()) {
            QTextCursor cursor(block);
            const QRect cursorRect = editor->cursorRect(cursor);
            const QPoint globalPos = editor->mapToGlobal(cursorRect.bottomLeft());
            move(globalPos.x(), globalPos.y() + 4);
        }
    }

    QWidget::show();
    m_nameEdit->setFocus();
}

void RenameSuggestionWidget::hide()
{
    QWidget::hide();
}

void RenameSuggestionWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Return) {
        emit accepted(m_oldName, m_nameEdit->text());
        QWidget::hide();
    } else if (event->key() == Qt::Key_Escape) {
        emit rejected();
        QWidget::hide();
    } else {
        QWidget::keyPressEvent(event);
    }
}
