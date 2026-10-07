// SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dolphinappearance.h"
#include "filechooserbackend.h"
#include "views/dolphinview.h"
#include <QApplication>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QLineEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class PortalBackendTest : public QObject
{
    Q_OBJECT
    QDBusConnection m_bus = QDBusConnection::sessionBus();
    QDBusConnection m_front = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("portal-front"));
    QDBusConnection m_intruder = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("intruder"));
    FileChooserBackend m_backend;
    QTemporaryDir m_dir;
    uint m_serial = 0;
    QString nextHandle()
    {
        return QStringLiteral("/org/freedesktop/portal/desktop/request/test/r%1").arg(++m_serial);
    }
    QDBusPendingCall call(const QDBusConnection &connection, const QString &method, const QString &handle, QVariantMap options = {})
    {
        options.insert(QStringLiteral("current_folder"), QByteArray(QFile::encodeName(m_dir.path()) + '\0'));
        auto msg = QDBusMessage::createMethodCall(m_bus.baseService(),
                                                  QStringLiteral("/org/freedesktop/portal/desktop"),
                                                  QStringLiteral("org.freedesktop.impl.portal.FileChooser"),
                                                  method);
        msg.setArguments({QVariant::fromValue(QDBusObjectPath(handle)), QStringLiteral("test.app"), QString(), QStringLiteral("Test picker"), options});
        return connection.asyncCall(msg);
    }
    QList<FileChooserDialog *> dialogs()
    {
        QList<FileChooserDialog *> found;
        for (auto widget : QApplication::topLevelWidgets())
            if (auto dialog = qobject_cast<FileChooserDialog *>(widget); dialog && dialog->isVisible())
                found.append(dialog);
        return found;
    }
