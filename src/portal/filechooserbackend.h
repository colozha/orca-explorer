// SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "filechooserdialog.h"
#include <QDBusContext>
#include <QHash>

class PortalRequest : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")
public:
    PortalRequest(QString sender, QObject *parent)
        : QObject(parent)
        , m_sender(std::move(sender))
    {
    }
public Q_SLOTS:
    void Close();
Q_SIGNALS:
    void cancelled();

private:
    QString m_sender;
};

class FileChooserBackend : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    explicit FileChooserBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    uint version() const
    {
        return 4;
    }
public Q_SLOTS:
    uint OpenFile(const QDBusObjectPath &handle,
                  const QString &appId,
                  const QString &parentWindow,
                  const QString &title,
                  const QVariantMap &options,
                  QVariantMap &results);
    uint SaveFile(const QDBusObjectPath &handle,
                  const QString &appId,
                  const QString &parentWindow,
                  const QString &title,
                  const QVariantMap &options,
                  QVariantMap &results);
    uint SaveFiles(const QDBusObjectPath &handle,
                   const QString &appId,
                   const QString &parentWindow,
                   const QString &title,
                   const QVariantMap &options,
                   QVariantMap &results);

private:
    uint choose(FileChooserDialog::Mode mode, const QDBusObjectPath &handle, const QString &parentWindow, const QString &title, const QVariantMap &options);
    QHash<QString, PortalRequest *> m_requests;
};
