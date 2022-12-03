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

#include "cppcodestylesettings.h"

#include "cppcodestylepreferences.h"
#include "cppeditorconstants.h"
#include "cpptoolssettings.h"

#include <projectexplorer/editorconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projecttree.h>

#include <texteditor/tabsettings.h>

#include <cplusplus/Overview.h>

#include <utils/qtcassert.h>
#include <utils/settingsutils.h>

static const char indentBlockBracesKey[] = "IndentBlockBraces";
static const char indentBlockBodyKey[] = "IndentBlockBody";
static const char indentClassBracesKey[] = "IndentClassBraces";
static const char indentEnumBracesKey[] = "IndentEnumBraces";
static const char indentNamespaceBracesKey[] = "IndentNamespaceBraces";
static const char indentNamespaceBodyKey[] = "IndentNamespaceBody";
static const char indentAccessSpecifiersKey[] = "IndentAccessSpecifiers";
static const char indentDeclarationsRelativeToAccessSpecifiersKey[] = "IndentDeclarationsRelativeToAccessSpecifiers";
static const char indentFunctionBodyKey[] = "IndentFunctionBody";
static const char indentFunctionBracesKey[] = "IndentFunctionBraces";
static const char indentSwitchLabelsKey[] = "IndentSwitchLabels";
static const char indentStatementsRelativeToSwitchLabelsKey[] = "IndentStatementsRelativeToSwitchLabels";
static const char indentBlocksRelativeToSwitchLabelsKey[] = "IndentBlocksRelativeToSwitchLabels";
static const char indentControlFlowRelativeToSwitchLabelsKey[] = "IndentControlFlowRelativeToSwitchLabels";
static const char bindStarToIdentifierKey[] = "BindStarToIdentifier";
static const char bindStarToTypeNameKey[] = "BindStarToTypeName";
static const char bindStarToLeftSpecifierKey[] = "BindStarToLeftSpecifier";
static const char bindStarToRightSpecifierKey[] = "BindStarToRightSpecifier";
static const char extraPaddingForConditionsIfConfusingAlignKey[] = "ExtraPaddingForConditionsIfConfusingAlign";
static const char alignAssignmentsKey[] = "AlignAssignments";
static const char shortGetterNameKey[] = "ShortGetterName";

