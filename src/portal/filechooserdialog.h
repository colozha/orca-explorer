// SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "portaltypes.h"
#include <QDialog>
#include <QUrl>
class DolphinView;
class QComboBox;
class QLabel;
class QLineEdit;
class KUrlNavigator;
class KFilePlacesView;

class FileChooserDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode {
        Open,
        Save,
        SaveMultiple
    };
    FileChooserDialog(Mode mode, const QString &title, const QString &parentWindow, const QVariantMap &options);
    QVariantMap results() const;
    void accept() override;
    static bool validFileName(const QString &name);
    static QString decodePath(QByteArray path);

private:
    void navigate(const QUrl &url);
    void applyFilter();
    void showError(const QString &error);
    QStringList selectedPaths() const;
    Mode m_mode;
    QVariantMap m_options;
    DolphinView *m_view;
    KUrlNavigator *m_navigator;
    KFilePlacesView *m_places;
    QLineEdit *m_name;
    QLabel *m_error;
    QComboBox *m_filters;
    OrcaPortal::Filters m_filterValues;
    QList<QPair<QString, QWidget *>> m_choices;
    bool m_nameEdited = false;
    QStringList m_uris;
};
