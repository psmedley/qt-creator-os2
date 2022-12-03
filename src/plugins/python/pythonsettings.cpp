/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "pythonsettings.h"

#include "pythonconstants.h"
#include "pythonplugin.h"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <languageclient/languageclient_global.h>
#include <languageclient/languageclientsettings.h>
#include <languageclient/languageclientmanager.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <utils/algorithm.h>
#include <utils/qtcassert.h>
#include <utils/detailswidget.h>
#include <utils/environment.h>
#include <utils/listmodel.h>
#include <utils/layoutbuilder.h>
#include <utils/pathchooser.h>
#include <utils/qtcprocess.h>
#include <utils/treemodel.h>
#include <utils/utilsicons.h>

#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QSettings>
#include <QStackedWidget>
#include <QTreeView>
#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QJsonDocument>
#include <QJsonObject>

using namespace ProjectExplorer;
using namespace Utils;
using namespace Layouting;

namespace Python {
namespace Internal {

static Interpreter createInterpreter(const FilePath &python,
                                     const QString &defaultName,
                                     bool windowedSuffix = false)
{
    Interpreter result;
    result.id = QUuid::createUuid().toString();
    result.command = python;

    QtcProcess pythonProcess;
    pythonProcess.setProcessChannelMode(QProcess::MergedChannels);
    pythonProcess.setTimeoutS(1);
    pythonProcess.setCommand({python, {"--version"}});
    pythonProcess.runBlocking();
    if (pythonProcess.result() == ProcessResult::FinishedWithSuccess)
        result.name = pythonProcess.cleanedStdOut().trimmed();
    if (result.name.isEmpty())
        result.name = defaultName;
    if (windowedSuffix)
        result.name += " (Windowed)";
    QDir pythonDir(python.parentDir().toString());
    if (pythonDir.exists() && pythonDir.exists("activate") && pythonDir.cdUp())
        result.name += QString(" (%1 Virtual Environment)").arg(pythonDir.dirName());

    return result;
}

class InterpreterDetailsWidget : public QWidget
{
    Q_OBJECT
public:
    InterpreterDetailsWidget()
        : m_name(new QLineEdit)
        , m_executable(new PathChooser())
    {
        m_executable->setExpectedKind(PathChooser::ExistingCommand);

        connect(m_name, &QLineEdit::textChanged, this, &InterpreterDetailsWidget::changed);
        connect(m_executable, &PathChooser::filePathChanged, this, &InterpreterDetailsWidget::changed);

        Form {
            PythonSettings::tr("Name:"), m_name, Break(),
            PythonSettings::tr("Executable"), m_executable
        }.attachTo(this, false);
    }

    void updateInterpreter(const Interpreter &interpreter)
    {
        QSignalBlocker blocker(this); // do not emit changed when we change the controls here
        m_currentInterpreter = interpreter;
        m_name->setText(interpreter.name);
        m_executable->setFilePath(interpreter.command);
    }

    Interpreter toInterpreter()
    {
        m_currentInterpreter.command = m_executable->filePath();
        m_currentInterpreter.name = m_name->text();
        return m_currentInterpreter;
    }
    QLineEdit *m_name = nullptr;
    PathChooser *m_executable = nullptr;
    Interpreter m_currentInterpreter;

signals:
    void changed();
};


class InterpreterOptionsWidget : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(Python::Internal::InterpreterOptionsWidget)
public:
    InterpreterOptionsWidget(const QList<Interpreter> &interpreters,
                             const QString &defaultInterpreter);

    void apply();

private:
    QTreeView m_view;
    ListModel<Interpreter> m_model;
    InterpreterDetailsWidget *m_detailsWidget = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_makeDefaultButton = nullptr;
    QPushButton *m_cleanButton = nullptr;
    QString m_defaultId;

