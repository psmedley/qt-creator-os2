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

#include "resourcehandler.h"
#include "designerconstants.h"

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <resourceeditor/resourcenode.h>
#include <utils/algorithm.h>
#include <utils/qtcassert.h>

#include <QDesignerFormWindowInterface>

using namespace ProjectExplorer;
using namespace Utils;

namespace Designer {
namespace Internal {

// ------------ ResourceHandler
ResourceHandler::ResourceHandler(QDesignerFormWindowInterface *fw) :
    QObject(fw),
    m_form(fw)
{
}

void ResourceHandler::ensureInitialized()
{
    if (m_initialized)
        return;

    m_initialized = true;

    auto connector = [this](Project *p) {
        connect(p,
                &Project::fileListChanged,
                this,
                &ResourceHandler::updateResources,
                Qt::QueuedConnection);
    };

    for (Project *p : SessionManager::projects())
        connector(p);

    connect(SessionManager::instance(), &SessionManager::projectAdded, this, connector);

    m_originalUiQrcPaths = m_form->activeResourceFilePaths();
    if (Designer::Constants::Internal::debug)
        qDebug() << "ResourceHandler::ensureInitialized() origPaths=" << m_originalUiQrcPaths;
}

ResourceHandler::~ResourceHandler() = default;

void ResourceHandler::updateResourcesHelper(bool updateProjectResources)
{
    if (m_handlingResources)
        return;

    ensureInitialized();

    const QString fileName = m_form->fileName();
    QTC_ASSERT(!fileName.isEmpty(), return);

    if (Designer::Constants::Internal::debug)
        qDebug() << "ResourceHandler::updateResources()" << fileName;

    // Filename could change in the meantime.
    Project *project = SessionManager::projectForFile(Utils::FilePath::fromUserInput(fileName));
    const bool dirty = m_form->property("_q_resourcepathchanged").toBool();
    if (dirty)
        m_form->setDirty(true);

    // Does the file belong to a project?
    if (project) {
        // Collect project resource files.

        // Find the (sub-)project the file belongs to. We don't want to find resources
        // from other parts of the project tree, e.g. via a qmake subdirs project.
        Node * const fileNode = project->rootProjectNode()->findNode([&fileName](const Node *n) {
            return n->filePath().toString() == fileName;
        });
        ProjectNode *projectNodeForUiFile = nullptr;
        if (fileNode) {
            // We do not want qbs groups or qmake .pri files here, as they contain only a subset
            // of the relevant files.
            projectNodeForUiFile = fileNode->parentProjectNode();
            while (projectNodeForUiFile && !projectNodeForUiFile->isProduct())
                projectNodeForUiFile = projectNodeForUiFile->parentProjectNode();
        }
        if (!projectNodeForUiFile)
            projectNodeForUiFile = project->rootProjectNode();

        const auto useQrcFile = [projectNodeForUiFile, project](const Node *qrcNode) {
            if (projectNodeForUiFile == project->rootProjectNode())
                return true;
            ProjectNode *projectNodeForQrcFile = qrcNode->parentProjectNode();
            while (projectNodeForQrcFile && !projectNodeForQrcFile->isProduct())
                projectNodeForQrcFile = projectNodeForQrcFile->parentProjectNode();
            return !projectNodeForQrcFile
                    || projectNodeForQrcFile == projectNodeForUiFile
                    || projectNodeForQrcFile->productType() != ProductType::App;
        };

        QStringList projectQrcFiles;
        project->rootProjectNode()->forEachNode([&](FileNode *node) {
            if (node->fileType() == FileType::Resource && useQrcFile(node))
                projectQrcFiles.append(node->filePath().toString());
        }, [&](FolderNode *node) {
            if (dynamic_cast<ResourceEditor::ResourceTopLevelNode *>(node) && useQrcFile(node))
                projectQrcFiles.append(node->filePath().toString());
        });
        // Check if the user has chosen to update the lacking resource inside designer
        if (dirty && updateProjectResources) {
            QStringList qrcPathsToBeAdded;
            for (const QString &originalQrcPath : qAsConst(m_originalUiQrcPaths)) {
                if (!projectQrcFiles.contains(originalQrcPath) && !qrcPathsToBeAdded.contains(originalQrcPath))
                    qrcPathsToBeAdded.append(originalQrcPath);
            }
            if (!qrcPathsToBeAdded.isEmpty()) {
                m_handlingResources = true;
                projectNodeForUiFile->addFiles(Utils::transform(qrcPathsToBeAdded, &FilePath::fromString));
                m_handlingResources = false;
                projectQrcFiles += qrcPathsToBeAdded;
            }
        }

        m_form->activateResourceFilePaths(projectQrcFiles);
        m_form->setResourceFileSaveMode(QDesignerFormWindowInterface::SaveOnlyUsedResourceFiles);
        if (Designer::Constants::Internal::debug)
            qDebug() << "ResourceHandler::updateResources()" << fileName
                    << " associated with project" << project->rootProjectNode()->filePath()
                    <<  " using project qrc files" << projectQrcFiles.size();
    } else {
        // Use resource file originally used in form
        m_form->activateResourceFilePaths(m_originalUiQrcPaths);
        m_form->setResourceFileSaveMode(QDesignerFormWindowInterface::SaveAllResourceFiles);
        if (Designer::Constants::Internal::debug)
            qDebug() << "ResourceHandler::updateResources()" << fileName << " not associated with project, using loaded qrc files.";
    }
}

} // namespace Internal
} // namespace Designer
