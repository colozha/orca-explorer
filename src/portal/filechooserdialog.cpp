// SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "filechooserdialog.h"
#include "dolphinplacesmodelsingleton.h"
#include "kitemviews/kitemlistcontainer.h"
#include "kitemviews/kitemlistcontroller.h"
#include "views/dolphinview.h"

#include <KFilePlacesView>
#include <KLocalizedString>
#include <KUrlNavigator>
#include <KWindowSystem>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>
#include <algorithm>

using namespace OrcaPortal;

QString FileChooserDialog::decodePath(QByteArray path)
{
    if (path.endsWith('\0'))
        path.chop(1);
    if (path.contains('\0'))
        return {};
    return QFile::decodeName(path);
}

bool FileChooserDialog::validFileName(const QString &name)
{
    return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..") && !name.contains(QLatin1Char('/')) && !name.contains(QChar::Null);
}

FileChooserDialog::FileChooserDialog(Mode mode, const QString &title, const QString &parentWindow, const QVariantMap &options)
    : m_mode(mode)
    , m_options(options)
{
    setObjectName(QStringLiteral("orcaFileChooser"));
    setWindowTitle(title.isEmpty() ? (mode == Open ? i18n("Open — Orca Explorer") : i18n("Save — Orca Explorer")) : title);
    resize(860, 580);
    setMinimumSize(480, 360);
    setModal(options.value(QStringLiteral("modal"), true).toBool());
    auto layout = new QVBoxLayout(this);
    auto placesModel = DolphinPlacesModelSingleton::instance().placesModel();
    QString folder = decodePath(options.value(QStringLiteral("current_folder")).toByteArray());
    const QString currentFile = decodePath(options.value(QStringLiteral("current_file")).toByteArray());
    if (!currentFile.isEmpty())
        folder = QFileInfo(currentFile).absolutePath();
    if (!QFileInfo(folder).isDir() || !QDir::isAbsolutePath(folder))
        folder = QDir::homePath();
    m_navigator = new KUrlNavigator(placesModel, QUrl::fromLocalFile(folder), this);
    m_navigator->setObjectName(QStringLiteral("navigator"));
    m_navigator->setAccessibleName(i18n("Folder location"));
    layout->addWidget(m_navigator);
    auto splitter = new QSplitter(this);
    m_places = new KFilePlacesView(splitter);
    m_places->setModel(placesModel);
    m_places->setAccessibleName(i18n("Places"));
    m_view = new DolphinView(QUrl::fromLocalFile(folder), splitter);
    m_view->setObjectName(QStringLiteral("fileView"));
    m_view->setViewPropertiesContext(QStringLiteral("orca-filechooser"));
    m_view->setActive(true);
    splitter->addWidget(m_places);
    splitter->addWidget(m_view);
    splitter->setSizes({180, 660});
    splitter->setCollapsible(0, true);
    splitter->setCollapsible(1, false);
    layout->addWidget(splitter, 1);
    auto form = new QFormLayout;
    m_name = new QLineEdit(this);
    m_name->setObjectName(QStringLiteral("fileName"));
    m_name->setClearButtonEnabled(true);
    m_name->setText(currentFile.isEmpty() ? options.value(QStringLiteral("current_name")).toString() : QFileInfo(currentFile).fileName());
    m_nameEdited = !m_name->text().isEmpty();
    m_name->setVisible(mode != SaveMultiple);
    if (mode != SaveMultiple)
        form->addRow(options.value(QStringLiteral("directory")).toBool() ? i18n("Folder:") : i18n("File name:"), m_name);
    m_filters = new QComboBox(this);
    m_filters->setObjectName(QStringLiteral("fileType"));
    m_filterValues = decode<Filters>(options.value(QStringLiteral("filters")));
    const Filter currentFilter = decode<Filter>(options.value(QStringLiteral("current_filter")));
    int currentIndex = 0;
    for (const auto &filter : std::as_const(m_filterValues)) {
        m_filters->addItem(filter.name);
        if (filter.name == currentFilter.name)
            currentIndex = m_filters->count() - 1;
    }
    if (!m_filterValues.isEmpty() && !options.value(QStringLiteral("directory")).toBool() && mode != SaveMultiple) {
        form->addRow(i18n("File type:"), m_filters);
        m_filters->setCurrentIndex(currentIndex);
    } else
        m_filters->hide();
    const auto choices = decode<Choices>(options.value(QStringLiteral("choices")));
    for (const auto &choice : choices) {
        QWidget *widget;
        if (choice.options.isEmpty()) {
            auto check = new QCheckBox(choice.label, this);
            check->setChecked(choice.selected == QLatin1String("true"));
            form->addRow(check);
            widget = check;
        } else {
            auto combo = new QComboBox(this);
            for (const auto &option : choice.options)
                combo->addItem(option.label, option.id);
            const int index = combo->findData(choice.selected);
            if (index >= 0)
                combo->setCurrentIndex(index);
            form->addRow(choice.label, combo);
            widget = combo;
        }
        widget->setObjectName(QStringLiteral("choice:") + choice.id);
        m_choices.append({choice.id, widget});
    }
    layout->addLayout(form);
    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("errorMessage"));
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QString acceptLabel = options.value(QStringLiteral("accept_label")).toString();
    if (acceptLabel.isEmpty())
        acceptLabel = mode == Open ? (options.value(QStringLiteral("directory")).toBool() ? i18n("Select Folder") : i18n("Open")) : i18n("Save");
    acceptLabel.replace(QLatin1Char('&'), QStringLiteral("&&"));
    acceptLabel.replace(QLatin1Char('_'), QLatin1Char('&'));
    auto acceptButton = buttons->addButton(acceptLabel, QDialogButtonBox::AcceptRole);
    acceptButton->setObjectName(QStringLiteral("acceptButton"));
    acceptButton->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &FileChooserDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    connect(m_name, &QLineEdit::textEdited, this, [this] {
        m_nameEdited = true;
        m_error->hide();
    });
    connect(m_name, &QLineEdit::returnPressed, this, &FileChooserDialog::accept);
    connect(m_navigator, &KUrlNavigator::urlChanged, this, &FileChooserDialog::navigate);
    connect(m_places, &KFilePlacesView::urlChanged, this, &FileChooserDialog::navigate);
    connect(m_view, &DolphinView::urlChanged, this, [this](const QUrl &url) {
        m_navigator->setLocationUrl(url);
        m_places->setUrl(url);
        if (m_mode == Open) {
            m_name->clear();
            m_nameEdited = false;
        }
    });
    connect(m_view, &DolphinView::selectionChanged, this, [this](const KFileItemList &items) {
        // Folder navigation must preserve a suggested or typed save filename.
        if (m_mode == Save && (items.size() != 1 || items.first().isDir()))
            return;
        if (items.size() == 1)
            m_name->setText(items.first().name());
        else {
            m_name->clear();
            m_name->setPlaceholderText(items.isEmpty() ? QString() : i18np("%1 file selected", "%1 files selected", items.size()));
        }
        m_nameEdited = false;
        m_error->hide();
    });
    connect(m_view, &DolphinView::itemActivated, this, [this](const KFileItem &item) {
        if (item.isDir())
            navigate(item.url());
        else {
            m_name->setText(item.name());
            m_nameEdited = true;
            accept();
        }
    });
    connect(m_view, &DolphinView::errorMessage, this, [this](const QString &error, int) {
        showError(error);
    });
    connect(m_filters, &QComboBox::currentIndexChanged, this, &FileChooserDialog::applyFilter);
    auto locationShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this);
    connect(locationShortcut, &QShortcut::activated, this, [this] {
        m_navigator->setUrlEditable(true);
        m_navigator->setFocus();
    });
    auto upShortcut = new QShortcut(QKeySequence(QStringLiteral("Alt+Up")), this);
    connect(upShortcut, &QShortcut::activated, this, [this] {
        navigate(m_view->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
    });
    auto hiddenShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+H")), this);
    connect(hiddenShortcut, &QShortcut::activated, this, [this] {
        m_view->setHiddenFilesShown(!m_view->hiddenFilesShown());
    });
    if (auto container = m_view->findChild<KItemListContainer *>()) {
        container->controller()->setSelectionBehavior(
            mode == Open && options.value(QStringLiteral("multiple")).toBool() ? KItemListController::MultiSelection : KItemListController::SingleSelection);
    }
    applyFilter();
    if (!parentWindow.isEmpty()) {
        winId();
        KWindowSystem::setMainWindow(windowHandle(), parentWindow);
    }
    if (mode == Save) {
        m_name->setFocus();
        m_name->selectAll();
    } else
        m_view->setFocus();
}

