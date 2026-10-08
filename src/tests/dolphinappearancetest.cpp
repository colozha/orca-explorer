// SPDX-FileCopyrightText: 2026 Dolphin Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dolphinappearance.h"
#include "dolphin_generalsettings.h"
#include "dolphin_iconsmodesettings.h"
#include "dolphinmainwindow.h"
#include "dolphinplacesmodelsingleton.h"
#include "dolphintabwidget.h"
#include "dolphinurlnavigator.h"
#include "dolphinviewcontainer.h"
#include "panels/places/placespanel.h"
#include "settings/interface/interfacesettingspage.h"
#include "testdir.h"
#include "views/dolphinview.h"
#include <KAbstractFileItemActionPlugin>
#include <KBookmarkManager>
#include <KFileItemListProperties>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QStandardPaths>

#include <KAboutData>
#include <KActionCollection>
#include <KColorSchemeManager>
#include <KConfigGroup>
#include <KIconTheme>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBar>
#include <KXMLGUIFactory>

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDirIterator>
#include <QDockWidget>
#include <QDomDocument>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStyleOptionToolButton>
#include <QStyleOptionViewItem>
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
    void sidebarContentPadding();
    void sidebarNavigation();
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

void DolphinAppearanceTest::sidebarNavigation()
{
    TestDir files;
    files.createDir(QStringLiteral("Sidebar target"));
    const QUrl firstUrl = files.url();
    const QUrl secondUrl = QUrl::fromLocalFile(files.path() + QStringLiteral("/Sidebar target/"));
    DolphinMainWindow window;
    const auto hideWindow = qScopeGuard([&]() {
        window.hide();
    });
    window.openDirectories({firstUrl}, false);
    window.resize(1024, 768);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *places = window.findChild<PlacesPanel *>();
    auto *tabs = window.findChild<DolphinTabWidget *>();
    QVERIFY(places);
    QVERIFY(tabs);
    auto *model = qobject_cast<KFilePlacesModel *>(places->model());
    QVERIFY(model);
    auto indexForUrl = [&](const QUrl &url) {
        for (int row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row, 0);
            if (model->url(index) == url) {
                return index;
            }
        }
        return QModelIndex();
    };
    model->addPlace(QStringLiteral("Sidebar first"), firstUrl);
    model->addPlace(QStringLiteral("Sidebar second"), secondUrl);
    const auto removePlaces = qScopeGuard([&]() {
        model->removePlace(indexForUrl(secondUrl));
        model->removePlace(indexForUrl(firstUrl));
    });
    const QPersistentModelIndex first = indexForUrl(firstUrl);
    const QPersistentModelIndex second = indexForUrl(secondUrl);
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QSignalSpy activated(places, &KFilePlacesView::placeActivated);
    for (const auto &index : {second, first}) {
        places->scrollTo(index);
        QTRY_VERIFY(places->visualRect(index).height() >= (m_gnome ? 38 : 16));
        const QRect rect = places->visualRect(index);
        QTest::mouseClick(places->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(rect.center().x(), rect.bottom() - qMin(18, rect.height() / 2)));
        QTRY_COMPARE(window.activeViewContainer()->url(), model->url(index));
    }
    QCOMPARE(activated.count(), 2);
    const int initialTabs = tabs->count();
    QSignalSpy requested(places, &KFilePlacesView::tabRequested);
    for (auto button : {Qt::MiddleButton, Qt::LeftButton}) {
        places->scrollTo(second);
        const QRect rect = places->visualRect(second);
        QTest::mouseClick(places->viewport(),
                          button,
                          button == Qt::LeftButton ? Qt::ControlModifier : Qt::NoModifier,
                          QPoint(rect.center().x(), rect.bottom() - qMin(18, rect.height() / 2)));
        QTRY_COMPARE(requested.count(), button == Qt::MiddleButton ? 1 : 2);
    }
    QTRY_COMPARE(tabs->count(), initialTabs + 2);
    QCOMPARE(window.activeViewContainer()->url(), firstUrl);
    places->setFocus();
    places->setCurrentIndex(first);
    QTest::keyClick(places, Qt::Key_Down);
    QTRY_COMPARE(places->currentIndex(), QModelIndex(second));
    QTest::keyClick(places, Qt::Key_Return);
    QTRY_COMPARE(window.activeViewContainer()->url(), secondUrl);
    const int activations = activated.count();
    // A section heading must not navigate or crash.
    const auto heading = model->index(0, 0);
    places->scrollTo(heading);
    QTRY_VERIFY(places->visualRect(heading).height() > places->visualRect(second).height());
    const QRect headerRect = places->visualRect(heading);
    QTest::mouseClick(places->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(headerRect.center().x(), headerRect.top() + 2));
    QCOMPARE(activated.count(), activations);
    QCOMPARE(window.activeViewContainer()->url(), secondUrl);
    if (m_gnome) {
        QVERIFY(!places->itemDelegate()->property("gnomePlacesContentPadding").isValid());
        QCOMPARE(places->itemDelegateForIndex(first)->property("gnomePlacesContentPadding").toInt(), 8);
    }
}

