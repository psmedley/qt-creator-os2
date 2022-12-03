/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "clangdfollowsymbol.h"

#include "clangdast.h"
#include "clangdclient.h"

#include <cppeditor/cppeditorwidget.h>
#include <cppeditor/cppvirtualfunctionassistprovider.h>
#include <cppeditor/cppvirtualfunctionproposalitem.h>
#include <languageclient/languageclientsymbolsupport.h>
#include <languageserverprotocol/lsptypes.h>
#include <languageserverprotocol/jsonrpcmessages.h>
#include <texteditor/codeassist/iassistprocessor.h>
#include <texteditor/codeassist/iassistprovider.h>
#include <texteditor/textdocument.h>

#include <QApplication>
#include <QPointer>

using namespace CppEditor;
using namespace LanguageServerProtocol;
using namespace TextEditor;
using namespace Utils;

namespace ClangCodeModel::Internal {
using SymbolData = QPair<QString, Link>;
using SymbolDataList = QList<SymbolData>;

class ClangdFollowSymbol::VirtualFunctionAssistProcessor : public IAssistProcessor
{
public:
    VirtualFunctionAssistProcessor(ClangdFollowSymbol *followSymbol)
        : m_followSymbol(followSymbol) {}

    void cancel() override { resetData(true); }
    bool running() override { return m_followSymbol; }
    void update();
    void finalize();
    void resetData(bool resetFollowSymbolData);

private:
    IAssistProposal *perform(const AssistInterface *) override
    {
        return nullptr;
    }

    IAssistProposal *immediateProposal(const AssistInterface *) override
    {
        return createProposal(false);
    }

    IAssistProposal *immediateProposalImpl() const;
    IAssistProposal *createProposal(bool final) const;
    VirtualFunctionProposalItem *createEntry(const QString &name, const Link &link) const;

    QPointer<ClangdFollowSymbol> m_followSymbol;
};

class ClangdFollowSymbol::VirtualFunctionAssistProvider : public IAssistProvider
{
public:
    VirtualFunctionAssistProvider(ClangdFollowSymbol *followSymbol)
        : m_followSymbol(followSymbol) {}

private:
    RunType runType() const override { return Asynchronous; }
    IAssistProcessor *createProcessor(const AssistInterface *) const override;

    const QPointer<ClangdFollowSymbol> m_followSymbol;
};

class ClangdFollowSymbol::Private
{
public:
    Private(ClangdFollowSymbol *q, ClangdClient *client, const QTextCursor &cursor,
            CppEditorWidget *editorWidget, const FilePath &filePath, const LinkHandler &callback,
            bool openInSplit)
        : q(q), client(client), cursor(cursor), editorWidget(editorWidget),
          uri(DocumentUri::fromFilePath(filePath)), callback(callback),
          virtualFuncAssistProvider(q),
          docRevision(editorWidget ? editorWidget->textDocument()->document()->revision() : -1),
          openInSplit(openInSplit) {}

    void handleGotoDefinitionResult();
    void sendGotoImplementationRequest(const Utils::Link &link);
    void handleGotoImplementationResult(const GotoImplementationRequest::Response &response);
    void handleDocumentInfoResults();
    void closeTempDocuments();
    bool addOpenFile(const FilePath &filePath);
    bool defLinkIsAmbiguous() const;

    ClangdFollowSymbol * const q;
    ClangdClient * const client;
    const QTextCursor cursor;
    const QPointer<CppEditor::CppEditorWidget> editorWidget;
    const DocumentUri uri;
    const LinkHandler callback;
    VirtualFunctionAssistProvider virtualFuncAssistProvider;
    QList<MessageId> pendingSymbolInfoRequests;
    QList<MessageId> pendingGotoImplRequests;
    QList<MessageId> pendingGotoDefRequests;
    const int docRevision;
    const bool openInSplit;