    void currentChanged(const QModelIndex &index, const QModelIndex &previous);
    void detailsChanged();
    void updateCleanButton();
    void addItem();
    void deleteItem();
    void makeDefault();
    void cleanUp();
};

InterpreterOptionsWidget::InterpreterOptionsWidget(const QList<Interpreter> &interpreters, const QString &defaultInterpreter)
    : m_detailsWidget(new InterpreterDetailsWidget())
    , m_defaultId(defaultInterpreter)
{
    m_model.setDataAccessor([this](const Interpreter &interpreter, int column, int role) -> QVariant {
        switch (role) {
        case Qt::DisplayRole:
            return interpreter.name;
        case Qt::FontRole: {
            QFont f = font();
            f.setBold(interpreter.id == m_defaultId);
            return f;
        }
        case Qt::ToolTipRole:
            if (interpreter.command.isEmpty())
                return tr("Executable is empty.");
            if (!interpreter.command.exists())
                return tr("%1 does not exist.").arg(interpreter.command.toUserOutput());
            if (!interpreter.command.isExecutableFile())
                return tr("%1 is not an executable file.").arg(interpreter.command.toUserOutput());
            break;
        case Qt::DecorationRole:
            if (column == 0 && !interpreter.command.isExecutableFile())
                return Utils::Icons::CRITICAL.icon();
            break;
        default:
            break;
        }
        return {};
    });
    m_model.setAllData(interpreters);

    m_view.setModel(&m_model);
    m_view.setHeaderHidden(true);
    m_view.setSelectionMode(QAbstractItemView::SingleSelection);
    m_view.setSelectionBehavior(QAbstractItemView::SelectItems);
    connect(m_view.selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &InterpreterOptionsWidget::currentChanged);

    auto addButton = new QPushButton(PythonSettings::tr("&Add"));
    connect(addButton, &QPushButton::pressed, this, &InterpreterOptionsWidget::addItem);

    m_deleteButton = new QPushButton(PythonSettings::tr("&Delete"));
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::pressed, this, &InterpreterOptionsWidget::deleteItem);

    m_makeDefaultButton = new QPushButton(PythonSettings::tr("&Make Default"));
    m_makeDefaultButton->setEnabled(false);
    connect(m_makeDefaultButton, &QPushButton::pressed, this, &InterpreterOptionsWidget::makeDefault);

    m_cleanButton = new QPushButton(PythonSettings::tr("&Clean Up"));
    connect(m_cleanButton, &QPushButton::pressed, this, &InterpreterOptionsWidget::cleanUp);
    m_cleanButton->setToolTip(
        PythonSettings::tr("Remove all Python interpreters without a valid executable."));

    updateCleanButton();

    m_detailsWidget->hide();
    connect(m_detailsWidget,
            &InterpreterDetailsWidget::changed,
            this,
            &InterpreterOptionsWidget::detailsChanged);

    Column buttons {
        addButton,
        m_deleteButton,
        m_makeDefaultButton,
        m_cleanButton,
        Stretch()
    };

    Column {
        Row { &m_view, buttons },
        m_detailsWidget
    }.attachTo(this);
}

void InterpreterOptionsWidget::apply()
{
    QList<Interpreter> interpreters;
    for (const TreeItem *treeItem : m_model)
        interpreters << static_cast<const ListItem<Interpreter> *>(treeItem)->itemData;
    PythonSettings::setInterpreter(interpreters, m_defaultId);
}

void InterpreterOptionsWidget::currentChanged(const QModelIndex &index, const QModelIndex &previous)
{
    if (previous.isValid()) {
        m_model.itemAt(previous.row())->itemData = m_detailsWidget->toInterpreter();
        emit m_model.dataChanged(previous, previous);
    }
    if (index.isValid()) {
        m_detailsWidget->updateInterpreter(m_model.itemAt(index.row())->itemData);
        m_detailsWidget->show();
    } else {
        m_detailsWidget->hide();
    }
    m_deleteButton->setEnabled(index.isValid());
    m_makeDefaultButton->setEnabled(index.isValid());
}

void InterpreterOptionsWidget::detailsChanged()
{
    const QModelIndex &index = m_view.currentIndex();
    if (index.isValid()) {
        m_model.itemAt(index.row())->itemData = m_detailsWidget->toInterpreter();
        emit m_model.dataChanged(index, index);
    }
    updateCleanButton();
}

