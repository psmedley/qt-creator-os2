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

#include "cppquickfix.h"
#include "cpptoolstestcase.h"

#include <projectexplorer/headerpath.h>

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>

namespace TextEditor { class QuickFixOperation; }

namespace CppEditor {
class CppCodeStylePreferences;

namespace Internal {
namespace Tests {

class BaseQuickFixTestCase : public CppEditor::Tests::TestCase
{
public:
    /// Exactly one QuickFixTestDocument must contain the cursor position marker '@'
    /// or "@{start}" and "@{end}"
    BaseQuickFixTestCase(const QList<TestDocumentPtr> &testDocuments,
                         const ProjectExplorer::HeaderPaths &headerPaths
                            = ProjectExplorer::HeaderPaths());

    ~BaseQuickFixTestCase();

protected:
    TestDocumentPtr m_documentWithMarker;
    QList<TestDocumentPtr> m_testDocuments;

private:
    QScopedPointer<CppEditor::Tests::TemporaryDir> m_temporaryDirectory;

    CppCodeStylePreferences *m_cppCodeStylePreferences;
    QByteArray m_cppCodeStylePreferencesOriginalDelegateId;

    ProjectExplorer::HeaderPaths m_headerPathsToRestore;
    bool m_restoreHeaderPaths;
};

/// Tests a concrete QuickFixOperation of a given CppQuickFixFactory
class QuickFixOperationTest : public BaseQuickFixTestCase
{
public:
    QuickFixOperationTest(const QList<TestDocumentPtr> &testDocuments,
                          CppQuickFixFactory *factory,
                          const ProjectExplorer::HeaderPaths &headerPaths
                            = ProjectExplorer::HeaderPaths(),
                          int operationIndex = 0,
                          const QByteArray &expectedFailMessage = QByteArray());

    static void run(const QList<TestDocumentPtr> &testDocuments,
                    CppQuickFixFactory *factory,
                    const QString &headerPath,
                    int operationIndex = 0);
};

QList<TestDocumentPtr> singleDocument(const QByteArray &original,
                                                const QByteArray &expected);

class QuickfixTest : public QObject
{
    Q_OBJECT

private slots:
    void testGeneric_data();
    void testGeneric();

    void testGenerateGetterSetterNamespaceHandlingCreate_data();
    void testGenerateGetterSetterNamespaceHandlingCreate();
    void testGenerateGetterSetterNamespaceHandlingAddUsing_data();
    void testGenerateGetterSetterNamespaceHandlingAddUsing();
    void testGenerateGetterSetterNamespaceHandlingFullyQualify_data();
    void testGenerateGetterSetterNamespaceHandlingFullyQualify();
    void testGenerateGetterSetterCustomNames_data();
    void testGenerateGetterSetterCustomNames();
    void testGenerateGetterSetterValueTypes_data();
    void testGenerateGetterSetterValueTypes();
    void testGenerateGetterSetterCustomTemplate();
    void testGenerateGetterSetterNeedThis();
    void testGenerateGetterSetterOfferedFixes_data();
    void testGenerateGetterSetterOfferedFixes();
    void testGenerateGetterSetterGeneralTests_data();
    void testGenerateGetterSetterGeneralTests();
    void testGenerateGetterSetterOnlyGetter();
    void testGenerateGetterSetterOnlySetter();
    void testGenerateGetterSetterAnonymousClass();
    void testGenerateGetterSetterInlineInHeaderFile();
    void testGenerateGetterSetterOnlySetterHeaderFileWithIncludeGuard();
    void testGenerateGetterFunctionAsTemplateArg();
    void testGenerateGettersSetters_data();
    void testGenerateGettersSetters();

    void testInsertQtPropertyMembers_data();
    void testInsertQtPropertyMembers();

    void testInsertMemberFromInitialization_data();
    void testInsertMemberFromInitialization();

    void testConvertQt4ConnectConnectOutOfClass();
    void testConvertQt4ConnectConnectWithinClass_data();
    void testConvertQt4ConnectConnectWithinClass();
    void testConvertQt4ConnectDifferentNamespace();

    void testInsertDefFromDeclAfterClass();
    void testInsertDefFromDeclHeaderSourceBasic1();
    void testInsertDefFromDeclHeaderSourceBasic2();
    void testInsertDefFromDeclHeaderSourceBasic3();
    void testInsertDefFromDeclHeaderSourceNamespace1();
    void testInsertDefFromDeclHeaderSourceNamespace2();
    void testInsertDefFromDeclInsideClass();
    void testInsertDefFromDeclNotTriggeringWhenDefinitionExists();
    void testInsertDefFromDeclFindRightImplementationFile();
    void testInsertDefFromDeclIgnoreSurroundingGeneratedDeclarations();
    void testInsertDefFromDeclRespectWsInOperatorNames1();
    void testInsertDefFromDeclRespectWsInOperatorNames2();
    void testInsertDefFromDeclNoexceptSpecifier();
    void testInsertDefFromDeclMacroUsesAtEndOfFile1();
    void testInsertDefFromDeclMacroUsesAtEndOfFile2();
    void testInsertDefFromDeclErroneousStatementAtEndOfFile();
    void testInsertDefFromDeclRvalueReference();
    void testInsertDefFromDeclFunctionTryBlock();
    void testInsertDefFromDeclUsingDecl();
    void testInsertDefFromDeclFindImplementationFile();
    void testInsertDefFromDeclUnicodeIdentifier();
    void testInsertDefFromDeclTemplateClass();
    void testInsertDefFromDeclTemplateClassWithValueParam();
    void testInsertDefFromDeclTemplateFunction();
    void testInsertDefFromDeclNotTriggeredForFriendFunc();
    void testInsertDefFromDeclMinimalFunctionParameterType();
    void testInsertDefFromDeclAliasTemplateAsReturnType();
    void testInsertDefsFromDecls_data();
    void testInsertDefsFromDecls();

