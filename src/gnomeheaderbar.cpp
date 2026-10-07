// SPDX-FileCopyrightText: 2026 Dolphin Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gnomeheaderbar.h"

#include "dolphinmainwindow.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <KToolBar>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDockWidget>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QWidgetAction>
#include <QWindow>

GnomeHeaderBar::GnomeHeaderBar(DolphinMainWindow *window, KToolBar *toolbar, QDockWidget *places)
    : QWidget(toolbar)
    , m_window(window)
    , m_toolbar(toolbar)
    , m_places(places)
    , m_prefixSpace(new QWidget(toolbar))
    , m_prefixAction(new QWidgetAction(this))
    , m_windowButtons(new QWidget(toolbar))
    , m_leftWindowButtons(new QWidget(toolbar))
    , m_nativeGnomeWayland(QGuiApplication::platformName().startsWith(QLatin1String("wayland"))
                           && qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':')).contains(QStringLiteral("GNOME")))
{
    setObjectName(QStringLiteral("gnomeSidebarHeader"));
    m_toolbar->setMinimumHeight(46);
    m_prefixSpace->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_prefixAction->setDefaultWidget(m_prefixSpace);
    m_prefixAction->setObjectName(QStringLiteral("gnomeHeaderSpace"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->setSpacing(4);
    auto *search = new QToolButton(this);
    search->setAutoRaise(true);
    search->setDefaultAction(window->actionCollection()->action(QStringLiteral("toggle_search")));
    auto *title = new QLabel(QGuiApplication::applicationDisplayName(), this);
    QFont font = title->font();
    font.setBold(true);
    title->setFont(font);
    title->setAlignment(Qt::AlignCenter);
    title->setObjectName(QStringLiteral("gnomeSidebarTitle"));
    auto *menu = new QToolButton(this);
    menu->setAutoRaise(true);
    menu->setDefaultAction(window->actionCollection()->action(QStringLiteral("hamburger_menu")));
    menu->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(search);
    layout->addWidget(title, 1);
    layout->addWidget(menu);

    m_windowButtons->setObjectName(QStringLiteral("gnomeWindowControls"));
    m_leftWindowButtons->setObjectName(QStringLiteral("gnomeLeftWindowControls"));
    for (auto *controls : {m_leftWindowButtons, m_windowButtons}) {
        auto *buttonLayout = new QHBoxLayout(controls);
        buttonLayout->setContentsMargins(4, 0, 4, 0);
        buttonLayout->setSpacing(4);
    }
    setButtonLayout(QStringLiteral(":minimize,maximize,close"));

    qApp->installEventFilter(this);

    if (m_nativeGnomeWayland) {
        QDBusInterface settings(QStringLiteral("org.freedesktop.portal.Desktop"),
                                QStringLiteral("/org/freedesktop/portal/desktop"),
                                QStringLiteral("org.freedesktop.portal.Settings"));
        auto *watcher = new QDBusPendingCallWatcher(
            settings.asyncCall(QStringLiteral("ReadOne"), QStringLiteral("org.gnome.desktop.wm.preferences"), QStringLiteral("button-layout")),
            this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
            const QDBusPendingReply<QDBusVariant> reply = *watcher;
            if (!reply.isError()) {
                setButtonLayout(reply.value().variant().toString());
                updateGeometryAndFrame();
            }
            watcher->deleteLater();
        });
        QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.portal.Desktop"),
                                              QStringLiteral("/org/freedesktop/portal/desktop"),
                                              QStringLiteral("org.freedesktop.portal.Settings"),
                                              QStringLiteral("SettingChanged"),
                                              this,
                                              SLOT(portalSettingChanged(QString, QString, QDBusVariant)));
    }
    updateGeometryAndFrame();
}

