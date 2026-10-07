// SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dolphinappearance.h"
#include "filechooserdialog.h"
#include "kitemviews/private/kfileitemmodelfilter.h"
#include "views/dolphinview.h"
#include <KColorSchemeManager>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class FileChooserTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_dir;
    QVariantMap options() const
    {
        return {{QStringLiteral("current_folder"), QByteArray(QFile::encodeName(m_dir.path()) + '\0')}};
    }
    void type(FileChooserDialog &dialog, const QString &name)
    {
        auto edit = dialog.findChild<QLineEdit *>(QStringLiteral("fileName"));
        edit->setFocus();
        edit->selectAll();
        QTest::keyClicks(edit, name);
    }
    void create(const QString &name)
    {
        QFile file(m_dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello\n");
    }
private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qApp->setApplicationName(QStringLiteral("orca-explorer"));
        OrcaPortal::registerTypes();
        DolphinAppearance::initialize();
        create(QStringLiteral("hello.txt"));
        create(QStringLiteral("extensionless"));
        create(QString::fromUtf8("berkas 日本語.txt"));
        QVERIFY(QDir(m_dir.path()).mkdir(QStringLiteral("subfolder")));
    }
    void fileNames()
    {
        QVERIFY(FileChooserDialog::validFileName(QString::fromUtf8("日本語 file.txt")));
        for (const auto &name :
             {QString(), QStringLiteral("."), QStringLiteral(".."), QStringLiteral("../escape"), QStringLiteral("a/b"), QString(QChar::Null)})
            QVERIFY(!FileChooserDialog::validFileName(name));
        QCOMPARE(FileChooserDialog::decodePath(QByteArray("/tmp/a\0", 7)), QStringLiteral("/tmp/a"));
        QVERIFY(FileChooserDialog::decodePath(QByteArray("a\0b", 3)).isEmpty());
    }
    void filtersKeepDirectories()
    {
        KFileItemModelFilter filter;
        filter.setFileChooserFilters({{0, QStringLiteral("*.png")}, {1, QStringLiteral("text/plain")}});
        QVERIFY(filter.matches(KFileItem(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("subfolder"))))));
        QVERIFY(filter.matches(KFileItem(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("extensionless"))))));
        QVERIFY(filter.matches(KFileItem(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("hello.txt"))))));
        filter.setFileChooserFilters({{0, QStringLiteral("*.png")}});
        QVERIFY(!filter.matches(KFileItem(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("hello.txt"))))));
    }
    void openTypedFile()
    {
        FileChooserDialog dialog(FileChooserDialog::Open, {}, {}, options());
        type(dialog, QStringLiteral("hello.txt"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.results().value(QStringLiteral("uris")).toStringList(),
                 QStringList{QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("hello.txt"))).toString(QUrl::FullyEncoded)});
    }
    void unicodeSelection()
    {
        FileChooserDialog dialog(FileChooserDialog::Open, {}, {}, options());
        auto edit = dialog.findChild<QLineEdit *>(QStringLiteral("fileName"));
        edit->setText(QString::fromUtf8("berkas 日本語.txt"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(QUrl(dialog.results().value(QStringLiteral("uris")).toStringList().first()).toLocalFile(),
                 m_dir.filePath(QString::fromUtf8("berkas 日本語.txt")));
    }
    void invalidAndCancelled()
    {
        FileChooserDialog dialog(FileChooserDialog::Open, {}, {}, options());
        type(dialog, QStringLiteral("missing"));
        dialog.accept();
        QVERIFY(dialog.result() != QDialog::Accepted);
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("errorMessage"))->text().isEmpty());
        dialog.reject();
        QVERIFY(dialog.results().value(QStringLiteral("uris")).toStringList().isEmpty());
    }
    void folderSelection()
    {
        auto opts = options();
        opts.insert(QStringLiteral("directory"), true);
        FileChooserDialog dialog(FileChooserDialog::Open, {}, {}, opts);
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(QUrl(dialog.results().value(QStringLiteral("uris")).toStringList().first()).toLocalFile(), m_dir.path());
    }
    void saveDoesNotWrite()
    {
        auto opts = options();
        opts.insert(QStringLiteral("current_name"), QStringLiteral("new file.txt"));
        FileChooserDialog dialog(FileChooserDialog::Save, {}, {}, opts);
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(!QFile::exists(m_dir.filePath(QStringLiteral("new file.txt"))));
    }
    void restrictedPermissions()
    {
        const QString filePath = m_dir.filePath(QStringLiteral("unreadable.txt"));
        create(QStringLiteral("unreadable.txt"));
        QVERIFY(QFile::setPermissions(filePath, {}));
        FileChooserDialog open(FileChooserDialog::Open, {}, {}, options());
        type(open, QStringLiteral("unreadable.txt"));
        open.accept();
        QVERIFY(open.result() != QDialog::Accepted);
        QVERIFY(QFile::setPermissions(filePath, QFile::ReadOwner | QFile::WriteOwner));
        const QString folder = m_dir.filePath(QStringLiteral("restricted"));
        QVERIFY(QDir().mkdir(folder));
        QVERIFY(QFile::setPermissions(folder, QFile::ReadOwner | QFile::ExeOwner));
        auto opts = options();
        opts.insert(QStringLiteral("current_folder"), QByteArray(QFile::encodeName(folder) + '\0'));
        opts.insert(QStringLiteral("current_name"), QStringLiteral("new.txt"));
        FileChooserDialog save(FileChooserDialog::Save, {}, {}, opts);
        save.accept();
        QVERIFY(save.result() != QDialog::Accepted);
        QVERIFY(QFile::setPermissions(folder, QFile::ReadOwner | QFile::WriteOwner));
        FileChooserDialog inaccessible(FileChooserDialog::Save, {}, {}, opts);
        inaccessible.accept();
        QVERIFY(inaccessible.result() != QDialog::Accepted);
        QVERIFY(QFile::setPermissions(folder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
    void saveNameSurvivesNavigation()
    {
        auto opts = options();
        opts.insert(QStringLiteral("current_name"), QStringLiteral("report.txt"));
        FileChooserDialog dialog(FileChooserDialog::Save, {}, {}, opts);
        auto view = dialog.findChild<DolphinView *>();
        auto name = dialog.findChild<QLineEdit *>(QStringLiteral("fileName"));
        QTRY_VERIFY(view->itemsCount() >= 3);
        view->selectItems(QRegularExpression(QStringLiteral("^subfolder$")), true);
        QTRY_COMPARE(view->selectedItemsCount(), 1);
        QCOMPARE(name->text(), QStringLiteral("report.txt"));
        view->setUrl(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("subfolder"))));
        QTRY_COMPARE(view->itemsCount(), 0);
        QCOMPARE(name->text(), QStringLiteral("report.txt"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(QUrl(dialog.results().value(QStringLiteral("uris")).toStringList().first()).toLocalFile(),
                 m_dir.filePath(QStringLiteral("subfolder/report.txt")));
    }
    void overwriteDeclinedAndConfirmed()
    {
        auto opts = options();
        opts.insert(QStringLiteral("current_name"), QStringLiteral("hello.txt"));
        FileChooserDialog dialog(FileChooserDialog::Save, {}, {}, opts);
        QTimer::singleShot(0, [] {
            if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(QMessageBox::No)->click();
        });
        dialog.accept();
        QVERIFY(dialog.result() != QDialog::Accepted);
        QTimer::singleShot(0, [] {
            if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Yes)->click();
        });
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QFile file(m_dir.filePath(QStringLiteral("hello.txt")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("hello\n"));
    }
    void saveMultipleAvoidsCollisions()
    {
        auto opts = options();
        opts.insert(QStringLiteral("files"), QVariant::fromValue(QList<QByteArray>{"hello.txt", "hello.txt", "new.txt"}));
        FileChooserDialog dialog(FileChooserDialog::SaveMultiple, {}, {}, opts);
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        const auto uris = dialog.results().value(QStringLiteral("uris")).toStringList();
        QCOMPARE(uris.size(), 3);
        QVERIFY(uris[0] != uris[1]);
        QVERIFY(!QFile::exists(QUrl(uris[0]).toLocalFile()));
        opts.insert(QStringLiteral("files"), QVariant::fromValue(QList<QByteArray>{"../escape"}));
        FileChooserDialog invalid(FileChooserDialog::SaveMultiple, {}, {}, opts);
        invalid.accept();
        QVERIFY(invalid.result() != QDialog::Accepted);
    }
    void choicesAndCurrentFilter()
    {
        auto opts = options();
        opts.insert(QStringLiteral("choices"),
                    QVariant::fromValue(OrcaPortal::Choices{{"read-only", "Read only", {}, "true"},
                                                            {"encoding", "Encoding", {{"utf8", "UTF-8"}, {"ascii", "ASCII"}}, "ascii"}}));
        OrcaPortal::Filter filter{"Text", {{0, "*.txt"}, {1, "text/plain"}}};
        opts.insert(QStringLiteral("filters"), QVariant::fromValue(OrcaPortal::Filters{filter}));
        opts.insert(QStringLiteral("current_filter"), QVariant::fromValue(filter));
        FileChooserDialog dialog(FileChooserDialog::Open, {}, {}, opts);
        type(dialog, QStringLiteral("hello.txt"));
        dialog.accept();
        const auto result = dialog.results();
        QCOMPARE(OrcaPortal::decode<OrcaPortal::ChoiceResults>(result.value(QStringLiteral("choices"))).last().label, QStringLiteral("ascii"));
        QCOMPARE(OrcaPortal::decode<OrcaPortal::Filter>(result.value(QStringLiteral("current_filter"))).name, QStringLiteral("Text"));
    }
    void keyboardAndCapture()
    {
        FileChooserDialog dialog(FileChooserDialog::Save, {}, {}, options());
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QTest::keyClick(&dialog, Qt::Key_L, Qt::ControlModifier);
        if (!qEnvironmentVariableIsEmpty("ORCA_PORTAL_CAPTURE")) {
            const QString prefix = qEnvironmentVariable("ORCA_PORTAL_CAPTURE");
            QTRY_VERIFY(dialog.findChild<DolphinView *>()->rootItem().isDir());
            const auto manager = KColorSchemeManager::instance();
            for (const auto &scheme : {QStringLiteral("OrcaExplorerGnomeLight"), QStringLiteral("OrcaExplorerGnomeDark")}) {
                QVERIFY(manager->indexForSchemeId(scheme).isValid());
                manager->activateSchemeId(scheme);
                QTRY_COMPARE(QApplication::palette().color(QPalette::Window).lightness() < 128, scheme.endsWith(QLatin1String("Dark")));
                dialog.resize(860, 580);
                QCoreApplication::processEvents();
                QVERIFY(dialog.grab().save(prefix + QLatin1Char('-') + scheme + QStringLiteral("-wide.png")));
                dialog.resize(520, 400);
                QCoreApplication::processEvents();
                QVERIFY(dialog.grab().save(prefix + QLatin1Char('-') + scheme + QStringLiteral("-compact.png")));
            }
        }
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }
};
QTEST_MAIN(FileChooserTest)
#include "filechoosertest.moc"
