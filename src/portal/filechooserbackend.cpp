// SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "filechooserbackend.h"
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>

void PortalRequest::Close()
{
    if (!calledFromDBus() || message().service() != m_sender) {
        if (calledFromDBus())
            sendErrorReply(QDBusError::AccessDenied, QStringLiteral("Only the portal may close this request."));
        return;
    }
    Q_EMIT cancelled();
}

uint FileChooserBackend::OpenFile(const QDBusObjectPath &handle,
                                  const QString &,
                                  const QString &parentWindow,
                                  const QString &title,
                                  const QVariantMap &options,
                                  QVariantMap &)
{
    return choose(FileChooserDialog::Open, handle, parentWindow, title, options);
}
uint FileChooserBackend::SaveFile(const QDBusObjectPath &handle,
                                  const QString &,
                                  const QString &parentWindow,
                                  const QString &title,
                                  const QVariantMap &options,
                                  QVariantMap &)
{
    return choose(FileChooserDialog::Save, handle, parentWindow, title, options);
}
uint FileChooserBackend::SaveFiles(const QDBusObjectPath &handle,
                                   const QString &,
                                   const QString &parentWindow,
                                   const QString &title,
                                   const QVariantMap &options,
                                   QVariantMap &)
{
    return choose(FileChooserDialog::SaveMultiple, handle, parentWindow, title, options);
}

uint FileChooserBackend::choose(FileChooserDialog::Mode mode,
                                const QDBusObjectPath &handle,
                                const QString &parentWindow,
                                const QString &title,
                                const QVariantMap &options)
{
    if (!calledFromDBus())
        return 2;
    const QDBusReply<QString> owner = connection().interface()->serviceOwner(QStringLiteral("org.freedesktop.portal.Desktop"));
    if (!owner.isValid() || owner.value() != message().service()) {
        sendErrorReply(QDBusError::AccessDenied, QStringLiteral("Only xdg-desktop-portal may call this backend."));
        return 2;
    }
    if (!handle.path().startsWith(QLatin1String("/org/freedesktop/portal/desktop/request/")) || m_requests.contains(handle.path())) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid or duplicate request handle."));
        return 2;
    }
    // Validate filter types before constructing a dialog; unknown entries must not broaden access.
    for (const auto &filter : OrcaPortal::decode<OrcaPortal::Filters>(options.value(QStringLiteral("filters")))) {
        for (const auto &entry : filter.entries) {
            if (entry.type > 1 || entry.value.isEmpty()) {
                sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid file filter."));
                return 2;
            }
        }
    }
    auto request = new PortalRequest(message().service(), this);
    if (!connection().registerObject(handle.path(), request, QDBusConnection::ExportAllSlots)) {
        delete request;
        sendErrorReply(QDBusError::Failed, QStringLiteral("Cannot register request."));
        return 2;
    }
    m_requests.insert(handle.path(), request);
    auto dialog = new FileChooserDialog(mode, title, parentWindow, options);
    const auto call = message();
    auto bus = connection();
    setDelayedReply(true);
    auto watcher = new QDBusServiceWatcher(call.service(), bus, QDBusServiceWatcher::WatchForUnregistration, request);
    connect(request, &PortalRequest::cancelled, dialog, &QDialog::reject);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, this, [this, dialog, request, call, bus, path = handle.path()](int code) mutable {
        if (!m_requests.remove(path))
            return;
        bus.send(
            call.createReply({QVariant::fromValue(uint(code == QDialog::Accepted ? 0 : 1)), code == QDialog::Accepted ? dialog->results() : QVariantMap{}}));
        bus.unregisterObject(path);
        request->deleteLater();
        dialog->deleteLater();
    });
    dialog->show();
    return 0;
}
