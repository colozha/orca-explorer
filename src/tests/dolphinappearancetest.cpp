// SPDX-FileCopyrightText: 2026 Dolphin Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dolphinappearance.h"
#include "dolphin_generalsettings.h"
#include "dolphin_iconsmodesettings.h"
#include "dolphinmainwindow.h"
#include "dolphinplacesmodelsingleton.h"
#include <KBookmarkManager>
#include <KAbstractFileItemActionPlugin>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KFileItemListProperties>
#include <QStandardPaths>
#include "dolphinurlnavigator.h"
#include "dolphinviewcontainer.h"
#include "panels/places/placespanel.h"
#include "settings/interface/interfacesettingspage.h"
#include "testdir.h"
#include "views/dolphinview.h"

#include <KAboutData>
#include <KActionCollection>
#include <KColorSchemeManager>
#include <KConfigGroup>
#include <KIconTheme>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBar>
#include <KXMLGUIFactory>

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDirIterator>
#include <QDockWidget>
#include <QDomDocument>
#include <QImage>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

class DolphinAppearanceTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void settingsPersistAndReset();
    void startupDefaults();
    void forkStorageIsolation();
    void privatePlugins();
    void iconsAndPalette();
    void mainWindowActions();
    void menuActionsRetained();
    void savedLayoutRetained();
    void visualCapture();

private:
    bool m_gnome = false;
    QString m_originalStyle;
};

void DolphinAppearanceTest::initTestCase()
{
    QCOMPARE(GeneralSettings::appearanceMode(), GeneralSettings::EnumAppearanceMode::GnomeFiles);
    m_originalStyle = qApp->style()->objectName();
    m_gnome = qEnvironmentVariable("DOLPHIN_TEST_APPEARANCE") == QLatin1String("gnome");
    GeneralSettings::setAppearanceMode(m_gnome ? GeneralSettings::EnumAppearanceMode::GnomeFiles : GeneralSettings::EnumAppearanceMode::Dolphin);
    GeneralSettings::setVersion(202);
    GeneralSettings::self()->save();
    // A saved value must take precedence over the appearance default.
    KConfigGroup(GeneralSettings::self()->config(), QStringLiteral("IconsMode")).writeEntry("PreviewSize", 128);
    GeneralSettings::self()->config()->sync();
    DolphinAppearance::initialize();
    QCOMPARE(DolphinAppearance::isEnabled(), m_gnome);
}

void DolphinAppearanceTest::settingsPersistAndReset()
{
    InterfaceSettingsPage page(nullptr);
    auto *combo = page.findChild<QComboBox *>(QStringLiteral("appearanceMode"));
    QVERIFY(combo);
    QCOMPARE(combo->currentData().toInt(), m_gnome ? GeneralSettings::EnumAppearanceMode::GnomeFiles : GeneralSettings::EnumAppearanceMode::Dolphin);
    QSignalSpy changed(&page, &InterfaceSettingsPage::changed);
    combo->setCurrentIndex(m_gnome ? 0 : 1);
    QVERIFY(!changed.isEmpty());
    page.applySettings();
    GeneralSettings::self()->load();
    QCOMPARE(GeneralSettings::appearanceMode(), m_gnome ? GeneralSettings::EnumAppearanceMode::Dolphin : GeneralSettings::EnumAppearanceMode::GnomeFiles);
    QCOMPARE(DolphinAppearance::isEnabled(), m_gnome); // Apply does not restyle a running window.
    page.restoreDefaults();
    page.applySettings();
    QCOMPARE(GeneralSettings::appearanceMode(), GeneralSettings::EnumAppearanceMode::GnomeFiles);
    GeneralSettings::setAppearanceMode(m_gnome ? GeneralSettings::EnumAppearanceMode::GnomeFiles : GeneralSettings::EnumAppearanceMode::Dolphin);
    GeneralSettings::self()->save();
}