void InterpreterOptionsWidget::updateCleanButton()
{
    m_cleanButton->setEnabled(Utils::anyOf(m_model.allData(), [](const Interpreter &interpreter) {
        return !interpreter.command.isExecutableFile();
    }));
}

void InterpreterOptionsWidget::addItem()
{
    const QModelIndex &index = m_model.indexForItem(
        m_model.appendItem({QUuid::createUuid().toString(), QString("Python"), FilePath(), false}));
    QTC_ASSERT(index.isValid(), return);
    m_view.setCurrentIndex(index);
    updateCleanButton();
}

void InterpreterOptionsWidget::deleteItem()
{
    const QModelIndex &index = m_view.currentIndex();
    if (index.isValid())
        m_model.destroyItem(m_model.itemAt(index.row()));
    updateCleanButton();
}

class InterpreterOptionsPage : public Core::IOptionsPage
{
    Q_OBJECT

public:
    InterpreterOptionsPage();

    void setInterpreter(const QList<Interpreter> &interpreters) { m_interpreters = interpreters; }
    void addInterpreter(const Interpreter &interpreter) { m_interpreters << interpreter; }
    QList<Interpreter> interpreters() const { return m_interpreters; }
    void setDefaultInterpreter(const QString &defaultId)
    { m_defaultInterpreterId = defaultId; }
    Interpreter defaultInterpreter() const;

    QWidget *widget() override;
    void apply() override;
    void finish() override;

private:
    QPointer<InterpreterOptionsWidget> m_widget;
    QList<Interpreter> m_interpreters;
    QString m_defaultInterpreterId;
};

InterpreterOptionsPage::InterpreterOptionsPage()
{
    setId(Constants::C_PYTHONOPTIONS_PAGE_ID);
    setDisplayName(PythonSettings::tr("Interpreters"));
    setCategory(Constants::C_PYTHON_SETTINGS_CATEGORY);
    setDisplayCategory(PythonSettings::tr("Python"));
    setCategoryIconPath(":/python/images/settingscategory_python.png");
}

Interpreter InterpreterOptionsPage::defaultInterpreter() const
{
    if (m_defaultInterpreterId.isEmpty())
        return {};
    return Utils::findOrDefault(m_interpreters, [this](const Interpreter &interpreter) {
        return interpreter.id == m_defaultInterpreterId;
    });
}

QWidget *InterpreterOptionsPage::widget()
{
    if (!m_widget)
        m_widget = new InterpreterOptionsWidget(m_interpreters, m_defaultInterpreterId);
    return m_widget;
}

void InterpreterOptionsPage::apply()
{
    if (m_widget)
        m_widget->apply();
}

void InterpreterOptionsPage::finish()
{
    delete m_widget;
    m_widget = nullptr;
}

static bool alreadyRegistered(const QList<Interpreter> &pythons, const FilePath &pythonExecutable)
{
    return Utils::anyOf(pythons, [pythonExecutable](const Interpreter &interpreter) {
        return interpreter.command.toFileInfo().canonicalFilePath()
               == pythonExecutable.toFileInfo().canonicalFilePath();
    });
}

static InterpreterOptionsPage &interpreterOptionsPage()
{
    static InterpreterOptionsPage page;
    return page;
}

static const QStringList &plugins()
{
    static const QStringList plugins{"flake8",
                                     "jedi_completion",
                                     "jedi_definition",
                                     "jedi_hover",
                                     "jedi_references",
                                     "jedi_signature_help",
                                     "jedi_symbols",
                                     "mccabe",
                                     "pycodestyle",
                                     "pydocstyle",
                                     "pyflakes",
                                     "pylint",
                                     "rope_completion",
                                     "yapf"};
    return plugins;
}

class PyLSConfigureWidget : public QWidget
{
public:
    PyLSConfigureWidget()
        : m_editor(LanguageClient::jsonEditor())
        , m_advancedLabel(new QLabel)
        , m_pluginsGroup(new QGroupBox(PythonSettings::tr("Plugins:")))
        , m_mainGroup(new QGroupBox(PythonSettings::tr("Use Python Language Server")))

