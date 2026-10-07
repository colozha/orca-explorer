// SPDX-FileCopyrightText: 2026 Dolphin Contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gnomeaboutdialog.h"

#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KActionCollection>
#include <KLocalizedString>
#include <KStandardAction>

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

void showGnomeAboutDialog(QWidget *parent, KActionCollection *actions)
{
    auto about = KAboutData::applicationData();
    about.addComponent(i18n("Adwaita Icon Theme"),
                       i18n("GNOME appearance icons by The GNOME Project"),
                       QStringLiteral("50.0"),
                       QStringLiteral("https://gitlab.gnome.org/GNOME/adwaita-icon-theme"),
                       KAboutLicense::LGPL_V3);
    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName(QStringLiteral("gnomeAboutDialog"));
    dialog->setWindowTitle(i18nc("@title:window", "About %1", about.displayName()));
    dialog->setMinimumWidth(360);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(18);

    auto *icon = new QLabel(dialog);
    icon->setPixmap(QIcon(QStringLiteral(":/orca-explorer/logo.svg")).pixmap(96, 96));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);
    auto *title = new QLabel(about.displayName(), dialog);
    QFont font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.8);
    font.setBold(true);
    title->setFont(font);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto *description = new QLabel(about.shortDescription(), dialog);
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *version = new QLabel(about.version(), dialog);
    version->setAlignment(Qt::AlignCenter);
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    version->setMinimumWidth(70);
    version->setFixedHeight(30);
    version->setStyleSheet(QStringLiteral("QLabel { background: palette(button); color: palette(link); border-radius: 15px; font-weight: bold; }"));
    layout->addWidget(version, 0, Qt::AlignHCenter);

    auto addCard = [dialog, layout](const QString &text, const QIcon &icon) {
        auto *button = new QPushButton(icon, text, dialog);
        button->setProperty("gnomeAboutCard", true);
        button->setMinimumHeight(54);
        layout->addWidget(button);
        return button;
    };
    if (!about.homepage().isEmpty()) {
        auto *website = addCard(i18nc("@action:button", "Website"), QIcon::fromTheme(QStringLiteral("go-next")));
        QObject::connect(website, &QPushButton::clicked, dialog, [about]() {
            QDesktopServices::openUrl(QUrl(about.homepage()));
        });
    }
    for (auto id : {KStandardAction::HelpContents, KStandardAction::ReportBug}) {
        if (auto *action = actions->action(KStandardAction::name(id))) {
            auto *button = addCard(action->text().remove(QLatin1Char('&')), action->icon());
            QObject::connect(button, &QPushButton::clicked, action, &QAction::trigger);
        }
    }
    auto *credits = addCard(i18nc("@action:button", "Credits and Legal"), QIcon::fromTheme(QStringLiteral("help-about")));
    QObject::connect(credits, &QPushButton::clicked, dialog, [dialog, about]() {
        auto *details = new KAboutApplicationDialog(about, dialog);
        details->setAttribute(Qt::WA_DeleteOnClose);
        details->show();
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog->show();
}
