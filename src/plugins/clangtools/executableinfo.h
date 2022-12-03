/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include <utils/filepath.h>

#include <QDateTime>
#include <QHash>
#include <QPair>
#include <QStringList>
#include <QVector>
#include <QVersionNumber>

namespace ClangTools {
namespace Internal {

QPair<Utils::FilePath, QString> getClangIncludeDirAndVersion(const Utils::FilePath &clangToolPath);

enum class QueryFailMode{ Silent, Noisy };
QString queryVersion(const Utils::FilePath &clangToolPath, QueryFailMode failMode);

class ClangTidyInfo
{
public:
    ClangTidyInfo(const Utils::FilePath &executablePath);
    QStringList defaultChecks;
    QStringList supportedChecks;
};

class ClazyCheck
{
public:
    QString name;
    int level;
    QStringList topics;
};
using ClazyChecks = QVector<ClazyCheck>;

class ClazyStandaloneInfo
{
public:
    static ClazyStandaloneInfo getInfo(const Utils::FilePath &executablePath);

    QVersionNumber version;
    QStringList defaultChecks;
    ClazyChecks supportedChecks;

private:
    ClazyStandaloneInfo(const Utils::FilePath &executablePath);

    static QHash<Utils::FilePath, QPair<QDateTime, ClazyStandaloneInfo>> cache;
};

} // namespace Internal
} // namespace ClangTools

