/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "clangformatutils.h"

#include "clangformatconstants.h"
#include "clangformatsettings.h"

#include <coreplugin/icore.h>
#include <cppeditor/cppcodestylesettings.h>
#include <texteditor/icodestylepreferences.h>
#include <texteditor/tabsettings.h>
#include <texteditor/texteditorsettings.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <utils/qtcassert.h>

#include <QCryptographicHash>

using namespace clang;
using namespace format;
using namespace llvm;
using namespace CppEditor;
using namespace ProjectExplorer;
using namespace TextEditor;
using namespace Utils;

namespace ClangFormat {

clang::format::FormatStyle qtcStyle()
{
    clang::format::FormatStyle style = getLLVMStyle();
    style.Language = FormatStyle::LK_Cpp;
    style.AccessModifierOffset = -4;
    style.AlignAfterOpenBracket = FormatStyle::BAS_Align;
#if LLVM_VERSION_MAJOR >= 15
    style.AlignConsecutiveAssignments = {false};
    style.AlignConsecutiveDeclarations = {false};
#elif LLVM_VERSION_MAJOR >= 12
    style.AlignConsecutiveAssignments = FormatStyle::ACS_None;
    style.AlignConsecutiveDeclarations = FormatStyle::ACS_None;
#else
    style.AlignConsecutiveAssignments = false;
    style.AlignConsecutiveDeclarations = false;
#endif
    style.AlignEscapedNewlines = FormatStyle::ENAS_DontAlign;
#if LLVM_VERSION_MAJOR >= 11
    style.AlignOperands = FormatStyle::OAS_Align;
#else
    style.AlignOperands = true;
#endif
    style.AlignTrailingComments = true;
    style.AllowAllParametersOfDeclarationOnNextLine = true;
#if LLVM_VERSION_MAJOR >= 10
    style.AllowShortBlocksOnASingleLine = FormatStyle::SBS_Never;
#else
    style.AllowShortBlocksOnASingleLine = false;
#endif
    style.AllowShortCaseLabelsOnASingleLine = false;
    style.AllowShortFunctionsOnASingleLine = FormatStyle::SFS_Inline;
#if LLVM_VERSION_MAJOR >= 9
    style.AllowShortIfStatementsOnASingleLine = FormatStyle::SIS_Never;
#else
    style.AllowShortIfStatementsOnASingleLine = false;
#endif
    style.AllowShortLoopsOnASingleLine = false;
    style.AlwaysBreakAfterReturnType = FormatStyle::RTBS_None;
    style.AlwaysBreakBeforeMultilineStrings = false;
    style.AlwaysBreakTemplateDeclarations = FormatStyle::BTDS_Yes;
    style.BinPackArguments = false;
    style.BinPackParameters = false;
    style.BraceWrapping.AfterClass = true;
#if LLVM_VERSION_MAJOR >= 10
    style.BraceWrapping.AfterControlStatement = FormatStyle::BWACS_Never;
#else
    style.BraceWrapping.AfterControlStatement = false;
#endif
    style.BraceWrapping.AfterEnum = false;
    style.BraceWrapping.AfterFunction = true;
    style.BraceWrapping.AfterNamespace = false;
    style.BraceWrapping.AfterObjCDeclaration = false;
    style.BraceWrapping.AfterStruct = true;
    style.BraceWrapping.AfterUnion = false;
    style.BraceWrapping.BeforeCatch = false;
    style.BraceWrapping.BeforeElse = false;
    style.BraceWrapping.IndentBraces = false;
    style.BraceWrapping.SplitEmptyFunction = false;
    style.BraceWrapping.SplitEmptyRecord = false;
    style.BraceWrapping.SplitEmptyNamespace = false;
    style.BreakBeforeBinaryOperators = FormatStyle::BOS_All;
    style.BreakBeforeBraces = FormatStyle::BS_Custom;
    style.BreakBeforeTernaryOperators = true;
    style.BreakConstructorInitializers = FormatStyle::BCIS_BeforeComma;
    style.BreakAfterJavaFieldAnnotations = false;
    style.BreakStringLiterals = true;
    style.ColumnLimit = 100;
    style.CommentPragmas = "^ IWYU pragma:";
    style.CompactNamespaces = false;
    style.ConstructorInitializerAllOnOneLineOrOnePerLine = false;
    style.ConstructorInitializerIndentWidth = 4;
    style.ContinuationIndentWidth = 4;
    style.Cpp11BracedListStyle = true;
    style.DerivePointerAlignment = false;
    style.DisableFormat = false;
    style.ExperimentalAutoDetectBinPacking = false;
    style.FixNamespaceComments = true;
    style.ForEachMacros = {"forever", "foreach", "Q_FOREACH", "BOOST_FOREACH"};
#if LLVM_VERSION_MAJOR >= 12
    style.IncludeStyle.IncludeCategories = {{"^<Q.*", 200, 200, true}};
#else
    style.IncludeStyle.IncludeCategories = {{"^<Q.*", 200, 200}};
#endif
    style.IncludeStyle.IncludeIsMainRegex = "(Test)?$";
    style.IndentCaseLabels = false;
    style.IndentWidth = 4;
    style.IndentWrappedFunctionNames = false;
    style.JavaScriptQuotes = FormatStyle::JSQS_Leave;
    style.JavaScriptWrapImports = true;
    style.KeepEmptyLinesAtTheStartOfBlocks = false;
    // Do not add QT_BEGIN_NAMESPACE/QT_END_NAMESPACE as this will indent lines in between.
    style.MacroBlockBegin = "";
    style.MacroBlockEnd = "";
    style.MaxEmptyLinesToKeep = 1;
    style.NamespaceIndentation = FormatStyle::NI_None;
    style.ObjCBlockIndentWidth = 4;
    style.ObjCSpaceAfterProperty = false;
    style.ObjCSpaceBeforeProtocolList = true;
    style.PenaltyBreakAssignment = 150;
    style.PenaltyBreakBeforeFirstCallParameter = 300;
    style.PenaltyBreakComment = 500;
    style.PenaltyBreakFirstLessLess = 400;
    style.PenaltyBreakString = 600;
    style.PenaltyExcessCharacter = 50;
    style.PenaltyReturnTypeOnItsOwnLine = 300;
    style.PointerAlignment = FormatStyle::PAS_Right;
    style.ReflowComments = false;
#if LLVM_VERSION_MAJOR >= 13
    style.SortIncludes = FormatStyle::SI_CaseSensitive;
#else
    style.SortIncludes = true;
#endif
    style.SortUsingDeclarations = true;
    style.SpaceAfterCStyleCast = true;
    style.SpaceAfterTemplateKeyword = false;
    style.SpaceBeforeAssignmentOperators = true;
    style.SpaceBeforeParens = FormatStyle::SBPO_ControlStatements;
    style.SpaceInEmptyParentheses = false;
    style.SpacesBeforeTrailingComments = 1;
#if LLVM_VERSION_MAJOR >= 13
    style.SpacesInAngles = FormatStyle::SIAS_Never;
#else
    style.SpacesInAngles = false;
#endif
    style.SpacesInContainerLiterals = false;
    style.SpacesInCStyleCastParentheses = false;
    style.SpacesInParentheses = false;
    style.SpacesInSquareBrackets = false;
    style.StatementMacros.emplace_back("Q_OBJECT");
    style.StatementMacros.emplace_back("QT_BEGIN_NAMESPACE");
    style.StatementMacros.emplace_back("QT_END_NAMESPACE");
    style.Standard = FormatStyle::LS_Cpp11;
    style.TabWidth = 4;
    style.UseTab = FormatStyle::UT_Never;
    return style;
}

static bool useGlobalOverriddenSettings()
{
    return ClangFormatSettings::instance().overrideDefaultFile();
}

QString currentProjectUniqueId()
{
    const Project *project = SessionManager::startupProject();
    if (!project)
        return QString();

    return QString::fromUtf8(QCryptographicHash::hash(project->projectFilePath().toString().toUtf8(),
                                                      QCryptographicHash::Md5)
                                 .toHex(0));
}

void saveStyleToFile(clang::format::FormatStyle style, Utils::FilePath filePath)
{
    std::string styleStr = clang::format::configurationAsText(style);

    // workaround: configurationAsText() add comment "# " before BasedOnStyle line
    const int pos = styleStr.find("# BasedOnStyle");
    if (pos != int(std::string::npos))
        styleStr.erase(pos, 2);
    styleStr.append("\n");
    filePath.writeFileContents(QByteArray::fromStdString(styleStr));
}

static bool useProjectOverriddenSettings()
{
    const Project *project = SessionManager::startupProject();
    return project ? project->namedSettings(Constants::OVERRIDE_FILE_ID).toBool() : false;
}

static Utils::FilePath globalPath()
{
    return Core::ICore::userResourcePath();
}

static Utils::FilePath projectPath()
{
    const Project *project = SessionManager::startupProject();
    if (project)
        return globalPath().pathAppended("clang-format/" + currentProjectUniqueId());

    return Utils::FilePath();
}

static QString findConfig(Utils::FilePath fileName)
{
    QDir parentDir(fileName.parentDir().toString());
    while (!parentDir.exists(Constants::SETTINGS_FILE_NAME)
           && !parentDir.exists(Constants::SETTINGS_FILE_ALT_NAME)) {
        if (!parentDir.cdUp())
            return QString();
    }

    if (parentDir.exists(Constants::SETTINGS_FILE_NAME))
        return parentDir.filePath(Constants::SETTINGS_FILE_NAME);
    return parentDir.filePath(Constants::SETTINGS_FILE_ALT_NAME);
}

static QString configForFile(Utils::FilePath fileName, bool checkForSettings)
{
    QDir overrideDir;
    if (!checkForSettings || useProjectOverriddenSettings()) {
        overrideDir.setPath(projectPath().toString());
        if (!overrideDir.isEmpty() && overrideDir.exists(Constants::SETTINGS_FILE_NAME))
            return overrideDir.filePath(Constants::SETTINGS_FILE_NAME);
    }

    if (!checkForSettings || useGlobalOverriddenSettings()) {
        overrideDir.setPath(globalPath().toString());
        if (!overrideDir.isEmpty() && overrideDir.exists(Constants::SETTINGS_FILE_NAME))
            return overrideDir.filePath(Constants::SETTINGS_FILE_NAME);
    }

    return findConfig(fileName);
}

QString configForFile(Utils::FilePath fileName)
{
    return configForFile(fileName, true);
}

Utils::FilePath assumedPathForConfig(const QString &configFile)
{
    Utils::FilePath fileName = Utils::FilePath::fromString(configFile);
    return fileName.parentDir().pathAppended("test.cpp");
}

static clang::format::FormatStyle constructStyle(const QByteArray &baseStyle = QByteArray())
{
    if (!baseStyle.isEmpty()) {
        // Try to get the style for this base style.
        Expected<FormatStyle> style = getStyle(baseStyle.toStdString(),
                                               "dummy.cpp",
                                               baseStyle.toStdString());
        if (style)
            return *style;

        handleAllErrors(style.takeError(), [](const ErrorInfoBase &) {
            // do nothing
        });
        // Fallthrough to the default style.
    }

    return qtcStyle();
}

void createStyleFileIfNeeded(bool isGlobal)
{
    const FilePath path = isGlobal ? globalPath() : projectPath();
    const FilePath configFile = path / Constants::SETTINGS_FILE_NAME;

    if (configFile.exists())
        return;

    QDir().mkpath(path.toString());
    if (!isGlobal) {
        const Project *project = SessionManager::startupProject();
        FilePath possibleProjectConfig = project->rootProjectDirectory()
                / Constants::SETTINGS_FILE_NAME;
        if (possibleProjectConfig.exists()) {
            // Just copy th .clang-format if current project has one.
            possibleProjectConfig.copyFile(configFile);
            return;
        }
    }

    std::fstream newStyleFile(configFile.toString().toStdString(), std::fstream::out);
    if (newStyleFile.is_open()) {
        newStyleFile << clang::format::configurationAsText(constructStyle());
        newStyleFile.close();
    }
}

static QByteArray configBaseStyleName(const QString &configFile)
{
    if (configFile.isEmpty())
        return QByteArray();

    QFile config(configFile);
    if (!config.open(QIODevice::ReadOnly))
        return QByteArray();

    const QByteArray content = config.readAll();
    const char basedOnStyle[] = "BasedOnStyle:";
    int basedOnStyleIndex = content.indexOf(basedOnStyle);
    if (basedOnStyleIndex < 0)
        return QByteArray();

    basedOnStyleIndex += sizeof(basedOnStyle) - 1;
    const int endOfLineIndex = content.indexOf('\n', basedOnStyleIndex);
    return content
        .mid(basedOnStyleIndex, endOfLineIndex < 0 ? -1 : endOfLineIndex - basedOnStyleIndex)
        .trimmed();
}

static clang::format::FormatStyle styleForFile(Utils::FilePath fileName, bool checkForSettings)
{
    QString configFile = configForFile(fileName, checkForSettings);
    if (configFile.isEmpty()) {
        // If no configuration is found create a global one (if it does not yet exist) and use it.
        createStyleFileIfNeeded(true);
        configFile = globalPath().pathAppended(Constants::SETTINGS_FILE_NAME).toString();
    }

    fileName = assumedPathForConfig(configFile);
    Expected<FormatStyle> style = format::getStyle("file",
                                                   fileName.toString().toStdString(),
                                                   "none");
    if (style)
        return *style;

    handleAllErrors(style.takeError(), [](const ErrorInfoBase &) {
        // do nothing
    });

    return constructStyle(configBaseStyleName(configFile));
}

clang::format::FormatStyle styleForFile(Utils::FilePath fileName)
{
    return styleForFile(fileName, true);
}

void addQtcStatementMacros(clang::format::FormatStyle &style)
{
    static const std::vector<std::string> macros = {"Q_OBJECT",
                                                    "QT_BEGIN_NAMESPACE",
                                                    "QT_END_NAMESPACE"};
    for (const std::string &macro : macros) {
        if (std::find(style.StatementMacros.begin(), style.StatementMacros.end(), macro)
            == style.StatementMacros.end())
            style.StatementMacros.emplace_back(macro);
    }
}

Utils::FilePath filePathToCurrentSettings(const TextEditor::ICodeStylePreferences *codeStyle)
{
    return Core::ICore::userResourcePath() / "clang-format/"
           / Utils::FileUtils::fileSystemFriendlyName(codeStyle->displayName())
           / QLatin1String(Constants::SETTINGS_FILE_NAME);
}

std::string readFile(const QString &path)
{
    const std::string defaultStyle = clang::format::configurationAsText(qtcStyle());

    QFile file(path);
    if (!file.open(QFile::ReadOnly))
        return defaultStyle;

    const std::string content = file.readAll().toStdString();
    file.close();

    clang::format::FormatStyle style;
    style.Language = clang::format::FormatStyle::LK_Cpp;
    const std::error_code error = clang::format::parseConfiguration(content, &style);
    QTC_ASSERT(error.value() == static_cast<int>(ParseError::Success), return defaultStyle);

    addQtcStatementMacros(style);
    std::string settings = clang::format::configurationAsText(style);

    // Needed workaround because parseConfiguration remove BasedOnStyle field
    // ToDo: standardize this behavior for future
    const size_t index = content.find("BasedOnStyle");
    if (index != std::string::npos) {
        const size_t size = content.find("\n", index) - index;
        const size_t insert_index = settings.find("\n");
        settings.insert(insert_index, "\n" + content.substr(index, size));
    }

    return settings;
}

std::string currentProjectConfigText()
{
    const QString configPath = projectPath().pathAppended(Constants::SETTINGS_FILE_NAME).toString();
    return readFile(configPath);
}

std::string currentGlobalConfigText()
{
    const QString configPath = globalPath().pathAppended(Constants::SETTINGS_FILE_NAME).toString();
    return readFile(configPath);
}

clang::format::FormatStyle currentProjectStyle()
{
    return styleForFile(projectPath().pathAppended(Constants::SAMPLE_FILE_NAME), false);
}

clang::format::FormatStyle currentGlobalStyle()
{
    return styleForFile(globalPath().pathAppended(Constants::SAMPLE_FILE_NAME), false);
}
} // namespace ClangFormat