void DolphinAppearanceTest::forkStorageIsolation()
{
    QCOMPARE(QCoreApplication::applicationName(), QStringLiteral("orca-explorer"));
    QVERIFY(GeneralSettings::self()->config()->name().endsWith(QStringLiteral("orca-explorerrc")));
    QVERIFY(KSharedConfig::openStateConfig()->name().endsWith(QStringLiteral("orca-explorerstaterc")));
    const QByteArray originalData = qgetenv("XDG_DATA_HOME");
    const QString sharedPlaces = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/user-places.xbel");
    QFile sentinel(sharedPlaces);
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    const QByteArray originalContents("Dolphin Places sentinel: must remain untouched");
    sentinel.write(originalContents);
    sentinel.close();
    auto *model = DolphinPlacesModelSingleton::instance().placesModel();
    QCOMPARE(qgetenv("XDG_DATA_HOME"), originalData);
    auto *manager = model->findChild<KBookmarkManager *>();
    QVERIFY(manager);
    QCOMPARE(manager->path(), QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/user-places.xbel"));
    model->addPlace(QStringLiteral("Orca test place"), QUrl::fromLocalFile(QDir::tempPath()));
    QVERIFY(sentinel.open(QIODevice::ReadOnly));
    QCOMPARE(sentinel.readAll(), originalContents);
    QVERIFY(model->rowCount() > 0);
}

void DolphinAppearanceTest::privatePlugins()
{
    TestDir files;
    files.createFile(QStringLiteral("example.txt"));
    const KFileItemListProperties fileProps(KFileItemList{KFileItem(QUrl::fromLocalFile(files.url().toLocalFile() + QStringLiteral("/example.txt")))});
    const KFileItemListProperties folderProps(KFileItemList{KFileItem(files.url())});
    const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("orca-explorer/kfileitemaction"));
#ifdef Q_OS_WIN
    QCOMPARE(plugins.size(), 2);
#else
    QCOMPARE(plugins.size(), 3);
#endif
    QWidget parent;
    KConfigGroup showGroup(KSharedConfig::openConfig(QStringLiteral("orca-explorer-servicemenurc")), QStringLiteral("Show"));
    showGroup.writeEntry("hidefileitemaction", true);
    showGroup.sync();
    for (const auto &metadata : plugins) {
        const auto result = KPluginFactory::instantiatePlugin<KAbstractFileItemActionPlugin>(metadata, &parent);
        QVERIFY2(result.plugin, qPrintable(result.errorString));
        if (metadata.pluginId() == QLatin1String("setfoldericonitemaction")) {
            QVERIFY(!metadata.supportsMimeType(QStringLiteral("text/plain")));
            QVERIFY(!result.plugin->actions(folderProps, &parent).isEmpty());
        } else {
            QVERIFY(!result.plugin->actions(fileProps, &parent).isEmpty());
        }
    }
}

void DolphinAppearanceTest::startupDefaults()
{
    QCOMPARE(qApp->style()->objectName(), m_gnome ? QStringLiteral("orca-explorer-gnome") : m_originalStyle);
    QCOMPARE(IconsModeSettings::iconSize(), m_gnome ? 96 : 32);
    QCOMPARE(IconsModeSettings::previewSize(), 128);
}

static int foregroundLightness(const QIcon &icon)
{
    const QImage image = icon.pixmap(16, 16).toImage();
    int total = 0;
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() > 200) {
                total += color.lightness();
                ++count;
            }
        }
    }
    return count ? total / count : -1;
}