    {
        m_mainGroup->setCheckable(true);

        auto mainGroupLayout = new QVBoxLayout;

        auto pluginsLayout = new QVBoxLayout;
        m_pluginsGroup->setLayout(pluginsLayout);
        m_pluginsGroup->setFlat(true);
        for (const QString &plugin : plugins()) {
            auto checkBox = new QCheckBox(plugin, this);
            connect(checkBox, &QCheckBox::clicked, this, [this, plugin, checkBox]() {
                updatePluginEnabled(checkBox->checkState(), plugin);
            });
            m_checkBoxes[plugin] = checkBox;
            pluginsLayout->addWidget(checkBox);
        }

        mainGroupLayout->addWidget(m_pluginsGroup);

        const QString labelText = PythonSettings::tr(
            "For a complete list of available options, consult the <a "
            "href=\"https://github.com/python-lsp/python-lsp-server/blob/develop/"
            "CONFIGURATION.md\">Python LSP Server configuration documentation</a>.");

        m_advancedLabel->setText(labelText);
        m_advancedLabel->setOpenExternalLinks(true);
        mainGroupLayout->addWidget(m_advancedLabel);
        mainGroupLayout->addWidget(m_editor->editorWidget(), 1);

        setAdvanced(false);

        mainGroupLayout->addStretch();

        auto advanced = new QCheckBox(PythonSettings::tr("Advanced"));
        advanced->setChecked(false);

        connect(advanced,
                &QCheckBox::toggled,
                this,
                &PyLSConfigureWidget::setAdvanced);

        mainGroupLayout->addWidget(advanced);

        m_mainGroup->setLayout(mainGroupLayout);

        QVBoxLayout *mainLayout = new QVBoxLayout;
        mainLayout->addWidget(m_mainGroup);
        setLayout(mainLayout);
    }

    void initialize(bool enabled, const QString &configuration)
    {
        m_editor->textDocument()->setPlainText(configuration);
        m_mainGroup->setChecked(enabled);
        updateCheckboxes();
    }

    void apply()
    {
        PythonSettings::setPylsEnabled(m_mainGroup->isChecked());
        PythonSettings::setPyLSConfiguration(m_editor->textDocument()->plainText());
    }
private:
    void setAdvanced(bool advanced)
    {
        m_editor->editorWidget()->setVisible(advanced);
        m_advancedLabel->setVisible(advanced);
        m_pluginsGroup->setVisible(!advanced);
        updateCheckboxes();
    }

    void updateCheckboxes()
    {
        const QJsonDocument document = QJsonDocument::fromJson(
            m_editor->textDocument()->plainText().toUtf8());
        if (document.isObject()) {
            const QJsonObject pluginsObject
                = document.object()["pylsp"].toObject()["plugins"].toObject();
            for (const QString &plugin : plugins()) {
                auto checkBox = m_checkBoxes[plugin];
                if (!checkBox)
                    continue;
                const QJsonValue enabled = pluginsObject[plugin].toObject()["enabled"];
                if (!enabled.isBool())
                    checkBox->setCheckState(Qt::PartiallyChecked);
                else
                    checkBox->setCheckState(enabled.toBool(false) ? Qt::Checked : Qt::Unchecked);
            }
        }
    }

    void updatePluginEnabled(Qt::CheckState check, const QString &plugin)
    {
        if (check == Qt::PartiallyChecked)
            return;
        QJsonDocument document = QJsonDocument::fromJson(
            m_editor->textDocument()->plainText().toUtf8());
        QJsonObject config;
        if (!document.isNull())
            config = document.object();
        QJsonObject pylsp = config["pylsp"].toObject();
        QJsonObject plugins = pylsp["plugins"].toObject();
        QJsonObject pluginValue = plugins[plugin].toObject();
        pluginValue.insert("enabled", check == Qt::Checked);
        plugins.insert(plugin, pluginValue);
        pylsp.insert("plugins", plugins);
        config.insert("pylsp", pylsp);
        document.setObject(config);
        m_editor->textDocument()->setPlainText(QString::fromUtf8(document.toJson()));
    }

