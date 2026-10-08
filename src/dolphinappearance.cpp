// SPDX-FileCopyrightText: 2026 Dolphin Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dolphinappearance.h"

#include "dolphin_generalsettings.h"

#include <KColorScheme>
#include <KColorSchemeManager>
#include <KConfigGroup>
#include <KIconTheme>
#include <KSharedConfig>

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QIcon>
#include <QPainter>
#include <QPixmapCache>
#include <QProxyStyle>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QStyleHints>
#include <QStyleOptionButton>
#include <QStyleOptionMenuItem>
#include <QStyleOptionToolButton>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

static void initAppearanceResources()
{
    Q_INIT_RESOURCE(dolphinappearance);
}

namespace
{
constexpr auto appearanceProperty = "dolphinGnomeAppearance";

class GnomeStyle final : public QProxyStyle
{
public:
    GnomeStyle()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {
        setObjectName(QStringLiteral("orca-explorer-gnome"));
    }

    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override
    {
        switch (metric) {
        case PM_ToolBarIconSize:
        case PM_SmallIconSize:
        case PM_ButtonIconSize:
            return 16;
        case PM_ToolBarFrameWidth:
            return 0;
        case PM_ToolBarItemSpacing:
            return 4;
        case PM_DockWidgetSeparatorExtent:
            return 1;
        case PM_ScrollBarExtent:
            return 12;
        default:
            return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const override
    {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == CT_ToolButton) {
            result = result.expandedTo(QSize(32, 32));
        } else if (type == CT_ItemViewItem && widget && widget->inherits("KFilePlacesView")) {
            result.setHeight(qMax(38, result.height()));
        }
        return result;
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override
    {
        if (element == PE_PanelItemViewItem || element == PE_PanelButtonTool || element == PE_FrameFocusRect) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            const QRectF rect = QRectF(option->rect).adjusted(2, 2, -2, -2);
            const bool selected = option->state & (State_Selected | State_On | State_Sunken);
            QColor fill = selected ? option->palette.color(QPalette::Highlight) : option->palette.color(QPalette::Text);
            if (!selected) {
                fill.setAlphaF(option->state & State_MouseOver ? 0.08 : 0.0);
            }
            if (element == PE_FrameFocusRect) {
                painter->setPen(QPen(KColorScheme(QPalette::Active, KColorScheme::View).decoration(KColorScheme::FocusColor).color(), 2));
                painter->setBrush(Qt::NoBrush);
            } else {
                painter->setPen(Qt::NoPen);
                painter->setBrush(fill);
            }
            painter->drawRoundedRect(rect, 7, 7);
            painter->restore();
            return;
        }
        if (element == PE_PanelButtonCommand || element == PE_PanelMenu || element == PE_PanelLineEdit || element == PE_FrameLineEdit) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            const bool input = element == PE_PanelLineEdit || element == PE_FrameLineEdit;
            QColor fill = option->palette.color(input ? QPalette::Base : QPalette::Button);
            if (option->state & State_MouseOver && !input) {
                fill = fill.lighter(110);
            }
            if (option->state & State_Sunken) {
                fill = option->palette.color(QPalette::Highlight);
            }
            painter->setBrush(element == PE_FrameLineEdit ? QBrush(Qt::NoBrush) : QBrush(fill));
            painter->setPen(input ? option->palette.color(QPalette::Mid) : QColor(Qt::transparent));
            if (input && option->state & State_HasFocus) {
                painter->setPen(QPen(KColorScheme(QPalette::Active, KColorScheme::View).decoration(KColorScheme::FocusColor).color(), 2));
            }
            painter->drawRoundedRect(QRectF(option->rect).adjusted(1, 1, -1, -1), 8, 8);
            painter->restore();
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override
    {
        if (element == CE_ToolBar) {
            painter->fillRect(option->rect, option->palette.color(QPalette::Base));
            return;
        }
        if (element == CE_PushButtonLabel && widget && widget->property("gnomeAboutCard").toBool()) {
            const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
            if (button) {
                const QRect rect = button->rect.adjusted(14, 0, -14, 0);
                drawItemText(painter,
                             rect.adjusted(0, 0, -28, 0),
                             Qt::AlignVCenter | Qt::AlignLeading,
                             button->palette,
                             button->state & State_Enabled,
                             button->text,
                             QPalette::ButtonText);
                drawItemPixmap(painter,
                               rect,
                               Qt::AlignVCenter | Qt::AlignTrailing,
                               button->icon.pixmap(16, 16, button->state & State_Enabled ? QIcon::Normal : QIcon::Disabled));
                return;
            }
        } else if (element == CE_MenuItem) {
            const auto *menu = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
            if (menu && menu->state & State_Selected) {
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setPen(Qt::NoPen);
                painter->setBrush(menu->palette.color(QPalette::Highlight));
                painter->drawRoundedRect(menu->rect.adjusted(4, 2, -4, -2), 6, 6);
                painter->restore();
                QStyleOptionMenuItem copy(*menu);
                copy.palette.setColor(QPalette::Highlight, Qt::transparent);
                QProxyStyle::drawControl(element, &copy, painter, widget);
                return;
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const override
    {
        const auto *button = qobject_cast<const QToolButton *>(widget);
        const auto *toolOption = qstyleoption_cast<const QStyleOptionToolButton *>(option);
        if (control == CC_ToolButton && button && toolOption && button->parentWidget() && button->parentWidget()->objectName() == QLatin1String("mainToolBar")
            && button->defaultAction()) {
            const QString name = button->defaultAction()->objectName();
            if (name == QLatin1String("go_back") || name == QLatin1String("go_forward") || name == QLatin1String("toggle_search")
                || name == QLatin1String("view_settings")) {
                // Qt paints toolbar buttons using the parent toolbar's icon size.
                QStyleOptionToolButton copy(*toolOption);
                copy.iconSize = QSize(14, 14);
                QProxyStyle::drawComplexControl(control, &copy, painter, widget);
                return;
            }
        }
        QProxyStyle::drawComplexControl(control, option, painter, widget);
    }

    void polish(QWidget *widget) override
    {
        QProxyStyle::polish(widget);
        if (auto *toolbar = qobject_cast<QToolBar *>(widget)) {
            toolbar->setIconSize(QSize(16, 16));
            toolbar->setMinimumHeight(46);
            if (toolbar->objectName() == QLatin1String("mainToolBar")) {
                toolbar->setBackgroundRole(QPalette::Base);
                toolbar->setAutoFillBackground(true);
            }
        }
    }
};

bool systemIsDark()
{
    const int portalScheme = qApp->property("dolphinDesktopColorScheme").toInt();
    if (portalScheme == 1 || portalScheme == 2) {
        return portalScheme == 1;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (qApp->styleHints()->colorScheme() != Qt::ColorScheme::Unknown) {
        return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
#endif
    return qApp->palette().color(QPalette::Window).lightness() < 128;
}

void updatePaletteAndIcons()
{
    auto *manager = KColorSchemeManager::instance();
    if (manager->activeSchemeId().isEmpty()) {
        const QString path = systemIsDark() ? QStringLiteral(":/orca-explorer/colors/GnomeDark.colors") : QStringLiteral(":/orca-explorer/colors/GnomeLight.colors");
        const auto config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
        // This is the same application-local color-scheme path used by KColorSchemeManager.
        // KColorScheme must see it too: Dolphin's graphics views read colors directly from KDE.
        qApp->setProperty("KDE_COLOR_SCHEME_PATH", path);
        const QPalette palette = KColorScheme::createApplicationPalette(config);
        if (qApp->palette() != palette) {
            qApp->setPalette(palette);
        }
    }
    const bool dark = qApp->palette().color(QPalette::Window).lightness() < 128;
    QString fallback = qApp->property("dolphinIconFallbackTheme").toString();
    if (fallback == QLatin1String("breeze") || fallback == QLatin1String("breeze-dark")) {
        // Qt's theme loader does not apply KIconEngine's dynamic SVG colors.
        fallback = dark ? QStringLiteral("breeze-dark") : QStringLiteral("breeze");
    }
    if (!fallback.isEmpty()) {
        QIcon::setFallbackThemeName(fallback);
    }
    const QString theme = dark ? QStringLiteral("orca-explorer-gnome-dark") : QStringLiteral("orca-explorer-gnome-light");
    if (QIcon::themeName() != theme) {
        QIcon::setThemeName(theme);
    }
    QPixmapCache::clear();
}

// Observe the public application palette event and the Settings portal. The portal
// also provides automatic light/dark preference on the minimum supported Qt 6.4.
class AppearanceWatcher final : public QTimer
{
    Q_OBJECT
public:
    AppearanceWatcher()
        : QTimer(qApp)
    {
        setSingleShot(true);
        connect(this, &QTimer::timeout, qApp, updatePaletteAndIcons);
        qApp->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
            start(0);
        });
#endif
        QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.portal.Desktop"),
                                              QStringLiteral("/org/freedesktop/portal/desktop"),
                                              QStringLiteral("org.freedesktop.portal.Settings"),
                                              QStringLiteral("SettingChanged"),
                                              this,
                                              SLOT(settingChanged(QString, QString, QDBusVariant)));
        // Do not synchronously introspect the frontend while a portal backend is starting.
        auto settings = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                       QStringLiteral("/org/freedesktop/portal/desktop"),
                                                       QStringLiteral("org.freedesktop.portal.Settings"),
                                                       QStringLiteral("ReadOne"));
        settings.setArguments({QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme")});
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(settings), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
            const QDBusPendingReply<QDBusVariant> reply = *watcher;
            if (!reply.isError()) {
                settingChanged(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme"), reply.value());
            }
            watcher->deleteLater();
        });
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == qApp && event->type() == QEvent::ApplicationPaletteChange) {
            start(0);
        }
        return QTimer::eventFilter(object, event);
    }

private Q_SLOTS:
    void settingChanged(const QString &group, const QString &key, const QDBusVariant &value)
    {
        if (group == QLatin1String("org.freedesktop.appearance") && key == QLatin1String("color-scheme")) {
            qApp->setProperty("dolphinDesktopColorScheme", value.variant().toUInt());
            start(0);
        }
    }
};
}

bool DolphinAppearance::isEnabled()
{
    return qApp && qApp->property(appearanceProperty).toBool();
}

void DolphinAppearance::initialize()
{
    if (isEnabled() || GeneralSettings::appearanceMode() != GeneralSettings::EnumAppearanceMode::GnomeFiles) {
        return;
    }
    initAppearanceResources();
    qApp->setProperty(appearanceProperty, true);
    qApp->setStyle(new GnomeStyle());

    const QString fallback = QIcon::themeName() == QStringLiteral("KIconEngine") ? KIconTheme::current() : QIcon::themeName();
    qApp->setProperty("dolphinIconFallbackTheme", fallback);
    auto paths = QIcon::themeSearchPaths();
    paths.prepend(QStringLiteral(":/icons"));
    // KIconEngine resolves KDE paths itself. Qt's resource theme loader also needs
    // the standard data paths so private-theme fallback can reach installed icons.
    for (const auto &dataPath : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        const QString path = dataPath + QStringLiteral("/icons");
        if (!paths.contains(path)) {
            paths.append(path);
        }
    }
    QIcon::setThemeSearchPaths(paths);
    updatePaletteAndIcons();

    new AppearanceWatcher();
}

#include "dolphinappearance.moc"