void DolphinAppearanceTest::sidebarContentPadding()
{
    if (!m_gnome) {
        PlacesPanel places(nullptr);
        QVERIFY(!places.itemDelegate()->property("gnomePlacesContentPadding").isValid());
        return;
    }
    class IconPlacesModel : public DolphinPlacesModel
    {
    public:
        QVariant data(const QModelIndex &index, int role) const override
        {
            if (role == Qt::DecorationRole) {
                QPixmap pixmap(16, 16);
                pixmap.fill(Qt::green);
                QIcon icon(pixmap);
                icon.addPixmap(pixmap, QIcon::Selected);
                return icon;
            }
            if (role == Qt::DisplayRole) {
                return QStringLiteral("A very long location name that needs to be elided");
            }
            return DolphinPlacesModel::data(index, role);
        }
    } model;
    PlacesPanel places(nullptr);
    places.setModel(&model);
    places.setIconSize(QSize(16, 16));
    QModelIndex index;
    for (int row = 1; row < model.rowCount(); ++row) {
        const auto candidate = model.index(row, 0);
        if (!model.isHidden(candidate) && !model.isDevice(candidate)
            && candidate.data(KFilePlacesModel::GroupRole) == model.index(row - 1, 0).data(KFilePlacesModel::GroupRole)) {
            index = candidate;
            break;
        }
    }
    QVERIFY(index.isValid());
    const auto manager = KColorSchemeManager::instance();
    const QString original = manager->activeSchemeId();
    const auto restore = qScopeGuard([&]() {
        manager->activateSchemeId(original);
    });
    for (const auto &scheme : {QStringLiteral("GnomeLight"), QStringLiteral("GnomeDark")}) {
        manager->activateSchemeId(scheme);
        QTRY_COMPARE(QIcon::themeName(),
                     scheme.endsWith(QLatin1String("Dark")) ? QStringLiteral("orca-explorer-gnome-dark") : QStringLiteral("orca-explorer-gnome-light"));
        for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            places.setLayoutDirection(direction);
            for (auto state : {QStyle::State_Selected, QStyle::State_MouseOver}) {
                QStyleOptionViewItem option;
                option.initFrom(&places);
                option.widget = &places;
                option.rect = QRect(0, 0, 240, 38);
                option.state = QStyle::State_Enabled | QStyle::State_Active | state;
                const QColor background = places.palette().color(QPalette::Window);
                QImage rendered(240, 38, QImage::Format_ARGB32_Premultiplied);
                rendered.fill(background);
                QImage expected = rendered;
                QPainter expectedPainter(&expected);
                places.style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, &expectedPainter, &places);
                expectedPainter.end();
                QPainter painter(&rendered);
                places.itemDelegateForIndex(index)->paint(&painter, option, index);
                painter.end();
                // Padding changes the contents, not the highlight's outside edges.
                for (int x : {0, 2, 8, 231, 237, 239}) {
                    QCOMPARE(rendered.pixelColor(x, 19), expected.pixelColor(x, 19));
                }
                int firstIcon = -1;
                int lastIcon = -1;
                for (int x = 0; x < rendered.width(); ++x) {
                    if (rendered.pixelColor(x, 19) == QColor(Qt::green)) {
                        if (firstIcon < 0) {
                            firstIcon = x;
                        }
                        lastIcon = x;
                    }
                }
                QCOMPARE(lastIcon - firstIcon + 1, 16);
                const int gap = direction == Qt::LeftToRight ? firstIcon - 2 : 237 - lastIcon;
                QVERIFY(gap >= 10 && gap <= 11);
                QCOMPARE(places.itemDelegateForIndex(index)->sizeHint(option, index).height(), 38);
            }
        }
    }
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
        auto *back = qobject_cast<QToolButton *>(toolbar->widgetForAction(window.actionCollection()->action(QStringLiteral("go_back"))));
        auto *forward = qobject_cast<QToolButton *>(toolbar->widgetForAction(window.actionCollection()->action(QStringLiteral("go_forward"))));
        QVERIFY(back);
        QVERIFY(forward);
        QTRY_VERIFY(back->isVisible());
        QTRY_VERIFY(back->geometry().left() >= header->geometry().right());
        QVERIFY(forward->geometry().left() > back->geometry().right());
        QCOMPARE(places->spacing(), 0);
        QCOMPARE(places->viewport()->geometry().left(), places->contentsRect().left() + 12);
        QCOMPARE(places->viewport()->geometry().top(), places->contentsRect().top() + 8);
        for (int row = 0; row < places->model()->rowCount(); ++row) {
            if (!places->isRowHidden(row)) {
                QVERIFY(places->visualRect(places->model()->index(row, 0)).height() >= 38);
            }
        }
        // Saved or customized toolbar sizes must not enlarge header icons.
        toolbar->setIconSize(QSize(32, 32));
        QToolButton *previousButton = nullptr;
        for (const auto *name : {"go_back", "go_forward", "toggle_search", "view_settings"}) {
            auto *button = qobject_cast<QToolButton *>(toolbar->widgetForAction(window.actionCollection()->action(QString::fromLatin1(name))));
            QVERIFY(button);
            QTRY_COMPARE(button->iconSize(), QSize(14, 14));
            QVERIFY(button->width() >= 32);
            QVERIFY(button->height() >= 32);
            // Qt can supply a larger parent-toolbar size to the paint option.
            QImage rendered(32, 32, QImage::Format_ARGB32_Premultiplied);
            rendered.fill(Qt::transparent);
            QStyleOptionToolButton option;
            option.initFrom(button);
            option.rect = rendered.rect();
            option.state = QStyle::State_Enabled | QStyle::State_AutoRaise;
            option.subControls = QStyle::SC_ToolButton;
            option.icon = button->icon();
            option.iconSize = QSize(32, 32);
            QPainter painter(&rendered);
            button->style()->drawComplexControl(QStyle::CC_ToolButton, &option, &painter, button);
            painter.end();
            QRect painted;
            for (int y = 0; y < rendered.height(); ++y) {
                for (int x = 0; x < rendered.width(); ++x) {
                    if (rendered.pixelColor(x, y).alpha()) {
                        painted = painted.united(QRect(x, y, 1, 1));
                    }
                }
            }
            QVERIFY(!painted.isEmpty());
            QVERIFY(painted.width() <= 14);
            QVERIFY(painted.height() <= 14);
            if (previousButton) {
                QTRY_VERIFY(button->geometry().left() > previousButton->geometry().right());
            }
            previousButton = button;
        }
        toolbar->setIconSize(QSize(16, 16));
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
        for (const auto *name : {"go_back", "go_forward", "toggle_search", "view_settings"}) {
            auto *button = qobject_cast<QToolButton *>(toolbar->widgetForAction(window.actionCollection()->action(QString::fromLatin1(name))));
            QVERIFY(button);
            QTRY_COMPARE(button->iconSize(), QSize(14, 14));
            QVERIFY(button->width() >= 32);
            QVERIFY(button->height() >= 32);
        }
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
        manager->activateSchemeId(scheme);
        QTRY_COMPARE(QApplication::palette().color(QPalette::Window).lightness() < 128, scheme.endsWith(QLatin1String("Dark")));
        QTRY_COMPARE(QIcon::themeName(),
                     scheme.endsWith(QLatin1String("Dark")) ? QStringLiteral("orca-explorer-gnome-dark") : QStringLiteral("orca-explorer-gnome-light"));
        for (const auto &size : {QSize(1920, 1033), QSize(1024, 768)}) {
            window.resize(size);
            QTRY_COMPARE(window.size(), size);
            // Capture after asynchronous theme changes and Places animations settle.
            QImage previous;
            QElapsedTimer unchanged;
            unchanged.start();
            QVERIFY(QTest::qWaitFor([&window, &previous, &unchanged]() {
                const QImage current = window.grab().toImage();
                if (current != previous) {
                    previous = current;
                    unchanged.restart();
                }
                return unchanged.elapsed() >= 200;
            }));
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