    QMap<QString, QCheckBox *> m_checkBoxes;
    TextEditor::BaseTextEditor *m_editor = nullptr;
    QLabel *m_advancedLabel = nullptr;
    QGroupBox *m_pluginsGroup = nullptr;
    QGroupBox *m_mainGroup = nullptr;
};


class PyLSOptionsPage : public Core::IOptionsPage
{
public:
    PyLSOptionsPage();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    void setConfiguration(const QString &configuration) { m_configuration = configuration; }
    QString configuration() const { return m_configuration; }

    QWidget *widget() override;
    void apply() override;
    void finish() override;

private:
    QPointer<PyLSConfigureWidget> m_widget;
    bool m_enabled = true;
    QString m_configuration;
};

PyLSOptionsPage::PyLSOptionsPage()
{
    setId(Constants::C_PYLSCONFIGURATION_PAGE_ID);
    setDisplayName(PythonSettings::tr("Language Server Configuration"));
    setCategory(Constants::C_PYTHON_SETTINGS_CATEGORY);
}

QWidget *PyLSOptionsPage::widget()
{
    if (!m_widget) {
        m_widget = new PyLSConfigureWidget();
        m_widget->initialize(m_enabled, m_configuration);
    }
    return m_widget;
}

void PyLSOptionsPage::apply()
{
    if (m_widget)
        m_widget->apply();
}

void PyLSOptionsPage::finish()
{
    delete m_widget;
    m_widget = nullptr;
}

void PyLSOptionsPage::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

static PyLSOptionsPage &pylspOptionsPage()
{
    static PyLSOptionsPage page;
    return page;
}

void InterpreterOptionsWidget::makeDefault()
{
    const QModelIndex &index = m_view.currentIndex();
    if (index.isValid()) {
        QModelIndex defaultIndex = m_model.findIndex([this](const Interpreter &interpreter) {
            return interpreter.id == m_defaultId;
        });
        m_defaultId = m_model.itemAt(index.row())->itemData.id;
        emit m_model.dataChanged(index, index, {Qt::FontRole});
        if (defaultIndex.isValid())
            emit m_model.dataChanged(defaultIndex, defaultIndex, {Qt::FontRole});
    }
}

void InterpreterOptionsWidget::cleanUp()
{
    m_model.destroyItems(
        [](const Interpreter &interpreter) { return !interpreter.command.isExecutableFile(); });
    updateCleanButton();
}

constexpr char settingsGroupKey[] = "Python";
constexpr char interpreterKey[] = "Interpeter";
constexpr char defaultKey[] = "DefaultInterpeter";
constexpr char pylsEnabledKey[] = "PylsEnabled";
constexpr char pylsConfigurationKey[] = "PylsConfiguration";

struct SavedSettings
{
    QList<Interpreter> pythons;
    QString defaultId;
    QString pylsConfiguration;
    bool pylsEnabled = true;
};

static QString defaultPylsConfiguration()
{
    static QJsonObject configuration;
    if (configuration.isEmpty()) {
        QJsonObject enabled;
        enabled.insert("enabled", true);
        QJsonObject disabled;
        disabled.insert("enabled", false);
        QJsonObject plugins;
        plugins.insert("flake8", disabled);
        plugins.insert("jedi_completion", enabled);
        plugins.insert("jedi_definition", enabled);
        plugins.insert("jedi_hover", enabled);
        plugins.insert("jedi_references", enabled);
        plugins.insert("jedi_signature_help", enabled);
        plugins.insert("jedi_symbols", enabled);
        plugins.insert("mccabe", disabled);
        plugins.insert("pycodestyle", disabled);
        plugins.insert("pydocstyle", disabled);
        plugins.insert("pyflakes", enabled);
        plugins.insert("pylint", disabled);
        plugins.insert("rope_completion", enabled);
        plugins.insert("yapf", enabled);
        QJsonObject pylsp;
        pylsp.insert("plugins", plugins);
        configuration.insert("pylsp", pylsp);
    }
    return QString::fromUtf8(QJsonDocument(configuration).toJson());
}

