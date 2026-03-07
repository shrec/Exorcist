#pragma once

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

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
    explicit TrajectoryReplayWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(6, 4, 6, 4);

        auto *title = new QLabel(tr("Trajectory Replay"), this);
        title->setStyleSheet(QStringLiteral("font-weight: bold;"));
        layout->addWidget(title);

        // Controls
        auto *controls = new QHBoxLayout;
        m_loadBtn = new QPushButton(tr("Load"), this);
        connect(m_loadBtn, &QPushButton::clicked, this, &TrajectoryReplayWidget::loadFile);
        controls->addWidget(m_loadBtn);

        m_prevBtn = new QPushButton(QStringLiteral("\u25C0"), this);
        m_prevBtn->setFixedWidth(30);
        connect(m_prevBtn, &QPushButton::clicked, this, &TrajectoryReplayWidget::prevStep);
        controls->addWidget(m_prevBtn);

        m_playBtn = new QPushButton(QStringLiteral("\u25B6"), this);
        m_playBtn->setFixedWidth(30);
        connect(m_playBtn, &QPushButton::clicked, this, &TrajectoryReplayWidget::togglePlay);
        controls->addWidget(m_playBtn);

        m_nextBtn = new QPushButton(QStringLiteral("\u25B6"), this);
        m_nextBtn->setFixedWidth(30);
        connect(m_nextBtn, &QPushButton::clicked, this, &TrajectoryReplayWidget::nextStep);
        controls->addWidget(m_nextBtn);

        m_stepLabel = new QLabel(QStringLiteral("0/0"), this);
        controls->addWidget(m_stepLabel);
        controls->addStretch();
        layout->addLayout(controls);

        // Slider
        m_slider = new QSlider(Qt::Horizontal, this);
        m_slider->setMinimum(0);
        connect(m_slider, &QSlider::valueChanged, this, [this](int val) {
            m_currentStep = val;
            showStep();
        });
        layout->addWidget(m_slider);

        // Content
        m_content = new QPlainTextEdit(this);
        m_content->setReadOnly(true);
        m_content->setFont(QFont(QStringLiteral("Consolas"), 10));
        layout->addWidget(m_content);

        m_timer = new QTimer(this);
        m_timer->setInterval(1500);
        connect(m_timer, &QTimer::timeout, this, &TrajectoryReplayWidget::nextStep);
    }

    // Load trajectory from JSON array
    void loadTrajectory(const QJsonArray &steps)
    {
        m_steps = steps;
        m_currentStep = 0;
        m_slider->setMaximum(qMax(0, steps.size() - 1));
        m_slider->setValue(0);
        showStep();
    }

signals:
    void stepChanged(int step, const QJsonObject &data);

private slots:
    void loadFile()
    {
        const QString path = QFileDialog::getOpenFileName(this, tr("Load Trajectory"),
            {}, tr("JSON Files (*.json)"));
        if (path.isEmpty()) return;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return;

        const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
        loadTrajectory(arr);
    }

    void nextStep()
    {
        if (m_currentStep < m_steps.size() - 1) {
            m_currentStep++;
            m_slider->setValue(m_currentStep);
        } else {
            m_timer->stop();
            m_playBtn->setText(QStringLiteral("\u25B6"));
        }
    }

    void prevStep()
    {
        if (m_currentStep > 0) {
            m_currentStep--;
            m_slider->setValue(m_currentStep);
        }
    }

    void togglePlay()
    {
        if (m_timer->isActive()) {
            m_timer->stop();
            m_playBtn->setText(QStringLiteral("\u25B6"));
        } else {
            m_timer->start();
            m_playBtn->setText(QStringLiteral("\u23F8"));
        }
    }

private:
    void showStep()
    {
        if (m_steps.isEmpty()) {
            m_content->clear();
            m_stepLabel->setText(QStringLiteral("0/0"));
            return;
        }

        const QJsonObject step = m_steps[m_currentStep].toObject();
        m_stepLabel->setText(QStringLiteral("%1/%2").arg(m_currentStep + 1).arg(m_steps.size()));

        const QString type = step.value(QStringLiteral("type")).toString();
        const QString content = step.value(QStringLiteral("content")).toString();
        const QString toolName = step.value(QStringLiteral("toolName")).toString();
        const QString ts = step.value(QStringLiteral("timestamp")).toString();

        QString display = QStringLiteral("[%1] %2").arg(ts, type);
        if (!toolName.isEmpty())
            display += QStringLiteral(" (%1)").arg(toolName);
        display += QStringLiteral("\n\n%1").arg(content);

        m_content->setPlainText(display);
        emit stepChanged(m_currentStep, step);
    }

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
