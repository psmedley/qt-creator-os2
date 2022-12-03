/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "mcutarget.h"
#include "mcukitmanager.h"
#include "mcupackage.h"
#include "mcusupportplugin.h"

#include <utils/algorithm.h>

using namespace Utils;

namespace McuSupport::Internal {

McuTarget::McuTarget(const QVersionNumber &qulVersion,
                     const Platform &platform,
                     OS os,
                     const Packages &packages,
                     const McuToolChainPackagePtr &toolChainPackage,
                     const McuPackagePtr &toolChainFilePackage,
                     int colorDepth)
    : m_qulVersion(qulVersion)
    , m_platform(platform)
    , m_os(os)
    , m_packages(packages)
    , m_toolChainPackage(toolChainPackage)
    , m_toolChainFilePackage(toolChainFilePackage)
    , m_colorDepth(colorDepth)
{}

Packages McuTarget::packages() const
{
    return m_packages;
}

McuToolChainPackagePtr McuTarget::toolChainPackage() const
{
    return m_toolChainPackage;
}

McuPackagePtr McuTarget::toolChainFilePackage() const
{
    return m_toolChainFilePackage;
}

McuTarget::OS McuTarget::os() const
{
    return m_os;
}

McuTarget::Platform McuTarget::platform() const
{
    return m_platform;
}

bool McuTarget::isValid() const
{
    return Utils::allOf(packages(), [](const McuPackagePtr &package) {
        package->updateStatus();
        return package->isValidStatus();
    });
}

void McuTarget::printPackageProblems() const
{
    for (auto package : packages()) {
        package->updateStatus();
        if (!package->isValidStatus())
            printMessage(tr("Error creating kit for target %1, package %2: %3")
                             .arg(McuKitManager::generateKitNameFromTarget(this),
                                  package->label(),
                                  package->statusText()),
                         true);
        if (package->status() == McuAbstractPackage::Status::ValidPackageMismatchedVersion)
            printMessage(tr("Warning creating kit for target %1, package %2: %3")
                             .arg(McuKitManager::generateKitNameFromTarget(this),
                                  package->label(),
                                  package->statusText()),
                         false);
    }
}

QVersionNumber McuTarget::qulVersion() const
{
    return m_qulVersion;
}

int McuTarget::colorDepth() const
{
    return m_colorDepth;
}

} // namespace McuSupport::Internal
