/*
 * SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "interfacesettingspage.h"
#include "dolphin_generalsettings.h"

#include "confirmationssettingspage.h"
#include "folderstabssettingspage.h"
#include "previewssettingspage.h"
#include "statusandlocationbarssettingspage.h"

#if HAVE_BALOO
#include "panelsettingspage.h"
#endif

#include <KLocalizedString>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

InterfaceSettingsPage::InterfaceSettingsPage(QWidget *parent)
    : SettingsPageBase(parent)
    , m_pages()
{
    QVBoxLayout *topLayout = new QVBoxLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);

    auto *appearanceLayout = new QFormLayout;
    m_appearanceMode = new QComboBox(this);
    m_appearanceMode->setObjectName(QStringLiteral("appearanceMode"));
    m_appearanceMode->addItem(i18nc("@item:inlistbox application appearance", "Dolphin Classic"), GeneralSettings::EnumAppearanceMode::Dolphin);
    m_appearanceMode->addItem(i18nc("@item:inlistbox application appearance", "GNOME Files"), GeneralSettings::EnumAppearanceMode::GnomeFiles);
    m_appearanceMode->setCurrentIndex(m_appearanceMode->findData(GeneralSettings::appearanceMode()));
    appearanceLayout->addRow(i18nc("@label:listbox", "Appearance:"), m_appearanceMode);
    auto *restartNote = new QLabel(i18nc("@info", "Appearance changes take effect after restarting Orca Explorer."), this);
    restartNote->setWordWrap(true);
    appearanceLayout->addRow(QString(), restartNote);
    topLayout->addLayout(appearanceLayout);
    connect(m_appearanceMode, &QComboBox::currentIndexChanged, this, &InterfaceSettingsPage::changed);

    QTabWidget *tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);
    tabWidget->tabBar()->setExpanding(true);

    // initialize 'Folders & Tabs' tab
    FoldersTabsSettingsPage *foldersTabsPage = new FoldersTabsSettingsPage(tabWidget);
    tabWidget->addTab(foldersTabsPage, i18nc("@title:tab Folders & Tabs settings", "Folders && Tabs"));
    connect(foldersTabsPage, &FoldersTabsSettingsPage::changed, this, &InterfaceSettingsPage::changed);

    // initialize 'Previews' tab
    PreviewsSettingsPage *previewsPage = new PreviewsSettingsPage(tabWidget);
    tabWidget->addTab(previewsPage, i18nc("@title:tab Previews settings", "Previews"));
    connect(previewsPage, &PreviewsSettingsPage::changed, this, &InterfaceSettingsPage::changed);

    // initialize 'Context Menu' tab
    ConfirmationsSettingsPage *confirmationsPage = new ConfirmationsSettingsPage(tabWidget);
    tabWidget->addTab(confirmationsPage, i18nc("@title:tab Confirmations settings", "Confirmations"));
    connect(confirmationsPage, &ConfirmationsSettingsPage::changed, this, &InterfaceSettingsPage::changed);

#if HAVE_BALOO
    // initialize 'Panel' tab
    PanelSettingsPage *panelPage = new PanelSettingsPage(tabWidget);
    tabWidget->addTab(panelPage, i18nc("@title:tab Panels settings", "Panels"));
    connect(panelPage, &PanelSettingsPage::changed, this, &InterfaceSettingsPage::changed);
#endif

    // initialize 'Status & location bars' tab
    StatusAndLocationBarsSettingsPage *statusAndLocationBarsPage = new StatusAndLocationBarsSettingsPage(tabWidget, foldersTabsPage);
    tabWidget->addTab(statusAndLocationBarsPage, i18nc("@title:tab Status & Location bars settings", "Status && Location bars"));
    connect(statusAndLocationBarsPage, &StatusAndLocationBarsSettingsPage::changed, this, &InterfaceSettingsPage::changed);

    m_pages.append(foldersTabsPage);
    m_pages.append(previewsPage);
    m_pages.append(confirmationsPage);

#if HAVE_BALOO
    m_pages.append(panelPage);
#endif

    m_pages.append(statusAndLocationBarsPage);

    topLayout->addWidget(tabWidget, 0, {});
}

InterfaceSettingsPage::~InterfaceSettingsPage() = default;

void InterfaceSettingsPage::applySettings()
{
    GeneralSettings::setAppearanceMode(m_appearanceMode->currentData().toInt());
    GeneralSettings::self()->save();
    for (SettingsPageBase *page : std::as_const(m_pages)) {
        page->applySettings();
    }
}

void InterfaceSettingsPage::restoreDefaults()
{
    m_appearanceMode->setCurrentIndex(m_appearanceMode->findData(GeneralSettings::EnumAppearanceMode::GnomeFiles));
    Q_EMIT changed();
    for (SettingsPageBase *page : std::as_const(m_pages)) {
        page->restoreDefaults();
    }
}

#include "moc_interfacesettingspage.cpp"
