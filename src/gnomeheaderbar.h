// SPDX-FileCopyrightText: 2026 Dolphin Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GNOMEHEADERBAR_H
#define GNOMEHEADERBAR_H

#include <QWidget>

class KToolBar;
class DolphinMainWindow;
class QDockWidget;
class QDBusVariant;
class QToolButton;
class QWidgetAction;

// Application-owned chrome around the existing toolbar. Actions remain owned by Dolphin.
class GnomeHeaderBar final : public QWidget
{
    Q_OBJECT
public:
    GnomeHeaderBar(DolphinMainWindow *window, KToolBar *toolbar, QDockWidget *places);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private Q_SLOTS:
    void portalSettingChanged(const QString &group, const QString &key, const QDBusVariant &value);

private:
    void updateGeometryAndFrame();
    void setButtonLayout(const QString &layout);
    void updateWindowButtons();
    void beginWindowOperation(const QPoint &position);

    DolphinMainWindow *m_window;
    KToolBar *m_toolbar;
    QDockWidget *m_places;
    QWidget *m_prefixSpace;
    QWidgetAction *m_prefixAction;
    QWidget *m_windowButtons;
    QWidget *m_leftWindowButtons;
    QToolButton *m_maximizeButton = nullptr;
    bool m_nativeGnomeWayland;
    bool m_updatePending = false;
    bool m_fallbackMenuAction = false;
};

#endif
