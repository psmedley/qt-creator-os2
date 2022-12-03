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

#include "languageclientquickfix.h"

#include "client.h"
#include "languageclientutils.h"

#include <texteditor/codeassist/assistinterface.h>
#include <texteditor/codeassist/genericproposal.h>
#include <texteditor/quickfix.h>


using namespace LanguageServerProtocol;
using namespace TextEditor;

namespace LanguageClient {

CodeActionQuickFixOperation::CodeActionQuickFixOperation(const CodeAction &action, Client *client)
    : m_action(action)
    , m_client(client)
{
    setDescription(action.title());
}

void CodeActionQuickFixOperation::perform()
{
    if (!m_client)
        return;
    if (Utils::optional<WorkspaceEdit> edit = m_action.edit())
        applyWorkspaceEdit(m_client, *edit);
    else if (Utils::optional<Command> command = m_action.command())
        m_client->executeCommand(*command);
}

CommandQuickFixOperation::CommandQuickFixOperation(const Command &command, Client *client)
    : m_command(command)
    , m_client(client)
{ setDescription(command.title()); }


void CommandQuickFixOperation::perform()
{
    if (m_client)
        m_client->executeCommand(m_command);
}

IAssistProposal *LanguageClientQuickFixAssistProcessor::perform(const AssistInterface *interface)
{
    m_assistInterface = QSharedPointer<const AssistInterface>(interface);

    CodeActionParams params;
    params.setContext({});
    QTextCursor cursor = interface->cursor();
    if (!cursor.hasSelection()) {
        if (cursor.atBlockEnd() || cursor.atBlockStart())
            cursor.select(QTextCursor::LineUnderCursor);
        else
            cursor.select(QTextCursor::WordUnderCursor);
    }
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::LineUnderCursor);
    Range range(cursor);
    params.setRange(range);
    auto uri = DocumentUri::fromFilePath(interface->filePath());
    params.setTextDocument(TextDocumentIdentifier(uri));
    CodeActionParams::CodeActionContext context;
    context.setDiagnostics(m_client->diagnosticsAt(uri, cursor));
    params.setContext(context);

    CodeActionRequest request(params);
    request.setResponseCallback([this](const CodeActionRequest::Response &response){
        handleCodeActionResponse(response);
    });

    m_client->addAssistProcessor(this);
    m_client->requestCodeActions(request);
    m_currentRequest = request.id();
    return nullptr;
}

void LanguageClientQuickFixAssistProcessor::cancel()
{
    if (running()) {
        m_client->cancelRequest(*m_currentRequest);
        m_client->removeAssistProcessor(this);
        m_currentRequest.reset();
    }
}

void LanguageClientQuickFixAssistProcessor::handleCodeActionResponse(const CodeActionRequest::Response &response)
{
    m_currentRequest.reset();
    if (const Utils::optional<CodeActionRequest::Response::Error> &error = response.error())
        m_client->log(*error);
    m_client->removeAssistProcessor(this);
    GenericProposal *proposal = nullptr;
    if (const Utils::optional<CodeActionResult> &result = response.result())
        proposal = handleCodeActionResult(*result);
    setAsyncProposalAvailable(proposal);
}

GenericProposal *LanguageClientQuickFixAssistProcessor::handleCodeActionResult(const CodeActionResult &result)
{
    if (auto list = Utils::get_if<QList<Utils::variant<Command, CodeAction>>>(&result)) {
        QuickFixOperations ops;
        for (const Utils::variant<Command, CodeAction> &item : *list) {
            if (auto action = Utils::get_if<CodeAction>(&item))
                ops << new CodeActionQuickFixOperation(*action, m_client);
            else if (auto command = Utils::get_if<Command>(&item))
                ops << new CommandQuickFixOperation(*command, m_client);
        }
        return GenericProposal::createProposal(m_assistInterface.data(), ops);
    }
    return nullptr;
}

LanguageClientQuickFixProvider::LanguageClientQuickFixProvider(Client *client)
    : IAssistProvider(client)
    , m_client(client)
{
    QTC_CHECK(client);
}

IAssistProvider::RunType LanguageClientQuickFixProvider::runType() const
{
    return Asynchronous;
}

IAssistProcessor *LanguageClientQuickFixProvider::createProcessor(const AssistInterface *) const
{
    return new LanguageClientQuickFixAssistProcessor(m_client);
}

} // namespace LanguageClient
