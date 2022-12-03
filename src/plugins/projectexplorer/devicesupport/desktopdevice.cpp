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

#include "desktopdevice.h"
#include "deviceprocesslist.h"
#include "localprocesslist.h"
#include "desktopprocesssignaloperation.h"

#include <coreplugin/fileutils.h>

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runcontrol.h>

#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/portlist.h>
#include <utils/qtcprocess.h>
#include <utils/url.h>

#include <QCoreApplication>
#include <QDateTime>

using namespace ProjectExplorer::Constants;
using namespace Utils;

namespace ProjectExplorer {

DesktopDevice::DesktopDevice()
{
    setupId(IDevice::AutoDetected, DESKTOP_DEVICE_ID);
    setType(DESKTOP_DEVICE_TYPE);
    setDefaultDisplayName(tr("Local PC"));
    setDisplayType(QCoreApplication::translate("ProjectExplorer::DesktopDevice", "Desktop"));

    setDeviceState(IDevice::DeviceStateUnknown);
    setMachineType(IDevice::Hardware);
    setOsType(HostOsInfo::hostOs());

    const QString portRange =
            QString::fromLatin1("%1-%2").arg(DESKTOP_PORT_START).arg(DESKTOP_PORT_END);
    setFreePorts(Utils::PortList::fromString(portRange));
    setOpenTerminal([](const Environment &env, const FilePath &workingDir) {
        Core::FileUtils::openTerminal(workingDir, env);
    });
}

IDevice::DeviceInfo DesktopDevice::deviceInformation() const
{
    return DeviceInfo();
}

IDeviceWidget *DesktopDevice::createWidget()
{
    return nullptr;
    // DesktopDeviceConfigurationWidget currently has just one editable field viz. free ports.
    // Querying for an available port is quite straightforward. Having a field for the port
    // range can be confusing to the user. Hence, disabling the widget for now.
}

bool DesktopDevice::canAutoDetectPorts() const
{
    return true;
}

bool DesktopDevice::canCreateProcessModel() const
{
    return true;
}

DeviceProcessList *DesktopDevice::createProcessListModel(QObject *parent) const
{
    return new Internal::LocalProcessList(sharedFromThis(), parent);
}

DeviceProcessSignalOperation::Ptr DesktopDevice::signalOperation() const
{
    return DeviceProcessSignalOperation::Ptr(new DesktopProcessSignalOperation());
}

class DesktopDeviceEnvironmentFetcher : public DeviceEnvironmentFetcher
{
public:
    DesktopDeviceEnvironmentFetcher() = default;

    void start() override
    {
        emit finished(Utils::Environment::systemEnvironment(), true);
    }
};

DeviceEnvironmentFetcher::Ptr DesktopDevice::environmentFetcher() const
{
    return DeviceEnvironmentFetcher::Ptr(new DesktopDeviceEnvironmentFetcher());
}

PortsGatheringMethod DesktopDevice::portsGatheringMethod() const
{
    return {
        [this](QAbstractSocket::NetworkLayerProtocol protocol) -> CommandLine {
            // We might encounter the situation that protocol is given IPv6
            // but the consumer of the free port information decides to open
            // an IPv4(only) port. As a result the next IPv6 scan will
            // report the port again as open (in IPv6 namespace), while the
            // same port in IPv4 namespace might still be blocked, and
            // re-use of this port fails.
            // GDBserver behaves exactly like this.

            Q_UNUSED(protocol)

            if (HostOsInfo::isWindowsHost() || HostOsInfo::isMacHost())
                return {filePath("netstat"), {"-a", "-n"}};
            if (HostOsInfo::isLinuxHost())
                return {filePath("/bin/sh"), {"-c", "cat /proc/net/tcp*"}};
            return {};
        },

        &Port::parseFromNetstatOutput
    };
}

QUrl DesktopDevice::toolControlChannel(const ControlChannelHint &) const
{
    QUrl url;
    url.setScheme(Utils::urlTcpScheme());
    url.setHost("localhost");
    return url;
}

bool DesktopDevice::handlesFile(const FilePath &filePath) const
{
    return !filePath.needsDevice();
}

void DesktopDevice::iterateDirectory(const FilePath &filePath,
                                     const std::function<bool(const FilePath &)> &callBack,
                                     const FileFilter &filter) const
{
    QTC_CHECK(!filePath.needsDevice());
    filePath.iterateDirectory(callBack, filter);
}

qint64 DesktopDevice::fileSize(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return -1);
    return filePath.fileSize();
}

QFile::Permissions DesktopDevice::permissions(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    return filePath.permissions();
}

bool DesktopDevice::setPermissions(const FilePath &filePath, QFile::Permissions permissions) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    return filePath.setPermissions(permissions);
}

FilePath DesktopDevice::mapToGlobalPath(const Utils::FilePath &pathOnDevice) const
{
    QTC_CHECK(!pathOnDevice.needsDevice());
    return pathOnDevice;
}

Environment DesktopDevice::systemEnvironment() const
{
    return Environment::systemEnvironment();
}

bool DesktopDevice::isExecutableFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.isExecutableFile();
}

bool DesktopDevice::isReadableFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.isReadableFile();
}

bool DesktopDevice::isWritableFile(const Utils::FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.isWritableFile();
}

bool DesktopDevice::isReadableDirectory(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.isReadableDir();
}

bool DesktopDevice::isWritableDirectory(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.isWritableDir();
}

bool DesktopDevice::isFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.isFile();
}

bool DesktopDevice::isDirectory(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.isDir();
}

bool DesktopDevice::createDirectory(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.createDir();
}

bool DesktopDevice::exists(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.exists();
}

bool DesktopDevice::ensureExistingFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.ensureExistingFile();
}

bool DesktopDevice::removeFile(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.removeFile();
}

bool DesktopDevice::removeRecursively(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.removeRecursively();
}

bool DesktopDevice::copyFile(const FilePath &filePath, const FilePath &target) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    return filePath.copyFile(target);
}

bool DesktopDevice::renameFile(const FilePath &filePath, const FilePath &target) const
{
    QTC_ASSERT(handlesFile(filePath), return false);
    QTC_ASSERT(handlesFile(target), return false);
    return filePath.renameFile(target);
}

QDateTime DesktopDevice::lastModified(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    return filePath.lastModified();
}

FilePath DesktopDevice::symLinkTarget(const FilePath &filePath) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    return filePath.symLinkTarget();
}

QByteArray DesktopDevice::fileContents(const FilePath &filePath, qint64 limit, qint64 offset) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    return filePath.fileContents(limit, offset);
}

bool DesktopDevice::writeFileContents(const Utils::FilePath &filePath, const QByteArray &data) const
{
    QTC_ASSERT(handlesFile(filePath), return {});
    return filePath.writeFileContents(data);
}

} // namespace ProjectExplorer
