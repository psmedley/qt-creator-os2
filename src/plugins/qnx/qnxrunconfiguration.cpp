/****************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
** Contact: KDAB (info@kdab.com)
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

#include "qnxrunconfiguration.h"

#include "qnxconstants.h"

#include <projectexplorer/buildsystem.h>
#include <projectexplorer/deployablefile.h>
#include <projectexplorer/project.h>
#include <projectexplorer/runcontrol.h>
#include <projectexplorer/target.h>

#include <remotelinux/remotelinuxenvironmentaspect.h>

#include <qtsupport/qtoutputformatter.h>

using namespace ProjectExplorer;
using namespace RemoteLinux;
using namespace Utils;

namespace Qnx {
namespace Internal {

QnxRunConfiguration::QnxRunConfiguration(Target *target, Id id)
    : RunConfiguration(target, id)
{
    auto exeAspect = addAspect<ExecutableAspect>(target, ExecutableAspect::RunDevice);
    exeAspect->setLabelText(tr("Executable on device:"));
    exeAspect->setPlaceHolderText(tr("Remote path not set"));
    exeAspect->makeOverridable("RemoteLinux.RunConfig.AlternateRemoteExecutable",
                               "RemoteLinux.RunConfig.UseAlternateRemoteExecutable");
    exeAspect->setHistoryCompleter("RemoteLinux.AlternateExecutable.History");

    auto symbolsAspect = addAspect<SymbolFileAspect>();
    symbolsAspect->setLabelText(tr("Executable on host:"));
    symbolsAspect->setDisplayStyle(SymbolFileAspect::LabelDisplay);

    auto envAspect = addAspect<RemoteLinuxEnvironmentAspect>(target);

    addAspect<ArgumentsAspect>(macroExpander());
    addAspect<WorkingDirectoryAspect>(macroExpander(), envAspect);
    addAspect<TerminalAspect>();

    auto libAspect = addAspect<StringAspect>();
    libAspect->setSettingsKey("Qt4ProjectManager.QnxRunConfiguration.QtLibPath");
    libAspect->setLabelText(tr("Path to Qt libraries on device"));
    libAspect->setDisplayStyle(StringAspect::LineEditDisplay);

    setUpdater([this, target, exeAspect, symbolsAspect] {
        const BuildTargetInfo bti = buildTargetInfo();
        const FilePath localExecutable = bti.targetFilePath;
        const DeployableFile depFile = target->deploymentData()
            .deployableForLocalFile(localExecutable);
        exeAspect->setExecutable(FilePath::fromString(depFile.remoteFilePath()));
        symbolsAspect->setFilePath(localExecutable);
    });

    setRunnableModifier([libAspect](Runnable &r) {
        QString libPath = libAspect->value();
        if (!libPath.isEmpty()) {
            r.environment.appendOrSet("LD_LIBRARY_PATH", libPath + "/lib:$LD_LIBRARY_PATH");
            r.environment.appendOrSet("QML_IMPORT_PATH", libPath + "/imports:$QML_IMPORT_PATH");
            r.environment.appendOrSet("QML2_IMPORT_PATH", libPath + "/qml:$QML2_IMPORT_PATH");
            r.environment.appendOrSet("QT_PLUGIN_PATH", libPath + "/plugins:$QT_PLUGIN_PATH");
            r.environment.set("QT_QPA_FONTDIR", libPath + "/lib/fonts");
        }
    });

    connect(target, &Target::buildSystemUpdated, this, &RunConfiguration::update);
}

// QnxRunConfigurationFactory

QnxRunConfigurationFactory::QnxRunConfigurationFactory()
{
    registerRunConfiguration<QnxRunConfiguration>("Qt4ProjectManager.QNX.QNXRunConfiguration.");
    addSupportedTargetDeviceType(Constants::QNX_QNX_OS_TYPE);
}

} // namespace Internal
} // namespace Qnx
