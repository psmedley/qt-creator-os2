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

#include "pythonutils.h"

#include "pythonproject.h"
#include "pythonrunconfiguration.h"
#include "pythonsettings.h"

#include <coreplugin/messagemanager.h>

#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <projectexplorer/target.h>

#include <utils/algorithm.h>
#include <utils/mimeutils.h>
#include <utils/qtcprocess.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace Python {
namespace Internal {

FilePath detectPython(const FilePath &documentPath)
{
    Project *project = documentPath.isEmpty() ? nullptr
                                              : SessionManager::projectForFile(documentPath);
    if (!project)
        project = SessionManager::startupProject();

    Environment env = Environment::systemEnvironment();

    if (project) {
        if (auto target = project->activeTarget()) {
            if (auto runConfig = target->activeRunConfiguration()) {
                if (auto interpreter = runConfig->aspect<InterpreterAspect>())
                    return interpreter->currentInterpreter().command;
                if (auto environmentAspect = runConfig->aspect<EnvironmentAspect>())
                    env = environmentAspect->environment();
            }
        }
    }

    // check whether this file is inside a python virtual environment
    const QList<Interpreter> venvInterpreters = PythonSettings::detectPythonVenvs(documentPath);
    if (!venvInterpreters.isEmpty())
        return venvInterpreters.first().command;

    auto defaultInterpreter = PythonSettings::defaultInterpreter().command;
    if (defaultInterpreter.exists())
        return defaultInterpreter;

    auto pythonFromPath = [=](const QString toCheck) {
        for (const FilePath &python : env.findAllInPath(toCheck)) {
            // Windows creates empty redirector files that may interfere
            if (python.exists() && python.osType() == OsTypeWindows && python.fileSize() != 0)
                return python;
        }
        return FilePath();
    };

    const FilePath fromPath3 = pythonFromPath("python3");
    if (fromPath3.exists())
        return fromPath3;

    const FilePath fromPath = pythonFromPath("python");
    if (fromPath.exists())
        return fromPath;

    return PythonSettings::interpreters().value(0).command;
}

static QStringList replImportArgs(const FilePath &pythonFile, ReplType type)
{
    using MimeTypes = QList<MimeType>;
    const MimeTypes mimeTypes = pythonFile.isEmpty() || type == ReplType::Unmodified
                                    ? MimeTypes()
                                    : mimeTypesForFileName(pythonFile.toString());
    const bool isPython = Utils::anyOf(mimeTypes, [](const MimeType &mt) {
        return mt.inherits("text/x-python") || mt.inherits("text/x-python3");
    });
    if (type == ReplType::Unmodified || !isPython)
        return {};
    const auto import = type == ReplType::Import
                            ? QString("import %1").arg(pythonFile.completeBaseName())
                            : QString("from %1 import *").arg(pythonFile.completeBaseName());
    return {"-c", QString("%1; print('Running \"%1\"')").arg(import)};
}

void openPythonRepl(QObject *parent, const FilePath &file, ReplType type)
{
    static const auto workingDir = [](const FilePath &file) {
        if (file.isEmpty()) {
            if (Project *project = SessionManager::startupProject())
                return project->projectDirectory();
            return FilePath::fromString(QDir::currentPath());
        }
        return file.absolutePath();
    };

    const auto args = QStringList{"-i"} + replImportArgs(file, type);
    auto process = new QtcProcess(parent);
    process->setTerminalMode(TerminalMode::On);
    const FilePath pythonCommand = detectPython(file);
    process->setCommand({pythonCommand, args});
    process->setWorkingDirectory(workingDir(file));
    const QString commandLine = process->commandLine().toUserOutput();
    QObject::connect(process, &QtcProcess::done, process, [process, commandLine] {
        if (process->error() != QProcess::UnknownError) {
            Core::MessageManager::writeDisrupting(QCoreApplication::translate("Python",
                  (process->error() == QProcess::FailedToStart)
                      ? "Failed to run Python (%1): \"%2\"."
                      : "Error while running Python (%1): \"%2\".")
                  .arg(commandLine, process->errorString()));
        }
        process->deleteLater();
    });
    process->start();
}

QString pythonName(const FilePath &pythonPath)
{
    static QHash<FilePath, QString> nameForPython;
    if (!pythonPath.exists())
        return {};
    QString name = nameForPython.value(pythonPath);
    if (name.isEmpty()) {
        QtcProcess pythonProcess;
        pythonProcess.setTimeoutS(2);
        pythonProcess.setCommand({pythonPath, {"--version"}});
        pythonProcess.runBlocking();
        if (pythonProcess.result() != ProcessResult::FinishedWithSuccess)
            return {};
        name = pythonProcess.allOutput().trimmed();
        nameForPython[pythonPath] = name;
    }
    return name;
}

PythonProject *pythonProjectForFile(const FilePath &pythonFile)
{
    for (Project *project : SessionManager::projects()) {
        if (auto pythonProject = qobject_cast<PythonProject *>(project)) {
            if (pythonProject->isKnownFile(pythonFile))
                return pythonProject;
        }
    }
    return nullptr;
}

} // namespace Internal
} // namespace Python
