/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "projectstorageupdater.h"

#include "filestatuscache.h"
#include "filesysteminterface.h"
#include "projectmanagerinterface.h"
#include "projectstorage.h"
#include "qmldocumentparserinterface.h"
#include "qmltypesparserinterface.h"
#include "sourcepathcache.h"

#include <sqlitedatabase.h>

#include <functional>

namespace QmlDesigner {
namespace {

ComponentReferences createComponentReferences(const QMultiHash<QString, QmlDirParser::Component> &components)
{
    ComponentReferences componentReferences;
    componentReferences.reserve(static_cast<std::size_t>(components.size()));

    for (const QmlDirParser::Component &component : components)
        componentReferences.push_back(std::cref(component));

    return componentReferences;
}

SourceIds filterNotUpdatedSourceIds(SourceIds updatedSourceIds, SourceIds notUpdatedSourceIds)
{
    std::sort(updatedSourceIds.begin(), updatedSourceIds.end());
    std::sort(notUpdatedSourceIds.begin(), notUpdatedSourceIds.end());

    SourceIds filteredUpdatedSourceIds;
    filteredUpdatedSourceIds.reserve(updatedSourceIds.size());

    std::set_difference(updatedSourceIds.cbegin(),
                        updatedSourceIds.cend(),
                        notUpdatedSourceIds.cbegin(),
                        notUpdatedSourceIds.cend(),
                        std::back_inserter(filteredUpdatedSourceIds));

    filteredUpdatedSourceIds.erase(std::unique(filteredUpdatedSourceIds.begin(),
                                               filteredUpdatedSourceIds.end()),
                                   filteredUpdatedSourceIds.end());

    return filteredUpdatedSourceIds;
}

void addSourceIds(SourceIds &sourceIds, const Storage::ProjectDatas &projectDatas)
{
    for (const auto &projectData : projectDatas)
        sourceIds.push_back(projectData.sourceId);
}

} // namespace

void ProjectUpdater::update()
{
    Storage::SynchronizationPackage package;

    SourceIds notUpdatedFileStatusSourceIds;
    SourceIds notUpdatedSourceIds;

    for (const QString &qmldirPath : m_projectManager.qtQmlDirs()) {
        SourcePath qmldirSourcePath{qmldirPath};
        SourceId qmlDirSourceId = m_pathCache.sourceId(qmldirSourcePath);

        auto state = fileState(qmlDirSourceId,
                               package.fileStatuses,
                               package.updatedFileStatusSourceIds,
                               notUpdatedFileStatusSourceIds);
        switch (state) {
        case FileState::Changed: {
            QmlDirParser parser;
            parser.parse(m_fileSystem.contentAsQString(qmldirPath));

            package.updatedSourceIds.push_back(qmlDirSourceId);

            SourceContextId directoryId = m_pathCache.sourceContextId(qmlDirSourceId);

            Utils::PathString moduleName{parser.typeNamespace()};
            ModuleId moduleId = m_projectStorage.moduleId(moduleName);

            const auto qmlProjectDatas = m_projectStorage.fetchProjectDatas(qmlDirSourceId);
            addSourceIds(package.updatedSourceIds, qmlProjectDatas);
            addSourceIds(package.updatedFileStatusSourceIds, qmlProjectDatas);

            parseTypeInfos(parser.typeInfos(),
                           qmlDirSourceId,
                           directoryId,
                           moduleId,
                           package,
                           notUpdatedFileStatusSourceIds,
                           notUpdatedSourceIds);
            parseQmlComponents(createComponentReferences(parser.components()),
                               qmlDirSourceId,
                               directoryId,
                               moduleId,
                               package,
                               notUpdatedFileStatusSourceIds);
            package.updatedProjectSourceIds.push_back(qmlDirSourceId);
            break;
        }
        case FileState::NotChanged: {
            const auto qmlProjectDatas = m_projectStorage.fetchProjectDatas(qmlDirSourceId);
            parseTypeInfos(qmlProjectDatas, package, notUpdatedFileStatusSourceIds, notUpdatedSourceIds);
            parseQmlComponents(qmlProjectDatas, package, notUpdatedFileStatusSourceIds);
            break;
        }
        case FileState::NotExists: {
            package.updatedSourceIds.push_back(qmlDirSourceId);
            auto qmlProjectDatas = m_projectStorage.fetchProjectDatas(qmlDirSourceId);
            for (const Storage::ProjectData &projectData : qmlProjectDatas) {
                package.updatedSourceIds.push_back(projectData.sourceId);
            }

            break;
        }
        }
    }

    package.updatedSourceIds = filterNotUpdatedSourceIds(std::move(package.updatedSourceIds),
                                                         std::move(notUpdatedSourceIds));
    package.updatedFileStatusSourceIds = filterNotUpdatedSourceIds(
        std::move(package.updatedFileStatusSourceIds), std::move(notUpdatedFileStatusSourceIds));

    m_projectStorage.synchronize(std::move(package));
}

void ProjectUpdater::pathsWithIdsChanged(const std::vector<IdPaths> &idPaths) {}

void ProjectUpdater::parseTypeInfos(const QStringList &typeInfos,
                                    SourceId qmldirSourceId,
                                    SourceContextId directoryId,
                                    ModuleId moduleId,
                                    Storage::SynchronizationPackage &package,
                                    SourceIds &notUpdatedFileStatusSourceIds,
                                    SourceIds &notUpdatedSourceIds)
{
    QString directory{m_pathCache.sourceContextPath(directoryId)};

    for (const QString &typeInfo : typeInfos) {
        SourceId sourceId = m_pathCache.sourceId(directoryId, Utils::SmallString{typeInfo});
        QString qmltypesPath = directory + "/" + typeInfo;

        auto projectData = package.projectDatas.emplace_back(qmldirSourceId,
                                                             sourceId,
                                                             moduleId,
                                                             Storage::FileType::QmlTypes);

        parseTypeInfo(projectData,
                      qmltypesPath,
                      package,
                      notUpdatedFileStatusSourceIds,
                      notUpdatedSourceIds);
    }
}

void ProjectUpdater::parseTypeInfos(const Storage::ProjectDatas &projectDatas,
                                    Storage::SynchronizationPackage &package,
                                    SourceIds &notUpdatedFileStatusSourceIds,
                                    SourceIds &notUpdatedSourceIds)
{
    for (const Storage::ProjectData &projectData : projectDatas) {
        if (projectData.fileType != Storage::FileType::QmlTypes)
            continue;

        QString qmltypesPath = m_pathCache.sourcePath(projectData.sourceId).toQString();

        parseTypeInfo(projectData,
                      qmltypesPath,
                      package,
                      notUpdatedFileStatusSourceIds,
                      notUpdatedSourceIds);
    }
}

void ProjectUpdater::parseTypeInfo(const Storage::ProjectData &projectData,
                                   const QString &qmltypesPath,
                                   Storage::SynchronizationPackage &package,
                                   SourceIds &notUpdatedFileStatusSourceIds,
                                   SourceIds &notUpdatedSourceIds)
{
    auto state = fileState(projectData.sourceId,
                           package.fileStatuses,
                           package.updatedFileStatusSourceIds,
                           notUpdatedFileStatusSourceIds);
    switch (state) {
    case FileState::Changed: {
        package.updatedSourceIds.push_back(projectData.sourceId);

        const auto content = m_fileSystem.contentAsQString(qmltypesPath);
        m_qmlTypesParser.parse(content, package.imports, package.types, projectData);
        break;
    }
    case FileState::NotChanged: {
        notUpdatedSourceIds.push_back(projectData.sourceId);
        break;
    }
    case FileState::NotExists:
        break;
    }
}

void ProjectUpdater::parseQmlComponent(Utils::SmallStringView fileName,
                                       Utils::SmallStringView directory,
                                       Utils::SmallStringView typeName,
                                       Storage::Version version,
                                       ModuleId moduleId,
                                       SourceId qmldirSourceId,
                                       SourceContextId directoryId,
                                       Storage::SynchronizationPackage &package,
                                       SourceIds &notUpdatedFileStatusSourceIds)
{
    SourceId sourceId = m_pathCache.sourceId(directoryId, fileName);

    Storage::Type type;

    auto state = fileState(sourceId,
                           package.fileStatuses,
                           package.updatedFileStatusSourceIds,
                           notUpdatedFileStatusSourceIds);
    switch (state) {
    case FileState::NotChanged:
        type.changeLevel = Storage::ChangeLevel::Minimal;
        break;
    case FileState::NotExists:
        throw CannotParseQmlDocumentFile{};
    case FileState::Changed:
        const auto content = m_fileSystem.contentAsQString(
            QString{Utils::PathString{directory} + "/" + fileName});
        type = m_qmlDocumentParser.parse(content, package.imports);
        break;
    }

    package.projectDatas.emplace_back(qmldirSourceId, sourceId, moduleId, Storage::FileType::QmlDocument);

    package.updatedSourceIds.push_back(sourceId);

    type.typeName = fileName;
    type.accessSemantics = Storage::TypeAccessSemantics::Reference;
    type.sourceId = sourceId;
    type.exportedTypes.push_back(Storage::ExportedType{moduleId, typeName, version});

    package.types.push_back(std::move(type));
}

void ProjectUpdater::parseQmlComponent(Utils::SmallStringView fileName,
                                       Utils::SmallStringView filePath,
                                       SourceId sourceId,
                                       Storage::SynchronizationPackage &package,
                                       SourceIds &notUpdatedFileStatusSourceIds)
{
    auto state = fileState(sourceId,
                           package.fileStatuses,
                           package.updatedFileStatusSourceIds,
                           notUpdatedFileStatusSourceIds);
    if (state != FileState::Changed)
        return;

    package.updatedSourceIds.push_back(sourceId);

    const auto content = m_fileSystem.contentAsQString(QString{filePath});
    auto type = m_qmlDocumentParser.parse(content, package.imports);

    type.typeName = fileName;
    type.accessSemantics = Storage::TypeAccessSemantics::Reference;
    type.sourceId = sourceId;
    type.changeLevel = Storage::ChangeLevel::ExcludeExportedTypes;

    package.types.push_back(std::move(type));
}

void ProjectUpdater::parseQmlComponents(ComponentReferences components,
                                        SourceId qmldirSourceId,
                                        SourceContextId directoryId,
                                        ModuleId moduleId,
                                        Storage::SynchronizationPackage &package,
                                        SourceIds &notUpdatedFileStatusSourceIds)
{
    std::sort(components.begin(), components.end(), [](auto &&first, auto &&second) {
        return std::tie(first.get().typeName, first.get().majorVersion, first.get().minorVersion)
               > std::tie(second.get().typeName, second.get().majorVersion, second.get().minorVersion);
    });

    auto newEnd = std::unique(components.begin(), components.end(), [](auto &&first, auto &&second) {
        return first.get().typeName == second.get().typeName
               && first.get().majorVersion == second.get().majorVersion;
    });

    components.erase(newEnd, components.end());

    auto directory = m_pathCache.sourceContextPath(directoryId);

    for (const QmlDirParser::Component &component : components) {
        parseQmlComponent(Utils::SmallString{component.fileName},
                          directory,
                          Utils::SmallString{component.typeName},
                          Storage::Version{component.majorVersion, component.minorVersion},
                          moduleId,
                          qmldirSourceId,
                          directoryId,
                          package,
                          notUpdatedFileStatusSourceIds);
    }
}

void ProjectUpdater::parseQmlComponents(const Storage::ProjectDatas &projectDatas,
                                        Storage::SynchronizationPackage &package,
                                        SourceIds &notUpdatedFileStatusSourceIds)
{
    for (const Storage::ProjectData &projectData : projectDatas) {
        if (projectData.fileType != Storage::FileType::QmlDocument)
            continue;

        SourcePath qmlDocumentPath = m_pathCache.sourcePath(projectData.sourceId);

        parseQmlComponent(qmlDocumentPath.name(),
                          qmlDocumentPath,
                          projectData.sourceId,
                          package,
                          notUpdatedFileStatusSourceIds);
    }
}

ProjectUpdater::FileState ProjectUpdater::fileState(SourceId sourceId,
                                                    FileStatuses &fileStatuses,
                                                    SourceIds &updatedSourceIds,
                                                    SourceIds &notUpdatedSourceIds) const
{
    auto currentFileStatus = m_fileStatusCache.find(sourceId);

    if (!currentFileStatus.isValid())
        return FileState::NotExists;

    auto projectStorageFileStatus = m_projectStorage.fetchFileStatus(sourceId);

    if (!projectStorageFileStatus.isValid() || projectStorageFileStatus != currentFileStatus) {
        fileStatuses.push_back(currentFileStatus);
        updatedSourceIds.push_back(currentFileStatus.sourceId);
        return FileState::Changed;
    }

    notUpdatedSourceIds.push_back(currentFileStatus.sourceId);
    return FileState::NotChanged;
}

} // namespace QmlDesigner