namespace CppEditor {

// ------------------ CppCodeStyleSettingsWidget

CppCodeStyleSettings::CppCodeStyleSettings() = default;

QVariantMap CppCodeStyleSettings::toMap() const
{
    return {
        {indentBlockBracesKey, indentBlockBraces},
        {indentBlockBodyKey, indentBlockBody},
        {indentClassBracesKey, indentClassBraces},
        {indentEnumBracesKey, indentEnumBraces},
        {indentNamespaceBracesKey, indentNamespaceBraces},
        {indentNamespaceBodyKey, indentNamespaceBody},
        {indentAccessSpecifiersKey, indentAccessSpecifiers},
        {indentDeclarationsRelativeToAccessSpecifiersKey, indentDeclarationsRelativeToAccessSpecifiers},
        {indentFunctionBodyKey, indentFunctionBody},
        {indentFunctionBracesKey, indentFunctionBraces},
        {indentSwitchLabelsKey, indentSwitchLabels},
        {indentStatementsRelativeToSwitchLabelsKey, indentStatementsRelativeToSwitchLabels},
        {indentBlocksRelativeToSwitchLabelsKey, indentBlocksRelativeToSwitchLabels},
        {indentControlFlowRelativeToSwitchLabelsKey, indentControlFlowRelativeToSwitchLabels},
        {bindStarToIdentifierKey, bindStarToIdentifier},
        {bindStarToTypeNameKey, bindStarToTypeName},
        {bindStarToLeftSpecifierKey, bindStarToLeftSpecifier},
        {bindStarToRightSpecifierKey, bindStarToRightSpecifier},
        {extraPaddingForConditionsIfConfusingAlignKey, extraPaddingForConditionsIfConfusingAlign},
        {alignAssignmentsKey, alignAssignments},
        {shortGetterNameKey, preferGetterNameWithoutGetPrefix}
    };
}

void CppCodeStyleSettings::fromMap(const QVariantMap &map)
{
    indentBlockBraces = map.value(indentBlockBracesKey, indentBlockBraces).toBool();
    indentBlockBody = map.value(indentBlockBodyKey, indentBlockBody).toBool();
    indentClassBraces = map.value(indentClassBracesKey, indentClassBraces).toBool();
    indentEnumBraces = map.value(indentEnumBracesKey, indentEnumBraces).toBool();
    indentNamespaceBraces = map.value(indentNamespaceBracesKey, indentNamespaceBraces).toBool();
    indentNamespaceBody = map.value(indentNamespaceBodyKey, indentNamespaceBody).toBool();
    indentAccessSpecifiers = map.value(indentAccessSpecifiersKey, indentAccessSpecifiers).toBool();
    indentDeclarationsRelativeToAccessSpecifiers =
            map.value(indentDeclarationsRelativeToAccessSpecifiersKey,
                      indentDeclarationsRelativeToAccessSpecifiers).toBool();
    indentFunctionBody = map.value(indentFunctionBodyKey, indentFunctionBody).toBool();
    indentFunctionBraces = map.value(indentFunctionBracesKey, indentFunctionBraces).toBool();
    indentSwitchLabels = map.value(indentSwitchLabelsKey, indentSwitchLabels).toBool();
    indentStatementsRelativeToSwitchLabels = map.value(indentStatementsRelativeToSwitchLabelsKey,
                                indentStatementsRelativeToSwitchLabels).toBool();
    indentBlocksRelativeToSwitchLabels = map.value(indentBlocksRelativeToSwitchLabelsKey,
                                indentBlocksRelativeToSwitchLabels).toBool();
    indentControlFlowRelativeToSwitchLabels = map.value(indentControlFlowRelativeToSwitchLabelsKey,
                                indentControlFlowRelativeToSwitchLabels).toBool();
    bindStarToIdentifier = map.value(bindStarToIdentifierKey, bindStarToIdentifier).toBool();
    bindStarToTypeName = map.value(bindStarToTypeNameKey, bindStarToTypeName).toBool();
    bindStarToLeftSpecifier = map.value(bindStarToLeftSpecifierKey, bindStarToLeftSpecifier).toBool();
    bindStarToRightSpecifier = map.value(bindStarToRightSpecifierKey, bindStarToRightSpecifier).toBool();
    extraPaddingForConditionsIfConfusingAlign = map.value(extraPaddingForConditionsIfConfusingAlignKey,
                                extraPaddingForConditionsIfConfusingAlign).toBool();
    alignAssignments = map.value(alignAssignmentsKey, alignAssignments).toBool();
    preferGetterNameWithoutGetPrefix = map.value(shortGetterNameKey,
                                preferGetterNameWithoutGetPrefix).toBool();
}

bool CppCodeStyleSettings::equals(const CppCodeStyleSettings &rhs) const
{
    return indentBlockBraces == rhs.indentBlockBraces
           && indentBlockBody == rhs.indentBlockBody
           && indentClassBraces == rhs.indentClassBraces
           && indentEnumBraces == rhs.indentEnumBraces
           && indentNamespaceBraces == rhs.indentNamespaceBraces
           && indentNamespaceBody == rhs.indentNamespaceBody
           && indentAccessSpecifiers == rhs.indentAccessSpecifiers
           && indentDeclarationsRelativeToAccessSpecifiers == rhs.indentDeclarationsRelativeToAccessSpecifiers
           && indentFunctionBody == rhs.indentFunctionBody
           && indentFunctionBraces == rhs.indentFunctionBraces
           && indentSwitchLabels == rhs.indentSwitchLabels
           && indentStatementsRelativeToSwitchLabels == rhs.indentStatementsRelativeToSwitchLabels
           && indentBlocksRelativeToSwitchLabels == rhs.indentBlocksRelativeToSwitchLabels
           && indentControlFlowRelativeToSwitchLabels == rhs.indentControlFlowRelativeToSwitchLabels
           && bindStarToIdentifier == rhs.bindStarToIdentifier
           && bindStarToTypeName == rhs.bindStarToTypeName
           && bindStarToLeftSpecifier == rhs.bindStarToLeftSpecifier
           && bindStarToRightSpecifier == rhs.bindStarToRightSpecifier
           && extraPaddingForConditionsIfConfusingAlign == rhs.extraPaddingForConditionsIfConfusingAlign
           && alignAssignments == rhs.alignAssignments
           && preferGetterNameWithoutGetPrefix == rhs.preferGetterNameWithoutGetPrefix
           ;
}

CppCodeStyleSettings CppCodeStyleSettings::getProjectCodeStyle(ProjectExplorer::Project *project)
{
    if (!project)
        return currentGlobalCodeStyle();

    ProjectExplorer::EditorConfiguration *editorConfiguration = project->editorConfiguration();
    QTC_ASSERT(editorConfiguration, return currentGlobalCodeStyle());

    TextEditor::ICodeStylePreferences *codeStylePreferences
        = editorConfiguration->codeStyle(Constants::CPP_SETTINGS_ID);
    QTC_ASSERT(codeStylePreferences, return currentGlobalCodeStyle());

    auto cppCodeStylePreferences =
        dynamic_cast<const CppCodeStylePreferences *>(codeStylePreferences);
    if (!cppCodeStylePreferences)
        return currentGlobalCodeStyle();

    return cppCodeStylePreferences->currentCodeStyleSettings();
}

CppCodeStyleSettings CppCodeStyleSettings::currentProjectCodeStyle()
{
    return getProjectCodeStyle(ProjectExplorer::ProjectTree::currentProject());
}

CppCodeStyleSettings CppCodeStyleSettings::currentGlobalCodeStyle()
{
    CppCodeStylePreferences *cppCodeStylePreferences = CppToolsSettings::instance()->cppCodeStyle();
    QTC_ASSERT(cppCodeStylePreferences, return CppCodeStyleSettings());

    return cppCodeStylePreferences->currentCodeStyleSettings();
}

TextEditor::TabSettings CppCodeStyleSettings::getProjectTabSettings(ProjectExplorer::Project *project)
{
    if (!project)
        return currentGlobalTabSettings();

    ProjectExplorer::EditorConfiguration *editorConfiguration = project->editorConfiguration();
    QTC_ASSERT(editorConfiguration, return currentGlobalTabSettings());

    TextEditor::ICodeStylePreferences *codeStylePreferences
        = editorConfiguration->codeStyle(Constants::CPP_SETTINGS_ID);
    QTC_ASSERT(codeStylePreferences, return currentGlobalTabSettings());
    return codeStylePreferences->currentTabSettings();
}

TextEditor::TabSettings CppCodeStyleSettings::currentProjectTabSettings()
{
    return getProjectTabSettings(ProjectExplorer::ProjectTree::currentProject());
}

TextEditor::TabSettings CppCodeStyleSettings::currentGlobalTabSettings()
{
    CppCodeStylePreferences *cppCodeStylePreferences
            = CppToolsSettings::instance()->cppCodeStyle();
    QTC_ASSERT(cppCodeStylePreferences, return TextEditor::TabSettings());

    return cppCodeStylePreferences->currentTabSettings();
}


static void configureOverviewWithCodeStyleSettings(CPlusPlus::Overview &overview,
                                                   const CppCodeStyleSettings &settings)
{
    overview.starBindFlags = {};
    if (settings.bindStarToIdentifier)
        overview.starBindFlags |= CPlusPlus::Overview::BindToIdentifier;
    if (settings.bindStarToTypeName)
        overview.starBindFlags |= CPlusPlus::Overview::BindToTypeName;
    if (settings.bindStarToLeftSpecifier)
        overview.starBindFlags |= CPlusPlus::Overview::BindToLeftSpecifier;
    if (settings.bindStarToRightSpecifier)
        overview.starBindFlags |= CPlusPlus::Overview::BindToRightSpecifier;
}

CPlusPlus::Overview CppCodeStyleSettings::currentProjectCodeStyleOverview()
{
    CPlusPlus::Overview overview;
    const Utils::optional<CppCodeStyleSettings> codeStyleSettings = currentProjectCodeStyle();
    configureOverviewWithCodeStyleSettings(overview,
                                           codeStyleSettings.value_or(currentGlobalCodeStyle()));
    return overview;
}

CPlusPlus::Overview CppCodeStyleSettings::currentGlobalCodeStyleOverview()
{
    CPlusPlus::Overview overview;
    configureOverviewWithCodeStyleSettings(overview, currentGlobalCodeStyle());
    return overview;
}

} // namespace CppEditor
