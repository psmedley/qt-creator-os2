/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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
#include "filesysteminterface.h"

#include <QtGlobal>

QT_FORWARD_DECLARE_CLASS(QFileInfo)

namespace QmlDesigner {

class FileStatusCache
{
public:
    using size_type = FileStatuses::size_type;

    FileStatusCache(FileSystemInterface &fileSystem)
        : m_fileSystem(fileSystem)
    {}
    FileStatusCache &operator=(const FileStatusCache &) = delete;
    FileStatusCache(const FileStatusCache &) = delete;

    long long lastModifiedTime(SourceId sourceId) const;
    long long fileSize(SourceId sourceId) const;
    const FileStatus &find(SourceId sourceId) const;

    void update(SourceId sourceId);
    void update(SourceIds sourceIds);
    SourceIds modified(SourceIds sourceIds) const;

    size_type size() const;

private:
    mutable FileStatuses m_cacheEntries;
    FileSystemInterface &m_fileSystem;
};

} // namespace QmlDesigner