void GnomeHeaderBar::setButtonLayout(const QString &layout)
{
    if (layout.count(QLatin1Char(':')) != 1) {
        return;
    }
    const auto sides = layout.split(QLatin1Char(':'));
    m_maximizeButton = nullptr;
    for (int side = 0; side < 2; ++side) {
        auto *controls = side == 0 ? m_leftWindowButtons : m_windowButtons;
        while (auto *item = controls->layout()->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        const auto names = sides.at(side).split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto &name : names) {
            if (name != QLatin1String("minimize") && name != QLatin1String("maximize") && name != QLatin1String("close")) {
                continue;
            }
            auto *button = new QToolButton(controls);
            button->setAutoRaise(true);
            button->setObjectName(QStringLiteral("gnomeWindow") + name);
            button->setIcon(QIcon::fromTheme(QStringLiteral("window-") + name));
            button->setFixedSize(32, 32);
            if (name == QLatin1String("close")) {
                button->setToolTip(i18nc("@action:button", "Close"));
                connect(button, &QToolButton::clicked, m_window, &QWidget::close);
            } else if (name == QLatin1String("minimize")) {
                button->setToolTip(i18nc("@action:button", "Minimize"));
                connect(button, &QToolButton::clicked, m_window, &QWidget::showMinimized);
            } else {
                m_maximizeButton = button;
                connect(button, &QToolButton::clicked, m_window, [this]() {
                    m_window->isMaximized() ? m_window->showNormal() : m_window->showMaximized();
                });
            }
            button->setAccessibleName(button->toolTip());
            controls->layout()->addWidget(button);
        }
    }
    updateWindowButtons();
}

void GnomeHeaderBar::updateWindowButtons()
{
    if (m_maximizeButton) {
        const bool maximized = m_window->isMaximized();
        m_maximizeButton->setIcon(QIcon::fromTheme(maximized ? QStringLiteral("window-restore") : QStringLiteral("window-maximize")));
        m_maximizeButton->setToolTip(maximized ? i18nc("@action:button", "Restore") : i18nc("@action:button", "Maximize"));
        m_maximizeButton->setAccessibleName(m_maximizeButton->toolTip());
    }
}

void GnomeHeaderBar::portalSettingChanged(const QString &group, const QString &key, const QDBusVariant &value)
{
    if (group == QLatin1String("org.gnome.desktop.wm.preferences") && key == QLatin1String("button-layout")) {
        setButtonLayout(value.variant().toString());
        updateGeometryAndFrame();
    }
}

void GnomeHeaderBar::updateGeometryAndFrame()
{
    const bool top = m_window->toolBarArea(m_toolbar) == Qt::TopToolBarArea && !m_toolbar->isHidden();
    const bool integrated = top && m_nativeGnomeWayland && !m_toolbar->isFloating();
    if (m_window->windowFlags().testFlag(Qt::FramelessWindowHint) != integrated) {
        const bool visible = m_window->isVisible();
        const Qt::WindowStates state = m_window->windowState();
        m_window->setWindowFlag(Qt::FramelessWindowHint, integrated);
        m_window->setWindowState(state);
        if (visible) {
            m_window->show();
        }
    }
    setVisible(top);
    m_windowButtons->setVisible(integrated);
    m_leftWindowButtons->setVisible(integrated);
    auto *menuAction = m_window->actionCollection()->action(QStringLiteral("hamburger_menu"));
    if (!top && !m_toolbar->actions().contains(menuAction)) {
        m_toolbar->addAction(menuAction);
        m_fallbackMenuAction = true;
    } else if (top && m_fallbackMenuAction) {
        m_toolbar->removeAction(menuAction);
        m_fallbackMenuAction = false;
    }
    const bool sidebarVisible = m_places->isVisible() && m_window->dockWidgetArea(m_places) == Qt::LeftDockWidgetArea;
    const int sidebarWidth = sidebarVisible ? qMax(112, m_places->width()) : 40;
    const bool roomySidebar = sidebarVisible && sidebarWidth >= 180;
    findChild<QLabel *>(QStringLiteral("gnomeSidebarTitle"))->setVisible(roomySidebar);
    findChildren<QToolButton *>().front()->setVisible(roomySidebar);
    const int leftWidth = integrated && m_leftWindowButtons->layout()->count() ? m_leftWindowButtons->sizeHint().width() : 0;
    const int rightWidth = integrated && m_windowButtons->layout()->count() ? m_windowButtons->sizeHint().width() : 0;
    // QToolBar lays out actions from x=0 even when its widget contentsRect is inset.
    // Reserve the prefix with a native widget action, and the trailing controls with
    // a right contents margin so they remain outside the toolbar's overflow menu.
    const int handleWidth = m_toolbar->isMovable() ? m_toolbar->style()->pixelMetric(QStyle::PM_ToolBarHandleExtent, nullptr, m_toolbar) : 0;
    m_prefixSpace->setFixedWidth(qMax(0, sidebarWidth + leftWidth - handleWidth));
    m_prefixAction->setVisible(top);
    const auto actions = m_toolbar->actions();
    if (actions.isEmpty() || actions.front() != m_prefixAction) {
        m_toolbar->removeAction(m_prefixAction);
        m_toolbar->insertAction(actions.isEmpty() ? nullptr : actions.front(), m_prefixAction);
    }
    m_toolbar->setContentsMargins(0, 0, rightWidth, 0);
    setGeometry(handleWidth + leftWidth, 0, qMax(0, sidebarWidth - handleWidth), m_toolbar->height());
    m_leftWindowButtons->setGeometry(handleWidth, 0, leftWidth, m_toolbar->height());
    m_windowButtons->setGeometry(m_toolbar->width() - rightWidth, 0, rightWidth, m_toolbar->height());
    raise();
    m_leftWindowButtons->raise();
    m_windowButtons->raise();
    updateWindowButtons();
}

