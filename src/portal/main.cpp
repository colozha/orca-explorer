// SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dolphinappearance.h"
#include "dolphinplacesmodelsingleton.h"
#include "filechooserbackend.h"
#include <KIconTheme>
#include <KLocalizedString>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <iostream>

int main(int argc, char **argv)
{
    // A portal backend must not call the frontend portal during platform startup.
    qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("orca-explorer"));
    app.setOrganizationDomain(QStringLiteral("colozha.github.io"));
    app.setDesktopFileName(QStringLiteral("io.github.colozha.OrcaExplorer"));
    app.setQuitOnLastWindowClosed(false);
    // KIO jobs can release the final quit lock even while no dialog is open.
    QCoreApplication::setQuitLockEnabled(false);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.colozha.OrcaExplorer")));
    app.addLibraryPath(QStringLiteral(ORCA_INSTALL_PLUGINDIR));
    KLocalizedString::setApplicationDomain("orca-explorer");
    OrcaPortal::registerTypes();
    auto bus = QDBusConnection::sessionBus();
    FileChooserBackend backend;
    if (!bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), &backend, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties)
        || !bus.registerService(QStringLiteral("org.freedesktop.impl.portal.desktop.orca"))) {
        std::cerr << qPrintable(bus.lastError().message()) << '\n';
        return EXIT_FAILURE;
    }
    DolphinAppearance::initialize();
    // Initialize isolated Places storage before any view starts background workers.
    DolphinPlacesModelSingleton::instance();
    return app.exec();
}