void FileChooserDialog::navigate(const QUrl &url)
{
    if (!url.isLocalFile()) {
        showError(i18n("Select a local folder or a mounted network folder."));
        m_navigator->setLocationUrl(m_view->url());
        return;
    }
    if (!QFileInfo(url.toLocalFile()).isDir()) {
        showError(i18n("This folder does not exist."));
        return;
    }
    m_error->hide();
    m_view->setUrl(url);
    m_navigator->setLocationUrl(url);
    m_places->setUrl(url);
}

void FileChooserDialog::applyFilter()
{
    QList<QPair<uint, QString>> entries;
    if (m_options.value(QStringLiteral("directory")).toBool() || m_mode == SaveMultiple)
        entries.append({1, QStringLiteral("inode/directory")});
    else if (m_filters->currentIndex() >= 0 && m_filters->currentIndex() < m_filterValues.size()) {
        for (const auto &entry : m_filterValues.at(m_filters->currentIndex()).entries)
            entries.append({entry.type, entry.value});
    }
    m_view->setFileChooserFilters(entries);
}

void FileChooserDialog::showError(const QString &error)
{
    m_error->setText(error);
    m_error->show();
    m_error->setAccessibleName(error);
}

QStringList FileChooserDialog::selectedPaths() const
{
    if ((m_mode == Save || m_nameEdited) && !m_name->text().isEmpty())
        return {QDir(m_view->url().toLocalFile()).absoluteFilePath(m_name->text())};
    QStringList paths;
    for (const auto &item : m_view->selectedItems())
        if (item.url().isLocalFile())
            paths.append(item.url().toLocalFile());
    if (paths.isEmpty() && !m_name->text().isEmpty())
        paths.append(QDir(m_view->url().toLocalFile()).absoluteFilePath(m_name->text()));
    return paths;
}