void DolphinAppearanceTest::iconsAndPalette()
{
    if (!m_gnome) {
        QVERIFY(!QIcon::themeName().startsWith(QLatin1String("orca-explorer-gnome")));
        return;
    }
    for (const auto &name : {QStringLiteral("go-previous"),
                             QStringLiteral("hamburger-menu"),
                             QStringLiteral("folder"),
                             QStringLiteral("application-zip"),
                             QStringLiteral("io.github.colozha.OrcaExplorer"),
                             QStringLiteral("view-list-tree"),
                             QStringLiteral("emblem-symbolic-link")}) {
        QVERIFY2(!QIcon::fromTheme(name).pixmap(48, 48).isNull(), qPrintable(name));
    }
    // Every resource alias must render, including Qt-generated disabled variants.
    QDirIterator icons(QStringLiteral(":/icons/orca-explorer-gnome-light"), {QStringLiteral("*.svg")}, QDir::Files, QDirIterator::Subdirectories);
    while (icons.hasNext()) {
        const QString name = QFileInfo(icons.next()).completeBaseName();
        QVERIFY2(!QIcon::fromTheme(name).pixmap(16, 16, QIcon::Disabled).isNull(), qPrintable(name));
    }
    auto *manager = KColorSchemeManager::instance();
    const QString original = manager->activeSchemeId();
    // Installed application schemes participate in the existing KDE menu.
    QVERIFY(manager->indexForSchemeId(QStringLiteral("GnomeLight")).isValid());
    QVERIFY(manager->indexForSchemeId(QStringLiteral("GnomeDark")).isValid());
    const QIcon control = QIcon::fromTheme(QStringLiteral("go-next"));
    manager->activateSchemeId(QStringLiteral("GnomeDark"));
    QTRY_COMPARE(QIcon::themeName(), QStringLiteral("orca-explorer-gnome-dark"));
    QVERIFY(foregroundLightness(control) > 220);
    if (QIcon::fallbackThemeName() == QLatin1String("breeze-dark")) {
        QVERIFY(foregroundLightness(QIcon::fromTheme(QStringLiteral("view-list-tree"))) > 220);
    }
    const QImage folder = QIcon(QStringLiteral(":/orca-explorer/file-icons/folder.svg")).pixmap(16, 16).toImage();
    QVERIFY(!folder.isNull());
    const QImage archive = QIcon(QStringLiteral(":/orca-explorer/file-icons/package-x-generic.svg")).pixmap(96, 96).toImage();
    QVERIFY(!archive.isNull());
    QCOMPARE(archive.pixelColor(0, 0).alpha(), 0); // Unsupported SVG masks must not paint a black square.
    const QColor folderColor = folder.pixelColor(folder.width() / 2, folder.height() / 2);
    QVERIFY(folderColor.blue() > folderColor.red());
    manager->activateSchemeId(QStringLiteral("GnomeLight"));
    QTRY_COMPARE(QIcon::themeName(), QStringLiteral("orca-explorer-gnome-light"));
    QVERIFY(foregroundLightness(control) < 100);
    QVERIFY(qApp->palette().color(QPalette::Base).lightness() > 220);
    manager->activateSchemeId(original);
}

