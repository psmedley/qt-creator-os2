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

#include "remotelinux_export.h"

#include <projectexplorer/devicesupport/idevice.h>

namespace RemoteLinux {

class REMOTELINUX_EXPORT RemoteLinuxSignalOperation
        : public ProjectExplorer::DeviceProcessSignalOperation
{
    Q_OBJECT
public:
    ~RemoteLinuxSignalOperation() override;

    void killProcess(qint64 pid) override;
    void killProcess(const QString &filePath) override;
    void interruptProcess(qint64 pid) override;
    void interruptProcess(const QString &filePath) override;

protected:
    RemoteLinuxSignalOperation(const ProjectExplorer::IDeviceConstPtr &device);

private:
    virtual QString killProcessByNameCommandLine(const QString &filePath) const;
    virtual QString interruptProcessByNameCommandLine(const QString &filePath) const;

    void runnerDone();
    void run(const QString &command);

    const ProjectExplorer::IDeviceConstPtr m_device;
    std::unique_ptr<Utils::QtcProcess> m_process;

    friend class LinuxDevice;
};

}
