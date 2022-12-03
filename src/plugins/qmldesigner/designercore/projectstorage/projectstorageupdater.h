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

#pragma once

#include "filestatus.h"
#include "nonlockingmutex.h"
#include "projectstorageids.h"
#include "projectstoragepathwatchertypes.h"
#include "projectstoragetypes.h"

#include <qmljs/parser/qmldirparser_p.h>

#include <QStringList>

#include <vector>

namespace Sqlite {
class Database;
}

namespace QmlDesigner {
class ProjectManagerInterface;
class FileSystemInterface;
class ProjectStorageInterface;
template<typename ProjectStorage, typename Mutex>
class SourcePathCache;
class FileStatusCache;
template<typename Database>
class ProjectStorage;
class QmlDocumentParserInterface;
class QmlTypesParserInterface;

using ComponentReferences = std::vector<std::reference_wrapper<const QmlDirParser::Component>>;

class ProjectUpdater
{
public:
    using PathCache = SourcePathCache<ProjectStorage<Sqlite::Database>, NonLockingMutex>;

    ProjectUpdater(ProjectManagerInterface &projectManager,
                   FileSystemInterface &fileSystem,
                   ProjectStorageInterface &projectStorage,
                   FileStatusCache &fileStatusCache,
                   PathCache &pathCache,
                   QmlDocumentParserInterface &qmlDocumentParser,
                   QmlTypesParserInterface &qmlTypesParser)
        : m_projectManager{projectManager}
        , m_fileSystem{fileSystem}
        , m_projectStorage{projectStorage}
        , m_fileStatusCache{fileStatusCache}
        , m_pathCache{pathCache}
        , m_qmlDocumentParser{qmlDocumentParser}
        , m_qmlTypesParser{qmlTypesParser}
    {}

    void update();
    void pathsWithIdsChanged(const std::vector<IdPaths> &idPaths);

private:
    enum class FileState {
        NotChanged,
        Changed,
        NotExists,
    };

    void parseTypeInfos(const QStringList &typeInfos,
                        SourceId qmldirSourceId,
                        SourceContextId directoryId,
                        ModuleId moduleId,
                        Storage::SynchronizationPackage &package,
                        SourceIds &notUpdatedFileStatusSourceIds,
                        SourceIds &notUpdatedSourceIds);
    void parseTypeInfos(const Storage::ProjectDatas &projectDatas,
                        Storage::SynchronizationPackage &package,
                        SourceIds &notUpdatedFileStatusSourceIds,
                        SourceIds &notUpdatedSourceIds);
    void parseTypeInfo(const Storage::ProjectData &projectData,
                       const QString &qmltypesPath,
                       Storage::SynchronizationPackage &package,
                       SourceIds &notUpdatedFileStatusSourceIds,
                       SourceIds &notUpdatedSourceIds);
    void parseQmlComponents(ComponentReferences components,
                            SourceId qmldirSourceId,
                            SourceContextId directoryId,
                            ModuleId moduleId,
                            Storage::SynchronizationPackage &package,
                            SourceIds &notUpdatedFileStatusSourceIds);
    void parseQmlComponents(const Storage::ProjectDatas &projectDatas,
                            Storage::SynchronizationPackage &package,
                            SourceIds &notUpdatedFileStatusSourceIds);
    void parseQmlComponent(Utils::SmallStringView fileName,
                           Utils::SmallStringView directory,
                           Utils::SmallStringView typeName,
                           Storage::Version version,
                           ModuleId moduleId,
                           SourceId qmldirSourceId,
                           SourceContextId directoryId,
                           Storage::SynchronizationPackage &package,
                           SourceIds &notUpdatedFileStatusSourceIds);
    void parseQmlComponent(Utils::SmallStringView fileName,
                           Utils::SmallStringView filePath,
                           SourceId sourceId,
                           Storage::SynchronizationPackage &package,
                           SourceIds &notUpdatedFileStatusSourceIds);

    FileState fileState(SourceId sourceId,
                        FileStatuses &fileStatuses,
                        SourceIds &updatedSourceIds,
                        SourceIds &notUpdatedSourceIds) const;

private:
    ProjectManagerInterface &m_projectManager;
    FileSystemInterface &m_fileSystem;
    ProjectStorageInterface &m_projectStorage;
    FileStatusCache &m_fileStatusCache;
    PathCache &m_pathCache;
    QmlDocumentParserInterface &m_qmlDocumentParser;
    QmlTypesParserInterface &m_qmlTypesParser;
};

} // namespace QmlDesigner
