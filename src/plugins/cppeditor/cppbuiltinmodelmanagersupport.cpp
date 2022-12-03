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

#include "cppbuiltinmodelmanagersupport.h"

#include "builtineditordocumentprocessor.h"
#include "cppcanonicalsymbol.h"
#include "cppcompletionassist.h"
#include "cppeditorwidget.h"
#include "cppelementevaluator.h"
#include "cppfollowsymbolundercursor.h"
#include "cppoverviewmodel.h"
#include "cpptoolsreuse.h"
#include "symbolfinder.h"

#include <app/app_version.h>
#include <texteditor/basehoverhandler.h>
#include <utils/executeondestruction.h>
#include <utils/qtcassert.h>

#include <QCoreApplication>

using namespace Core;
using namespace TextEditor;

namespace CppEditor::Internal {
namespace {
class CppHoverHandler : public TextEditor::BaseHoverHandler
{
private:
    void identifyMatch(TextEditor::TextEditorWidget *editorWidget,
                       int pos,
                       ReportPriority report) override
    {
        if (CppModelManager::usesClangd(editorWidget->textDocument())) {
            report(Priority_None);
            return;
        }

        Utils::ExecuteOnDestruction reportPriority([this, report](){ report(priority()); });

        QTextCursor tc(editorWidget->document());
        tc.setPosition(pos);

        CppElementEvaluator evaluator(editorWidget);
        evaluator.setTextCursor(tc);
        evaluator.execute();
        QString tip;
        if (evaluator.hasDiagnosis()) {
            tip += evaluator.diagnosis();
            setPriority(Priority_Diagnostic);
        }
        const QStringList fallback = identifierWordsUnderCursor(tc);
        if (evaluator.identifiedCppElement()) {
            const QSharedPointer<CppElement> &cppElement = evaluator.cppElement();
            const QStringList candidates = cppElement->helpIdCandidates;
            const HelpItem helpItem(candidates + fallback, cppElement->helpMark, cppElement->helpCategory);
            setLastHelpItemIdentified(helpItem);
            if (!helpItem.isValid())
                tip += cppElement->tooltip;
        } else {
            setLastHelpItemIdentified({fallback, {}, HelpItem::Unknown});
        }
        setToolTip(tip);
    }
};
} // anonymous namespace

QString BuiltinModelManagerSupportProvider::id() const
{
    return QLatin1String("CppEditor.BuiltinCodeModel");
}

QString BuiltinModelManagerSupportProvider::displayName() const
{
    return QCoreApplication::translate("ModelManagerSupportInternal::displayName",
                                       "%1 Built-in").arg(Core::Constants::IDE_DISPLAY_NAME);
}

ModelManagerSupport::Ptr BuiltinModelManagerSupportProvider::createModelManagerSupport()
{
    return ModelManagerSupport::Ptr(new BuiltinModelManagerSupport);
}

BuiltinModelManagerSupport::BuiltinModelManagerSupport()
    : m_completionAssistProvider(new InternalCompletionAssistProvider),
      m_followSymbol(new FollowSymbolUnderCursor)
{
}

BuiltinModelManagerSupport::~BuiltinModelManagerSupport() = default;

BaseEditorDocumentProcessor *BuiltinModelManagerSupport::createEditorDocumentProcessor(
        TextEditor::TextDocument *baseTextDocument)
{
    return new BuiltinEditorDocumentProcessor(baseTextDocument);
}

CppCompletionAssistProvider *BuiltinModelManagerSupport::completionAssistProvider()
{
    return m_completionAssistProvider.data();
}


TextEditor::BaseHoverHandler *BuiltinModelManagerSupport::createHoverHandler()
{
    return new CppHoverHandler;
}

std::unique_ptr<AbstractOverviewModel> BuiltinModelManagerSupport::createOverviewModel()
{
    return std::make_unique<OverviewModel>();
}

void BuiltinModelManagerSupport::followSymbol(const CursorInEditor &data,
                                              const Utils::LinkHandler &processLinkCallback,
                                              bool resolveTarget, bool inNextSplit)
{
    SymbolFinder finder;
    m_followSymbol->findLink(data, processLinkCallback,
            resolveTarget, CppModelManager::instance()->snapshot(),
            data.editorWidget()->semanticInfo().doc, &finder, inNextSplit);
}

void BuiltinModelManagerSupport::switchDeclDef(const CursorInEditor &data,
                                               const Utils::LinkHandler &processLinkCallback)
{
    SymbolFinder finder;
    m_followSymbol->switchDeclDef(data, processLinkCallback,
            CppModelManager::instance()->snapshot(), data.editorWidget()->semanticInfo().doc,
            &finder);
}

void BuiltinModelManagerSupport::startLocalRenaming(const CursorInEditor &data,
                                                    const ProjectPart *,
                                                    RenameCallback &&renameSymbolsCallback)
{
    CppEditorWidget *editorWidget = data.editorWidget();
    QTC_ASSERT(editorWidget, renameSymbolsCallback(QString(), {}, 0); return;);
    editorWidget->updateSemanticInfo();
    // Call empty callback
    renameSymbolsCallback(QString(), {}, data.cursor().document()->revision());
}

void BuiltinModelManagerSupport::globalRename(const CursorInEditor &data,
                                              const QString &replacement)
{
    CppModelManager *modelManager = CppModelManager::instance();
    if (!modelManager)
        return;

    CppEditorWidget *editorWidget = data.editorWidget();
    QTC_ASSERT(editorWidget, return;);

    SemanticInfo info = editorWidget->semanticInfo();
    info.snapshot = modelManager->snapshot();
    info.snapshot.insert(info.doc);
    const QTextCursor &cursor = data.cursor();
    if (const CPlusPlus::Macro *macro = findCanonicalMacro(cursor, info.doc)) {
        modelManager->renameMacroUsages(*macro, replacement);
    } else {
        Internal::CanonicalSymbol cs(info.doc, info.snapshot);
        CPlusPlus::Symbol *canonicalSymbol = cs(cursor);
        if (canonicalSymbol)
            modelManager->renameUsages(canonicalSymbol, cs.context(), replacement);
    }
}

void BuiltinModelManagerSupport::findUsages(const CursorInEditor &data) const
{
    CppModelManager *modelManager = CppModelManager::instance();
    if (!modelManager)
        return;

    CppEditorWidget *editorWidget = data.editorWidget();
    QTC_ASSERT(editorWidget, return;);

    SemanticInfo info = editorWidget->semanticInfo();
    info.snapshot = modelManager->snapshot();
    info.snapshot.insert(info.doc);
    const QTextCursor &cursor = data.cursor();
    if (const CPlusPlus::Macro *macro = findCanonicalMacro(cursor, info.doc)) {
        modelManager->findMacroUsages(*macro);
    } else {
        Internal::CanonicalSymbol cs(info.doc, info.snapshot);
        CPlusPlus::Symbol *canonicalSymbol = cs(cursor);
        if (canonicalSymbol)
            modelManager->findUsages(canonicalSymbol, cs.context());
    }
}

void BuiltinModelManagerSupport::switchHeaderSource(const Utils::FilePath &filePath,
                                                    bool inNextSplit)
{
    const auto otherFile = Utils::FilePath::fromString(
                correspondingHeaderOrSource(filePath.toString()));
    if (!otherFile.isEmpty())
        openEditor(otherFile, inNextSplit);
}

} // namespace CppEditor::Internal