void FileChooserDialog::accept()
{
    QStringList paths;
    if (m_mode == SaveMultiple) {
        const auto names = decode<QList<QByteArray>>(m_options.value(QStringLiteral("files")));
        if (names.isEmpty()) {
            showError(i18n("No file names were supplied."));
            return;
        }
        const auto selected = m_view->selectedItems();
        const QString folder = selected.size() == 1 && selected.first().isDir() ? selected.first().url().toLocalFile() : m_view->url().toLocalFile();
        QSet<QString> reserved;
        for (const auto &bytes : names) {
            const QString name = decodePath(bytes);
            if (!validFileName(name)) {
                showError(i18n("A supplied file name is invalid."));
                return;
            }
            QString candidate = QDir(folder).filePath(name);
            const QFileInfo original(name);
            int suffix = 1;
            while (QFileInfo::exists(candidate) || QFileInfo(candidate).isSymLink() || reserved.contains(candidate)) {
                candidate = QDir(folder).filePath(original.completeBaseName() + QStringLiteral(" (%1)").arg(suffix++)
                                                  + (original.suffix().isEmpty() ? QString() : QLatin1Char('.') + original.suffix()));
            }
            paths.append(candidate);
            reserved.insert(candidate);
        }
    } else {
        paths = selectedPaths();
        if (m_mode == Open && m_options.value(QStringLiteral("directory")).toBool() && paths.isEmpty())
            paths.append(m_view->url().toLocalFile());
    }
    if (paths.isEmpty()) {
        showError(i18n("Choose a file to continue."));
        return;
    }
    if (m_mode == Open && !m_options.value(QStringLiteral("multiple")).toBool() && paths.size() != 1) {
        showError(i18n("Choose one item to continue."));
        return;
    }
    QStringList uris;
    for (const QString &path : std::as_const(paths)) {
        if (path.contains(QChar::Null)) {
            showError(i18n("The path is invalid."));
            return;
        }
        const QFileInfo info(path);
        if (m_mode == Open) {
            const bool directory = m_options.value(QStringLiteral("directory")).toBool();
            if (!info.exists() || !info.isReadable() || (info.isDir() && !info.isExecutable())) {
                showError(i18n("The selected item cannot be read."));
                return;
            }
            if (info.isDir() && !directory) {
                navigate(QUrl::fromLocalFile(path));
                return;
            }
            if (directory && !info.isDir()) {
                showError(i18n("Choose a folder."));
                return;
            }
            if (!directory && !info.isFile()) {
                showError(i18n("Choose a regular file."));
                return;
            }
        } else {
            if (!validFileName(info.fileName())) {
                showError(i18n("Enter a valid file name."));
                return;
            }
            if (!info.dir().exists() || !QFileInfo(info.absolutePath()).isWritable() || !QFileInfo(info.absolutePath()).isExecutable()) {
                showError(i18n("The destination folder is not writable."));
                return;
            }
            if (info.exists() && (!info.isFile() || !info.isWritable())) {
                showError(i18n("The destination cannot be replaced."));
                return;
            }
            if (info.isSymLink() && !info.exists()) {
                showError(i18n("The destination is a broken symbolic link."));
                return;
            }
            if (m_mode == Save && info.exists()
                && QMessageBox::question(this,
                                         i18n("Replace File?"),
                                         i18n("Replace “%1”?", info.fileName()),
                                         QMessageBox::Yes | QMessageBox::No,
                                         QMessageBox::No)
                    != QMessageBox::Yes)
                return;
        }
        uris.append(QUrl::fromLocalFile(info.absoluteFilePath()).toString(QUrl::FullyEncoded));
    }
    m_uris = uris;
    QDialog::accept();
}

QVariantMap FileChooserDialog::results() const
{
    QVariantMap result{{QStringLiteral("uris"), m_uris}};
    ChoiceResults selected;
    for (const auto &[id, widget] : m_choices) {
        if (auto check = qobject_cast<QCheckBox *>(widget))
            selected.append({id, check->isChecked() ? QStringLiteral("true") : QStringLiteral("false")});
        else if (auto combo = qobject_cast<QComboBox *>(widget))
            selected.append({id, combo->currentData().toString()});
    }
    if (!selected.isEmpty())
        result.insert(QStringLiteral("choices"), QVariant::fromValue(selected));
    if (!m_filters->isHidden() && m_filters->currentIndex() >= 0)
        result.insert(QStringLiteral("current_filter"), QVariant::fromValue(m_filterValues.at(m_filters->currentIndex())));
    if (m_mode == Open) {
        const bool writable = std::all_of(m_uris.cbegin(), m_uris.cend(), [](const QString &uri) {
            return QFileInfo(QUrl(uri).toLocalFile()).isWritable();
        });
        result.insert(QStringLiteral("writable"), writable);
    }
    return result;
}
