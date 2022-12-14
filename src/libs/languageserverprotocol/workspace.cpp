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

#include "workspace.h"

namespace LanguageServerProtocol {

constexpr const char WorkSpaceFolderRequest::methodName[];
constexpr const char DidChangeWorkspaceFoldersNotification::methodName[];
constexpr const char DidChangeConfigurationNotification::methodName[];
constexpr const char ConfigurationRequest::methodName[];
constexpr const char WorkspaceSymbolRequest::methodName[];
constexpr const char ExecuteCommandRequest::methodName[];
constexpr const char ApplyWorkspaceEditRequest::methodName[];
constexpr const char DidChangeWatchedFilesNotification::methodName[];

WorkSpaceFolderRequest::WorkSpaceFolderRequest()
    : Request(methodName, nullptr)
{ }

DidChangeWorkspaceFoldersNotification::DidChangeWorkspaceFoldersNotification(
        const DidChangeWorkspaceFoldersParams &params)
    : Notification(methodName, params)
{ }

DidChangeConfigurationNotification::DidChangeConfigurationNotification(
        const DidChangeConfigurationParams &params)
    : Notification(methodName, params)
{ }

ConfigurationRequest::ConfigurationRequest(const ConfigurationParams &params)
    : Request(methodName, params)
{ }

WorkspaceSymbolRequest::WorkspaceSymbolRequest(const WorkspaceSymbolParams &params)
    : Request(methodName, params)
{ }

ExecuteCommandRequest::ExecuteCommandRequest(const ExecuteCommandParams &params)
    : Request(methodName, params)
{ }

ApplyWorkspaceEditRequest::ApplyWorkspaceEditRequest(const ApplyWorkspaceEditParams &params)
    : Request(methodName, params)
{ }

WorkspaceFoldersChangeEvent::WorkspaceFoldersChangeEvent()
{
    insert(addedKey, QJsonArray());
    insert(removedKey, QJsonArray());
}

DidChangeWatchedFilesNotification::DidChangeWatchedFilesNotification(
        const DidChangeWatchedFilesParams &params)
    : Notification(methodName, params)
{ }

ExecuteCommandParams::ExecuteCommandParams(const Command &command)
{
    setCommand(command.command());
    if (command.arguments().has_value())
        setArguments(*command.arguments());
}

LanguageServerProtocol::WorkSpaceFolderResult::operator const QJsonValue() const
{
    if (!Utils::holds_alternative<QList<WorkSpaceFolder>>(*this))
        return QJsonValue::Null;
    QJsonArray array;
    for (const auto &folder : Utils::get<QList<WorkSpaceFolder>>(*this))
        array.append(QJsonValue(folder));
    return array;
}

} // namespace LanguageServerProtocol
