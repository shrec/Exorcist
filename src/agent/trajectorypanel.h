#pragma once

#include <QWidget>
#include "agentsession.h"

class QTextBrowser;
class QToolButton;
class QLabel;
class QVBoxLayout;

/// Visual timeline panel showing agent execution trajectory.
/// Displays each AgentStep in a turn as a visual node: model calls,
/// tool invocations, final answers, and errors.
class TrajectoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TrajectoryPanel(QWidget *parent = nullptr);

    /// Set the turn to visualize (replaces any previous display).
    void setTurn(const AgentTurn &turn);

    /// Append a single step to the current trajectory (live updates).
    void appendStep(const AgentStep &step);

public slots:
    void clear();

private:
    QString stepToHtml(const AgentStep &step, int index) const;
    QString escHtml(const QString &s) const;

    QLabel       *m_headerLabel;
    QTextBrowser *m_timeline;
    QToolButton  *m_clearBtn;
    int           m_stepCount = 0;
};
