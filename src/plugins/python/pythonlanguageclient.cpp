/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "pythonlanguageclient.h"

#include "pipsupport.h"
#include "pysideuicextracompiler.h"
#include "pythonconstants.h"
#include "pythonplugin.h"
#include "pythonproject.h"
#include "pythonrunconfiguration.h"
#include "pythonsettings.h"
#include "pythonutils.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <languageclient/languageclientinterface.h>
#include <languageclient/languageclientmanager.h>
#include <languageserverprotocol/textsynchronization.h>
#include <languageserverprotocol/workspace.h>
#include <projectexplorer/extracompiler.h>
#include <projectexplorer/session.h>
#include <projectexplorer/target.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <utils/infobar.h>
#include <utils/qtcprocess.h>
#include <utils/runextensions.h>
#include <utils/variablechooser.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>

using namespace LanguageClient;
using namespace LanguageServerProtocol;
using namespace ProjectExplorer;
using namespace Utils;

namespace Python {
namespace Internal {

static constexpr char installPylsInfoBarId[] = "Python::InstallPyls";

class PythonLanguageServerState
{
public:
    enum {
        CanNotBeInstalled,
        CanBeInstalled,
        AlreadyInstalled
    } state;
    FilePath pylsModulePath;
};

static QHash<FilePath, PyLSClient*> &pythonClients()
{
    static QHash<FilePath, PyLSClient*> clients;
    return clients;
}

FilePath getPylsModulePath(CommandLine pylsCommand)
{
    static QMutex mutex; // protect the access to the cache
    QMutexLocker locker(&mutex);
    static QMap<FilePath, FilePath> cache;
    const FilePath &modulePath = cache.value(pylsCommand.executable());
    if (!modulePath.isEmpty())
        return modulePath;

    pylsCommand.addArg("-h");

    QtcProcess pythonProcess;
    Environment env = pythonProcess.environment();
    env.set("PYTHONVERBOSE", "x");
    pythonProcess.setEnvironment(env);
    pythonProcess.setCommand(pylsCommand);
    pythonProcess.runBlocking();

    static const QString pylsInitPattern = "(.*)"
                                           + QRegularExpression::escape(
                                               QDir::toNativeSeparators("/pylsp/__init__.py"))
                                           + '$';
    static const QRegularExpression regexCached(" matches " + pylsInitPattern,
                                                QRegularExpression::MultilineOption);
    static const QRegularExpression regexNotCached(" code object from " + pylsInitPattern,
                                                   QRegularExpression::MultilineOption);

    const QString output = pythonProcess.allOutput();
    for (const auto &regex : {regexCached, regexNotCached}) {
        const QRegularExpressionMatch result = regex.match(output);
        if (result.hasMatch()) {
            const FilePath &modulePath = FilePath::fromUserInput(result.captured(1));
            cache[pylsCommand.executable()] = modulePath;
            return modulePath;
        }
    }
    return {};
}

static PythonLanguageServerState checkPythonLanguageServer(const FilePath &python)
{
    using namespace LanguageClient;
    const CommandLine pythonLShelpCommand(python, {"-m", "pylsp", "-h"});
    const FilePath &modulePath = getPylsModulePath(pythonLShelpCommand);

    QtcProcess pythonProcess;
    pythonProcess.setCommand(pythonLShelpCommand);
    pythonProcess.runBlocking();
    if (pythonProcess.allOutput().contains("Python Language Server"))
        return {PythonLanguageServerState::AlreadyInstalled, modulePath};

    pythonProcess.setCommand({python, {"-m", "pip", "-V"}});
    pythonProcess.runBlocking();
    if (pythonProcess.allOutput().startsWith("pip "))
        return {PythonLanguageServerState::CanBeInstalled, FilePath()};
    else
        return {PythonLanguageServerState::CanNotBeInstalled, FilePath()};
}


class PyLSInterface : public StdIOClientInterface
{
public:
    PyLSInterface()
        : m_extraPythonPath("QtCreator-pyls-XXXXXX")
    { }

