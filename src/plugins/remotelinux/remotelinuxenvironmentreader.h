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

#include <projectexplorer/devicesupport/idevicefwd.h>
#include <utils/environment.h>

#include <QObject>
#include <QSharedPointer>

namespace Utils { class QtcProcess; }

namespace RemoteLinux {
namespace Internal {

class RemoteLinuxEnvironmentReader : public QObject
{
    Q_OBJECT

public:
    RemoteLinuxEnvironmentReader(const ProjectExplorer::IDeviceConstPtr &device,
                                 QObject *parent = nullptr);
    void start();
    void stop();

    Utils::Environment remoteEnvironment() const { return m_env; }
    void handleCurrentDeviceConfigChanged();

signals:
    void finished();
    void error(const QString &error);

private:
    void handleDone();
    void setFinished();

    Utils::Environment m_env;
    ProjectExplorer::IDeviceConstPtr m_device;
    Utils::QtcProcess *m_deviceProcess = nullptr;
};

} // namespace Internal
} // namespace RemoteLinux