static void disableOutdatedPylsNow()
{
    using namespace LanguageClient;
    const QList<BaseSettings *>
            settings = LanguageClientSettings::pageSettings();
    for (const BaseSettings *setting : settings) {
        if (setting->m_settingsTypeId != LanguageClient::Constants::LANGUAGECLIENT_STDIO_SETTINGS_ID)
            continue;
        auto stdioSetting = static_cast<const StdIOSettings *>(setting);
        if (stdioSetting->arguments().startsWith("-m pyls")
                && stdioSetting->m_languageFilter.isSupported("foo.py", Constants::C_PY_MIMETYPE)) {
            LanguageClientManager::enableClientSettings(stdioSetting->m_id, false);
        }
    }
}

static void disableOutdatedPyls()
{
    using namespace ExtensionSystem;
    if (PluginManager::isInitializationDone()) {
        disableOutdatedPylsNow();
    } else {
        QObject::connect(PluginManager::instance(), &PluginManager::initializationDone,
                         PythonPlugin::instance(), &disableOutdatedPylsNow);
    }
}

static SavedSettings fromSettings(QSettings *settings)
{
    SavedSettings result;
    settings->beginGroup(settingsGroupKey);
    const QVariantList interpreters = settings->value(interpreterKey).toList();
    QList<Interpreter> oldSettings;
    for (const QVariant &interpreterVar : interpreters) {
        auto interpreterList = interpreterVar.toList();
        const Interpreter interpreter{interpreterList.value(0).toString(),
                                      interpreterList.value(1).toString(),
                                      FilePath::fromVariant(interpreterList.value(2)),
                                      interpreterList.value(3, true).toBool()};
        if (interpreterList.size() == 3)
            oldSettings << interpreter;
        else if (interpreterList.size() == 4)
            result.pythons << interpreter;
    }

    for (const Interpreter &interpreter : qAsConst(oldSettings)) {
        if (Utils::anyOf(result.pythons, Utils::equal(&Interpreter::id, interpreter.id)))
            continue;
        result.pythons << interpreter;
    }

    result.pythons = Utils::filtered(result.pythons, [](const Interpreter &interpreter){
        return !interpreter.autoDetected || interpreter.command.isExecutableFile();
    });

    result.defaultId = settings->value(defaultKey).toString();

    QVariant pylsEnabled = settings->value(pylsEnabledKey);
    if (pylsEnabled.isNull())
        disableOutdatedPyls();
    else
        result.pylsEnabled = pylsEnabled.toBool();
    const QVariant pylsConfiguration = settings->value(pylsConfigurationKey);
    if (!pylsConfiguration.isNull())
        result.pylsConfiguration = pylsConfiguration.toString();
    else
        result.pylsConfiguration = defaultPylsConfiguration();
    settings->endGroup();
    return result;
}

static void toSettings(QSettings *settings, const SavedSettings &savedSettings)
{
    settings->beginGroup(settingsGroupKey);
    QVariantList interpretersVar;
    for (const Interpreter &interpreter : savedSettings.pythons) {
        QVariantList interpreterVar{interpreter.id,
                                    interpreter.name,
                                    interpreter.command.toVariant()};
        interpretersVar.append(QVariant(interpreterVar)); // old settings
        interpreterVar.append(interpreter.autoDetected);
        interpretersVar.append(QVariant(interpreterVar)); // new settings
    }
    settings->setValue(interpreterKey, interpretersVar);
    settings->setValue(defaultKey, savedSettings.defaultId);
    settings->setValue(pylsConfigurationKey, savedSettings.pylsConfiguration);
    settings->setValue(pylsEnabledKey, savedSettings.pylsEnabled);
    settings->endGroup();
}

