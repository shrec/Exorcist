#pragma once

#include <QObject>
#include <QString>

struct AIRequest
{
    QString prompt;
};

struct AIResponse
{
    QString text;
};

class IAIProvider
{
public:
    virtual ~IAIProvider() = default;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool isAvailable() const = 0;
    virtual AIResponse request(const AIRequest &request) = 0;
};

#define EXORCIST_AI_IID "org.exorcist.IAIProvider/1.0"

Q_DECLARE_INTERFACE(IAIProvider, EXORCIST_AI_IID)
