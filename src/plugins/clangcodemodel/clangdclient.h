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

#pragma once

#include <coreplugin/find/searchresultitem.h>
#include <cppeditor/baseeditordocumentparser.h>
#include <cppeditor/cppcodemodelsettings.h>
#include <cppeditor/cursorineditor.h>
#include <languageclient/client.h>
#include <utils/link.h>
#include <utils/optional.h>

#include <QVersionNumber>

namespace CppEditor { class CppEditorWidget; }
namespace LanguageServerProtocol { class Range; }
namespace ProjectExplorer {
class Project;
class Task;
}
namespace TextEditor {
class BaseTextEditor;
class IAssistProposal;
}

namespace ClangCodeModel {
namespace Internal {
class ClangdAstNode;

Q_DECLARE_LOGGING_CATEGORY(clangdLog);
Q_DECLARE_LOGGING_CATEGORY(clangdLogAst);

void setupClangdConfigFile();

class ClangdClient : public LanguageClient::Client
{
    Q_OBJECT
public:
    ClangdClient(ProjectExplorer::Project *project, const Utils::FilePath &jsonDbDir);
    ~ClangdClient() override;

    bool isFullyIndexed() const;
    QVersionNumber versionNumber() const;
    CppEditor::ClangdSettings::Data settingsData() const;

    void openExtraFile(const Utils::FilePath &filePath, const QString &content = {});
    void closeExtraFile(const Utils::FilePath &filePath);

    void findUsages(TextEditor::TextDocument *document, const QTextCursor &cursor,
                    const Utils::optional<QString> &replacement);
    void followSymbol(TextEditor::TextDocument *document,
            const QTextCursor &cursor,
            CppEditor::CppEditorWidget *editorWidget,
            const Utils::LinkHandler &callback,
            bool resolveTarget,
            bool openInSplit);

    void switchDeclDef(TextEditor::TextDocument *document,
            const QTextCursor &cursor,
            CppEditor::CppEditorWidget *editorWidget,
            const Utils::LinkHandler &callback);
    void switchHeaderSource(const Utils::FilePath &filePath, bool inNextSplit);

    void findLocalUsages(TextEditor::TextDocument *document, const QTextCursor &cursor,
                         CppEditor::RenameCallback &&callback);

    void gatherHelpItemForTooltip(
            const LanguageServerProtocol::HoverRequest::Response &hoverResponse,
            const LanguageServerProtocol::DocumentUri &uri);

    void setVirtualRanges(const Utils::FilePath &filePath,
                          const QList<LanguageServerProtocol::Range> &ranges, int revision);

    void enableTesting();
    bool testingEnabled() const;

    static QString displayNameFromDocumentSymbol(LanguageServerProtocol::SymbolKind kind,
                                                 const QString &name, const QString &detail);

    static void handleUiHeaderChange(const QString &fileName);

    void updateParserConfig(const Utils::FilePath &filePath,
                            const CppEditor::BaseEditorDocumentParser::Configuration &config);
    void switchIssuePaneEntries(const Utils::FilePath &filePath);
    void addTask(const ProjectExplorer::Task &task);
    void clearTasks(const Utils::FilePath &filePath);
    Utils::optional<bool> hasVirtualFunctionAt(TextEditor::TextDocument *doc, int revision,
                                               const LanguageServerProtocol::Range &range);

    using TextDocOrFile = Utils::variant<const TextEditor::TextDocument *, Utils::FilePath>;
    using AstHandler = std::function<void(const ClangdAstNode &ast,
                                                const LanguageServerProtocol::MessageId &)>;
    enum class AstCallbackMode { SyncIfPossible, AlwaysAsync };
    LanguageServerProtocol::MessageId getAndHandleAst(const TextDocOrFile &doc,
                                                     const AstHandler &astHandler,
                                                      AstCallbackMode callbackMode,
                                                      const LanguageServerProtocol::Range &range);

    using SymbolInfoHandler = std::function<void(const QString &name, const QString &prefix,
                                                 const LanguageServerProtocol::MessageId &)>;
    LanguageServerProtocol::MessageId requestSymbolInfo(
            const Utils::FilePath &filePath,
            const LanguageServerProtocol::Position &position,
            const SymbolInfoHandler &handler);

signals:
    void indexingFinished();
    void foundReferences(const QList<Core::SearchResultItem> &items);
    void findUsagesDone();
    void helpItemGathered(const Core::HelpItem &helpItem);
    void highlightingResultsReady(const TextEditor::HighlightingResults &results,
                                  const Utils::FilePath &file);
    void proposalReady(TextEditor::IAssistProposal *proposal);
    void textMarkCreated(const Utils::FilePath &file);

private:
    void handleDiagnostics(const LanguageServerProtocol::PublishDiagnosticsParams &params) override;
    void handleDocumentOpened(TextEditor::TextDocument *doc) override;
    void handleDocumentClosed(TextEditor::TextDocument *doc) override;
    QTextCursor adjustedCursorForHighlighting(const QTextCursor &cursor,
                                              TextEditor::TextDocument *doc) override;
    const CustomInspectorTabs createCustomInspectorTabs() override;
    TextEditor::RefactoringChangesData *createRefactoringChangesBackend() const override;
    LanguageClient::DiagnosticManager *createDiagnosticManager() override;
    bool referencesShadowFile(const TextEditor::TextDocument *doc,
                              const Utils::FilePath &candidate) override;

    class Private;
    class VirtualFunctionAssistProcessor;
    class VirtualFunctionAssistProvider;
    class ClangdFunctionHintProcessor;
    class ClangdCompletionAssistProcessor;
    class ClangdCompletionAssistProvider;
    Private * const d;
};

class ClangdDiagnostic : public LanguageServerProtocol::Diagnostic
{
public:
    using Diagnostic::Diagnostic;
    Utils::optional<QList<LanguageServerProtocol::CodeAction>> codeActions() const;
    QString category() const;
};

} // namespace Internal
} // namespace ClangCodeModel