static void addPythonsFromRegistry(QList<Interpreter> &pythons)
{
    QSettings pythonRegistry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore",
                             QSettings::NativeFormat);
    for (const QString &versionGroup : pythonRegistry.childGroups()) {
        pythonRegistry.beginGroup(versionGroup);
        QString name = pythonRegistry.value("DisplayName").toString();
        QVariant regVal = pythonRegistry.value("InstallPath/ExecutablePath");
        if (regVal.isValid()) {
            const FilePath &executable = FilePath::fromUserInput(regVal.toString());
            if (executable.exists() && !alreadyRegistered(pythons, executable)) {
                pythons << Interpreter{QUuid::createUuid().toString(),
                                       name,
                                       FilePath::fromUserInput(regVal.toString())};
            }
        }
        regVal = pythonRegistry.value("InstallPath/WindowedExecutablePath");
        if (regVal.isValid()) {
            const FilePath &executable = FilePath::fromUserInput(regVal.toString());
            if (executable.exists() && !alreadyRegistered(pythons, executable)) {
                pythons << Interpreter{QUuid::createUuid().toString(),
                                       name + PythonSettings::tr(" (Windowed)"),
                                       FilePath::fromUserInput(regVal.toString())};
            }
        }
        regVal = pythonRegistry.value("InstallPath/.");
        if (regVal.isValid()) {
            const FilePath &path = FilePath::fromUserInput(regVal.toString());
            const FilePath python = path.pathAppended("python").withExecutableSuffix();
            if (python.exists() && !alreadyRegistered(pythons, python))
                pythons << createInterpreter(python, "Python " + versionGroup);
            const FilePath pythonw = path.pathAppended("pythonw").withExecutableSuffix();
            if (pythonw.exists() && !alreadyRegistered(pythons, pythonw))
                pythons << createInterpreter(pythonw, "Python " + versionGroup, true);
        }
        pythonRegistry.endGroup();
    }
}

static void addPythonsFromPath(QList<Interpreter> &pythons)
{
    const auto &env = Environment::systemEnvironment();

    if (HostOsInfo::isWindowsHost()) {
        for (const FilePath &executable : env.findAllInPath("python")) {
            // Windows creates empty redirector files that may interfere
            if (executable.toFileInfo().size() == 0)
                continue;
            if (executable.exists() && !alreadyRegistered(pythons, executable))
                pythons << createInterpreter(executable, "Python from Path");
        }
        for (const FilePath &executable : env.findAllInPath("pythonw")) {
            if (executable.exists() && !alreadyRegistered(pythons, executable))
                pythons << createInterpreter(executable, "Python from Path", true);
        }
    } else {
        const QStringList filters = {"python",
                                     "python[1-9].[0-9]",
                                     "python[1-9].[1-9][0-9]",
                                     "python[1-9]"};
        for (const FilePath &path : env.path()) {
            const QDir dir(path.toString());
            for (const QFileInfo &fi : dir.entryInfoList(filters)) {
                const FilePath executable = Utils::FilePath::fromFileInfo(fi);
                if (executable.exists() && !alreadyRegistered(pythons, executable))
                    pythons << createInterpreter(executable, "Python from Path");
            }
        }
    }
}

static QString idForPythonFromPath(QList<Interpreter> pythons)
{
    FilePath pythonFromPath = Environment::systemEnvironment().searchInPath("python3");
    if (pythonFromPath.isEmpty())
        pythonFromPath = Environment::systemEnvironment().searchInPath("python");
    if (pythonFromPath.isEmpty())
        return {};
    const Interpreter &defaultInterpreter
        = findOrDefault(pythons, [pythonFromPath](const Interpreter &interpreter) {
              return interpreter.command == pythonFromPath;
          });
    return defaultInterpreter.id;
}

static PythonSettings *settingsInstance = nullptr;
PythonSettings::PythonSettings() = default;

void PythonSettings::init()
{
    QTC_ASSERT(!settingsInstance, return );
    settingsInstance = new PythonSettings();

    const SavedSettings &settings = fromSettings(Core::ICore::settings());
    pylspOptionsPage().setConfiguration(settings.pylsConfiguration);
    pylspOptionsPage().setEnabled(settings.pylsEnabled);

    QList<Interpreter> pythons = settings.pythons;

    if (HostOsInfo::isWindowsHost())
        addPythonsFromRegistry(pythons);
    addPythonsFromPath(pythons);

    const QString &defaultId = !settings.defaultId.isEmpty() ? settings.defaultId
                                                             : idForPythonFromPath(pythons);
    setInterpreter(pythons, defaultId);
}