void DolphinAppearanceTest::mainWindowActions()
{
    TestDir files;
    files.createDir(QStringLiteral("Documents"));
    files.createFile(QStringLiteral("a very long file name for checking labels and keyboard navigation.txt"));
    DolphinMainWindow window;
    const auto hideWindow = qScopeGuard([&window]() {
        window.hide();
    });
    auto *bookmarks = window.findChild<KBookmarkManager *>();
    QVERIFY(bookmarks);
    QCOMPARE(bookmarks->path(), QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/bookmarks.xml"));
    bookmarks->root().addBookmark(QStringLiteral("Orca test bookmark"), files.url(), QStringLiteral("folder"));
    QVERIFY(bookmarks->save());
    window.openDirectories({files.url()}, false);
    window.resize(1024, 768);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY(window.activeViewContainer()->view()->itemsCount() == 2);
    QCOMPARE(window.findChild<QWidget *>(QStringLiteral("gnomeSidebarHeader")) != nullptr, m_gnome);
    auto *places = window.findChild<PlacesPanel *>();
    QVERIFY(places);
    QVERIFY(places->model());
    auto *toolbar = window.toolBar();
    QVERIFY(toolbar);
    for (const auto &name : {QStringLiteral("split_view"),
                             QStringLiteral("toggle_filter"),
                             QStringLiteral("toggle_search"),
                             QStringLiteral("new_tab"),
                             QStringLiteral("show_preview"),
                             QStringLiteral("editable_location"),
                             QStringLiteral("properties")}) {
        QVERIFY2(window.actionCollection()->action(name), qPrintable(name));
    }
    if (m_gnome) {
        auto *header = window.findChild<QWidget *>(QStringLiteral("gnomeSidebarHeader"));
        auto *back = toolbar->widgetForAction(window.actionCollection()->action(QStringLiteral("go_back")));
        auto *forward = toolbar->widgetForAction(window.actionCollection()->action(QStringLiteral("go_forward")));
        QVERIFY(back);
        QVERIFY(forward);
        QTRY_VERIFY(back->isVisible());
        QTRY_VERIFY(back->geometry().left() >= header->geometry().right());
        QVERIFY(forward->geometry().left() > back->geometry().left());
        auto *navigator = window.findChild<DolphinUrlNavigator *>();
        const QImage breadcrumb = navigator->grab().toImage();
        QCOMPARE(breadcrumb.pixelColor(breadcrumb.width() / 2, breadcrumb.height() - 3), navigator->palette().color(QPalette::Button));
    }
    window.actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QTRY_COMPARE(window.viewContainers().size(), 2);
    QCOMPARE(window.findChildren<DolphinUrlNavigator *>().size(), 2);
    window.actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QTRY_COMPARE(window.viewContainers().size(), 1);
    if (m_gnome) {
        window.addToolBar(Qt::BottomToolBarArea, toolbar);
        QTRY_VERIFY(!window.windowFlags().testFlag(Qt::FramelessWindowHint));
        QTRY_VERIFY(toolbar->actions().contains(window.actionCollection()->action(QStringLiteral("hamburger_menu"))));
        window.addToolBar(Qt::TopToolBarArea, toolbar);
        QTRY_VERIFY(window.findChild<QWidget *>(QStringLiteral("gnomeSidebarHeader")));
        QTRY_VERIFY(window.findChild<QWidget *>(QStringLiteral("gnomeSidebarHeader"))->isVisible());
        // KEditToolBar rebuilds the XMLGUI client after customization.
        window.guiFactory()->removeClient(&window);
        window.guiFactory()->addClient(&window);
        QVERIFY(QMetaObject::invokeMethod(&window, "saveNewToolbarConfig", Qt::DirectConnection));
        toolbar = window.toolBar();
        QTRY_VERIFY(window.findChild<QWidget *>(QStringLiteral("gnomeSidebarHeader")));
        QTRY_VERIFY(window.findChild<QWidget *>(QStringLiteral("gnomeSidebarHeader"))->isVisible());
        QTRY_VERIFY(toolbar->widgetForAction(window.actionCollection()->action(QStringLiteral("go_back")))->isVisible());
    }
    // Native header close must still take Dolphin's confirmation path.
    GeneralSettings::setRememberOpenedTabs(false);
    GeneralSettings::setConfirmClosingMultipleTabs(true);
    window.actionCollection()->action(QStringLiteral("new_tab"))->trigger();
    bool sawConfirmation = false;
    QTimer::singleShot(0, &window, [&sawConfirmation]() {
        if (auto *dialog = qobject_cast<QDialog *>(qApp->activeModalWidget())) {
            sawConfirmation = true;
            dialog->reject();
        }
    });
    if (auto *close = window.findChild<QToolButton *>(QStringLiteral("gnomeWindowclose")); close && close->isVisible()) {
        close->click();
    } else {
        QVERIFY(!window.close());
    }
    QVERIFY(sawConfirmation);
    QVERIFY(window.isVisible());
}

void DolphinAppearanceTest::menuActionsRetained()
{
    QStringList names[2];
    int index = 0;
    for (const auto &file : {QStringLiteral(":/kxmlgui5/orca-explorer/dolphinui.rc"), QStringLiteral(":/kxmlgui5/orca-explorer/dolphinguignome.rc")}) {
        QFile xml(file);
        QVERIFY(xml.open(QIODevice::ReadOnly));
        QDomDocument document;
        QVERIFY(document.setContent(&xml));
        const auto actions = document.documentElement().firstChildElement(QStringLiteral("MenuBar")).elementsByTagName(QStringLiteral("Action"));
        for (int i = 0; i < actions.size(); ++i) {
            names[index].append(actions.at(i).toElement().attribute(QStringLiteral("name")));
        }
        ++index;
    }
    QCOMPARE(names[0], names[1]);
}

void DolphinAppearanceTest::savedLayoutRetained()
{
    if (!m_gnome) {
        return;
    }
    const auto config = KSharedConfig::openStateConfig();
    KConfigGroup(config, QStringLiteral("GnomeState")).deleteGroup();
    QMainWindow original;
    auto *originalPlaces = new QDockWidget(&original);
    originalPlaces->setObjectName(QStringLiteral("placesDock"));
    originalPlaces->setWidget(new QWidget(originalPlaces));
    original.addDockWidget(Qt::RightDockWidgetArea, originalPlaces);
    const QByteArray savedLayout = original.saveState().toBase64();
    KConfigGroup nativeState(config, QStringLiteral("State"));
    nativeState.writeEntry("State", savedLayout);
    nativeState.writeEntry("MigrationMarker", true);
    nativeState.sync();

    DolphinMainWindow window;
    const auto hideWindow = qScopeGuard([&window]() {
        window.hide();
    });
    TestDir files;
    window.openDirectories({files.url()}, false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *places = window.findChild<QDockWidget *>(QStringLiteral("placesDock"));
    QVERIFY(places);
    QTRY_COMPARE(window.dockWidgetArea(places), Qt::RightDockWidgetArea);
    QCOMPARE(nativeState.readEntry("State", QByteArray()), savedLayout);
    QVERIFY(KConfigGroup(config, QStringLiteral("GnomeState")).readEntry("MigrationMarker", false));
}

void DolphinAppearanceTest::visualCapture()
{
    const QString output = qEnvironmentVariable("ORCA_CAPTURE_DIR");
    if (output.isEmpty()) {
        QSKIP("Set ORCA_CAPTURE_DIR to capture the actual main window.");
    }
    QVERIFY(QDir().mkpath(output));
    GeneralSettings::setVersion(0);
    IconsModeSettings::setPreviewSize(96);
    IconsModeSettings::setIconSize(96);
    TestDir files;
    for (const auto &folder : {QStringLiteral("Documents"), QStringLiteral("Downloads"), QStringLiteral("Pictures"), QStringLiteral("Music"), QStringLiteral("Projects"), QStringLiteral("Empty folder")}) {
        files.createDir(folder);
    }
    files.createFiles({QStringLiteral("Notes.txt"), QStringLiteral("README.md"), QStringLiteral("a very long file name for checking labels and keyboard navigation.txt"), QStringLiteral(".hidden.txt"), QStringLiteral("Archive.zip")});
    QImage thumbnail(160, 100, QImage::Format_RGB32);
    thumbnail.fill(QColor(QStringLiteral("#72a5cd")));
    QVERIFY(thumbnail.save(files.url().toLocalFile() + QStringLiteral("/Preview.png")));
    QVERIFY(QFile::link(files.url().toLocalFile() + QStringLiteral("/Documents"), files.url().toLocalFile() + QStringLiteral("/Linked documents")));
    DolphinMainWindow window;
    window.openDirectories({files.url()}, false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY(window.activeViewContainer()->view()->itemsCount() >= 11);
    window.activeViewContainer()->view()->setHiddenFilesShown(true);
    const auto manager = KColorSchemeManager::instance();
    for (const auto &scheme : {QStringLiteral("GnomeLight"), QStringLiteral("GnomeDark")}) {
        manager->activateScheme(manager->indexForScheme(scheme));
        QTRY_COMPARE(QApplication::palette().color(QPalette::Window).lightness() < 128, scheme.endsWith(QLatin1String("Dark")));
        for (const auto &size : {QSize(1920, 1033), QSize(1024, 768)}) {
            window.resize(size);
            QTRY_COMPARE(window.size(), size);
            QCoreApplication::processEvents();
            QVERIFY(window.grab().save(output + QStringLiteral("/orca-%1-%2x%3-%4x.png").arg(scheme).arg(size.width()).arg(size.height()).arg(window.devicePixelRatioF())));
        }
    }
    window.actionCollection()->action(QStringLiteral("split_view"))->trigger();
    QTRY_COMPARE(window.viewContainers().size(), 2);
    QVERIFY(window.grab().save(output + QStringLiteral("/orca-split.png")));
    window.hide();
}

int main(int argc, char **argv)
{
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", (config.path() + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_STATE_HOME", (config.path() + QStringLiteral("/state")).toUtf8());
    qputenv("XDG_DATA_HOME", (config.path() + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CACHE_HOME", (config.path() + QStringLiteral("/cache")).toUtf8());
    const QString schemes = config.path() + QStringLiteral("/data/color-schemes");
    QDir().mkpath(schemes);
    for (const auto &name : {QStringLiteral("GnomeLight"), QStringLiteral("GnomeDark")}) {
        QFile::copy(QStringLiteral(":/orca-explorer/colors/") + name + QStringLiteral(".colors"), schemes + QLatin1Char('/') + name + QStringLiteral(".colors"));
    }
    KIconTheme::initTheme();
    QApplication app(argc, argv);
    KAboutData::setApplicationData(KAboutData(QStringLiteral("orca-explorer"), QStringLiteral("Orca Explorer"), QStringLiteral("test")));
    KLocalizedString::setApplicationDomain("orca-explorer");
    DolphinAppearanceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "dolphinappearancetest.moc"
