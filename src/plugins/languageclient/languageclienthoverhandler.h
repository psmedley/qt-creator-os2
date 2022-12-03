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

#pragma once

#include "languageclient_global.h"

#include <languageserverprotocol/languagefeatures.h>
#include <texteditor/basehoverhandler.h>

#include <functional>

namespace LanguageClient {

class Client;

using HelpItemProvider = std::function<void(const LanguageServerProtocol::HoverRequest::Response &,
                                            const LanguageServerProtocol::DocumentUri &uri)>;

class LANGUAGECLIENT_EXPORT HoverHandler final : public TextEditor::BaseHoverHandler
{
    Q_DECLARE_TR_FUNCTIONS(HoverHandler)
public:
    explicit HoverHandler(Client *client);
    ~HoverHandler() override;

    void abort() override;

    /// If prefer diagnostics is enabled the hover handler checks whether a diagnostics is at the
    /// pos passed to identifyMatch _before_ sending hover request to the server. If a diagnostic
    /// can be found it will be used as a tooltip and no hover request is sent to the server.
    /// If prefer diagnostics is disabled the diagnostics are only checked if the response is empty.
    /// Defaults to prefer diagnostics.
    void setPreferDiagnosticts(bool prefer);

    void setHelpItemProvider(const HelpItemProvider &provider) { m_helpItemProvider = provider; }
    void setHelpItem(const LanguageServerProtocol::MessageId &msgId, const Core::HelpItem &help);

protected:
    void identifyMatch(TextEditor::TextEditorWidget *editorWidget,
                       int pos,
                       ReportPriority report) override;

private:
    void handleResponse(const LanguageServerProtocol::HoverRequest::Response &response,
                        const QTextCursor &cursor);
    void setContent(const LanguageServerProtocol::HoverContent &content);
    bool reportDiagnostics(const QTextCursor &cursor);

    QPointer<Client> m_client;
    Utils::optional<LanguageServerProtocol::MessageId> m_currentRequest;
    LanguageServerProtocol::DocumentUri m_uri;
    LanguageServerProtocol::HoverRequest::Response m_response;
    TextEditor::BaseHoverHandler::ReportPriority m_report;
    HelpItemProvider m_helpItemProvider;
    bool m_preferDiagnostics = true;
};

} // namespace LanguageClient
