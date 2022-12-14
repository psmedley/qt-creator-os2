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

#include <utils/id.h>

#include <QDialog>

namespace ProjectExplorer { class Kit; }
namespace Utils { class FilePath; }

namespace Debugger {
namespace Internal {

class AttachCoreDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AttachCoreDialog(QWidget *parent);
    ~AttachCoreDialog() override;

    int exec() override;

    Utils::FilePath symbolFile() const;
    Utils::FilePath localCoreFile() const;
    Utils::FilePath remoteCoreFile() const;
    Utils::FilePath overrideStartScript() const;
    Utils::FilePath sysRoot() const;
    bool useLocalCoreFile() const;
    bool forcesLocalCoreFile() const;
    bool isLocalKit() const;

    // For persistance.
    ProjectExplorer::Kit *kit() const;
    void setSymbolFile(const Utils::FilePath &symbolFilePath);
    void setLocalCoreFile(const Utils::FilePath &coreFilePath);
    void setRemoteCoreFile(const Utils::FilePath &coreFilePath);
    void setOverrideStartScript(const Utils::FilePath &scriptName);
    void setSysRoot(const Utils::FilePath &sysRoot);
    void setKitId(Utils::Id id);
    void setForceLocalCoreFile(bool on);

private:
    void changed();
    void coreFileChanged(const QString &core);
    void selectRemoteCoreFile();

    class AttachCoreDialogPrivate *d;
};

} // namespace Debugger
} // namespace Internal
