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

#pragma once

#include "resource_global.h"
#include <projectexplorer/projectnodes.h>

namespace ResourceEditor {
namespace Internal { class ResourceFileWatcher; }

class RESOURCE_EXPORT ResourceTopLevelNode : public ProjectExplorer::FolderNode
{
public:
    ResourceTopLevelNode(const Utils::FilePath &filePath,
                         const Utils::FilePath &basePath,
                         const QString &contents = {});
    ~ResourceTopLevelNode() override;

    void setupWatcherIfNeeded();
    void addInternalNodes();

    bool supportsAction(ProjectExplorer::ProjectAction action, const Node *node) const override;
    bool addFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded) override;
    ProjectExplorer::RemovedFilesFromProject removeFiles(const Utils::FilePaths &filePaths,
                                                        Utils::FilePaths *notRemoved) override;

    bool addPrefix(const QString &prefix, const QString &lang);
    bool removePrefix(const QString &prefix, const QString &lang);

    AddNewInformation addNewInformation(const Utils::FilePaths &files, Node *context) const override;
    bool showInSimpleTree() const override;
    bool removeNonExistingFiles();

    QString contents() const { return m_contents; }

private:
    Internal::ResourceFileWatcher *m_document = nullptr;
    QString m_contents;
};

class RESOURCE_EXPORT ResourceFolderNode : public ProjectExplorer::FolderNode
{
public:
    ResourceFolderNode(const QString &prefix, const QString &lang, ResourceTopLevelNode *parent);
    ~ResourceFolderNode() override;

    bool supportsAction(ProjectExplorer::ProjectAction action, const Node *node) const override;

    QString displayName() const override;

    bool addFiles(const Utils::FilePaths &filePaths, Utils::FilePaths *notAdded) override;
    ProjectExplorer::RemovedFilesFromProject removeFiles(const Utils::FilePaths &filePaths,
                                                         Utils::FilePaths *notRemoved) override;
    bool canRenameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) override;
    bool renameFile(const Utils::FilePath &oldFilePath, const Utils::FilePath &newFilePath) override;

    bool renamePrefix(const QString &prefix, const QString &lang);

    AddNewInformation addNewInformation(const Utils::FilePaths &files, Node *context) const override;

    QString prefix() const;
    QString lang() const;
    ResourceTopLevelNode *resourceNode() const;

private:
    ResourceTopLevelNode *m_topLevelNode;
    QString m_prefix;
    QString m_lang;
};

} // namespace ResourceEditor
