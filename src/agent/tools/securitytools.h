#pragma once

#include "../itool.h"

#include <functional>

// ── SecureKeyStorageTool ──────────────────────────────────────────────────────
// Provides four operations on OS-backed secure key storage (Windows Credential
// Manager, macOS Keychain, Linux Secret Service):
//   store_secret  — persist a secret under a named service key
//   get_secret    — retrieve a secret by service key
//   list_secrets  — enumerate all stored service keys (no values)
//   delete_secret — remove a secret by service key

class StoreSecretTool : public ITool
{
public:
    using SecretStorer = std::function<bool(const QString &service, const QString &value)>;

    explicit StoreSecretTool(SecretStorer storer);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    SecretStorer m_storer;
};

class GetSecretTool : public ITool
{
public:
    using SecretGetter = std::function<QString(const QString &service)>;

    explicit GetSecretTool(SecretGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    SecretGetter m_getter;
};

class ListSecretsTool : public ITool
{
public:
    using SecretLister = std::function<QStringList()>;

    explicit ListSecretsTool(SecretLister lister);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    SecretLister m_lister;
};

class DeleteSecretTool : public ITool
{
public:
    using SecretDeleter = std::function<bool(const QString &service)>;

    explicit DeleteSecretTool(SecretDeleter deleter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    SecretDeleter m_deleter;
};