void GnomeHeaderBar::beginWindowOperation(const QPoint &position)
{
    constexpr int border = 6;
    Qt::Edges edges;
    if (position.x() < border)
        edges |= Qt::LeftEdge;
    if (position.x() >= m_window->width() - border)
        edges |= Qt::RightEdge;
    if (position.y() < border)
        edges |= Qt::TopEdge;
    if (position.y() >= m_window->height() - border)
        edges |= Qt::BottomEdge;
    if (edges && !m_window->isMaximized()) {
        m_window->windowHandle()->startSystemResize(edges);
    } else {
        m_window->windowHandle()->startSystemMove();
    }
}

bool GnomeHeaderBar::eventFilter(QObject *object, QEvent *event)
{
    if ((object == this || object == m_toolbar || object == m_places || object == m_window)
        && (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::Show || event->type() == QEvent::Hide
            || event->type() == QEvent::WindowStateChange || event->type() == QEvent::PaletteChange || event->type() == QEvent::LayoutRequest
            || event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved)) {
        if (!m_updatePending) {
            m_updatePending = true;
            QTimer::singleShot(0, this, [this]() {
                m_updatePending = false;
                updateGeometryAndFrame();
            });
        }
    }
    if (m_window->windowFlags().testFlag(Qt::FramelessWindowHint)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            auto *target = qobject_cast<QWidget *>(object);
            if (!target || target->window() != m_window) {
                return false;
            }
            const QPoint position = m_window->mapFromGlobal(mouse->globalPosition().toPoint());
            const bool edge = position.x() < 6 || position.y() < 6 || position.x() >= m_window->width() - 6 || position.y() >= m_window->height() - 6;
            const int handleWidth = m_toolbar->isMovable() ? m_toolbar->style()->pixelMetric(QStyle::PM_ToolBarHandleExtent, nullptr, m_toolbar) : 0;
            const bool toolbarSpace = object == m_toolbar && mouse->position().x() >= handleWidth;
            if (mouse->button() == Qt::LeftButton && (edge || object == this || toolbarSpace)) {
                beginWindowOperation(position);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && (object == this || object == m_toolbar)) {
                m_window->isMaximized() ? m_window->showNormal() : m_window->showMaximized();
                return true;
            }
        }
    }
    return QWidget::eventFilter(object, event);
}

void GnomeHeaderBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
}