    void testInsertDeclFromDef();
    void testInsertDeclFromDefTemplateFuncTypename();
    void testInsertDeclFromDefTemplateFuncInt();
    void testInsertDeclFromDefTemplateReturnType();
    void testInsertDeclFromDefNotTriggeredForTemplateFunc();

    void testAddIncludeForUndefinedIdentifier_data();
    void testAddIncludeForUndefinedIdentifier();
    void testAddIncludeForUndefinedIdentifierNoDoubleQtHeaderInclude();

    void testAddForwardDeclForUndefinedIdentifier_data();
    void testAddForwardDeclForUndefinedIdentifier();

    void testMoveFuncDefOutsideMemberFuncToCpp();
    void testMoveFuncDefOutsideMemberFuncToCppInsideNS();
    void testMoveFuncDefOutsideMemberFuncOutside1();
    void testMoveFuncDefOutsideMemberFuncOutside2();
    void testMoveFuncDefOutsideMemberFuncToCppNS();
    void testMoveFuncDefOutsideMemberFuncToCppNSUsing();
    void testMoveFuncDefOutsideMemberFuncOutsideWithNs();
    void testMoveFuncDefOutsideFreeFuncToCpp();
    void testMoveFuncDefOutsideFreeFuncToCppNS();
    void testMoveFuncDefOutsideCtorWithInitialization1();
    void testMoveFuncDefOutsideCtorWithInitialization2();
    void testMoveFuncDefOutsideAfterClass();
    void testMoveFuncDefOutsideRespectWsInOperatorNames1();
    void testMoveFuncDefOutsideRespectWsInOperatorNames2();
    void testMoveFuncDefOutsideMacroUses();
    void testMoveFuncDefOutsideTemplate();
    void testMoveFuncDefOutsideTemplateSpecializedClass();
    void testMoveFuncDefOutsideUnnamedTemplate();
    void testMoveFuncDefOutsideMemberFuncToCppStatic();
    void testMoveFuncDefOutsideMemberFuncToCppWithInlinePartOfName();
    void testMoveFuncDefOutsideMixedQualifiers();

    void testMoveAllFuncDefOutsideMemberFuncToCpp();
    void testMoveAllFuncDefOutsideMemberFuncOutside();
    void testMoveAllFuncDefOutsideDoNotTriggerOnBaseClass();
    void testMoveAllFuncDefOutsideClassWithBaseClass();
    void testMoveAllFuncDefOutsideIgnoreMacroCode();

    void testMoveFuncDefToDeclMemberFunc();
    void testMoveFuncDefToDeclMemberFuncOutside();
    void testMoveFuncDefToDeclMemberFuncToCppNS();
    void testMoveFuncDefToDeclMemberFuncToCppNSUsing();
    void testMoveFuncDefToDeclMemberFuncOutsideWithNs();
    void testMoveFuncDefToDeclFreeFuncToCpp();
    void testMoveFuncDefToDeclFreeFuncToCppNS();
    void testMoveFuncDefToDeclCtorWithInitialization();
    void testMoveFuncDefToDeclStructWithAssignedVariable();
    void testMoveFuncDefToDeclMacroUses();
    void testMoveFuncDefToDeclOverride();
    void testMoveFuncDefToDeclTemplate();
    void testMoveFuncDefToDeclTemplateFunction();

    void testAssignToLocalVariableTemplates();

    void testExtractFunction_data();
    void testExtractFunction();

    void testExtractLiteralAsParameterTypeDeduction_data();
    void testExtractLiteralAsParameterTypeDeduction();
    void testExtractLiteralAsParameterFreeFunctionSeparateFiles();
    void testExtractLiteralAsParameterMemberFunctionSeparateFiles();
    void testExtractLiteralAsParameterNotTriggeringForInvalidCode();

    void testAddCurlyBraces();

    void testRemoveUsingNamespace_data();
    void testRemoveUsingNamespace();
    void testRemoveUsingNamespaceSimple_data();
    void testRemoveUsingNamespaceSimple();
    void testRemoveUsingNamespaceDifferentSymbols();

    void testGenerateConstructor_data();
    void testGenerateConstructor();
};

} // namespace Tests
} // namespace Internal
} // namespace CppEditor
