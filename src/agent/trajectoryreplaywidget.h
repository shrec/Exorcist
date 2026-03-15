#pragma once

#include <QJsonArray>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QTimer;
class QJsonObject;

// ─────────────────────────────────────────────────────────────────────────────
// TrajectoryReplayWidget — step-by-step replay of saved agent reasoning.
//
// Loads a trajectory JSON file and allows stepping through agent actions
// with play/pause/step controls.
// ─────────────────────────────────────────────────────────────────────────────

class TrajectoryReplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrajectoryReplayWidget(QWidget *parent = nullptr);

    void loadTrajectory(const QJsonArray &steps);

signals:
    void stepChanged(int step, const QJsonObject &data);

private slots:
    void loadFile();
    void nextStep();
    void prevStep();
    void togglePlay();

private:
    void showStep();

    QPushButton *m_loadBtn;
    QPushButton *m_prevBtn;
    QPushButton *m_playBtn;
    QPushButton *m_nextBtn;
    QLabel *m_stepLabel;
    QSlider *m_slider;
    QPlainTextEdit *m_content;
    QTimer *m_timer;

    QJsonArray m_steps;
    int m_currentStep = 0;
};
