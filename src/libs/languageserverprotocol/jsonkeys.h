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

#pragma once

namespace LanguageServerProtocol {

constexpr char16_t actionsKey[] = u"actions";
constexpr char16_t activeParameterKey[] = u"activeParameter";
constexpr char16_t activeParameterSupportKey[] = u"activeParameterSupport";
constexpr char16_t activeSignatureKey[] = u"activeSignature";
constexpr char16_t addedKey[] = u"added";
constexpr char16_t additionalTextEditsKey[] = u"additionalTextEdits";
constexpr char16_t alphaKey[] = u"alpha";
constexpr char16_t appliedKey[] = u"applied";
constexpr char16_t applyEditKey[] = u"applyEdit";
constexpr char16_t argumentsKey[] = u"arguments";
constexpr char16_t blueKey[] = u"blue";
constexpr char16_t cancellableKey[] = u"cancellable";
constexpr char16_t capabilitiesKey[] = u"capabilities";
constexpr char16_t chKey[] = u"ch";
constexpr char16_t changeKey[] = u"change";
constexpr char16_t changeNotificationsKey[] = u"changeNotifications";
constexpr char16_t changesKey[] = u"changes";
constexpr char16_t characterKey[] = u"character";
constexpr char16_t childrenKey[] = u"children";
constexpr char16_t clientInfoKey[] = u"clientInfo";
constexpr char16_t codeActionKey[] = u"codeAction";
constexpr char16_t codeActionKindKey[] = u"codeActionKind";
constexpr char16_t codeActionKindsKey[] = u"codeActionKinds";
constexpr char16_t codeActionLiteralSupportKey[] = u"codeActionLiteralSupport";
constexpr char16_t codeActionProviderKey[] = u"codeActionProvider";
constexpr char16_t codeKey[] = u"code";
constexpr char16_t codeLensKey[] = u"codeLens";
constexpr char16_t codeLensProviderKey[] = u"codeLensProvider";
constexpr char16_t colorInfoKey[] = u"colorInfo";
constexpr char16_t colorKey[] = u"color";
constexpr char16_t colorProviderKey[] = u"colorProvider";
constexpr char16_t commandKey[] = u"command";
constexpr char16_t commandsKey[] = u"commands";
constexpr char16_t commitCharacterSupportKey[] = u"commitCharacterSupport";
constexpr char16_t commitCharactersKey[] = u"commitCharacters";
constexpr char16_t completionItemKey[] = u"completionItem";
constexpr char16_t completionItemKindKey[] = u"completionItemKind";
constexpr char16_t completionKey[] = u"completion";
constexpr char16_t completionProviderKey[] = u"completionProvider";
constexpr char16_t configurationKey[] = u"configuration";
constexpr char16_t containerNameKey[] = u"containerName";
constexpr char16_t contentChangesKey[] = u"contentChanges";
constexpr char16_t contentFormatKey[] = u"contentFormat";
constexpr char16_t contentKey[] = u"value";
constexpr char16_t contentsKey[] = u"contents";
constexpr char16_t contextKey[] = u"context";
constexpr char16_t contextSupportKey[] = u"contextSupport";
constexpr char16_t dataKey[] = u"data";
constexpr char16_t definitionKey[] = u"definition";
constexpr char16_t definitionProviderKey[] = u"definitionProvider";
constexpr char16_t deleteCountKey[] = u"deleteCount";
constexpr char16_t deltaKey[] = u"delta";
constexpr char16_t deprecatedKey[] = u"deprecated";
constexpr char16_t detailKey[] = u"detail";
constexpr char16_t diagnosticsKey[] = u"diagnostics";
constexpr char16_t didChangeConfigurationKey[] = u"didChangeConfiguration";
constexpr char16_t didChangeWatchedFilesKey[] = u"didChangeWatchedFiles";
constexpr char16_t didSaveKey[] = u"didSave";
constexpr char16_t documentChangesKey[] = u"documentChanges";
constexpr char16_t documentFormattingProviderKey[] = u"documentFormattingProvider";
constexpr char16_t documentHighlightKey[] = u"documentHighlight";
constexpr char16_t documentHighlightProviderKey[] = u"documentHighlightProvider";
constexpr char16_t documentLinkKey[] = u"documentLink";
constexpr char16_t documentLinkProviderKey[] = u"documentLinkProvider";
constexpr char16_t documentRangeFormattingProviderKey[] = u"documentRangeFormattingProvider";
constexpr char16_t documentSelectorKey[] = u"documentSelector";
constexpr char16_t documentSymbolKey[] = u"documentSymbol";
constexpr char16_t documentSymbolProviderKey[] = u"documentSymbolProvider";
constexpr char16_t documentationFormatKey[] = u"documentationFormat";
constexpr char16_t documentationKey[] = u"documentation";
constexpr char16_t dynamicRegistrationKey[] = u"dynamicRegistration";
constexpr char16_t editKey[] = u"edit";
constexpr char16_t editsKey[] = u"edits";
constexpr char16_t endKey[] = u"end";
constexpr char16_t errorKey[] = u"error";
constexpr char16_t eventKey[] = u"event";
constexpr char16_t executeCommandKey[] = u"executeCommand";
constexpr char16_t executeCommandProviderKey[] = u"executeCommandProvider";
constexpr char16_t experimentalKey[] = u"experimental";
constexpr char16_t filterTextKey[] = u"filterText";
constexpr char16_t firstTriggerCharacterKey[] = u"firstTriggerCharacter";
constexpr char16_t formatsKey[] = u"formats";
constexpr char16_t formattingKey[] = u"formatting";
constexpr char16_t fullKey[] = u"full";
constexpr char16_t greenKey[] = u"green";
constexpr char16_t hierarchicalDocumentSymbolSupportKey[] = u"hierarchicalDocumentSymbolSupport";
constexpr char16_t hoverKey[] = u"hover";
constexpr char16_t hoverProviderKey[] = u"hoverProvider";
constexpr char16_t idKey[] = u"id";
constexpr char16_t implementationKey[] = u"implementation";
constexpr char16_t implementationProviderKey[] = u"implementationProvider";
constexpr char16_t includeDeclarationKey[] = u"includeDeclaration";
constexpr char16_t includeTextKey[] = u"includeText";
constexpr char16_t initializationOptionsKey[] = u"initializationOptions";
constexpr char16_t insertFinalNewlineKey[] = u"insertFinalNewline";
constexpr char16_t insertSpaceKey[] = u"insertSpace";
constexpr char16_t insertTextFormatKey[] = u"insertTextFormat";
constexpr char16_t insertTextKey[] = u"insertText";
constexpr char16_t isIncompleteKey[] = u"isIncomplete";
constexpr char16_t itemsKey[] = u"items";
constexpr char16_t jsonRpcVersionKey[] = u"jsonrpc";
constexpr char16_t kindKey[] = u"kind";
constexpr char16_t labelKey[] = u"label";
constexpr char16_t languageIdKey[] = u"languageId";
constexpr char16_t languageKey[] = u"language";
constexpr char16_t legendKey[] = u"legend";
constexpr char16_t lineKey[] = u"line";
constexpr char16_t linesKey[] = u"lines";
constexpr char16_t locationKey[] = u"location";
constexpr char16_t messageKey[] = u"message";
constexpr char16_t methodKey[] = u"method";
constexpr char16_t moreTriggerCharacterKey[] = u"moreTriggerCharacter";
constexpr char16_t multiLineTokenSupportKey[] = u"multiLineTokenSupport";
constexpr char16_t nameKey[] = u"name";
constexpr char16_t newNameKey[] = u"newName";
constexpr char16_t newTextKey[] = u"newText";
constexpr char16_t onTypeFormattingKey[] = u"onTypeFormatting";
constexpr char16_t onlyKey[] = u"only";
constexpr char16_t openCloseKey[] = u"openClose";
constexpr char16_t optionsKey[] = u"options";
constexpr char16_t overlappingTokenSupportKey[] = u"overlappingTokenSupport";
constexpr char16_t parametersKey[] = u"parameters";
constexpr char16_t paramsKey[] = u"params";
constexpr char16_t patternKey[] = u"pattern";
constexpr char16_t percentageKey[] = u"percentage";
constexpr char16_t placeHolderKey[] = u"placeHolder";
constexpr char16_t positionKey[] = u"position";
constexpr char16_t prepareProviderKey[] = u"prepareProvider";
constexpr char16_t prepareSupportKey[] = u"prepareSupport";
constexpr char16_t previousResultIdKey[] = u"previousResultId";
constexpr char16_t processIdKey[] = u"processId";
constexpr char16_t queryKey[] = u"query";
constexpr char16_t rangeFormattingKey[] = u"rangeFormatting";
constexpr char16_t rangeKey[] = u"range";
constexpr char16_t rangeLengthKey[] = u"rangeLength";
constexpr char16_t reasonKey[] = u"reason";
constexpr char16_t redKey[] = u"red";
constexpr char16_t referencesKey[] = u"references";
constexpr char16_t referencesProviderKey[] = u"referencesProvider";
constexpr char16_t refreshSupportKey[] = u"refreshSupport";
constexpr char16_t registerOptionsKey[] = u"registerOptions";
constexpr char16_t registrationsKey[] = u"registrations";
constexpr char16_t removedKey[] = u"removed";
constexpr char16_t renameKey[] = u"rename";
constexpr char16_t renameProviderKey[] = u"renameProvider";
constexpr char16_t requestsKey[] = u"requests";
constexpr char16_t resolveProviderKey[] = u"resolveProvider";
constexpr char16_t resultIdKey[] = u"resultId";
constexpr char16_t resultKey[] = u"result";
constexpr char16_t retryKey[] = u"retry";
constexpr char16_t rootPathKey[] = u"rootPath";
constexpr char16_t rootUriKey[] = u"rootUri";
constexpr char16_t saveKey[] = u"save";
constexpr char16_t schemeKey[] = u"scheme";
constexpr char16_t scopeUriKey[] = u"scopeUri";
constexpr char16_t sectionKey[] = u"section";
constexpr char16_t selectionRangeKey[] = u"selectionRange";
constexpr char16_t semanticTokensKey[] = u"semanticTokens";
constexpr char16_t semanticTokensProviderKey[] = u"semanticTokensProvider";
constexpr char16_t serverInfoKey[] = u"serverInfo";
constexpr char16_t settingsKey[] = u"settings";
constexpr char16_t severityKey[] = u"severity";
constexpr char16_t signatureHelpKey[] = u"signatureHelp";
constexpr char16_t signatureHelpProviderKey[] = u"signatureHelpProvider";
constexpr char16_t signatureInformationKey[] = u"signatureInformation";
constexpr char16_t signaturesKey[] = u"signatures";
constexpr char16_t snippetSupportKey[] = u"snippetSupport";
constexpr char16_t sortTextKey[] = u"sortText";
constexpr char16_t sourceKey[] = u"source";
constexpr char16_t startKey[] = u"start";
constexpr char16_t supportedKey[] = u"supported";
constexpr char16_t symbolKey[] = u"symbol";
constexpr char16_t symbolKindKey[] = u"symbolKind";
constexpr char16_t syncKindKey[] = u"syncKind";
constexpr char16_t synchronizationKey[] = u"synchronization";
constexpr char16_t tabSizeKey[] = u"tabSize";
constexpr char16_t tagsKey[] = u"tags";
constexpr char16_t targetKey[] = u"target";
constexpr char16_t textDocumentKey[] = u"textDocument";
constexpr char16_t textDocumentSyncKey[] = u"textDocumentSync";
constexpr char16_t textEditKey[] = u"textEdit";
constexpr char16_t textKey[] = u"text";
constexpr char16_t titleKey[] = u"title";
constexpr char16_t tokenKey[] = u"token";
constexpr char16_t tokenModifiersKey[] = u"tokenModifiers";
constexpr char16_t tokenTypesKey[] = u"tokenTypes";
constexpr char16_t traceKey[] = u"trace";
constexpr char16_t triggerCharacterKey[] = u"triggerCharacter";
constexpr char16_t triggerCharactersKey[] = u"triggerCharacters";
constexpr char16_t triggerKindKey[] = u"triggerKind";
constexpr char16_t trimFinalNewlinesKey[] = u"trimFinalNewlines";
constexpr char16_t trimTrailingWhitespaceKey[] = u"trimTrailingWhitespace";
constexpr char16_t typeDefinitionKey[] = u"typeDefinition";
constexpr char16_t typeDefinitionProviderKey[] = u"typeDefinitionProvider";
constexpr char16_t typeKey[] = u"type";
constexpr char16_t unregistrationsKey[] = u"unregistrations";
constexpr char16_t uriKey[] = u"uri";
constexpr char16_t valueKey[] = u"value";
constexpr char16_t valueSetKey[] = u"valueSet";
constexpr char16_t versionKey[] = u"version";
constexpr char16_t willSaveKey[] = u"willSave";
constexpr char16_t willSaveWaitUntilKey[] = u"willSaveWaitUntil";
constexpr char16_t windowKey[] = u"window";
constexpr char16_t workDoneProgressKey[] = u"workDoneProgress";
constexpr char16_t workspaceEditKey[] = u"workspaceEdit";
constexpr char16_t workspaceFoldersKey[] = u"workspaceFolders";
constexpr char16_t workspaceKey[] = u"workspace";
constexpr char16_t workspaceSymbolProviderKey[] = u"workspaceSymbolProvider";

} // namespace LanguageServerProtocol
