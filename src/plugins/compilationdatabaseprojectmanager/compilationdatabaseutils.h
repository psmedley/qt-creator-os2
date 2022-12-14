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

#include <cppeditor/cppprojectfile.h>
#include <utils/filepath.h>

#include <QHash>
#include <QStringList>

namespace ProjectExplorer {
class HeaderPath;
class Macro;
}

namespace CompilationDatabaseProjectManager {
namespace Internal {

class DbEntry {
public:
    QStringList flags;
    Utils::FilePath fileName;
    QString workingDir;
};

class DbContents {
public:
    std::vector<DbEntry> entries;
    QString extraFileName;
    QStringList extras;
};

using MimeBinaryCache = QHash<QString, bool>;

QStringList filterFromFileName(const QStringList &flags, QString baseName);

void filteredFlags(const QString &fileName,
                   const QString &workingDir,
                   QStringList &flags,
                   QVector<ProjectExplorer::HeaderPath> &headerPaths,
                   QVector<ProjectExplorer::Macro> &macros,
                   CppEditor::ProjectFile::Kind &fileKind,
                   Utils::FilePath &sysRoot);

QStringList splitCommandLine(QString commandLine, QSet<QString> &flagsCache);

} // namespace Internal
} // namespace CompilationDatabaseProjectManager
