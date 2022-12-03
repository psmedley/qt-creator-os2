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

#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/devicesupport/idevicefactory.h>
#include <coreplugin/documentmanager.h>

#include <utils/aspects.h>

namespace Docker {
namespace Internal {

class DockerDeviceData
{
public:
    // Used for "docker run"
    QString repoAndTag() const;

    QString imageId;
    QString repo;
    QString tag;
    QString size;
    bool useLocalUidGid = true;
    QStringList mounts = { Core::DocumentManager::projectsDirectory().toString() };
};

class DockerDevice : public ProjectExplorer::IDevice
{
    Q_DECLARE_TR_FUNCTIONS(Docker::Internal::DockerDevice)

public:
    using Ptr = QSharedPointer<DockerDevice>;
    using ConstPtr = QSharedPointer<const DockerDevice>;

    explicit DockerDevice(const DockerDeviceData &data);
    ~DockerDevice();

    static Ptr create(const DockerDeviceData &data) { return Ptr(new DockerDevice(data)); }

    ProjectExplorer::IDeviceWidget *createWidget() override;
    QList<ProjectExplorer::Task> validate() const override;

    Utils::ProcessInterface *createProcessInterface() const override;

    bool canAutoDetectPorts() const override;
    ProjectExplorer::PortsGatheringMethod portsGatheringMethod() const override;
    bool canCreateProcessModel() const override { return false; }
    ProjectExplorer::DeviceProcessList *createProcessListModel(QObject *parent) const override;
    bool hasDeviceTester() const override { return false; }
    ProjectExplorer::DeviceTester *createDeviceTester() const override;
    ProjectExplorer::DeviceProcessSignalOperation::Ptr signalOperation() const override;
    ProjectExplorer::DeviceEnvironmentFetcher::Ptr environmentFetcher() const override;

    Utils::FilePath mapToGlobalPath(const Utils::FilePath &pathOnDevice) const override;
    QString mapToDevicePath(const Utils::FilePath &globalPath) const override;

    bool handlesFile(const Utils::FilePath &filePath) const override;
    bool isExecutableFile(const Utils::FilePath &filePath) const override;
    bool isReadableFile(const Utils::FilePath &filePath) const override;
    bool isWritableFile(const Utils::FilePath &filePath) const override;
    bool isReadableDirectory(const Utils::FilePath &filePath) const override;
    bool isWritableDirectory(const Utils::FilePath &filePath) const override;
    bool isFile(const Utils::FilePath &filePath) const override;
    bool isDirectory(const Utils::FilePath &filePath) const override;
    bool createDirectory(const Utils::FilePath &filePath) const override;
    bool exists(const Utils::FilePath &filePath) const override;
    bool ensureExistingFile(const Utils::FilePath &filePath) const override;
    bool removeFile(const Utils::FilePath &filePath) const override;
    bool removeRecursively(const Utils::FilePath &filePath) const override;
    bool copyFile(const Utils::FilePath &filePath, const Utils::FilePath &target) const override;
    bool renameFile(const Utils::FilePath &filePath, const Utils::FilePath &target) const override;
    Utils::FilePath symLinkTarget(const Utils::FilePath &filePath) const override;
    void iterateDirectory(const Utils::FilePath &filePath,
                          const std::function<bool(const Utils::FilePath &)> &callBack,
                          const Utils::FileFilter &filter) const override;
    QByteArray fileContents(const Utils::FilePath &filePath, qint64 limit, qint64 offset) const override;
    bool writeFileContents(const Utils::FilePath &filePath, const QByteArray &data) const override;
    QDateTime lastModified(const Utils::FilePath &filePath) const override;
    qint64 fileSize(const Utils::FilePath &filePath) const override;
    QFileDevice::Permissions permissions(const Utils::FilePath &filePath) const override;
    bool setPermissions(const Utils::FilePath &filePath, QFileDevice::Permissions permissions) const override;

    Utils::Environment systemEnvironment() const override;

    const DockerDeviceData &data() const;
    DockerDeviceData &data();

    void updateContainerAccess() const;
    void setMounts(const QStringList &mounts) const;

    Utils::CommandLine withDockerExecCmd(const Utils::CommandLine& cmd, bool interactive = false) const;

protected:
    void fromMap(const QVariantMap &map) final;
    QVariantMap toMap() const final;

private:
    void iterateWithFind(const Utils::FilePath &filePath,
                         const std::function<bool(const Utils::FilePath &)> &callBack,
                         const Utils::FileFilter &filter) const;

    void aboutToBeRemoved() const final;

    class DockerDevicePrivate *d = nullptr;
    friend class DockerDeviceSetupWizard;
    friend class DockerDeviceWidget;
};

class DockerDeviceFactory final : public ProjectExplorer::IDeviceFactory
{
public:
    DockerDeviceFactory();
};

} // Internal
} // Docker

Q_DECLARE_METATYPE(Docker::Internal::DockerDeviceData)