private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_bus.isConnected());
        QVERIFY(m_dir.isValid());
        QVERIFY(m_front.registerService(QStringLiteral("org.freedesktop.portal.Desktop")));
        QVERIFY(m_bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"),
                                     &m_backend,
                                     QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties));
        qApp->setApplicationName(QStringLiteral("orca-explorer"));
        OrcaPortal::registerTypes();
        DolphinAppearance::initialize();
        QFile file(m_dir.filePath(QStringLiteral("test.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test");
        QFile second(m_dir.filePath(QStringLiteral("second.txt")));
        QVERIFY(second.open(QIODevice::WriteOnly));
        second.write("second");
    }
    void cleanup()
    {
        for (auto dialog : dialogs())
            dialog->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void deniesNonPortalCaller()
    {
        QDBusPendingCallWatcher watcher(call(m_intruder, QStringLiteral("OpenFile"), nextHandle()));
        QSignalSpy done(&watcher, &QDBusPendingCallWatcher::finished);
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(watcher.reply().errorName(), QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));
        QVERIFY(dialogs().isEmpty());
    }
    void acceptedReplyWithTypedFilters()
    {
        QVariantMap opts;
        opts.insert(QStringLiteral("filters"), QVariant::fromValue(OrcaPortal::Filters{{"Text", {{0, "*.txt"}, {1, "text/plain"}}}}));
        opts.insert(QStringLiteral("choices"), QVariant::fromValue(OrcaPortal::Choices{{"encoding", "Encoding", {{"utf8", "UTF-8"}}, "utf8"}}));
        QDBusPendingCallWatcher watcher(call(m_front, QStringLiteral("OpenFile"), nextHandle(), opts));
        QSignalSpy done(&watcher, &QDBusPendingCallWatcher::finished);
        QTRY_COMPARE(dialogs().size(), 1);
        auto dialog = dialogs().first();
        dialog->findChild<QLineEdit *>(QStringLiteral("fileName"))->setText(QStringLiteral("test.txt"));
        dialog->accept();
        QTRY_COMPARE(done.count(), 1);
        QDBusPendingReply<uint, QVariantMap> reply = watcher;
        QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
        QCOMPARE(reply.argumentAt<0>(), 0u);
        const auto result = reply.argumentAt<1>();
        QCOMPARE(result.value(QStringLiteral("uris")).toStringList().size(), 1);
        QCOMPARE(OrcaPortal::decode<OrcaPortal::Filter>(result.value(QStringLiteral("current_filter"))).name, QStringLiteral("Text"));
        QCOMPARE(OrcaPortal::decode<OrcaPortal::ChoiceResults>(result.value(QStringLiteral("choices"))).first().label, QStringLiteral("utf8"));
    }
    void multipleSelection()
    {
        QDBusPendingCallWatcher watcher(call(m_front, QStringLiteral("OpenFile"), nextHandle(), {{QStringLiteral("multiple"), true}}));
        QSignalSpy done(&watcher, &QDBusPendingCallWatcher::finished);
        QTRY_COMPARE(dialogs().size(), 1);
        auto dialog = dialogs().first();
        auto view = dialog->findChild<DolphinView *>();
        QTRY_COMPARE(view->itemsCount(), 2);
        view->selectItems(QRegularExpression(QStringLiteral(".*\\.txt$")), true);
        QTRY_COMPARE(view->selectedItemsCount(), 2);
        dialog->accept();
        QTRY_COMPARE(done.count(), 1);
        QDBusPendingReply<uint, QVariantMap> reply = watcher;
        QCOMPARE(reply.argumentAt<0>(), 0u);
        QCOMPARE(reply.argumentAt<1>().value(QStringLiteral("uris")).toStringList().size(), 2);
    }
    void cancellationAndConcurrentRequests()
    {
        const QString first = nextHandle(), second = nextHandle();
        QDBusPendingCallWatcher a(call(m_front, QStringLiteral("OpenFile"), first));
        QDBusPendingCallWatcher b(call(m_front, QStringLiteral("SaveFile"), second));
        QSignalSpy aDone(&a, &QDBusPendingCallWatcher::finished), bDone(&b, &QDBusPendingCallWatcher::finished);
        QTRY_COMPARE(dialogs().size(), 2);
        auto close = QDBusMessage::createMethodCall(m_bus.baseService(), first, QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close"));
        QDBusPendingCallWatcher denied(m_intruder.asyncCall(close));
        QSignalSpy deniedDone(&denied, &QDBusPendingCallWatcher::finished);
        QTRY_COMPARE(deniedDone.count(), 1);
        QCOMPARE(denied.reply().errorName(), QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"));
        QDBusPendingCallWatcher closed(m_front.asyncCall(close));
        QTRY_COMPARE(aDone.count(), 1);
        QCOMPARE(bDone.count(), 0);
        QDBusPendingReply<uint, QVariantMap> reply = a;
        QCOMPARE(reply.argumentAt<0>(), 1u);
        QVERIFY(reply.argumentAt<1>().isEmpty());
        QTRY_COMPARE(dialogs().size(), 1);
        dialogs().first()->reject();
        QTRY_COMPARE(bDone.count(), 1);
    }
    void invalidRequestAndFilter()
    {
        QDBusPendingCallWatcher watcher(call(m_front, QStringLiteral("OpenFile"), QStringLiteral("/invalid")));
        QSignalSpy done(&watcher, &QDBusPendingCallWatcher::finished);
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(watcher.reply().errorName(), QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
        QVariantMap opts{{QStringLiteral("filters"), QVariant::fromValue(OrcaPortal::Filters{{"Invalid", {{8, "bad"}}}})}};
        QDBusPendingCallWatcher invalid(call(m_front, QStringLiteral("OpenFile"), nextHandle(), opts));
        QSignalSpy invalidDone(&invalid, &QDBusPendingCallWatcher::finished);
        QTRY_COMPARE(invalidDone.count(), 1);
        QCOMPARE(invalid.reply().errorName(), QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"));
    }
    void backendStaysAliveWithoutDialogs()
    {
        QProcess process;
        process.start(QString::fromUtf8(ORCA_TEST_PORTAL_EXECUTABLE), {});
        QVERIFY(process.waitForStarted());
        QTRY_VERIFY(m_bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.impl.portal.desktop.orca")).value());
        QSignalSpy finished(&process, &QProcess::finished);
        QVERIFY(!finished.wait(1500));
        QCOMPARE(process.state(), QProcess::Running);
        process.terminate();
        QVERIFY(process.waitForFinished(10000));
    }
    void fileManagerFlagAndConflict()
    {
        const QString executable = QString::fromUtf8(ORCA_TEST_EXECUTABLE);
        QProcess invalid;
        invalid.start(executable, {QStringLiteral("--file-manager-service")});
        QVERIFY(invalid.waitForFinished(10000));
        QVERIFY(invalid.exitCode() != 0);
        QVERIFY(invalid.readAllStandardError().contains("requires --daemon"));
        QProcess ordinary;
        ordinary.start(executable, {QStringLiteral("--daemon")});
        QVERIFY(ordinary.waitForStarted());
        QTRY_VERIFY(m_bus.interface()->isServiceRegistered(QStringLiteral("io.github.colozha.OrcaExplorer")).value());
        QVERIFY(!m_bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.FileManager1")).value());
        QVERIFY(m_front.registerService(QStringLiteral("org.freedesktop.FileManager1")));
        QProcess conflict;
        conflict.start(executable, {QStringLiteral("--daemon"), QStringLiteral("--file-manager-service")});
        QVERIFY(conflict.waitForFinished(10000));
        QVERIFY(conflict.exitCode() != 0);
        QVERIFY(conflict.readAllStandardError().contains("Cannot claim"));
        QCOMPARE(m_bus.interface()->serviceOwner(QStringLiteral("org.freedesktop.FileManager1")).value(), m_front.baseService());
        m_front.unregisterService(QStringLiteral("org.freedesktop.FileManager1"));
        QProcess daemon;
        daemon.start(executable, {QStringLiteral("--daemon"), QStringLiteral("--file-manager-service")});
        QVERIFY(daemon.waitForStarted());
        QTRY_VERIFY(m_bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.FileManager1")).value());
        auto msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                                  QStringLiteral("/org/freedesktop/FileManager1"),
                                                  QStringLiteral("org.freedesktop.FileManager1"),
                                                  QStringLiteral("ShowFolders"));
        msg.setArguments({QStringList{QUrl::fromLocalFile(m_dir.path()).toString()}, QString()});
        QDBusPendingCallWatcher dispatch(m_front.asyncCall(msg));
        QSignalSpy done(&dispatch, &QDBusPendingCallWatcher::finished);
        QTRY_COMPARE(done.count(), 1);
        QVERIFY2(dispatch.reply().type() != QDBusMessage::ErrorMessage, qPrintable(dispatch.reply().errorMessage()));
        QCOMPARE(ordinary.state(), QProcess::Running);
        QVERIFY(m_bus.interface()->isServiceRegistered(QStringLiteral("io.github.colozha.OrcaExplorerFileManager")).value());
        daemon.terminate();
        QVERIFY(daemon.waitForFinished(10000));
        ordinary.terminate();
        QVERIFY(ordinary.waitForFinished(10000));
    }
};
QTEST_MAIN(PortalBackendTest)
#include "portalbackendtest.moc"