void PythonSettings::setInterpreter(const QList<Interpreter> &interpreters, const QString &defaultId)
{
    if (defaultId == interpreterOptionsPage().defaultInterpreter().id
            && interpreters == interpreterOptionsPage().interpreters()) {
        return;
    }
    interpreterOptionsPage().setInterpreter(interpreters);
    interpreterOptionsPage().setDefaultInterpreter(defaultId);
    saveSettings();
}

void PythonSettings::setPyLSConfiguration(const QString &configuration)
{
    if (configuration == pylspOptionsPage().configuration())
        return;
    pylspOptionsPage().setConfiguration(configuration);
    saveSettings();
    emit instance()->pylsConfigurationChanged(configuration);
}

void PythonSettings::setPylsEnabled(const bool &enabled)
{
    if (enabled == pylspOptionsPage().enabled())
        return;
    pylspOptionsPage().setEnabled(enabled);
    saveSettings();
    emit instance()->pylsEnabledChanged(enabled);
}

bool PythonSettings::pylsEnabled()
{
    return pylspOptionsPage().enabled();
}

QString PythonSettings::pyLSConfiguration()
{
    return pylspOptionsPage().configuration();
}

void PythonSettings::addInterpreter(const Interpreter &interpreter, bool isDefault)
{
    interpreterOptionsPage().addInterpreter(interpreter);
    if (isDefault)
        interpreterOptionsPage().setDefaultInterpreter(interpreter.id);
    saveSettings();
}

PythonSettings *PythonSettings::instance()
{
    QTC_CHECK(settingsInstance);
    return settingsInstance;
}

QList<Interpreter> PythonSettings::detectPythonVenvs(const FilePath &path)
{
    QList<Interpreter> result;
    QDir dir = path.toFileInfo().isDir() ? QDir(path.toString()) : path.toFileInfo().dir();
    if (dir.exists()) {
        const QString venvPython = HostOsInfo::withExecutableSuffix("python");
        const QString activatePath = HostOsInfo::isWindowsHost() ? QString{"Scripts"}
                                                                 : QString{"bin"};
        do {
            for (const QString &directory : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                if (dir.cd(directory)) {
                    if (dir.cd(activatePath)) {
                        if (dir.exists("activate") && dir.exists(venvPython)) {
                            FilePath python = FilePath::fromString(dir.absoluteFilePath(venvPython));
                            dir.cdUp();
                            const QString defaultName = QString("Python (%1 Virtual Environment)")
                                                            .arg(dir.dirName());
                            Interpreter interpreter
                                = Utils::findOrDefault(PythonSettings::interpreters(),
                                                       Utils::equal(&Interpreter::command, python));
                            if (interpreter.command.isEmpty()) {
                                interpreter = createInterpreter(python, defaultName);
                                PythonSettings::addInterpreter(interpreter);
                            }
                            result << interpreter;
                        } else {
                            dir.cdUp();
                        }
                    }
                    dir.cdUp();
                }
            }
        } while (dir.cdUp() && !(dir.isRoot() && Utils::HostOsInfo::isAnyUnixHost()));
    }
    return result;
}

void PythonSettings::saveSettings()
{
    const QList<Interpreter> &interpreters = interpreterOptionsPage().interpreters();
    const QString defaultId = interpreterOptionsPage().defaultInterpreter().id;
    const QString pylsConfiguration = pylspOptionsPage().configuration();
    const bool pylsEnabled = pylspOptionsPage().enabled();
    toSettings(Core::ICore::settings(), {interpreters, defaultId, pylsConfiguration, pylsEnabled});
    if (QTC_GUARD(settingsInstance))
        emit settingsInstance->interpretersChanged(interpreters, defaultId);
}

QList<Interpreter> PythonSettings::interpreters()
{
    return interpreterOptionsPage().interpreters();
}

Interpreter PythonSettings::defaultInterpreter()
{
    return interpreterOptionsPage().defaultInterpreter();
}

Interpreter PythonSettings::interpreter(const QString &interpreterId)
{
    const QList<Interpreter> interpreters = PythonSettings::interpreters();
    return Utils::findOrDefault(interpreters, Utils::equal(&Interpreter::id, interpreterId));
}

} // namespace Internal
} // namespace Python

#include "pythonsettings.moc"
