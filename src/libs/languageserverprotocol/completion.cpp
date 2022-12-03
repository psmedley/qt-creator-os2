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

#include "completion.h"

namespace LanguageServerProtocol {

constexpr const char CompletionRequest::methodName[];
constexpr const char CompletionItemResolveRequest::methodName[];

CompletionRequest::CompletionRequest(const CompletionParams &params)
    : Request(methodName, params)
{ }

Utils::optional<MarkupOrString> CompletionItem::documentation() const
{
    QJsonValue documentation = value(documentationKey);
    if (documentation.isUndefined())
        return Utils::nullopt;
    return MarkupOrString(documentation);
}

Utils::optional<CompletionItem::InsertTextFormat> CompletionItem::insertTextFormat() const
{
    if (Utils::optional<int> value = optionalValue<int>(insertTextFormatKey))
        return Utils::make_optional(CompletionItem::InsertTextFormat(*value));
    return Utils::nullopt;
}

Utils::optional<QList<CompletionItem::CompletionItemTag>> CompletionItem::tags() const
{
    if (const auto value = optionalValue<QJsonArray>(tagsKey)) {
        QList<CompletionItemTag> tags;
        for (auto it = value->cbegin(); it != value->cend(); ++it)
            tags << static_cast<CompletionItemTag>(it->toInt());
        return tags;
    }
    return {};
}

CompletionItemResolveRequest::CompletionItemResolveRequest(const CompletionItem &params)
    : Request(methodName, params)
{ }

CompletionResult::CompletionResult(const QJsonValue &value)
{
    if (value.isNull()) {
        emplace<std::nullptr_t>(nullptr);
    } else if (value.isArray()) {
        QList<CompletionItem> items;
        for (auto arrayElement : value.toArray())
            items << CompletionItem(arrayElement);
        emplace<QList<CompletionItem>>(items);
    } else if (value.isObject()) {
        emplace<CompletionList>(CompletionList(value));
    }
}

} // namespace LanguageServerProtocol
