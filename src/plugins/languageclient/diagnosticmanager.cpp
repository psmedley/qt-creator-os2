/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include "diagnosticmanager.h"

#include "client.h"

#include <coreplugin/editormanager/documentmodel.h>
#include <projectexplorer/project.h>
#include <texteditor/fontsettings.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>
#include <texteditor/textmark.h>
#include <texteditor/textstyles.h>
#include <utils/utilsicons.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>

using namespace LanguageServerProtocol;
using namespace Utils;
using namespace TextEditor;

namespace LanguageClient {

class TextMark : public TextEditor::TextMark
{
public:
    TextMark(const FilePath &fileName, const Diagnostic &diag, const Id &clientId)
        : TextEditor::TextMark(fileName, diag.range().start().line() + 1, clientId)
    {
        setLineAnnotation(diag.message());
        setToolTip(diag.message());
        const bool isError
            = diag.severity().value_or(DiagnosticSeverity::Hint) == DiagnosticSeverity::Error;
        setColor(isError ? Theme::CodeModel_Error_TextMarkColor
                         : Theme::CodeModel_Warning_TextMarkColor);

        setIcon(isError ? Icons::CODEMODEL_ERROR.icon()
                        : Icons::CODEMODEL_WARNING.icon());
    }
};

DiagnosticManager::DiagnosticManager(Client *client)
    : m_client(client)
    , m_extraSelectionsId(TextEditorWidget::CodeWarningsSelection)
{
}

DiagnosticManager::~DiagnosticManager()
{
    clearDiagnostics();
}

void DiagnosticManager::setDiagnostics(const DocumentUri &uri,
                                       const QList<Diagnostic> &diagnostics,
                                       const Utils::optional<int> &version)
{
    hideDiagnostics(uri.toFilePath());
    m_diagnostics[uri] = {version, filteredDiagnostics(diagnostics)};
}

void DiagnosticManager::hideDiagnostics(const Utils::FilePath &filePath)
{
    if (auto doc = TextDocument::textDocumentForFilePath(filePath)) {
        for (BaseTextEditor *editor : BaseTextEditor::textEditorsForDocument(doc))
            editor->editorWidget()->setExtraSelections(m_extraSelectionsId, {});
    }
    m_marks.remove(filePath);
}

QList<Diagnostic> DiagnosticManager::filteredDiagnostics(const QList<Diagnostic> &diagnostics) const
{
    return diagnostics;
}

void DiagnosticManager::disableDiagnostics(TextEditor::TextDocument *document)
{

    Marks &marks = m_marks[document->filePath()];
    if (!marks.enabled)
        return;
    for (TextEditor::TextMark *mark : marks.marks)
        mark->setColor(Utils::Theme::Color::IconsDisabledColor);
    marks.enabled = false;
}

void DiagnosticManager::showDiagnostics(const DocumentUri &uri, int version)
{
    const FilePath &filePath = uri.toFilePath();
    if (TextDocument *doc = TextDocument::textDocumentForFilePath(filePath)) {
        QList<QTextEdit::ExtraSelection> extraSelections;
        const VersionedDiagnostics &versionedDiagnostics = m_diagnostics.value(uri);
        if (versionedDiagnostics.version.value_or(version) == version
            && !versionedDiagnostics.diagnostics.isEmpty()) {
            Marks &marks = m_marks[filePath];
            const bool isProjectFile = m_client->project()
                                       && m_client->project()->isKnownFile(filePath);
            for (const Diagnostic &diagnostic : versionedDiagnostics.diagnostics) {
                const QTextEdit::ExtraSelection selection
                    = createDiagnosticSelection(diagnostic, doc->document());
                if (!selection.cursor.isNull())
                    extraSelections << selection;
                if (TextEditor::TextMark *mark = createTextMark(filePath, diagnostic, isProjectFile))
                    marks.marks.append(mark);
            }
            if (!marks.marks.isEmpty())
                emit textMarkCreated(filePath);
        }

        for (BaseTextEditor *editor : BaseTextEditor::textEditorsForDocument(doc))
            editor->editorWidget()->setExtraSelections(m_extraSelectionsId, extraSelections);
    }
}

TextEditor::TextMark *DiagnosticManager::createTextMark(const FilePath &filePath,
                                                        const Diagnostic &diagnostic,
                                                        bool /*isProjectFile*/) const
{
    static const auto icon = QIcon::fromTheme("edit-copy", Utils::Icons::COPY.icon());
    static const QString tooltip = tr("Copy to Clipboard");
    auto mark = new TextMark(filePath, diagnostic, m_client->id());
    mark->setActionsProvider([text = diagnostic.message()] {
        QAction *action = new QAction();
        action->setIcon(icon);
        action->setToolTip(tooltip);
        QObject::connect(action, &QAction::triggered, [text] {
            QApplication::clipboard()->setText(text);
        });
        return QList<QAction *>{action};
    });
    return mark;
}

QTextEdit::ExtraSelection DiagnosticManager::createDiagnosticSelection(
    const LanguageServerProtocol::Diagnostic &diagnostic, QTextDocument *textDocument) const
{
    QTextCursor cursor(textDocument);
    cursor.setPosition(diagnostic.range().start().toPositionInDocument(textDocument));
    cursor.setPosition(diagnostic.range().end().toPositionInDocument(textDocument),
                       QTextCursor::KeepAnchor);

    const FontSettings &fontSettings = TextEditorSettings::fontSettings();
    const DiagnosticSeverity severity = diagnostic.severity().value_or(DiagnosticSeverity::Warning);
    const TextStyle style = severity == DiagnosticSeverity::Error ? C_ERROR : C_WARNING;

    return QTextEdit::ExtraSelection{cursor, fontSettings.toTextCharFormat(style)};
}

void DiagnosticManager::setExtraSelectionsId(const Utils::Id &extraSelectionsId)
{
    // this function should be called before any diagnostics are handled
    QTC_CHECK(m_diagnostics.isEmpty());
    m_extraSelectionsId = extraSelectionsId;
}

void DiagnosticManager::forAllMarks(std::function<void (TextEditor::TextMark *)> func)
{
    for (const Marks &marks : qAsConst(m_marks)) {
        for (TextEditor::TextMark *mark : marks.marks)
            func(mark);
    }
}

void DiagnosticManager::clearDiagnostics()
{
    for (const DocumentUri &uri : m_diagnostics.keys())
        hideDiagnostics(uri.toFilePath());
    m_diagnostics.clear();
    QTC_ASSERT(m_marks.isEmpty(), m_marks.clear());
}

QList<Diagnostic> DiagnosticManager::diagnosticsAt(const DocumentUri &uri,
                                                   const QTextCursor &cursor) const
{
    const int documentRevision = m_client->documentVersion(uri.toFilePath());
    auto it = m_diagnostics.find(uri);
    if (it == m_diagnostics.end())
        return {};
    if (documentRevision != it->version.value_or(documentRevision))
        return {};
    return Utils::filtered(it->diagnostics, [range = Range(cursor)](const Diagnostic &diagnostic) {
        return diagnostic.range().overlaps(range);
    });
}

bool DiagnosticManager::hasDiagnostic(const LanguageServerProtocol::DocumentUri &uri,
                                      const TextDocument *doc,
                                      const LanguageServerProtocol::Diagnostic &diag) const
{
    if (!doc)
        return false;
    const auto it = m_diagnostics.find(uri);
    if (it == m_diagnostics.end())
        return {};
    const int revision = m_client->documentVersion(uri.toFilePath());
    if (revision != it->version.value_or(revision))
        return false;
    return it->diagnostics.contains(diag);
}

bool DiagnosticManager::hasDiagnostics(const TextDocument *doc) const
{
    const FilePath docPath = doc->filePath();
    const auto it = m_diagnostics.find(DocumentUri::fromFilePath(docPath));
    if (it == m_diagnostics.end())
        return {};
    const int revision = m_client->documentVersion(docPath);
    if (revision != it->version.value_or(revision))
        return false;
    return !it->diagnostics.isEmpty();
}

DiagnosticManager::Marks::~Marks()
{
    qDeleteAll(marks);
}

} // namespace LanguageClient
