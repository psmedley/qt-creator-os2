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

#include "executableinfo.h"

#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <utils/environment.h>
#include <utils/qtcprocess.h>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

using namespace Utils;

namespace ClangTools {
namespace Internal {

static QString runExecutable(const Utils::CommandLine &commandLine, QueryFailMode queryFailMode)
{
    if (commandLine.executable().isEmpty() || !commandLine.executable().toFileInfo().isExecutable())
        return {};

    QtcProcess cpp;
    Environment env = Environment::systemEnvironment();
    env.setupEnglishOutput();
    cpp.setEnvironment(env);
    cpp.setCommand(commandLine);

    cpp.runBlocking();
    if (cpp.result() != ProcessResult::FinishedWithSuccess
            && (queryFailMode == QueryFailMode::Noisy
            || cpp.result() != ProcessResult::FinishedWithError)) {
        Core::MessageManager::writeFlashing(cpp.exitMessage());
        Core::MessageManager::writeFlashing(QString::fromUtf8(cpp.allRawOutput()));
        return {};
    }

    return cpp.cleanedStdOut();
}

static QStringList queryClangTidyChecks(const FilePath &executable,
                                        const QString &checksArgument)
{
    QStringList arguments = QStringList("-list-checks");
    if (!checksArgument.isEmpty())
        arguments.prepend(checksArgument);

    const CommandLine commandLine(executable, arguments);
    QString output = runExecutable(commandLine, QueryFailMode::Noisy);
    if (output.isEmpty())
        return {};

    // Expected output is (clang-tidy 8.0):
    //   Enabled checks:
    //       abseil-duration-comparison
    //       abseil-duration-division
    //       abseil-duration-factory-float
    //       ...

    QTextStream stream(&output);
    QString line = stream.readLine();
    if (!line.startsWith("Enabled checks:"))
        return {};

    QStringList checks;
    while (!stream.atEnd()) {
        const QString candidate = stream.readLine().trimmed();
        if (!candidate.isEmpty())
            checks << candidate;
    }

    return checks;
}

static ClazyChecks querySupportedClazyChecks(const FilePath &executablePath)
{
    static const QString queryFlag = "-supported-checks-json";
    QString jsonOutput = runExecutable(CommandLine(executablePath, {queryFlag}),
                                       QueryFailMode::Noisy);

    // Some clazy 1.6.x versions have a bug where they expect an argument after the
    // option.
    if (jsonOutput.isEmpty())
        jsonOutput = runExecutable(CommandLine(executablePath, {queryFlag, "dummy"}),
                                   QueryFailMode::Noisy);
    if (jsonOutput.isEmpty())
        return {};

    // Expected output is (clazy-standalone 1.6):
    //   {
    //       "available_categories" : ["readability", "qt4", "containers", ... ],
    //       "checks" : [
    //           {
    //               "name"  : "qt-keywords",
    //               "level" : -1,
    //               "fixits" : [ { "name" : "qt-keywords" } ]
    //           },
    //           ...
    //           {
    //               "name"  : "inefficient-qlist",
    //               "level" : -1,
    //               "categories" : ["containers", "performance"],
    //               "visits_decls" : true
    //           },
    //           ...
    //       ]
    //   }

    ClazyChecks infos;

    const QJsonDocument document = QJsonDocument::fromJson(jsonOutput.toUtf8());
    if (document.isNull())
        return {};
    const QJsonArray checksArray = document.object()["checks"].toArray();

    for (const QJsonValue &item: checksArray) {
        const QJsonObject checkObject = item.toObject();

        ClazyCheck info;
        info.name = checkObject["name"].toString().trimmed();
        if (info.name.isEmpty())
            continue;
        info.level = checkObject["level"].toInt();
        for (const QJsonValue &item : checkObject["categories"].toArray())
            info.topics.append(item.toString().trimmed());

        infos << info;
    }

    return infos;
}

ClangTidyInfo::ClangTidyInfo(const FilePath &executablePath)
    : defaultChecks(queryClangTidyChecks(executablePath, {}))
    , supportedChecks(queryClangTidyChecks(executablePath, "-checks=*"))
{}

ClazyStandaloneInfo ClazyStandaloneInfo::getInfo(const FilePath &executablePath)
{
    const QDateTime timeStamp = executablePath.lastModified();
    const auto it = cache.find(executablePath);
    if (it == cache.end()) {
        const ClazyStandaloneInfo info(executablePath);
        cache.insert(executablePath, qMakePair(timeStamp, info));
        return info;
    }
    if (it->first != timeStamp) {
        it->first = timeStamp;
        it->second = ClazyStandaloneInfo::getInfo(executablePath);
    }
    return it->second;
}

ClazyStandaloneInfo::ClazyStandaloneInfo(const FilePath &executablePath)
    : defaultChecks(queryClangTidyChecks(executablePath, {})) // Yup, behaves as clang-tidy.
    , supportedChecks(querySupportedClazyChecks(executablePath))
{
    QString output = runExecutable({executablePath, {"--version"}}, QueryFailMode::Silent);
    QTextStream stream(&output);
    while (!stream.atEnd()) {
        // It's just "clazy version " right now, but let's be prepared for someone adding a colon
        // later on.
        static const QStringList versionPrefixes{"clazy version ", "clazy version: "};
        const QString line = stream.readLine().simplified();
        for (const QString &prefix : versionPrefixes) {
            if (line.startsWith(prefix)) {
                version = QVersionNumber::fromString(line.mid(prefix.length()));
                break;
            }
        }
    }
}

static FilePath queryResourceDir(const FilePath &clangToolPath)
{
    QString output = runExecutable(CommandLine(clangToolPath, {"someFilePath", "--",
                                                               "-print-resource-dir"}),
                                   QueryFailMode::Silent);

    // Expected output is (clang-tidy 10):
    //   lib/clang/10.0.1
    //   Error while trying to load a compilation database:
    //   ...

    // Parse
    QTextStream stream(&output);
    const QString path = clangToolPath.parentDir().parentDir()
            .pathAppended(stream.readLine()).toString();
    const auto filePath = FilePath::fromUserInput(QDir::cleanPath(path));
    if (filePath.exists())
        return filePath;
    return {};
}

QString queryVersion(const FilePath &clangToolPath, QueryFailMode failMode)
{
    QString output = runExecutable(CommandLine(clangToolPath, {"--version"}), failMode);
    QTextStream stream(&output);
    while (!stream.atEnd()) {
        static const QStringList versionPrefixes{"LLVM version ", "clang version: "};
        const QString line = stream.readLine().simplified();
        for (const QString &prefix : versionPrefixes) {
            auto idx = line.indexOf(prefix);
            if (idx >= 0)
                return line.mid(idx + prefix.length());
        }
    }
    return {};
}

QPair<FilePath, QString> getClangIncludeDirAndVersion(const FilePath &clangToolPath)
{
    const FilePath dynamicResourceDir = queryResourceDir(clangToolPath);
    const QString dynamicVersion = queryVersion(clangToolPath, QueryFailMode::Noisy);
    if (dynamicResourceDir.isEmpty() || dynamicVersion.isEmpty())
        return qMakePair(FilePath::fromString(CLANG_INCLUDE_DIR), QString(CLANG_VERSION));
    return qMakePair(dynamicResourceDir + "/include", dynamicVersion);
}

QHash<Utils::FilePath, QPair<QDateTime, ClazyStandaloneInfo>> ClazyStandaloneInfo::cache;

} // namespace Internal
} // namespace ClangTools
