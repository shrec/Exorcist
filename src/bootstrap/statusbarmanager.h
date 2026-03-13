#pragma once

#include <QObject>

class QLabel;
class QStatusBar;

class StatusBarManager : public QObject
{
    Q_OBJECT

public:
    explicit StatusBarManager(QStatusBar *statusBar, QObject *parent = nullptr);

    /// Create labels and add them to the status bar.
    void initialize();

    QLabel *posLabel()           const { return m_posLabel; }
    QLabel *encodingLabel()      const { return m_encodingLabel; }
    QLabel *backgroundLabel()    const { return m_backgroundLabel; }
    QLabel *branchLabel()        const { return m_branchLabel; }
    QLabel *copilotStatusLabel() const { return m_copilotStatusLabel; }
    QLabel *indexLabel()         const { return m_indexLabel; }
    QLabel *memoryLabel()        const { return m_memoryLabel; }

signals:
    void copilotStatusClicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QStatusBar *m_statusBar = nullptr;

    QLabel *m_posLabel           = nullptr;
    QLabel *m_encodingLabel      = nullptr;
    QLabel *m_backgroundLabel    = nullptr;
    QLabel *m_branchLabel        = nullptr;
    QLabel *m_copilotStatusLabel = nullptr;
    QLabel *m_indexLabel         = nullptr;
    QLabel *m_memoryLabel        = nullptr;
};