    TemporaryDirectory m_extraPythonPath;
protected:
    void startImpl() override
    {
        if (!m_cmd.executable().needsDevice()) {
            // todo check where to put this tempdir in remote setups
            Environment env = Environment::systemEnvironment();
            env.appendOrSet("PYTHONPATH",
                            m_extraPythonPath.path().toString(),
                            OsSpecificAspects::pathListSeparator(env.osType()));
            setEnvironment(env);
        }
        StdIOClientInterface::startImpl();
    }
};

PyLSClient *clientForPython(const FilePath &python)
{
    if (auto client = pythonClients()[python])
        return client;
    auto interface = new PyLSInterface;
    interface->setCommandLine(CommandLine(python, {"-m", "pylsp"}));
    auto client = new PyLSClient(interface);
    client->setName(PyLSClient::tr("Python Language Server (%1)").arg(python.toUserOutput()));
    client->setActivateDocumentAutomatically(true);
    client->updateConfiguration();
    LanguageFilter filter;
    filter.mimeTypes = QStringList() << Constants::C_PY_MIMETYPE << Constants::C_PY3_MIMETYPE;
    client->setSupportedLanguage(filter);
    client->start();
    pythonClients()[python] = client;
    return client;
}

PyLSClient::PyLSClient(PyLSInterface *interface)
    : Client(interface)
    , m_extraCompilerOutputDir(interface->m_extraPythonPath.path())
{
    connect(this, &Client::initialized, this, &PyLSClient::updateConfiguration);
    connect(PythonSettings::instance(), &PythonSettings::pylsConfigurationChanged,
            this, &PyLSClient::updateConfiguration);
    connect(PythonSettings::instance(), &PythonSettings::pylsEnabledChanged,
            this, [this](const bool enabled){
                if (!enabled)
                    LanguageClientManager::shutdownClient(this);
            });

}

PyLSClient::~PyLSClient()
{
    pythonClients().remove(pythonClients().key(this));
}

void PyLSClient::updateConfiguration()
{
    const auto doc = QJsonDocument::fromJson(PythonSettings::pyLSConfiguration().toUtf8());
    if (doc.isArray())
        Client::updateConfiguration(doc.array());
    else if (doc.isObject())
        Client::updateConfiguration(doc.object());
}

void PyLSClient::openDocument(TextEditor::TextDocument *document)
{
    using namespace LanguageServerProtocol;
    if (reachable()) {
        const FilePath documentPath = document->filePath();
        if (PythonProject *project = pythonProjectForFile(documentPath)) {
            if (Target *target = project->activeTarget()) {
                if (auto rc = qobject_cast<PythonRunConfiguration *>(target->activeRunConfiguration()))
                    updateExtraCompilers(project, rc->extraCompilers());
            }
        } else if (isSupportedDocument(document)) {
            const FilePath workspacePath = documentPath.parentDir();
            if (!m_extraWorkspaceDirs.contains(workspacePath)) {
                WorkspaceFoldersChangeEvent event;
                event.setAdded({WorkSpaceFolder(DocumentUri::fromFilePath(workspacePath),
                                                workspacePath.fileName())});
                DidChangeWorkspaceFoldersParams params;
                params.setEvent(event);
                DidChangeWorkspaceFoldersNotification change(params);
                sendMessage(change);
                m_extraWorkspaceDirs.append(workspacePath);
            }
        }
    }
    Client::openDocument(document);
}

void PyLSClient::projectClosed(ProjectExplorer::Project *project)
{
    for (ProjectExplorer::ExtraCompiler *compiler : m_extraCompilers.value(project))
        closeExtraCompiler(compiler);
    Client::projectClosed(project);
}

void PyLSClient::updateExtraCompilers(ProjectExplorer::Project *project,
                                      const QList<PySideUicExtraCompiler *> &extraCompilers)
{
    auto oldCompilers = m_extraCompilers.take(project);
    for (PySideUicExtraCompiler *extraCompiler : extraCompilers) {
        QTC_ASSERT(extraCompiler->targets().size() == 1 , continue);
        int index = oldCompilers.indexOf(extraCompiler);
        if (index < 0) {
            m_extraCompilers[project] << extraCompiler;
            connect(extraCompiler,
                    &ExtraCompiler::contentsChanged,
                    this,
                    [this, extraCompiler](const FilePath &file) {
                        updateExtraCompilerContents(extraCompiler, file);
                    });
            if (extraCompiler->isDirty())
                static_cast<ExtraCompiler *>(extraCompiler)->run();
        } else {
            m_extraCompilers[project] << oldCompilers.takeAt(index);
        }
    }
    for (ProjectExplorer::ExtraCompiler *compiler : oldCompilers)
        closeExtraCompiler(compiler);
}

void PyLSClient::updateExtraCompilerContents(ExtraCompiler *compiler, const FilePath &file)
{
    const QString text = QString::fromUtf8(compiler->content(file));
    const FilePath target = m_extraCompilerOutputDir.pathAppended(file.fileName());

    target.writeFileContents(compiler->content(file));
}

void PyLSClient::closeExtraCompiler(ProjectExplorer::ExtraCompiler *compiler)
{
    const FilePath file = compiler->targets().first();
    m_extraCompilerOutputDir.pathAppended(file.fileName()).removeFile();
    compiler->disconnect(this);
}

PyLSClient *PyLSClient::clientForPython(const FilePath &python)
{
    return pythonClients()[python];
}

PyLSConfigureAssistant *PyLSConfigureAssistant::instance()
{
    static auto *instance = new PyLSConfigureAssistant(PythonPlugin::instance());
    return instance;
}

void PyLSConfigureAssistant::installPythonLanguageServer(const FilePath &python,
                                                         QPointer<TextEditor::TextDocument> document)
{
    document->infoBar()->removeInfo(installPylsInfoBarId);

    // Hide all install info bar entries for this python, but keep them in the list
    // so the language server will be setup properly after the installation is done.
    for (TextEditor::TextDocument *additionalDocument : m_infoBarEntries[python])
        additionalDocument->infoBar()->removeInfo(installPylsInfoBarId);

    auto install = new PipInstallTask(python);

    connect(install, &PipInstallTask::finished, this, [=](const bool success) {
        if (success) {
            if (document) {
                if (PyLSClient *client = clientForPython(python))
                    LanguageClientManager::openDocumentWithClient(document, client);
            }
        }
        install->deleteLater();
    });

    install->setPackage(PipPackage{"python-lsp-server[all]", "Python Language Server"});
    install->run();
}

void PyLSConfigureAssistant::openDocumentWithPython(const FilePath &python,
                                                    TextEditor::TextDocument *document)
{
    if (!PythonSettings::pylsEnabled())
        return;

    if (auto client = pythonClients().value(python)) {
        LanguageClientManager::openDocumentWithClient(document, client);
        return;
    }

    using CheckPylsWatcher = QFutureWatcher<PythonLanguageServerState>;
    QPointer<CheckPylsWatcher> watcher = new CheckPylsWatcher();

    // cancel and delete watcher after a 10 second timeout
    QTimer::singleShot(10000, instance(), [watcher]() {
        if (watcher) {
            watcher->cancel();
            watcher->deleteLater();
        }
    });

    connect(watcher,
            &CheckPylsWatcher::resultReadyAt,
            instance(),
            [=, document = QPointer<TextEditor::TextDocument>(document)]() {
                if (!document || !watcher)
                    return;
                instance()->handlePyLSState(python, watcher->result(), document);
                watcher->deleteLater();
            });
    watcher->setFuture(Utils::runAsync(&checkPythonLanguageServer, python));
}

void PyLSConfigureAssistant::handlePyLSState(const FilePath &python,
                                             const PythonLanguageServerState &state,
                                             TextEditor::TextDocument *document)
{
    if (state.state == PythonLanguageServerState::CanNotBeInstalled)
        return;

    resetEditorInfoBar(document);
    Utils::InfoBar *infoBar = document->infoBar();
    if (state.state == PythonLanguageServerState::CanBeInstalled
        && infoBar->canInfoBeAdded(installPylsInfoBarId)) {
        auto message = tr("Install Python language server (PyLS) for %1 (%2). "
                          "The language server provides Python specific completion and annotation.")
                           .arg(pythonName(python), python.toUserOutput());
        Utils::InfoBarEntry info(installPylsInfoBarId,
                                 message,
                                 Utils::InfoBarEntry::GlobalSuppression::Enabled);
        info.addCustomButton(tr("Install"),
                             [=]() { installPythonLanguageServer(python, document); });
        infoBar->addInfo(info);
        m_infoBarEntries[python] << document;
    } else if (state.state == PythonLanguageServerState::AlreadyInstalled) {
        if (auto client = clientForPython(python))
            LanguageClientManager::openDocumentWithClient(document, client);
    }
}

void PyLSConfigureAssistant::updateEditorInfoBars(const FilePath &python, Client *client)
{
    for (TextEditor::TextDocument *document : instance()->m_infoBarEntries.take(python)) {
        instance()->resetEditorInfoBar(document);
        if (client)
            LanguageClientManager::openDocumentWithClient(document, client);
    }
}

void PyLSConfigureAssistant::resetEditorInfoBar(TextEditor::TextDocument *document)
{
    for (QList<TextEditor::TextDocument *> &documents : m_infoBarEntries)
        documents.removeAll(document);
    document->infoBar()->removeInfo(installPylsInfoBarId);
}

PyLSConfigureAssistant::PyLSConfigureAssistant(QObject *parent)
    : QObject(parent)
{
    Core::EditorManager::instance();

    connect(Core::EditorManager::instance(),
            &Core::EditorManager::documentClosed,
            this,
            [this](Core::IDocument *document) {
                if (auto textDocument = qobject_cast<TextEditor::TextDocument *>(document))
                    resetEditorInfoBar(textDocument);
            });
}

} // namespace Internal
} // namespace Python
