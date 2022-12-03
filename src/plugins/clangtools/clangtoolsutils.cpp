/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "clangtoolsutils.h"

#include "clangtool.h"
#include "clangtoolsconstants.h"
#include "clangtoolsdiagnostic.h"
#include "clangtoolssettings.h"

#include <coreplugin/icore.h>
#include <cppeditor/cppeditorconstants.h>
#include <cppeditor/cpptoolsreuse.h>
#include <projectexplorer/projectexplorerconstants.h>

#include <utils/checkablemessagebox.h>
#include <utils/environment.h>
#include <utils/filepath.h>
#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>

#include <cppeditor/clangdiagnosticconfigsmodel.h>

using namespace CppEditor;
using namespace Utils;

namespace ClangTools {
namespace Internal {

static QString lineColumnString(const Debugger::DiagnosticLocation &location)
{
    return QString("%1:%2").arg(QString::number(location.line), QString::number(location.column));
}

static QString fixitStatus(FixitStatus status)
{
    switch (status) {
    case FixitStatus::NotAvailable:
        return QCoreApplication::translate("ClangToolsDiagnosticModel", "No Fixits");
    case FixitStatus::NotScheduled:
        return QCoreApplication::translate("ClangToolsDiagnosticModel", "Not Scheduled");
    case FixitStatus::Invalidated:
        return QCoreApplication::translate("ClangToolsDiagnosticModel", "Invalidated");
    case FixitStatus::Scheduled:
        return QCoreApplication::translate("ClangToolsDiagnosticModel", "Scheduled");
    case FixitStatus::FailedToApply:
        return QCoreApplication::translate("ClangToolsDiagnosticModel", "Failed to Apply");
    case FixitStatus::Applied:
        return QCoreApplication::translate("ClangToolsDiagnosticModel", "Applied");
    }
    return QString();
}

QString createDiagnosticToolTipString(
    const Diagnostic &diagnostic,
    Utils::optional<FixitStatus> status,
    bool showSteps)
{
    using StringPair = QPair<QString, QString>;
    QList<StringPair> lines;

    if (!diagnostic.category.isEmpty()) {
        lines << qMakePair(
                     QCoreApplication::translate("ClangTools::Diagnostic", "Category:"),
                     diagnostic.category.toHtmlEscaped());
    }

    if (!diagnostic.type.isEmpty()) {
        lines << qMakePair(
                     QCoreApplication::translate("ClangTools::Diagnostic", "Type:"),
                     diagnostic.type.toHtmlEscaped());
    }

    if (!diagnostic.description.isEmpty()) {
        lines << qMakePair(
                     QCoreApplication::translate("ClangTools::Diagnostic", "Description:"),
                     diagnostic.description.toHtmlEscaped());
    }

    lines << qMakePair(
                 QCoreApplication::translate("ClangTools::Diagnostic", "Location:"),
                 createFullLocationString(diagnostic.location));

    if (status) {
        lines << qMakePair(QCoreApplication::translate("ClangTools::Diagnostic", "Fixit status:"),
                           fixitStatus(*status));
    }

    if (showSteps && !diagnostic.explainingSteps.isEmpty()) {
        StringPair steps;
        steps.first = QCoreApplication::translate("ClangTools::Diagnostic", "Steps:");
        for (const ExplainingStep &step : diagnostic.explainingSteps) {
            if (!steps.second.isEmpty())
                steps.second += "<br>";
            steps.second += QString("%1:%2: %3")
                    .arg(step.location.filePath.toUserOutput(),
                         lineColumnString(step.location),
                         step.message);
        }
        lines << steps;
    }

    const QString url = documentationUrl(diagnostic.name);
    if (!url.isEmpty()) {
        lines << qMakePair(QCoreApplication::translate("ClangTools::Diagnostic", "Documentation:"),
                           QString("<a href=\"%1\">%1</a>").arg(url));
    }

    QString html = QLatin1String("<html>"
                                 "<head>"
                                 "<style>dt { font-weight:bold; } dd { font-family: monospace; }</style>"
                                 "</head>\n"
                                 "<body><dl>");

    for (const StringPair &pair : qAsConst(lines)) {
        html += QLatin1String("<dt>");
        html += pair.first;
        html += QLatin1String("</dt><dd>");
        html += pair.second;
        html += QLatin1String("</dd>\n");
    }
    html += QLatin1String("</dl></body></html>");
    return html;
}

QString createFullLocationString(const Debugger::DiagnosticLocation &location)
{
    return location.filePath.toUserOutput() + QLatin1Char(':') + QString::number(location.line)
            + QLatin1Char(':') + QString::number(location.column);
}

QString hintAboutBuildBeforeAnalysis()
{
    return ClangTool::tr(
        "In general, the project should be built before starting the analysis to ensure that the "
        "code to analyze is valid.<br/><br/>"
        "Building the project might also run code generators that update the source files as "
        "necessary.");
}

void showHintAboutBuildBeforeAnalysis()
{
    Utils::CheckableMessageBox::doNotShowAgainInformation(
        Core::ICore::dialogParent(),
        ClangTool::tr("Info About Build the Project Before Analysis"),
        hintAboutBuildBeforeAnalysis(),
        Core::ICore::settings(),
        "ClangToolsDisablingBuildBeforeAnalysisHint");
}

FilePath shippedClangTidyExecutable()
{
    const FilePath shippedExecutable = Core::ICore::clangTidyExecutable(CLANG_BINDIR);
    if (shippedExecutable.isExecutableFile())
        return shippedExecutable;
    return {};
}

FilePath shippedClazyStandaloneExecutable()
{
    const FilePath shippedExecutable = Core::ICore::clazyStandaloneExecutable(CLANG_BINDIR);
    if (shippedExecutable.isExecutableFile())
        return shippedExecutable;
    return {};
}

FilePath fullPath(const FilePath &executable)
{
    FilePath candidate = executable;
    const bool hasSuffix = candidate.endsWith(QTC_HOST_EXE_SUFFIX);

    if (candidate.isAbsolutePath()) {
        if (!hasSuffix)
            candidate = candidate.withExecutableSuffix();
    } else {
        const Environment environment = Environment::systemEnvironment();
        const FilePath expandedPath = environment.searchInPath(candidate.toString());
        if (!expandedPath.isEmpty())
            candidate = expandedPath;
    }

    return candidate;
}

static FilePath findValidExecutable(const FilePaths &candidates)
{
    for (const FilePath &candidate : candidates) {
        const FilePath expandedPath = fullPath(candidate);
        if (expandedPath.isExecutableFile())
            return expandedPath;
    }

    return {};
}

FilePath clangTidyFallbackExecutable()
{
    return findValidExecutable({
        shippedClangTidyExecutable(),
        Constants::CLANG_TIDY_EXECUTABLE_NAME,
    });
}

FilePath clangTidyExecutable()
{
    const FilePath fromSettings = ClangToolsSettings::instance()->clangTidyExecutable();
    if (!fromSettings.isEmpty())
        return fullPath(fromSettings);
    return clangTidyFallbackExecutable();
}

FilePath clazyStandaloneFallbackExecutable()
{
    return findValidExecutable({
        shippedClazyStandaloneExecutable(),
        Constants::CLAZY_STANDALONE_EXECUTABLE_NAME,
    });
}

FilePath clazyStandaloneExecutable()
{
    const FilePath fromSettings = ClangToolsSettings::instance()->clazyStandaloneExecutable();
    if (!fromSettings.isEmpty())
        return fullPath(fromSettings);
    return clazyStandaloneFallbackExecutable();
}

static void addBuiltinConfigs(ClangDiagnosticConfigsModel &model)
{
    model.appendOrUpdate(builtinConfig());
}

ClangDiagnosticConfig builtinConfig()
{
    ClangDiagnosticConfig config;
    config.setId(Constants::DIAG_CONFIG_TIDY_AND_CLAZY);
    config.setDisplayName(QCoreApplication::translate("ClangDiagnosticConfigsModel",
                                                      "Default Clang-Tidy and Clazy checks"));
    config.setIsReadOnly(true);
    config.setClangOptions({"-w"}); // Do not emit any clang-only warnings
    config.setClangTidyMode(ClangDiagnosticConfig::TidyMode::UseDefaultChecks);
    config.setClazyMode(ClangDiagnosticConfig::ClazyMode::UseDefaultChecks);
    return config;
}

ClangDiagnosticConfigsModel diagnosticConfigsModel(const ClangDiagnosticConfigs &customConfigs)
{
    ClangDiagnosticConfigsModel model;
    addBuiltinConfigs(model);
    for (const ClangDiagnosticConfig &config : customConfigs)
        model.appendOrUpdate(config);
    return model;
}

ClangDiagnosticConfigsModel diagnosticConfigsModel()
{
    return Internal::diagnosticConfigsModel(ClangToolsSettings::instance()->diagnosticConfigs());
}

QString documentationUrl(const QString &checkName)
{
    QString name = checkName;
    const QString clangPrefix = "clang-diagnostic-";
    if (name.startsWith(clangPrefix))
        return {}; // No documentation for this.

    QString url;
    const QString clazyPrefix = "clazy-";
    const QString clangStaticAnalyzerPrefix = "clang-analyzer-core.";
    if (name.startsWith(clazyPrefix)) {
        name = checkName.mid(clazyPrefix.length());
        url = clazyDocUrl(name);
    } else if (name.startsWith(clangStaticAnalyzerPrefix)) {
        url = CppEditor::Constants::CLANG_STATIC_ANALYZER_DOCUMENTATION_URL;
    } else {
        url = clangTidyDocUrl(name);
    }

    return url;
}

ClangDiagnosticConfig diagnosticConfig(const Utils::Id &diagConfigId)
{
    const ClangDiagnosticConfigsModel configs = diagnosticConfigsModel();
    QTC_ASSERT(configs.hasConfigWithId(diagConfigId), return ClangDiagnosticConfig());
    return configs.configWithId(diagConfigId);
}

static QStringList extraOptions(const char *envVar)
{
    if (!qEnvironmentVariableIsSet(envVar))
        return QStringList();
    QString arguments = qEnvironmentVariable(envVar);
    return Utils::ProcessArgs::splitArgs(arguments);
}

QStringList extraClangToolsPrependOptions()
{
    constexpr char csaPrependOptions[] = "QTC_CLANG_CSA_CMD_PREPEND";
    constexpr char toolsPrependOptions[] = "QTC_CLANG_TOOLS_CMD_PREPEND";
    static const QStringList options = extraOptions(csaPrependOptions)
                                       + extraOptions(toolsPrependOptions);
    if (!options.isEmpty())
        qWarning() << "ClangTools options are prepended with " << options.toVector();
    return options;
}

QStringList extraClangToolsAppendOptions()
{
    constexpr char csaAppendOptions[] = "QTC_CLANG_CSA_CMD_APPEND";
    constexpr char toolsAppendOptions[] = "QTC_CLANG_TOOLS_CMD_APPEND";
    static const QStringList options = extraOptions(csaAppendOptions)
                                       + extraOptions(toolsAppendOptions);
    if (!options.isEmpty())
        qWarning() << "ClangTools options are appended with " << options.toVector();
    return options;
}

QString clangTidyDocUrl(const QString &check)
{
    QVersionNumber version = ClangToolsSettings::clangTidyVersion();
    version = QVersionNumber(version.majorVersion(), 0, 0);
    if (version == QVersionNumber(0))
        version = QVersionNumber(12);
    static const char urlTemplate[]
            = "https://releases.llvm.org/%1/tools/clang/tools/extra/docs/clang-tidy/checks/%2.html";
    return QString::fromLatin1(urlTemplate).arg(version.toString(), check);
}

QString clazyDocUrl(const QString &check)
{
    QVersionNumber version = ClangToolsSettings::clazyVersion();
    if (!version.isNull())
        version = QVersionNumber(version.majorVersion(), version.minorVersion());
    const QString versionString = version.isNull() ? "master" : version.toString();
    static const char urlTemplate[]
            = "https://github.com/KDE/clazy/blob/%1/docs/checks/README-%2.md";
    return QString::fromLatin1(urlTemplate).arg(versionString, check);
}

} // namespace Internal
} // namespace ClangTools
