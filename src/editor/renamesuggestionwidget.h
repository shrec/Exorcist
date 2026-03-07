#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QString>

class EditorView;

/// Small floating widget that shows AI rename suggestions inline
class RenameSuggestionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RenameSuggestionWidget(QWidget *parent = nullptr);

    void showSuggestion(const QString &oldName, const QString &newName,
                        EditorView *editor, int line, int column);
    void hide();

signals:
    void accepted(const QString &oldName, const QString &newName);
    void rejected();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLabel      *m_label;
    QLineEdit   *m_nameEdit;
    QPushButton *m_acceptBtn;
    QPushButton *m_rejectBtn;
    QString      m_oldName;
};
