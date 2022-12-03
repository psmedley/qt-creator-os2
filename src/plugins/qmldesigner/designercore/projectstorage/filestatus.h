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

#include "projectstorageids.h"

#include <vector>

namespace QmlDesigner {

class FileStatus
{
public:
    explicit FileStatus() = default;
    explicit FileStatus(SourceId sourceId, long long size, long long lastModified)
        : sourceId{sourceId}
        , size{size}
        , lastModified{lastModified}
    {}

    explicit FileStatus(int sourceId, long long size, long long lastModified)
        : sourceId{sourceId}
        , size{size}
        , lastModified{lastModified}
    {}

    friend bool operator==(const FileStatus &first, const FileStatus &second)
    {
        return first.sourceId == second.sourceId && first.size == second.size
               && first.lastModified == second.lastModified && first.size >= 0
               && first.lastModified >= 0;
    }

    friend bool operator!=(const FileStatus &first, const FileStatus &second)
    {
        return !(first == second);
    }

    friend bool operator<(const FileStatus &first, const FileStatus &second)
    {
        return first.sourceId < second.sourceId;
    }

    friend bool operator<(SourceId first, const FileStatus &second)
    {
        return first < second.sourceId;
    }

    friend bool operator<(const FileStatus &first, SourceId second)
    {
        return first.sourceId < second;
    }

    bool isValid() const { return sourceId && size >= 0 && lastModified >= 0; }

    explicit operator bool() const { return isValid(); }

public:
    SourceId sourceId;
    long long size = -1;
    long long lastModified = -1;
};

using FileStatuses = std::vector<FileStatus>;
} // namespace QmlDesigner