    Link defLink;
    Links allLinks;
    QHash<Link, Link> declDefMap;
    optional<ClangdAstNode> cursorNode;
    ClangdAstNode defLinkNode;
    SymbolDataList symbolsToDisplay;
    std::set<FilePath> openedFiles;
    VirtualFunctionAssistProcessor *virtualFuncAssistProcessor = nullptr;
    QMetaObject::Connection focusChangedConnection;
    bool done = false;
};

ClangdFollowSymbol::ClangdFollowSymbol(ClangdClient *client, const QTextCursor &cursor,
        CppEditorWidget *editorWidget, TextDocument *document, const LinkHandler &callback,
        bool openInSplit)
    : QObject(client),
      d(new Private(this, client, cursor, editorWidget, document->filePath(), callback,
                    openInSplit))
{
    // Abort if the user does something else with the document in the meantime.
    connect(document, &TextDocument::contentsChanged, this, [this] { emitDone(); },
            Qt::QueuedConnection);
    if (editorWidget) {
        connect(editorWidget, &CppEditorWidget::cursorPositionChanged,
                this, [this] { emitDone(); }, Qt::QueuedConnection);
    }
    d->focusChangedConnection = connect(qApp, &QApplication::focusChanged,
                                        this, [this] { emitDone(); }, Qt::QueuedConnection);

    // Step 1: Follow the symbol via "Go to Definition". At the same time, request the
    //         AST node corresponding to the cursor position, so we can find out whether
    //         we have to look for overrides.
    const auto gotoDefCallback = [self = QPointer(this)](const Utils::Link &link) {
        qCDebug(clangdLog) << "received go to definition response";
        if (!self)
            return;
        if (!link.hasValidTarget()) {
            self->emitDone();
            return;
        }
        self->d->defLink = link;
        if (self->d->cursorNode)
            self->d->handleGotoDefinitionResult();
    };
    client->symbolSupport().findLinkAt(document, cursor, std::move(gotoDefCallback), true);

    const auto astHandler = [self = QPointer(this)](const ClangdAstNode &ast, const MessageId &) {
        qCDebug(clangdLog) << "received ast response for cursor";
        if (!self)
            return;
        self->d->cursorNode = ast;
        if (self->d->defLink.hasValidTarget())
            self->d->handleGotoDefinitionResult();
    };
    client->getAndHandleAst(document, astHandler, ClangdClient::AstCallbackMode::AlwaysAsync,
                            Range(cursor));
}

ClangdFollowSymbol::~ClangdFollowSymbol()
{
    d->closeTempDocuments();
    if (d->virtualFuncAssistProcessor)
        d->virtualFuncAssistProcessor->resetData(false);
    for (const MessageId &id : qAsConst(d->pendingSymbolInfoRequests))
        d->client->cancelRequest(id);
    for (const MessageId &id : qAsConst(d->pendingGotoImplRequests))
        d->client->cancelRequest(id);
    for (const MessageId &id : qAsConst(d->pendingGotoDefRequests))
        d->client->cancelRequest(id);
    delete d;
}

void ClangdFollowSymbol::clear()
{
    d->openedFiles.clear();
    d->pendingSymbolInfoRequests.clear();
    d->pendingGotoImplRequests.clear();
    d->pendingGotoDefRequests.clear();
}

void ClangdFollowSymbol::emitDone(const Link &link)
{
    if (d->done)
        return;

    d->done = true;
    if (link.hasValidTarget())
        d->callback(link);
    emit done();
}

bool ClangdFollowSymbol::Private::defLinkIsAmbiguous() const
{
    // Even if the call is to a virtual function, it might not be ambiguous:
    // class A { virtual void f(); }; class B : public A { void f() override { A::f(); } };
    if (!cursorNode->mightBeAmbiguousVirtualCall() && !cursorNode->isPureVirtualDeclaration())
        return false;

    // If we have up-to-date highlighting info, we know whether we are dealing with
    // a virtual call.
    if (editorWidget) {
        const auto result = client->hasVirtualFunctionAt(editorWidget->textDocument(),
                                                         docRevision, cursorNode->range());
        if (result.has_value())
            return *result;
    }

    // Otherwise, we accept potentially doing more work than needed rather than not catching
    // possible overrides.
    return true;
}

bool ClangdFollowSymbol::Private::addOpenFile(const FilePath &filePath)
{
    return openedFiles.insert(filePath).second;
}

void ClangdFollowSymbol::Private::handleDocumentInfoResults()
{
    closeTempDocuments();

    // If something went wrong, we just follow the original link.
    if (symbolsToDisplay.isEmpty()) {
        q->emitDone(defLink);
        return;
    }

    if (symbolsToDisplay.size() == 1) {
        q->emitDone(symbolsToDisplay.first().second);
        return;
    }

    QTC_ASSERT(virtualFuncAssistProcessor && virtualFuncAssistProcessor->running(),
               q->emitDone(); return);
    virtualFuncAssistProcessor->finalize();
}

void ClangdFollowSymbol::Private::sendGotoImplementationRequest(const Link &link)
{
    if (!client->documentForFilePath(link.targetFilePath) && addOpenFile(link.targetFilePath))
        client->openExtraFile(link.targetFilePath);
    const Position position(link.targetLine - 1, link.targetColumn);
    const TextDocumentIdentifier documentId(DocumentUri::fromFilePath(link.targetFilePath));
    GotoImplementationRequest req(TextDocumentPositionParams(documentId, position));
    req.setResponseCallback([sentinel = QPointer(q), this, reqId = req.id()]
                            (const GotoImplementationRequest::Response &response) {
        qCDebug(clangdLog) << "received go to implementation reply";
        if (!sentinel)
            return;
        pendingGotoImplRequests.removeOne(reqId);
        handleGotoImplementationResult(response);
    });
    client->sendMessage(req, ClangdClient::SendDocUpdates::Ignore);
    pendingGotoImplRequests << req.id();
    qCDebug(clangdLog) << "sending go to implementation request" << link.targetLine;
}

void ClangdFollowSymbol::VirtualFunctionAssistProcessor::update()
{
    if (!m_followSymbol->d->editorWidget)
        return;
    setAsyncProposalAvailable(createProposal(false));
}

void ClangdFollowSymbol::VirtualFunctionAssistProcessor::finalize()
{
    if (!m_followSymbol->d->editorWidget)
        return;
    const auto proposal = createProposal(true);
    if (m_followSymbol->d->editorWidget->isInTestMode()) {
        m_followSymbol->d->symbolsToDisplay.clear();
        const auto immediateProposal = createProposal(false);
        m_followSymbol->d->editorWidget->setProposals(immediateProposal, proposal);
    } else {
        setAsyncProposalAvailable(proposal);
    }
    resetData(true);
}

void ClangdFollowSymbol::VirtualFunctionAssistProcessor::resetData(bool resetFollowSymbolData)
{
    if (!m_followSymbol)
        return;
    m_followSymbol->d->virtualFuncAssistProcessor = nullptr;
    if (resetFollowSymbolData)
        m_followSymbol->emitDone();
    m_followSymbol = nullptr;
}

IAssistProposal *
ClangdFollowSymbol::VirtualFunctionAssistProcessor::createProposal(bool final) const
{
    QTC_ASSERT(m_followSymbol, return nullptr);

    QList<AssistProposalItemInterface *> items;
    bool needsBaseDeclEntry = !m_followSymbol->d->defLinkNode.range()
            .contains(Position(m_followSymbol->d->cursor));
    for (const SymbolData &symbol : qAsConst(m_followSymbol->d->symbolsToDisplay)) {
        Link link = symbol.second;
        if (m_followSymbol->d->defLink == link) {
            if (!needsBaseDeclEntry)
                continue;
            needsBaseDeclEntry = false;
        } else {
            const Link defLink = m_followSymbol->d->declDefMap.value(symbol.second);
            if (defLink.hasValidTarget())
                link = defLink;
        }
        items << createEntry(symbol.first, link);
    }
    if (needsBaseDeclEntry)
        items << createEntry({}, m_followSymbol->d->defLink);
    if (!final) {
        const auto infoItem = new VirtualFunctionProposalItem({}, false);
        infoItem->setText(ClangdClient::tr("collecting overrides ..."));
        infoItem->setOrder(-1);
        items << infoItem;
    }

    return new VirtualFunctionProposal(m_followSymbol->d->cursor.position(), items,
                                       m_followSymbol->d->openInSplit);
}

CppEditor::VirtualFunctionProposalItem *
ClangdFollowSymbol::VirtualFunctionAssistProcessor::createEntry(const QString &name,
                                                                const Link &link) const
{
    const auto item = new VirtualFunctionProposalItem(link, m_followSymbol->d->openInSplit);
    QString text = name;
    if (link == m_followSymbol->d->defLink) {
        item->setOrder(1000); // Ensure base declaration is on top.
        if (text.isEmpty()) {
            text = ClangdClient::tr("<base declaration>");
        } else if (m_followSymbol->d->defLinkNode.isPureVirtualDeclaration()
                   || m_followSymbol->d->defLinkNode.isPureVirtualDefinition()) {
            text += " = 0";
        }
    }
    item->setText(text);
    return item;
}

IAssistProcessor *
ClangdFollowSymbol::VirtualFunctionAssistProvider::createProcessor(const AssistInterface *) const
{
    return m_followSymbol->d->virtualFuncAssistProcessor
            = new VirtualFunctionAssistProcessor(m_followSymbol);
}

void ClangdFollowSymbol::Private::handleGotoDefinitionResult()
{
    QTC_ASSERT(defLink.hasValidTarget(), return);

    qCDebug(clangdLog) << "handling go to definition result";

    // No dis-ambiguation necessary. Call back with the link and finish.
    if (!defLinkIsAmbiguous()) {
        q->emitDone(defLink);
        return;
    }

    // Step 2: Get all possible overrides via "Go to Implementation".
    // Note that we have to do this for all member function calls, because
    // we cannot tell here whether the member function is virtual.
    allLinks << defLink;
    sendGotoImplementationRequest(defLink);
}

void ClangdFollowSymbol::Private::handleGotoImplementationResult(
        const GotoImplementationRequest::Response &response)
{
    if (const optional<GotoResult> &result = response.result()) {
        QList<Link> newLinks;
        if (const auto ploc = get_if<Location>(&*result))
            newLinks = {ploc->toLink()};
        if (const auto plloc = get_if<QList<Location>>(&*result))
            newLinks = transform(*plloc, &Location::toLink);
        for (const Link &link : qAsConst(newLinks)) {
            if (!allLinks.contains(link)) {
                allLinks << link;

                // We must do this recursively, because clangd reports only the first
                // level of overrides.
                sendGotoImplementationRequest(link);
            }
        }
    }

    // We didn't find any further candidates, so jump to the original definition link.
    if (allLinks.size() == 1 && pendingGotoImplRequests.isEmpty()) {
        q->emitDone(allLinks.first());
        return;
    }

    // As soon as we know that there is more than one candidate, we start the code assist
    // procedure, to let the user know that things are happening.
    if (allLinks.size() > 1 && !virtualFuncAssistProcessor && editorWidget) {
        QObject::disconnect(focusChangedConnection);
        editorWidget->invokeTextEditorWidgetAssist(FollowSymbol, &virtualFuncAssistProvider);
    }

    if (!pendingGotoImplRequests.isEmpty())
        return;

    // Step 3: We are done looking for overrides, and we found at least one.
    //         Make a symbol info request for each link to get the class names.
    //         Also get the AST for the base declaration, so we can find out whether it's
    //         pure virtual and mark it accordingly.
    //         In addition, we need to follow all override links, because for these, clangd
    //         gives us the declaration instead of the definition.
    for (const Link &link : qAsConst(allLinks)) {
        if (!client->documentForFilePath(link.targetFilePath) && addOpenFile(link.targetFilePath))
            client->openExtraFile(link.targetFilePath);
        const auto symbolInfoHandler = [sentinel = QPointer(q), this, link](
                const QString &name, const QString &prefix, const MessageId &reqId) {
            qCDebug(clangdLog) << "handling symbol info reply"
                               << link.targetFilePath.toUserOutput() << link.targetLine;
            if (!sentinel)
                return;
            if (!name.isEmpty())
                symbolsToDisplay << qMakePair(prefix + name, link);
            pendingSymbolInfoRequests.removeOne(reqId);
            virtualFuncAssistProcessor->update();
            if (pendingSymbolInfoRequests.isEmpty() && pendingGotoDefRequests.isEmpty()
                    && defLinkNode.isValid()) {
                handleDocumentInfoResults();
            }
        };
        const Position pos(link.targetLine - 1, link.targetColumn);
        const MessageId reqId = client->requestSymbolInfo(link.targetFilePath, pos,
                                                          symbolInfoHandler);
        pendingSymbolInfoRequests << reqId;
        qCDebug(clangdLog) << "sending symbol info request";

        if (link == defLink)
            continue;

        const TextDocumentIdentifier doc(DocumentUri::fromFilePath(link.targetFilePath));
        const TextDocumentPositionParams params(doc, pos);
        GotoDefinitionRequest defReq(params);
        defReq.setResponseCallback([this, link, sentinel = QPointer(q), reqId = defReq.id()]
                (const GotoDefinitionRequest::Response &response) {
            qCDebug(clangdLog) << "handling additional go to definition reply for"
                               << link.targetFilePath << link.targetLine;
            if (!sentinel)
                return;
            Link newLink;
            if (optional<GotoResult> _result = response.result()) {
                const GotoResult result = _result.value();
                if (const auto ploc = get_if<Location>(&result)) {
                    newLink = ploc->toLink();
                } else if (const auto plloc = get_if<QList<Location>>(&result)) {
                    if (!plloc->isEmpty())
                        newLink = plloc->value(0).toLink();
                }
            }
            qCDebug(clangdLog) << "def link is" << newLink.targetFilePath << newLink.targetLine;
            declDefMap.insert(link, newLink);
            pendingGotoDefRequests.removeOne(reqId);
            if (pendingSymbolInfoRequests.isEmpty() && pendingGotoDefRequests.isEmpty()
                    && defLinkNode.isValid()) {
                handleDocumentInfoResults();
            }
        });
        pendingGotoDefRequests << defReq.id();
        qCDebug(clangdLog) << "sending additional go to definition request"
                           << link.targetFilePath << link.targetLine;
        client->sendMessage(defReq, ClangdClient::SendDocUpdates::Ignore);
    }

    const FilePath defLinkFilePath = defLink.targetFilePath;
    const TextDocument * const defLinkDoc = client->documentForFilePath(defLinkFilePath);
    const auto defLinkDocVariant = defLinkDoc ? ClangdClient::TextDocOrFile(defLinkDoc)
                                              : ClangdClient::TextDocOrFile(defLinkFilePath);
    const Position defLinkPos(defLink.targetLine - 1, defLink.targetColumn);
    const auto astHandler = [this, sentinel = QPointer(q)]
            (const ClangdAstNode &ast, const MessageId &) {
        qCDebug(clangdLog) << "received ast response for def link";
        if (!sentinel)
            return;
        defLinkNode = ast;
        if (pendingSymbolInfoRequests.isEmpty() && pendingGotoDefRequests.isEmpty())
            handleDocumentInfoResults();
    };
    client->getAndHandleAst(defLinkDocVariant, astHandler,
                            ClangdClient::AstCallbackMode::AlwaysAsync,
                            Range(defLinkPos, defLinkPos));
}

void ClangdFollowSymbol::Private::closeTempDocuments()
{
    for (const FilePath &fp : qAsConst(openedFiles)) {
        if (!client->documentForFilePath(fp))
            client->closeExtraFile(fp);
    }
    openedFiles.clear();
}

} // namespace ClangCodeModel::Internal
