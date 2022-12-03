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
#include <languageserverprotocol/servercapabilities.h>
#include <texteditor/semantichighlighter.h>
#include <texteditor/textdocument.h>

#include <QSet>
#include <QTextCharFormat>

#include <functional>

namespace Core { class IEditor; }

namespace LanguageClient {
class Client;

class LANGUAGECLIENT_EXPORT ExpandedSemanticToken
{
public:
    friend bool operator==(const ExpandedSemanticToken &t1, const ExpandedSemanticToken &t2)
    {
        return t1.line == t2.line && t1.column == t2.column && t1.length == t2.length
                && t1.type == t2.type && t1.modifiers == t2.modifiers;
    }

    int line = -1;
    int column = -1;
    int length = -1;
    QString type;
    QStringList modifiers;
};

using SemanticTokensHandler = std::function<void(TextEditor::TextDocument *,
                                                 const QList<ExpandedSemanticToken> &, int, bool)>;

class SemanticTokenSupport : public QObject
{
public:
    explicit SemanticTokenSupport(Client *client);

    void refresh();
    void reloadSemanticTokens(TextEditor::TextDocument *doc);
    void updateSemanticTokens(TextEditor::TextDocument *doc);
    void clearHighlight(TextEditor::TextDocument *doc);
    void rehighlight();
    void setLegend(const LanguageServerProtocol::SemanticTokensLegend &legend);
    void clearTokens();

    void setTokenTypesMap(const QMap<QString, int> &tokenTypesMap);
    void setTokenModifiersMap(const QMap<QString, int> &tokenModifiersMap);

    void setAdditionalTokenTypeStyles(const QHash<int, TextEditor::TextStyle> &typeStyles);
    // TODO: currently only declaration and definition modifiers are supported. The TextStyles
    // mixin capabilities need to be extended to be able to support more
//    void setAdditionalTokenModifierStyles(const QHash<int, TextEditor::TextStyle> &modifierStyles);

    void setTokensHandler(const SemanticTokensHandler &handler) { m_tokensHandler = handler; }

private:
    void reloadSemanticTokensImpl(TextEditor::TextDocument *doc, int remainingRerequests = 3);
    void updateSemanticTokensImpl(TextEditor::TextDocument *doc, int remainingRerequests = 3);
    void queueDocumentReload(TextEditor::TextDocument *doc);
    LanguageServerProtocol::SemanticRequestTypes supportedSemanticRequests(
        TextEditor::TextDocument *document) const;
    void handleSemanticTokens(const Utils::FilePath &filePath,
                              const LanguageServerProtocol::SemanticTokensResult &result,
                              int documentVersion);
    void handleSemanticTokensDelta(const Utils::FilePath &filePath,
                                   const LanguageServerProtocol::SemanticTokensDeltaResult &result,
                                   int documentVersion);
    void highlight(const Utils::FilePath &filePath, bool force = false);
    void updateFormatHash();
    void currentEditorChanged();
    void onCurrentEditorChanged(Core::IEditor *editor);

    Client *m_client = nullptr;

    struct VersionedTokens
    {
        LanguageServerProtocol::SemanticTokens tokens;
        int version;
    };
    QHash<Utils::FilePath, VersionedTokens> m_tokens;
    QList<int> m_tokenTypes;
    QList<int> m_tokenModifiers;
    QHash<int, QTextCharFormat> m_formatHash;
    QHash<int, TextEditor::TextStyle> m_additionalTypeStyles;
//    QHash<int, TextEditor::TextStyle> m_additionalModifierStyles;
    QMap<QString, int> m_tokenTypesMap;
    QMap<QString, int> m_tokenModifiersMap;
    SemanticTokensHandler m_tokensHandler;
    QStringList m_tokenTypeStrings;
    QStringList m_tokenModifierStrings;
    QSet<TextEditor::TextDocument *> m_docReloadQueue;
    QHash<Utils::FilePath, LanguageServerProtocol::MessageId> m_runningRequests;
};

} // namespace LanguageClient
